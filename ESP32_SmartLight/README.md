# ESP32 Smart Light with Embedded Machine Learning

A complete edge ML solution that runs a Decision Tree model locally on ESP32 to control LED brightness automatically, with online learning capabilities based on user adjustments.

## 🌟 Features

- **Real-time ML Inference**: Decision Tree model runs directly on ESP32
- **Automatic Brightness Control**: Based on ambient light, motion, and time of day
- **Manual Override**: Potentiometer provides -50% to +50% adjustment
- **Online Learning**: Model retrains itself based on your adjustments
- **Persistent Storage**: Model and training data saved to SPIFFS
- **Low Latency**: Sub-millisecond prediction time

## 📋 Hardware Requirements

| Component | Quantity | Purpose |
|-----------|----------|---------|
| ESP32 DevKit | 1 | Main controller |
| LDR (GL5528 or similar) | 1 | Ambient light sensing (0-50K lux) |
| PIR Motion Sensor (HC-SR501) | 1 | Motion detection |
| RTC Module (DS3231) | 1 | Accurate timekeeping |
| Potentiometer (10KΩ) | 1 | Manual brightness adjustment |
| LED Strip/Bulb | 1 | Light output (PWM controllable) |
| MOSFET (IRLZ44N) | 1 | High-power LED switching |
| Resistors | - | 10KΩ (LDR divider), 10KΩ (pot pulldown) |
| Breadboard & Wires | - | Prototyping |

## 🔌 Wiring Diagram

```
ESP32 Pin Configuration:
========================

┌─────────────────────────────────────────────────────────────────┐
│                         ESP32 DevKit                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  GPIO34 (ADC1_CH6) ───────┐                                     │
│                            │  LDR Circuit                        │
│                          [LDR]                                   │
│                            │                                     │
│                          [10KΩ]                                  │
│                            │                                     │
│                           GND                                    │
│                                                                  │
│  GPIO23 ──────────────── PIR OUT                                │
│                                                                  │
│  GPIO35 (ADC1_CH7) ──────┐                                      │
│                        [10KΩ POT]                                │
│                            │                                     │
│                           GND                                    │
│                                                                  │
│  GPIO16 ────────────┐                                           │
│                     │                                            │
│                   [1KΩ]                                          │
│                     │                                            │
│                 MOSFET Gate (IRLZ44N)                            │
│                     │                                            │
│                   Drain ───── LED Strip (+)                      │
│                     │                                            │
│                   Source ─── GND                                 │
│                                                                  │
│  GPIO21 (SDA) ───────── RTC SDA (DS3231)                        │
│  GPIO22 (SCL) ───────── RTC SCL (DS3231)                        │
│                                                                  │
│  3.3V ─────────────────── RTC VCC, PIR VCC                      │
│  GND ──────────────────── Common Ground                          │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘

LED Power Supply:
=================
12V Power Supply (+) ──── LED Strip (+)
                     │
                     └──── MOSFET Drain

12V Power Supply (-) ──── GND (Common)


LDR Voltage Divider Detail:
============================
3.3V ──┬── [10KΩ Resistor] ──┬── [LDR] ── GND
       │                      │
       │                   GPIO34
       
As light increases → LDR resistance decreases → Voltage at GPIO34 increases


Potentiometer Detail:
=====================
3.3V ──┬── Potentiometer ──┬── GND
       │                   │
                        GPIO35
                        
Center position → ~1.65V (0% adjustment)
Full CW → 3.3V (+50% adjustment)
Full CCW → 0V (-50% adjustment)
```

## 📦 Software Dependencies

### Arduino IDE Libraries

Install these via Arduino Library Manager:

```
1. ArduinoJson (v6.x or later)
2. RTClib (by Adafruit)
3. ESP32 Board Support (by Espressif)
```

### Python Libraries (for model export)

```bash
pip install scikit-learn pandas numpy matplotlib seaborn
```

## 🚀 Installation & Setup

### Step 1: Train Your Model (Python)

1. Run your training script with the provided dataset:

```bash
python your_training_script.py
```

2. After training completes, add this code at the end:

```python
from export_model_to_esp32 import export_tree_to_json, export_preprocessing_params

# Export decision tree structure
export_tree_to_json(best_model, features_final, 'tree_model.json')

# Export preprocessing parameters  
export_preprocessing_params(le, MAX_LUX, 'preprocessing.json')
```

This generates:
- `tree_model.json` - Decision tree structure
- `preprocessing.json` - Normalization parameters

### Step 2: Upload Model to ESP32

1. Install **ESP32 Filesystem Uploader** for Arduino IDE:
   - Download from: https://github.com/me-no-dev/arduino-esp32fs-plugin
   - Extract to `<Arduino>/tools/ESP32FS/tool/`
   - Restart Arduino IDE

2. Create data folder in sketch directory:
```
ESP32_SmartLight/
├── ESP32_SmartLight.ino
└── data/
    ├── tree_model.json
    └── preprocessing.json
```

3. Copy `tree_model.json` and `preprocessing.json` to `data/` folder

4. In Arduino IDE: **Tools → ESP32 Sketch Data Upload**
   - This uploads files to SPIFFS

### Step 3: Upload Arduino Sketch

1. Open `ESP32_SmartLight.ino` in Arduino IDE

2. Configure board settings:
   - Board: "ESP32 Dev Module"
   - Upload Speed: 921600
   - Flash Frequency: 80MHz
   - Flash Mode: QIO
   - Flash Size: 4MB (with SPIFFS)
   - Partition Scheme: "Default 4MB with spiffs"
   - Port: Select your ESP32 port

3. Click **Upload** (takes ~30 seconds)

### Step 4: Verify Operation

1. Open Serial Monitor (115200 baud)

2. You should see:
```
================================================================================
ESP32 SMART LIGHT WITH EMBEDDED ML
================================================================================

[1] Initializing Hardware...
  ✓ RTC initialized
  ✓ SPIFFS initialized

[2] Loading ML Model...
  ✓ Decision tree loaded
    - Features: 6
    - Max Depth: 10
    - Leaves: 42
  ✓ Preprocessing params loaded (max_lux: 50000)

[3] System Ready!
  - Automatic brightness control: ENABLED
  - Manual adjustment: ENABLED (potentiometer)
  - Online learning: ENABLED
================================================================================
```

## 🎯 Usage

### Automatic Mode

The system reads sensors every 10 seconds and automatically adjusts LED brightness based on:
- **Ambient light** (LDR reading)
- **Motion detection** (PIR sensor)
- **Time of day** (RTC module)

Serial output shows real-time predictions:
```
[22:45:30] Lux: 250 | Motion: 1 | DT: 78.5% | Pot: +0.0% | Final: 78.5%
[22:45:40] Lux: 240 | Motion: 1 | DT: 79.2% | Pot: +0.0% | Final: 79.2%
```

### Manual Adjustment Mode

Turn the potentiometer to add an offset:
- **Center position**: 0% offset (pure ML control)
- **Clockwise**: +50% boost (brighter)
- **Counter-clockwise**: -50% reduction (dimmer)

Example:
```
[22:45:50] Lux: 235 | Motion: 1 | DT: 79.0% | Pot: +15.0% | Final: 94.0%
  → User adjustment detected! Collecting training sample...
```

### Online Learning

When you adjust the potentiometer by more than 5%:
1. System records: `[sensor values] → [your corrected brightness]`
2. After 50 adjustments, model automatically retrains
3. Updated model is saved to SPIFFS

```
  ⚡ Sufficient samples collected! Retraining model...
  
--------------------------------------------------------------------------------
RETRAINING MODEL WITH USER FEEDBACK
--------------------------------------------------------------------------------
  Sample 1: Adjusted leaf 79.0% → 81.5%
  Sample 2: Adjusted leaf 45.2% → 48.8%
  ...
  Sample 50: Adjusted leaf 92.1% → 90.3%
  ✓ Updated model saved to SPIFFS
--------------------------------------------------------------------------------
```

The model now incorporates your preferences!

## 🧪 Testing & Calibration

### LDR Calibration

Your LDR may have different characteristics. Calibrate using this process:

1. Measure lux with a calibrated lux meter in various conditions
2. Read ESP32 voltage at GPIO34
3. Adjust the conversion formula in `readLux()`:

```cpp
// Current formula (for GL5528)
float lux = pow(10, (log10(resistance / 1000.0) - 1.5) / -0.7);

// Adjust coefficients based on your LDR datasheet
```

### Test Scenarios

| Scenario | Expected Lux | Motion | Expected Brightness |
|----------|--------------|--------|---------------------|
| Bright day | 10,000+ | No | 0-5% |
| Bright day | 10,000+ | Yes | 5-15% |
| Evening | 200-500 | Yes | 60-80% |
| Night (dark) | 0-50 | Yes | 75-95% |
| Night (lit) | 1000+ | Yes | 10-20% |

### Potentiometer Testing

1. Set pot to center → Serial should show `Pot: +0.0%`
2. Turn fully clockwise → `Pot: +50.0%`
3. Turn fully counter-clockwise → `Pot: -50.0%`

If readings are inverted, swap the pot's outer pins.

## 🔧 Troubleshooting

### Model Not Loading

**Symptom**: `✗ tree_model.json not found in SPIFFS`

**Solution**:
1. Verify files exist in `data/` folder
2. Re-run "ESP32 Sketch Data Upload"
3. Check SPIFFS partition size (min 1MB required)
4. Use `SPIFFS.format()` to clear and re-upload

### Erratic Brightness

**Symptom**: LED flickers or jumps randomly

**Possible causes**:
1. Noisy LDR readings → Add 100nF capacitor across LDR
2. Loose wiring → Check all connections
3. Power supply instability → Use regulated 12V supply
4. PWM frequency too low → Increase `PWM_FREQ` to 10000

### RTC Time Incorrect

**Symptom**: Wrong time of day predictions

**Solution**:
```cpp
// Force set RTC time
rtc.adjust(DateTime(2026, 5, 25, 22, 30, 0));  // YYYY, MM, DD, HH, MM, SS
```

### SPIFFS Full

**Symptom**: Cannot save training data

**Solution**:
1. Delete old learning data:
```cpp
SPIFFS.remove("/learning_data.bin");
```
2. Increase SPIFFS partition: Tools → Partition Scheme → "Minimal SPIFFS"

### Model Predictions Don't Match Python

**Symptom**: ESP32 predictions differ from Python model

**Check**:
1. Feature order matches exactly
2. `maxLux` matches training dataset
3. Time encoding matches LabelEncoder output
4. Floating-point precision (use `%.6f` for debugging)

## 📊 Performance Metrics

| Metric | Value |
|--------|-------|
| Inference Time | < 1ms |
| Memory Usage | ~150KB (with model) |
| SPIFFS Usage | ~50KB (model + data) |
| Power Consumption | ~150mA (ESP32 + sensors) |
| Model Update Time | ~2-5 seconds (50 samples) |
| Flash Wear | Minimal (~1 write per hour) |

## 🔐 Advanced Configuration

### Adjust Learning Parameters

In the Arduino sketch:

```cpp
#define LEARNING_SAMPLES 50       // Retrain after N samples (↑ = slower adaptation)
#define SAMPLE_INTERVAL  10000    // Sample every N ms (↓ = more responsive)
#define ADJUSTMENT_THRESHOLD 5.0  // Min adjustment to learn (↑ = less sensitive)

float learningRate = 0.1;  // In retrainModel() (↑ = faster adaptation)
```

### Feature Engineering

Add new features by modifying:

```cpp
float features[6] = {
    lux_norm, 
    (float)motion_detected,
    hour_sin,
    hour_cos,
    (float)time_of_day_enc,
    effective_need
    // Add your custom features here
};
```

Update `tree_model.json` to include new features in training.

### Remote Model Updates

To update model via WiFi:

```cpp
#include <WiFi.h>
#include <HTTPClient.h>

void downloadModel(const char* url) {
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode == 200) {
        File file = SPIFFS.open("/tree_model.json", "w");
        http.writeToStream(&file);
        file.close();
        
        // Reload model
        loadModel();
    }
    http.end();
}
```

## 📈 Monitoring & Debugging

### Serial Output Format

```
[HH:MM:SS] Lux: <lux> | Motion: <0/1> | DT: <pred%> | Pot: <adj%> | Final: <final%>
```

### Enable Verbose Debugging

Add to `setup()`:

```cpp
Serial.setDebugOutput(true);
```

### Export Training Data

Add USB serial command handler:

```cpp
if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 'd') {  // Dump training data
        for (int i = 0; i < sampleCount; i++) {
            Serial.printf("%d,%.4f,%d,%.4f,%.4f,%d,%.4f,%.2f\n",
                i, 
                learningBuffer[i].lux_norm,
                learningBuffer[i].motion_detected,
                // ... etc
            );
        }
    }
}
```

## 🛡️ Safety & Limitations

### Safety Considerations

1. **Electrical Safety**:
   - Use proper insulation for high-power LED circuits
   - Include fuse on 12V power supply
   - Ensure proper heat sinking for MOSFETs

2. **Fire Safety**:
   - Monitor LED temperature (add thermistor if >10W)
   - Implement thermal shutdown
   - Use flame-retardant enclosure

3. **Eye Safety**:
   - Limit max brightness for direct-view LEDs
   - Add soft-start ramp for sudden changes

### Known Limitations

1. **Training Quality**: On-device training is simplified (leaf adjustment only)
   - Full retraining requires more computation
   - Recommend periodic cloud-based retraining for optimal performance

2. **Memory Constraints**: Limited to 200 training samples in buffer
   - Older samples are overwritten
   - Consider SD card for long-term storage

3. **Sensor Accuracy**: LDR is not precision-calibrated
   - ±20% accuracy typical
   - RTC drift: ~2 min/year without battery backup

4. **Model Size**: Deep trees may exceed SPIFFS limits
   - Max practical depth: ~15 levels
   - Consider ensemble of shallow trees

## 🤝 Contributing

Improvements welcome! Areas for enhancement:

- [ ] Implement full CART algorithm for on-device retraining
- [ ] Add WiFi/MQTT for remote monitoring
- [ ] Support for multiple ML models (Random Forest, XGBoost)
- [ ] Mobile app for configuration
- [ ] Cloud sync for federated learning

## 📄 License

MIT License - Use freely for personal and commercial projects

## 🙏 Acknowledgments

- Decision Tree training based on scikit-learn
- RTC support via Adafruit RTClib
- JSON parsing via ArduinoJson

## 📞 Support

For issues or questions:
1. Check Serial Monitor output first
2. Verify all connections match wiring diagram
3. Test each sensor independently
4. Review troubleshooting section above

---

**Built with ❤️ for edge ML enthusiasts**

*Last updated: May 25, 2026*
