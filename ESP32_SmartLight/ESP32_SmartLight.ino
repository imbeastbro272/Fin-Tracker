/*
 * ESP32 Smart Light with Embedded Machine Learning
 * ==================================================
 * Features:
 * - Real-time brightness prediction using Decision Tree
 * - Manual adjustment via potentiometer (additive offset)
 * - Online learning: Retrains model based on user adjustments
 * - Automatic data collection and periodic model updates
 * 
 * Hardware Requirements:
 * - ESP32 DevKit
 * - LDR (Light Dependent Resistor) for ambient light sensing
 * - PIR Motion Sensor
 * - RTC Module (DS3231) for time tracking
 * - Potentiometer (10K) for manual adjustment
 * - LED strip or bulb (PWM controlled)
 * - 10K resistor for LDR voltage divider
 * 
 * Pin Configuration (see README for wiring diagram):
 * - LDR:           GPIO34 (ADC1_CH6)
 * - Motion Sensor: GPIO23
 * - Potentiometer: GPIO35 (ADC1_CH7)
 * - LED PWM:       GPIO16
 * - RTC I2C:       SDA=GPIO21, SCL=GPIO22
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
#define MOTION_PIN       23   // Digital input for PIR sensor
#define POT_PIN          35   // Analog input for potentiometer
#define LED_PWM_PIN      16   // PWM output for LED control
#define LED_CHANNEL      0    // PWM channel
#define PWM_FREQ         5000 // PWM frequency (Hz)
#define PWM_RESOLUTION   8    // 8-bit resolution (0-255)

// ============================================================================
// CONFIGURATION
// ============================================================================
#define MAX_LUX          50000.0  // Will be loaded from preprocessing.json
#define LEARNING_SAMPLES 50       // Retrain after this many samples
#define SAMPLE_INTERVAL  10000    // Sample every 10 seconds (ms)
#define ADJUSTMENT_THRESHOLD 5.0  // Minimum adjustment to trigger learning (%)

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
RTC_DS3231 rtc;

// Model parameters
float maxLux = MAX_LUX;
int timeEncodings[5];  // Mappings for time of day

// Decision tree structure (loaded from JSON)
JsonDocument treeDoc;
JsonObject treeRoot;

// Online learning buffer
struct DataSample {
    float lux_norm;
    int motion_detected;
    float hour_sin;
    float hour_cos;
    int time_of_day_enc;
    float effective_need;
    float target_brightness;  // DT prediction + pot adjustment
};

#define MAX_SAMPLES 200
DataSample learningBuffer[MAX_SAMPLES];
int sampleCount = 0;

// Runtime state
unsigned long lastSampleTime = 0;
float lastDTPrediction = 0.0;
float lastPotAdjustment = 0.0;
float currentBrightness = 0.0;

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n" + String('=', 80));
    Serial.println("ESP32 SMART LIGHT WITH EMBEDDED ML");
    Serial.println(String('=', 80));
    
    // Initialize pins
    pinMode(MOTION_PIN, INPUT);
    pinMode(LDR_PIN, INPUT);
    pinMode(POT_PIN, INPUT);
    
    // Setup PWM for LED
    ledcSetup(LED_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(LED_PWM_PIN, LED_CHANNEL);
    ledcWrite(LED_CHANNEL, 0);  // Start with LED off
    
    Serial.println("\n[1] Initializing Hardware...");
    
    // Initialize I2C for RTC
    Wire.begin(21, 22);  // SDA, SCL
    
    if (!rtc.begin()) {
        Serial.println("  ✗ RTC not found! Using fallback time.");
    } else {
        Serial.println("  ✓ RTC initialized");
        if (rtc.lostPower()) {
            Serial.println("  ⚠ RTC lost power, setting time...");
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
    }
    
    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("  ✗ SPIFFS initialization failed!");
        return;
    }
    Serial.println("  ✓ SPIFFS initialized");
    
    // Load model and preprocessing parameters
    Serial.println("\n[2] Loading ML Model...");
    if (!loadModel()) {
        Serial.println("  ✗ Failed to load model! Running without ML.");
    }
    
    // Load existing training data
    loadLearningData();
    
    Serial.println("\n[3] System Ready!");
    Serial.println("  - Automatic brightness control: ENABLED");
    Serial.println("  - Manual adjustment: ENABLED (potentiometer)");
    Serial.println("  - Online learning: ENABLED");
    Serial.println(String('=', 80) + "\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
    unsigned long currentTime = millis();
    
    // Sample sensors and update brightness
    if (currentTime - lastSampleTime >= SAMPLE_INTERVAL) {
        lastSampleTime = currentTime;
        
        // Read sensors
        float lux = readLux();
        int motion = digitalRead(MOTION_PIN);
        DateTime now = rtc.now();
        int hour = now.hour();
        String timeOfDay = getTimeOfDay(hour);
        
        // Compute features
        float lux_norm = lux / maxLux;
        float hour_sin = sin(2.0 * PI * hour / 24.0);
        float hour_cos = cos(2.0 * PI * hour / 24.0);
        int time_enc = getTimeEncoding(timeOfDay);
        float effective_need = (1.0 - lux_norm) * motion;
        
        // Predict brightness using decision tree
        float dtBrightness = predictBrightness(lux_norm, motion, hour_sin, hour_cos, time_enc, effective_need);
        lastDTPrediction = dtBrightness;
        
        // Read potentiometer adjustment (-50% to +50%)
        float potAdjustment = readPotAdjustment();
        lastPotAdjustment = potAdjustment;
        
        // Final brightness = DT prediction + pot adjustment
        float finalBrightness = constrain(dtBrightness + potAdjustment, 0.0, 100.0);
        currentBrightness = finalBrightness;
        
        // Update LED
        setLEDBrightness(finalBrightness);
        
        // Log to serial
        Serial.printf("[%02d:%02d:%02d] Lux: %.0f | Motion: %d | DT: %.1f%% | Pot: %+.1f%% | Final: %.1f%%\n",
                      hour, now.minute(), now.second(), lux, motion, dtBrightness, potAdjustment, finalBrightness);
        
        // Check if user made significant adjustment -> trigger learning
        if (abs(potAdjustment) > ADJUSTMENT_THRESHOLD) {
            Serial.printf("  → User adjustment detected! Collecting training sample...\n");
            
            // Store sample for retraining
            if (sampleCount < MAX_SAMPLES) {
                DataSample sample;
                sample.lux_norm = lux_norm;
                sample.motion_detected = motion;
                sample.hour_sin = hour_sin;
                sample.hour_cos = hour_cos;
                sample.time_of_day_enc = time_enc;
                sample.effective_need = effective_need;
                sample.target_brightness = finalBrightness;  // Corrected value
                
                learningBuffer[sampleCount++] = sample;
                
                // Check if we should retrain
                if (sampleCount >= LEARNING_SAMPLES) {
                    Serial.println("\n  ⚡ Sufficient samples collected! Retraining model...");
                    retrainModel();
                    saveLearningData();
                    sampleCount = 0;  // Reset buffer
                }
            }
        }
    }
    
    delay(100);  // Small delay to prevent overwhelming the serial output
}

// ============================================================================
// SENSOR READING FUNCTIONS
// ============================================================================

float readLux() {
    // Read LDR and convert to lux
    // Assuming voltage divider: VCC -- 10K -- LDR -- GND
    int raw = analogRead(LDR_PIN);
    float voltage = raw * (3.3 / 4095.0);
    
    // Convert voltage to resistance (simplified)
    float resistance = (3.3 - voltage) * 10000.0 / voltage;
    
    // Convert resistance to lux (calibration needed for your specific LDR)
    // Using typical GL5528 LDR characteristic curve
    float lux = pow(10, (log10(resistance / 1000.0) - 1.5) / -0.7);
    
    return constrain(lux, 0, maxLux);
}

float readPotAdjustment() {
    // Read potentiometer and map to -50% to +50%
    int raw = analogRead(POT_PIN);
    float adjustment = map(raw, 0, 4095, -50, 50);
    return adjustment;
}

String getTimeOfDay(int hour) {
    if (hour >= 5 && hour < 8) return "Early Morning";
    if (hour >= 8 && hour < 12) return "Morning";
    if (hour >= 12 && hour < 17) return "Afternoon";
    if (hour >= 17 && hour < 20) return "Evening";
    return "Night";
}

int getTimeEncoding(String timeOfDay) {
    // Map time of day to encoding (loaded from preprocessing.json)
    if (timeOfDay == "Afternoon") return 0;
    if (timeOfDay == "Early Morning") return 1;
    if (timeOfDay == "Evening") return 2;
    if (timeOfDay == "Morning") return 3;
    if (timeOfDay == "Night") return 4;
    return 0;
}

// ============================================================================
// LED CONTROL
// ============================================================================

void setLEDBrightness(float percent) {
    // Convert 0-100% to 0-255 PWM value
    int pwmValue = (int)(percent * 255.0 / 100.0);
    ledcWrite(LED_CHANNEL, pwmValue);
}

// ============================================================================
// DECISION TREE INFERENCE
// ============================================================================

float predictBrightness(float lux_norm, int motion, float hour_sin, float hour_cos, 
                        int time_enc, float effective_need) {
    if (treeRoot.isNull()) {
        // Fallback if model not loaded
        return motion ? 70.0 : 0.0;
    }
    
    // Feature array in correct order
    float features[6] = {lux_norm, (float)motion, hour_sin, hour_cos, (float)time_enc, effective_need};
    
    // Traverse tree
    JsonObject node = treeRoot;
    
    while (node["type"] == "split") {
        int featureIdx = node["feature_idx"];
        float threshold = node["threshold"];
        
        if (features[featureIdx] <= threshold) {
            node = node["left"].as<JsonObject>();
        } else {
            node = node["right"].as<JsonObject>();
        }
    }
    
    // Leaf node
    return node["value"];
}

// ============================================================================
// MODEL PERSISTENCE
// ============================================================================

bool loadModel() {
    // Load tree model
    File file = SPIFFS.open("/tree_model.json", "r");
    if (!file) {
        Serial.println("  ✗ tree_model.json not found in SPIFFS");
        return false;
    }
    
    DeserializationError error = deserializeJson(treeDoc, file);
    file.close();
    
    if (error) {
        Serial.println("  ✗ Failed to parse tree_model.json");
        return false;
    }
    
    treeRoot = treeDoc["tree"].as<JsonObject>();
    Serial.println("  ✓ Decision tree loaded");
    Serial.printf("    - Features: %d\n", (int)treeDoc["n_features"]);
    Serial.printf("    - Max Depth: %d\n", (int)treeDoc["max_depth"]);
    Serial.printf("    - Leaves: %d\n", (int)treeDoc["n_leaves"]);
    
    // Load preprocessing parameters
    file = SPIFFS.open("/preprocessing.json", "r");
    if (!file) {
        Serial.println("  ✗ preprocessing.json not found");
        return false;
    }
    
    JsonDocument prepDoc;
    error = deserializeJson(prepDoc, file);
    file.close();
    
    if (error) {
        Serial.println("  ✗ Failed to parse preprocessing.json");
        return false;
    }
    
    maxLux = prepDoc["max_lux"];
    Serial.printf("  ✓ Preprocessing params loaded (max_lux: %.0f)\n", maxLux);
    
    return true;
}

void saveLearningData() {
    // Save collected training samples to SPIFFS
    File file = SPIFFS.open("/learning_data.bin", "w");
    if (!file) {
        Serial.println("  ✗ Failed to open learning_data.bin for writing");
        return;
    }
    
    file.write((uint8_t*)&sampleCount, sizeof(int));
    file.write((uint8_t*)learningBuffer, sizeof(DataSample) * sampleCount);
    file.close();
    
    Serial.printf("  ✓ Saved %d training samples\n", sampleCount);
}

void loadLearningData() {
    File file = SPIFFS.open("/learning_data.bin", "r");
    if (!file) {
        Serial.println("  ⚠ No existing learning data found (fresh start)");
        sampleCount = 0;
        return;
    }
    
    file.read((uint8_t*)&sampleCount, sizeof(int));
    if (sampleCount > MAX_SAMPLES) sampleCount = MAX_SAMPLES;
    file.read((uint8_t*)learningBuffer, sizeof(DataSample) * sampleCount);
    file.close();
    
    Serial.printf("  ✓ Loaded %d existing training samples\n", sampleCount);
}

// ============================================================================
// ONLINE LEARNING (SIMPLIFIED DECISION TREE RETRAINING)
// ============================================================================

void retrainModel() {
    Serial.println("\n" + String('-', 80));
    Serial.println("RETRAINING MODEL WITH USER FEEDBACK");
    Serial.println(String('-', 80));
    
    // In a production system, you would implement a lightweight decision tree
    // training algorithm here (simplified CART or ID3).
    // 
    // For ESP32, full retraining is computationally expensive. Options:
    // 1. Incremental update (adjust leaf values based on new samples)
    // 2. Upload samples to cloud for retraining, download updated model
    // 3. Implement lightweight gradient boosting or ensemble update
    
    // SIMPLIFIED APPROACH: Update leaf node values
    // Find which leaf each sample falls into and adjust its prediction
    
    for (int i = 0; i < sampleCount; i++) {
        DataSample& sample = learningBuffer[i];
        
        // Find the leaf node this sample falls into
        JsonObject node = treeRoot;
        float features[6] = {
            sample.lux_norm, 
            (float)sample.motion_detected,
            sample.hour_sin,
            sample.hour_cos,
            (float)sample.time_of_day_enc,
            sample.effective_need
        };
        
        // Traverse to leaf
        while (node["type"] == "split") {
            int featureIdx = node["feature_idx"];
            float threshold = node["threshold"];
            
            if (features[featureIdx] <= threshold) {
                node = node["left"].as<JsonObject>();
            } else {
                node = node["right"].as<JsonObject>();
            }
        }
        
        // Update leaf value (simple moving average with learning rate)
        float currentValue = node["value"];
        float learningRate = 0.1;  // How much to adjust
        float newValue = currentValue + learningRate * (sample.target_brightness - currentValue);
        node["value"] = newValue;
        
        Serial.printf("  Sample %d: Adjusted leaf %.1f%% → %.1f%%\n", 
                      i + 1, currentValue, newValue);
    }
    
    // Save updated model
    File file = SPIFFS.open("/tree_model.json", "w");
    if (file) {
        serializeJson(treeDoc, file);
        file.close();
        Serial.println("  ✓ Updated model saved to SPIFFS");
    } else {
        Serial.println("  ✗ Failed to save updated model");
    }
    
    Serial.println(String('-', 80) + "\n");
}
