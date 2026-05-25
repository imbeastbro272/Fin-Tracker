/*
 * ESP32 Smart Light with Embedded Machine Learning
 * ==================================================
 * Compatible with ESP32 Arduino Core v2.x AND v3.x
 *
 * Features:
 * - Real-time brightness prediction using Decision Tree
 * - Manual adjustment via potentiometer (additive offset)
 * - Online learning: Retrains model based on user adjustments
 * - Automatic data collection and periodic model updates
 *
 * Hardware:
 * - ESP32 DevKit
 * - LDR (Light Dependent Resistor) for ambient light sensing
 * - PIR Motion Sensor
 * - RTC Module (DS3231) for time tracking
 * - Potentiometer (10K) for manual adjustment
 * - LED strip or bulb (PWM controlled)
 *
 * Pin Configuration:
 * - LDR:           GPIO34 (ADC1_CH6)
 * - Motion Sensor: GPIO27
 * - Potentiometer: GPIO33 (ADC1_CH5)
 * - LED PWM:       GPIO2  (built-in LED on most boards)
 * - RTC I2C:       SDA=GPIO21, SCL=GPIO22
 *
 * Required Libraries (install via Arduino Library Manager):
 * - ArduinoJson  (v6.21 or v7.x by Benoit Blanchon)
 * - RTClib       (by Adafruit)
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <Wire.h>
#include <RTClib.h>
#include <math.h>

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
#define LDR_PIN          34   // Analog input for LDR
#define MOTION_PIN       27   // Digital input for PIR sensor
#define POT_PIN          33   // Analog input for potentiometer
#define LED_PWM_PIN      2    // PWM output for LED control (built-in LED)
#define PWM_FREQ         5000 // PWM frequency (Hz)
#define PWM_RESOLUTION   8    // 8-bit resolution (0-255)

// On older ESP32 Core (v2.x), we still use channels.
// On v3.x, channel parameter is ignored by our wrapper.
#define LED_CHANNEL      0

// ============================================================================
// COMPATIBILITY: ESP32 Arduino Core v2.x vs v3.x LEDC API
// ============================================================================
// In v3.0+ the API changed:
//   v2.x:  ledcSetup(ch, freq, res); ledcAttachPin(pin, ch); ledcWrite(ch, val);
//   v3.x:  ledcAttach(pin, freq, res);                       ledcWrite(pin, val);
//
// We detect the version using ESP_ARDUINO_VERSION_MAJOR and pick the correct API.

void pwmSetup(uint8_t pin, uint8_t channel, uint32_t freq, uint8_t resolution) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    // ESP32 Arduino Core v3.x and newer
    ledcAttach(pin, freq, resolution);
#else
    // ESP32 Arduino Core v2.x and older
    ledcSetup(channel, freq, resolution);
    ledcAttachPin(pin, channel);
#endif
}

void pwmWrite(uint8_t pin, uint8_t channel, uint32_t value) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcWrite(pin, value);
#else
    ledcWrite(channel, value);
#endif
}

// ============================================================================
// CONFIGURATION
// ============================================================================
#define MAX_LUX                50000.0  // Will be loaded from preprocessing.json
#define LEARNING_SAMPLES       50       // Retrain after this many samples
#define SAMPLE_INTERVAL        1000     // Sample every 1 second (ms) - changed from 10s
#define ADJUSTMENT_THRESHOLD   5.0      // Min adjustment CHANGE to trigger learning (%)

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
RTC_DS3231 rtc;
bool rtcAvailable = false;
unsigned long bootMillis = 0;

// Model parameters
float maxLux = MAX_LUX;

// Decision tree (loaded from JSON). 16KB capacity should fit most trees.
// If your tree is larger, increase this.
StaticJsonDocument<16384> treeDoc;
JsonObject treeRoot;

// Online learning buffer
struct DataSample {
    float lux_norm;
    int   motion_detected;
    float hour_sin;
    float hour_cos;
    int   time_of_day_enc;
    float effective_need;
    float target_brightness;  // DT prediction + pot adjustment (corrected value)
};

#define MAX_SAMPLES 200
DataSample learningBuffer[MAX_SAMPLES];
int sampleCount = 0;

// Runtime state
unsigned long lastSampleTime = 0;
float lastDTPrediction = 0.0;
float lastPotAdjustment = 0.0;
float previousPotAdjustment = 0.0;  // Track previous pot value to detect CHANGES
float currentBrightness = 0.0;
float serialAdjustment = 0.0;  // Manual adjustment from serial input
bool useSerialAdjustment = false;  // Flag to use serial instead of pot

// ============================================================================
// HELPER: Print a separator line (replaces invalid String('=', 80))
// ============================================================================
void printSeparator(char ch, int count) {
    for (int i = 0; i < count; i++) Serial.print(ch);
    Serial.println();
}

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
bool   loadModel();
void   loadLearningData();
void   saveLearningData();
void   retrainModel();
float  readLux();
float  readPotAdjustment();
void   handleSerialInput();
void   printHelp();
String getTimeOfDay(int hour);
int    getTimeEncoding(const String& timeOfDay);
void   setLEDBrightness(float percent);
float  predictBrightness(float lux_norm, int motion, float hour_sin,
                         float hour_cos, int time_enc, float effective_need);

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    bootMillis = millis();

    Serial.println();
    printSeparator('=', 80);
    Serial.println("ESP32 SMART LIGHT WITH EMBEDDED ML");
    printSeparator('=', 80);

    // Initialize pins
    pinMode(MOTION_PIN, INPUT);
    pinMode(LDR_PIN, INPUT);
    pinMode(POT_PIN, INPUT);

    // Setup PWM for LED (works on both v2.x and v3.x ESP32 cores)
    pwmSetup(LED_PWM_PIN, LED_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    pwmWrite(LED_PWM_PIN, LED_CHANNEL, 0);  // Start with LED off

    Serial.println();
    Serial.println("[1] Initializing Hardware...");

    // Initialize I2C for RTC
    Wire.begin(21, 22);  // SDA, SCL

    if (!rtc.begin()) {
        Serial.println("  X RTC not found. Using millis()-based fallback time.");
        rtcAvailable = false;
    } else {
        rtcAvailable = true;
        Serial.println("  + RTC initialized");
        if (rtc.lostPower()) {
            Serial.println("  ! RTC lost power, setting time from compile time...");
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
    }

    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("  X SPIFFS initialization failed!");
        return;
    }
    Serial.println("  + SPIFFS initialized");

    // Load model and preprocessing parameters
    Serial.println();
    Serial.println("[2] Loading ML Model...");
    if (!loadModel()) {
        Serial.println("  X Failed to load model. Running with fallback rules.");
    }

    // Load existing training data (from previous sessions)
    loadLearningData();

    Serial.println();
    Serial.println("[3] System Ready!");
    Serial.println("  - Automatic brightness control: ENABLED");
    Serial.println("  - Manual adjustment: ENABLED (potentiometer + serial)");
    Serial.println("  - Online learning: ENABLED");
    Serial.println();
    printHelp();
    printSeparator('=', 80);
    Serial.println();
}

// ============================================================================
// SERIAL INPUT HANDLER (for manual brightness control)
// ============================================================================

void handleSerialInput() {
    String input = Serial.readStringUntil('\n');
    input.trim();
    input.toLowerCase();
    
    if (input.length() == 0) return;
    
    // Check for numeric input (-50 to +50)
    if (input.startsWith("+") || input.startsWith("-") || isdigit(input.charAt(0))) {
        float value = input.toFloat();
        if (value >= -50.0 && value <= 50.0) {
            serialAdjustment = value;
            useSerialAdjustment = true;
            Serial.printf("Manual adjustment set to: %+.1f%% (via serial)\n", serialAdjustment);
            Serial.println("This will be added to DT prediction.");
        } else {
            Serial.println("ERROR: Value must be between -50 and +50");
        }
    }
    // Commands
    else if (input == "pot" || input == "potentiometer") {
        useSerialAdjustment = false;
        Serial.println("Switched to POTENTIOMETER mode");
    }
    else if (input == "auto" || input == "ml") {
        serialAdjustment = 0.0;
        useSerialAdjustment = true;
        Serial.println("AUTO mode: Pure ML control (adjustment = 0%)");
    }
    else if (input == "status" || input == "info") {
        Serial.println("\n=== CURRENT STATUS ===");
        Serial.printf("DT Prediction:      %.1f%%\n", lastDTPrediction);
        Serial.printf("Pot Adjustment:     %+.1f%%\n", lastPotAdjustment);
        Serial.printf("Serial Adjustment:  %+.1f%%\n", serialAdjustment);
        Serial.printf("Active Mode:        %s\n", useSerialAdjustment ? "SERIAL" : "POTENTIOMETER");
        Serial.printf("Final Brightness:   %.1f%%\n", currentBrightness);
        Serial.printf("Samples Collected:  %d / %d\n", sampleCount, LEARNING_SAMPLES);
        Serial.println("======================\n");
    }
    else if (input == "help" || input == "?") {
        printHelp();
    }
    else if (input == "reset") {
        serialAdjustment = 0.0;
        useSerialAdjustment = true;
        Serial.println("Serial adjustment reset to 0%");
    }
    else {
        Serial.println("Unknown command. Type 'help' for commands.");
    }
}

void printHelp() {
    Serial.println("\n=== MANUAL CONTROL COMMANDS ===");
    Serial.println("Brightness Adjustment:");
    Serial.println("  +20      : Add +20% to DT prediction");
    Serial.println("  -15      : Subtract 15% from DT prediction");
    Serial.println("  0        : Use pure DT prediction (no adjustment)");
    Serial.println("  (Range: -50 to +50)");
    Serial.println();
    Serial.println("Mode Selection:");
    Serial.println("  pot      : Switch to potentiometer control");
    Serial.println("  auto     : Pure ML mode (no adjustment)");
    Serial.println("  reset    : Reset serial adjustment to 0%");
    Serial.println();
    Serial.println("Information:");
    Serial.println("  status   : Show current values");
    Serial.println("  help     : Show this help");
    Serial.println();
    Serial.println("Examples:");
    Serial.println("  Type '+10' to add 10% brightness");
    Serial.println("  Type '-20' to reduce 20% brightness");
    Serial.println("  Type 'pot' to use physical knob");
    Serial.println("================================\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
    unsigned long currentTime = millis();
    
    // Check for serial input commands
    if (Serial.available()) {
        handleSerialInput();
    }

    if (currentTime - lastSampleTime >= SAMPLE_INTERVAL) {
        lastSampleTime = currentTime;

        // ---- Read sensors ----
        float lux    = readLux();
        int   motion = digitalRead(MOTION_PIN);

        int hour, minute, second;
        if (rtcAvailable) {
            DateTime now = rtc.now();
            hour   = now.hour();
            minute = now.minute();
            second = now.second();
        } else {
            // Fallback: derive a pseudo time-of-day from boot millis
            unsigned long secs = (currentTime - bootMillis) / 1000UL;
            hour   = (int)((secs / 3600UL) % 24UL);
            minute = (int)((secs / 60UL) % 60UL);
            second = (int)(secs % 60UL);
        }

        String timeOfDay = getTimeOfDay(hour);

        // ---- Compute features ----
        float lux_norm       = lux / maxLux;
        if (lux_norm > 1.0f) lux_norm = 1.0f;
        if (lux_norm < 0.0f) lux_norm = 0.0f;
        float hour_sin       = sin(2.0 * PI * hour / 24.0);
        float hour_cos       = cos(2.0 * PI * hour / 24.0);
        int   time_enc       = getTimeEncoding(timeOfDay);
        float effective_need = (1.0 - lux_norm) * motion;

        // ---- ML inference ----
        float dtBrightness = predictBrightness(lux_norm, motion, hour_sin,
                                               hour_cos, time_enc, effective_need);
        
        // RULE-BASED OVERRIDE: Cap brightness in bright conditions
        // This fixes the issue where model predicts high values in bright light
        if (lux > 2000.0f && motion == 1) {
            float cappedValue = 20.0f;  // Max 20% when bright + motion
            if (dtBrightness > cappedValue) {
                Serial.printf("  [Override] Bright condition (%.0f lux) - capping %.1f%% -> %.1f%%\n", 
                              lux, dtBrightness, cappedValue);
                dtBrightness = cappedValue;
            }
        }
        if (lux > 5000.0f) {
            float cappedValue = 10.0f;  // Max 10% when very bright
            if (dtBrightness > cappedValue) {
                Serial.printf("  [Override] Very bright (%.0f lux) - capping %.1f%% -> %.1f%%\n", 
                              lux, dtBrightness, cappedValue);
                dtBrightness = cappedValue;
            }
        }
        if (lux < 100.0f && motion == 0) {
            float cappedValue = 10.0f;  // Max 10% when dark + no motion
            if (dtBrightness > cappedValue) {
                Serial.printf("  [Override] Dark + no motion - capping %.1f%% -> %.1f%%\n", 
                              dtBrightness, cappedValue);
                dtBrightness = cappedValue;
            }
        }
        
        lastDTPrediction = dtBrightness;

        // ---- Manual adjustment (potentiometer OR serial) ----
        float currentAdjustment;
        const char* adjustmentSource;
        
        if (useSerialAdjustment) {
            // Use serial input
            currentAdjustment = serialAdjustment;
            adjustmentSource = "Serial";
            lastPotAdjustment = currentAdjustment;  // Update for display
        } else {
            // Use potentiometer
            currentAdjustment = readPotAdjustment();
            lastPotAdjustment = currentAdjustment;
            adjustmentSource = "Pot";
        }

        // ---- Final brightness ----
        float finalBrightness = dtBrightness + currentAdjustment;
        if (finalBrightness < 0.0f)   finalBrightness = 0.0f;
        if (finalBrightness > 100.0f) finalBrightness = 100.0f;
        currentBrightness = finalBrightness;

        setLEDBrightness(finalBrightness);

        Serial.printf("[%02d:%02d:%02d] Lux: %6.0f | Motion: %d | DT: %5.1f%% | %s: %+5.1f%% | Final: %5.1f%%\n",
                      hour, minute, second, lux, motion,
                      dtBrightness, adjustmentSource, currentAdjustment, finalBrightness);

        // ---- Online learning trigger ----
        // FIXED: Only trigger when adjustment CHANGES by >5%, not when it's just >5%
        float adjustmentChange = fabs(currentAdjustment - previousPotAdjustment);
        
        if (adjustmentChange > ADJUSTMENT_THRESHOLD) {
            Serial.printf("  -> User adjustment CHANGED by %.1f%%. Collecting training sample...\n", adjustmentChange);

            if (sampleCount < MAX_SAMPLES) {
                DataSample& s = learningBuffer[sampleCount++];
                s.lux_norm          = lux_norm;
                s.motion_detected   = motion;
                s.hour_sin          = hour_sin;
                s.hour_cos          = hour_cos;
                s.time_of_day_enc   = time_enc;
                s.effective_need    = effective_need;
                s.target_brightness = finalBrightness;
                
                Serial.printf("     Sample %d/%d collected (target brightness: %.1f%%)\n", 
                             sampleCount, LEARNING_SAMPLES, finalBrightness);
            }

            if (sampleCount >= LEARNING_SAMPLES) {
                Serial.println();
                Serial.println("  ** Sufficient samples collected. Retraining model... **");
                retrainModel();
                saveLearningData();
                sampleCount = 0;
            }
            
            // Update previous value AFTER learning trigger
            previousPotAdjustment = currentAdjustment;
        }
    }

    delay(100);
}

// ============================================================================
// SENSOR READING
// ============================================================================

float readLux() {
    // Average multiple samples for stability
    const int SAMPLES = 20;
    long sum = 0;
    for (int i = 0; i < SAMPLES; i++) {
        sum += analogRead(LDR_PIN);
        delay(2);
    }
    int raw = sum / SAMPLES;
    
    if (raw <= 0) raw = 1;  // avoid div-by-zero

    // Convert ADC reading to voltage
    float voltage = (raw / 4095.0f) * 3.3f;
    if (voltage <= 0.01f) voltage = 0.01f;
    if (voltage >= 3.29f) voltage = 3.29f;

    // Calculate LDR resistance using voltage divider formula
    // Voltage divider: 3.3V -- LDR -- ADC_PIN -- 10K -- GND
    // Formula: V_adc = V_cc * R_fixed / (R_ldr + R_fixed)
    // Rearranged: R_ldr = R_fixed * V_adc / (V_cc - V_adc)
    const float R_FIXED = 10000.0f;   // 10K pull-down resistor
    const float V_CC = 3.3f;
    
    float r_ldr = R_FIXED * voltage / (V_CC - voltage);
    if (r_ldr <= 0.0f) r_ldr = 1.0f;

    // Convert resistance to lux using calibrated formula
    // Formula derived from GL5528 datasheet: Lux = A * R^B
    const float LDR_A = 32768000.0f;  // Calibration constant A
    const float LDR_B = -1.4f;        // Calibration exponent B
    
    float lux = LDR_A * pow(r_ldr, LDR_B);

    // Clamp to valid range
    if (lux < 0.0f)    lux = 0.0f;
    if (lux > maxLux)  lux = maxLux;
    
    return lux;
}

float readPotAdjustment() {
    // Read pot and map to -50..+50%
    int raw = analogRead(POT_PIN);
    float adjustment = ((float)raw / 4095.0f) * 100.0f - 50.0f;  // -50..+50
    return adjustment;
}

String getTimeOfDay(int hour) {
    if (hour >= 5  && hour < 8)  return "Early Morning";
    if (hour >= 8  && hour < 12) return "Morning";
    if (hour >= 12 && hour < 17) return "Afternoon";
    if (hour >= 17 && hour < 20) return "Evening";
    return "Night";
}

int getTimeEncoding(const String& timeOfDay) {
    // Must match preprocessing.json ordering (alphabetical from sklearn LabelEncoder)
    if (timeOfDay == "Afternoon")     return 0;
    if (timeOfDay == "Early Morning") return 1;
    if (timeOfDay == "Evening")       return 2;
    if (timeOfDay == "Morning")       return 3;
    if (timeOfDay == "Night")         return 4;
    return 0;
}

// ============================================================================
// LED CONTROL
// ============================================================================

void setLEDBrightness(float percent) {
    if (percent < 0.0f)   percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;
    int pwmValue = (int)(percent * 255.0f / 100.0f);
    pwmWrite(LED_PWM_PIN, LED_CHANNEL, pwmValue);
}

// ============================================================================
// DECISION TREE INFERENCE
// ============================================================================

float predictBrightness(float lux_norm, int motion, float hour_sin,
                        float hour_cos, int time_enc, float effective_need) {
    if (treeRoot.isNull()) {
        // Fallback rule if model failed to load
        return motion ? 70.0f : 0.0f;
    }

    float features[6] = {
        lux_norm,
        (float)motion,
        hour_sin,
        hour_cos,
        (float)time_enc,
        effective_need
    };

    JsonObject node = treeRoot;
    int safety = 0;

    while (node["type"] == "split" && safety++ < 100) {
        int   featureIdx = node["feature_idx"];
        float threshold  = node["threshold"];

        if (featureIdx < 0 || featureIdx >= 6) break;

        if (features[featureIdx] <= threshold) {
            node = node["left"].as<JsonObject>();
        } else {
            node = node["right"].as<JsonObject>();
        }
    }

    return node["value"];
}

// ============================================================================
// MODEL PERSISTENCE
// ============================================================================

bool loadModel() {
    // Load tree model
    File file = SPIFFS.open("/tree_model.json", "r");
    if (!file) {
        Serial.println("  X tree_model.json not found in SPIFFS");
        return false;
    }

    DeserializationError error = deserializeJson(treeDoc, file);
    file.close();

    if (error) {
        Serial.print("  X Failed to parse tree_model.json: ");
        Serial.println(error.c_str());
        return false;
    }

    treeRoot = treeDoc["tree"].as<JsonObject>();
    Serial.println("  + Decision tree loaded");
    Serial.printf("    - Features:  %d\n", (int)treeDoc["n_features"]);
    Serial.printf("    - Max Depth: %d\n", (int)treeDoc["max_depth"]);
    Serial.printf("    - Leaves:    %d\n", (int)treeDoc["n_leaves"]);

    // Load preprocessing parameters
    file = SPIFFS.open("/preprocessing.json", "r");
    if (!file) {
        Serial.println("  X preprocessing.json not found");
        return false;
    }

    StaticJsonDocument<512> prepDoc;
    error = deserializeJson(prepDoc, file);
    file.close();

    if (error) {
        Serial.print("  X Failed to parse preprocessing.json: ");
        Serial.println(error.c_str());
        return false;
    }

    maxLux = prepDoc["max_lux"];
    Serial.printf("  + Preprocessing params loaded (max_lux: %.0f)\n", maxLux);

    return true;
}

void saveLearningData() {
    File file = SPIFFS.open("/learning_data.bin", "w");
    if (!file) {
        Serial.println("  X Failed to open learning_data.bin for writing");
        return;
    }

    file.write((uint8_t*)&sampleCount, sizeof(int));
    file.write((uint8_t*)learningBuffer, sizeof(DataSample) * sampleCount);
    file.close();

    Serial.printf("  + Saved %d training samples\n", sampleCount);
}

void loadLearningData() {
    File file = SPIFFS.open("/learning_data.bin", "r");
    if (!file) {
        Serial.println("  ! No existing learning data found (fresh start)");
        sampleCount = 0;
        return;
    }

    file.read((uint8_t*)&sampleCount, sizeof(int));
    if (sampleCount > MAX_SAMPLES) sampleCount = MAX_SAMPLES;
    if (sampleCount < 0)           sampleCount = 0;
    file.read((uint8_t*)learningBuffer, sizeof(DataSample) * sampleCount);
    file.close();

    Serial.printf("  + Loaded %d existing training samples\n", sampleCount);
}

// ============================================================================
// ONLINE LEARNING (incremental leaf-value update)
// ============================================================================

void retrainModel() {
    Serial.println();
    printSeparator('-', 80);
    Serial.println("RETRAINING MODEL WITH USER FEEDBACK");
    printSeparator('-', 80);

    // Simplified online learning: for each new sample, find the leaf it
    // falls into and nudge that leaf's value toward the user's correction.
    // Full CART retraining is too heavy for ESP32 in real-time.

    const float learningRate = 0.1f;

    for (int i = 0; i < sampleCount; i++) {
        DataSample& sample = learningBuffer[i];

        float features[6] = {
            sample.lux_norm,
            (float)sample.motion_detected,
            sample.hour_sin,
            sample.hour_cos,
            (float)sample.time_of_day_enc,
            sample.effective_need
        };

        JsonObject node = treeRoot;
        int safety = 0;
        while (node["type"] == "split" && safety++ < 100) {
            int   featureIdx = node["feature_idx"];
            float threshold  = node["threshold"];

            if (featureIdx < 0 || featureIdx >= 6) break;

            if (features[featureIdx] <= threshold) {
                node = node["left"].as<JsonObject>();
            } else {
                node = node["right"].as<JsonObject>();
            }
        }

        float currentValue = node["value"];
        float newValue     = currentValue + learningRate * (sample.target_brightness - currentValue);
        node["value"]      = newValue;

        Serial.printf("  Sample %3d: leaf %5.1f%% -> %5.1f%%  (target %5.1f%%)\n",
                      i + 1, currentValue, newValue, sample.target_brightness);
    }

    // Persist updated model back to SPIFFS
    File file = SPIFFS.open("/tree_model.json", "w");
    if (file) {
        serializeJson(treeDoc, file);
        file.close();
        Serial.println("  + Updated model saved to SPIFFS");
    } else {
        Serial.println("  X Failed to save updated model");
    }

    printSeparator('-', 80);
    Serial.println();
}
