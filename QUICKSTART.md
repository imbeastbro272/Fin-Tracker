# Quick Start Guide - ESP32 Smart Light with ML

Get your smart light running in 30 minutes! ⚡

## Overview

This project turns your ESP32 into an intelligent lighting controller that:
- 🧠 Runs machine learning locally (no cloud needed)
- 💡 Automatically adjusts brightness based on environment
- 🎛️ Learns from your manual adjustments
- 📊 Continuously improves over time

## What You'll Build

```
[Ambient Light] ──┐
[Motion Sensor] ──┤
[Time of Day]   ──┼──> [ESP32 + ML Model] ──> [Smart LED]
[Your Knob]     ──┘         ↑
                            │
                    (learns from you)
```

## Prerequisites

### Hardware ($25-40 total)
- ✅ ESP32 DevKit (~$6)
- ✅ LDR sensor (~$1)
- ✅ PIR motion sensor (~$2)
- ✅ DS3231 RTC module (~$2)
- ✅ 10K potentiometer (~$1)
- ✅ LED strip or bulb (~$5-15)
- ✅ MOSFET (IRLZ44N) (~$1)
- ✅ Resistors, breadboard, wires (~$5)

### Software (all free)
- ✅ Python 3.7+ with scikit-learn
- ✅ Arduino IDE 1.8.x or 2.x
- ✅ USB cable for ESP32

## Step-by-Step Setup

### Part A: Train Your Model (10 minutes)

1. **Prepare your dataset** (CSV with these columns):
   - `ambient_light_lux` - Light sensor reading (0-50000)
   - `motion_detected` - Motion (0 or 1)
   - `time of day` - Time category (Morning/Afternoon/Evening/Night/Early Morning)
   - `hour_sin` - Sin(hour) for circular time
   - `hour_cos` - Cos(hour) for circular time
   - `Bulb Intensity` - Desired brightness (0-100%)

2. **Train the model**:
   ```bash
   # Run your provided training script
   python your_training_script.py
   ```

3. **Export for ESP32**:
   Add these lines at the END of your training script:
   ```python
   from export_model_to_esp32 import export_tree_to_json, export_preprocessing_params
   
   export_tree_to_json(best_model, features_final, 'tree_model.json')
   export_preprocessing_params(le, MAX_LUX, 'preprocessing.json')
   ```

   This creates:
   - `tree_model.json` (your trained model)
   - `preprocessing.json` (normalization parameters)

### Part B: Wire Up Hardware (10 minutes)

**Minimal wiring diagram:**

```
ESP32               Components
=====               ==========

GPIO34 ──────┬──── [LDR] ──── 3.3V
             └──── [10KΩ] ─── GND

GPIO23 ────────── PIR OUT

GPIO35 ──────┬──── [POT] ──── 3.3V
             └──── [POT] ──── GND

GPIO16 ──── [1KΩ] ──── MOSFET Gate
                        │
                    Drain ──── LED+ (12V)
                        │
                    Source ─── GND

GPIO21 ──── RTC SDA
GPIO22 ──── RTC SCL

3.3V ──── RTC VCC, PIR VCC
GND ───── Common ground for all
```

**Power**: 
- ESP32: USB (5V) or external 5V
- LED: 12V separate power supply (share GND with ESP32)

### Part C: Upload to ESP32 (10 minutes)

1. **Install Arduino libraries**:
   - Open Arduino IDE
   - Tools → Manage Libraries
   - Search and install:
     - `ArduinoJson` (v6.21+)
     - `RTClib` (by Adafruit)

2. **Install ESP32 board**:
   - File → Preferences
   - Additional Board Manager URLs: 
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Tools → Board → Boards Manager
   - Search "ESP32" and install

3. **Install SPIFFS uploader**:
   - Download: https://github.com/me-no-dev/arduino-esp32fs-plugin/releases
   - Extract to: `<Arduino_folder>/tools/ESP32FS/tool/`
   - Restart Arduino IDE

4. **Prepare files for upload**:
   ```bash
   cd ESP32_SmartLight
   python upload_to_spiffs.py
   ```
   
   This creates `data/` folder with your model files.

5. **Configure Arduino IDE**:
   - Tools → Board → "ESP32 Dev Module"
   - Tools → Partition Scheme → "Default 4MB with spiffs"
   - Tools → Port → (select your ESP32)

6. **Upload everything**:
   ```
   Step 1: Tools → ESP32 Sketch Data Upload  (uploads model files)
   Step 2: Sketch → Upload                    (uploads code)
   ```

7. **Verify it works**:
   - Tools → Serial Monitor (set to 115200 baud)
   - You should see:
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
   ```

## Using Your Smart Light

### Automatic Mode
Just let it run! The system reads sensors every 10 seconds and adjusts brightness automatically.

**Serial output example:**
```
[22:45:30] Lux: 250 | Motion: 1 | DT: 78.5% | Pot: +0.0% | Final: 78.5%
```

- **Lux**: Ambient light level
- **Motion**: 0 (no motion) or 1 (motion detected)
- **DT**: Decision tree prediction
- **Pot**: Potentiometer adjustment
- **Final**: Actual LED brightness

### Manual Adjustment
Turn the potentiometer to adjust brightness:
- **Center**: No adjustment (pure ML control)
- **Clockwise**: Up to +50% brighter
- **Counter-clockwise**: Up to -50% dimmer

When you adjust by >5%, the system records it as training data!

### Online Learning
After 50 manual adjustments, the model automatically retrains:

```
⚡ Sufficient samples collected! Retraining model...

--------------------------------------------------------------------------------
RETRAINING MODEL WITH USER FEEDBACK
--------------------------------------------------------------------------------
  Sample 1: Adjusted leaf 79.0% → 81.5%
  Sample 2: Adjusted leaf 45.2% → 48.8%
  ...
  ✓ Updated model saved to SPIFFS
--------------------------------------------------------------------------------
```

Your preferences are now learned! 🎉

## Testing

### Basic Tests

1. **Cover LDR** → LED should brighten (if motion detected)
2. **Uncover LDR** (bright light) → LED should dim
3. **Trigger motion sensor** → LED should respond
4. **Turn pot clockwise** → LED should get brighter
5. **Wait 10 seconds** → Check serial output for new reading

### Advanced Testing

Add this to `setup()` in the Arduino sketch to run automated tests:

```cpp
void setup() {
    // ... existing setup code ...
    
    runModelTests();  // Verify model predictions
}
```

## Troubleshooting

| Problem | Solution |
|---------|----------|
| "Model not found" | Re-run SPIFFS upload (Tools → ESP32 Sketch Data Upload) |
| LED not working | Check MOSFET wiring, verify GPIO16 PWM output |
| Wrong brightness | Calibrate LDR in `readLux()` function |
| RTC time wrong | Set manually: `rtc.adjust(DateTime(2026,5,25,22,30,0))` |
| Erratic readings | Add 100nF capacitor across LDR |

## What's Next?

### Customization
- Adjust `LEARNING_SAMPLES` (default 50) for faster/slower adaptation
- Modify `SAMPLE_INTERVAL` (default 10s) for more/less frequent updates
- Change `ADJUSTMENT_THRESHOLD` (default 5%) to control learning sensitivity

### Advanced Features
- Add WiFi for remote monitoring
- Log data to SD card for analysis
- Create mobile app for configuration
- Implement multiple lighting zones

## Files Reference

```
Fin-Tracker/
├── export_model_to_esp32.py          # Model export utility
├── complete_workflow_example.py       # End-to-end workflow
├── QUICKSTART.md                      # This file
│
└── ESP32_SmartLight/
    ├── ESP32_SmartLight.ino          # Main Arduino code
    ├── README.md                      # Detailed documentation
    ├── upload_to_spiffs.py           # SPIFFS preparation
    ├── platformio.ini                 # PlatformIO config (optional)
    │
    └── data/                          # Generated by upload_to_spiffs.py
        ├── tree_model.json            # Your trained model
        └── preprocessing.json         # Normalization params
```

## Help & Support

- 📖 **Full docs**: See `ESP32_SmartLight/README.md`
- 🐛 **Debugging**: Check Serial Monitor first (115200 baud)
- 🔧 **Wiring help**: See detailed diagrams in README.md
- ⚡ **Performance**: Expect <1ms inference, ~150KB memory usage

## Success Checklist

✅ Trained model with your dataset  
✅ Exported `tree_model.json` and `preprocessing.json`  
✅ Wired all components correctly  
✅ Uploaded SPIFFS data to ESP32  
✅ Uploaded Arduino sketch  
✅ Verified in Serial Monitor  
✅ Tested automatic brightness control  
✅ Tested manual adjustment  
✅ Verified learning triggers on adjustment  

**Congratulations! You now have a self-learning smart light! 🎉**

---

*Built with ❤️ for edge ML enthusiasts*  
*Questions? Check README.md for detailed troubleshooting*
