# 📋 ESP32 Smart Light Setup Checklist

Use this checklist to track your progress from model training to deployment.

## ✅ Phase 1: Model Training (Python)

### Prerequisites
- [ ] Python 3.7+ installed
- [ ] Required libraries installed:
  ```bash
  pip install scikit-learn pandas numpy matplotlib seaborn
  ```
- [ ] Dataset prepared (CSV with required columns)

### Training Steps
- [ ] Run your training script successfully
- [ ] Achieve R² > 0.85 (target: >0.95)
- [ ] Add export code to end of training script:
  ```python
  from export_model_to_esp32 import export_tree_to_json, export_preprocessing_params
  export_tree_to_json(best_model, features_final, 'tree_model.json')
  export_preprocessing_params(le, MAX_LUX, 'preprocessing.json')
  ```
- [ ] Verify `tree_model.json` created
- [ ] Verify `preprocessing.json` created
- [ ] Check model file size < 100KB

**✓ Phase 1 Complete** when both JSON files exist and validate.

---

## ⚡ Phase 2: Hardware Assembly

### Components Checklist
- [ ] ESP32 DevKit (any variant with USB)
- [ ] LDR sensor (GL5528 or equivalent)
- [ ] PIR motion sensor (HC-SR501 or equivalent)
- [ ] DS3231 RTC module
- [ ] 10KΩ potentiometer (linear taper)
- [ ] LED strip or bulb (12V PWM compatible)
- [ ] MOSFET (IRLZ44N or equivalent logic-level)
- [ ] Resistors: 2x 10KΩ, 1x 1KΩ
- [ ] Breadboard & jumper wires
- [ ] Power supply: USB (5V) + 12V for LED
- [ ] USB cable for ESP32

### Wiring Checklist
**Use QUICKSTART.md wiring diagram as reference**

#### LDR Circuit
- [ ] 3.3V → 10KΩ resistor → GPIO34
- [ ] GPIO34 → LDR → GND
- [ ] Verify voltage changes when covering LDR

#### Motion Sensor
- [ ] PIR VCC → 3.3V
- [ ] PIR GND → GND
- [ ] PIR OUT → GPIO23
- [ ] Test: Wave hand, LED should light

#### RTC Module
- [ ] RTC VCC → 3.3V
- [ ] RTC GND → GND
- [ ] RTC SDA → GPIO21
- [ ] RTC SCL → GPIO22
- [ ] Insert CR2032 battery

#### Potentiometer
- [ ] One outer pin → 3.3V
- [ ] Center pin → GPIO35
- [ ] Other outer pin → GND
- [ ] Test: Turn knob, voltage should change

#### LED Circuit
- [ ] GPIO16 → 1KΩ resistor → MOSFET Gate
- [ ] MOSFET Source → GND (common with ESP32)
- [ ] MOSFET Drain → LED Strip (+)
- [ ] 12V Power Supply (+) → LED Strip (+)
- [ ] 12V Power Supply (-) → GND (common)
- [ ] **IMPORTANT**: Share GND between ESP32 and LED power supply

#### Power Connections
- [ ] ESP32 powered via USB (5V)
- [ ] All sensors use 3.3V from ESP32
- [ ] LED uses separate 12V supply
- [ ] Common ground for all components

**✓ Phase 2 Complete** when all connections verified with multimeter.

---

## 💻 Phase 3: Software Setup

### Arduino IDE Setup
- [ ] Arduino IDE 1.8.x or 2.x installed
- [ ] ESP32 board support installed:
  - [ ] File → Preferences → Additional Board Manager URLs
  - [ ] Add: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
  - [ ] Tools → Board → Boards Manager → "ESP32" → Install

### Library Installation
- [ ] Tools → Manage Libraries → Search and install:
  - [ ] ArduinoJson (v6.21 or later)
  - [ ] RTClib (by Adafruit, v2.1 or later)

### SPIFFS Uploader Plugin
- [ ] Download: https://github.com/me-no-dev/arduino-esp32fs-plugin/releases
- [ ] Extract to: `<Arduino_folder>/tools/ESP32FS/tool/`
- [ ] Restart Arduino IDE
- [ ] Verify: Tools → ESP32 Sketch Data Upload appears

**✓ Phase 3 Complete** when all libraries show in Sketch → Include Library menu.

---

## 📤 Phase 4: File Preparation & Upload

### Prepare Files
- [ ] Copy `tree_model.json` to project directory
- [ ] Copy `preprocessing.json` to project directory
- [ ] Run preparation script:
  ```bash
  cd ESP32_SmartLight
  python upload_to_spiffs.py
  ```
- [ ] Verify `data/` folder created with both JSON files
- [ ] Check output shows "✓ Preparation complete!"

### Arduino IDE Configuration
- [ ] Open `ESP32_SmartLight.ino`
- [ ] Tools → Board → "ESP32 Dev Module"
- [ ] Tools → Flash Size → "4MB (32Mb)"
- [ ] Tools → Partition Scheme → "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)"
- [ ] Tools → Upload Speed → 921600
- [ ] Tools → Port → Select your ESP32 port

### Upload to ESP32
- [ ] Connect ESP32 via USB
- [ ] **Step 1**: Tools → ESP32 Sketch Data Upload
  - [ ] Wait for "SPIFFS Image Uploaded" message
  - [ ] Should take 10-30 seconds
- [ ] **Step 2**: Sketch → Upload (or Ctrl+U)
  - [ ] Wait for "Done uploading" message
  - [ ] Should take 20-40 seconds

**✓ Phase 4 Complete** when upload shows success and ESP32 reboots.

---

## 🧪 Phase 5: Testing & Verification

### Initial Startup Test
- [ ] Open Serial Monitor (Tools → Serial Monitor)
- [ ] Set baud rate to 115200
- [ ] Press ESP32 reset button
- [ ] Verify startup messages appear:
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
    ✓ Preprocessing params loaded
  
  [3] System Ready!
  ```

### Sensor Tests
- [ ] **LDR Test**: Cover LDR → LED should brighten (if motion detected)
- [ ] **LDR Test**: Shine light → LED should dim
- [ ] **Motion Test**: Trigger PIR → LED should respond
- [ ] **Time Test**: Check Serial shows correct time format
- [ ] **Pot Test**: Turn knob clockwise → LED brighter
- [ ] **Pot Test**: Turn knob counter-clockwise → LED dimmer

### Monitoring Test
- [ ] Wait 10 seconds for first sensor reading
- [ ] Verify Serial output format:
  ```
  [HH:MM:SS] Lux: ### | Motion: 0/1 | DT: ##.#% | Pot: +#.#% | Final: ##.#%
  ```
- [ ] Check all values are reasonable:
  - [ ] Lux: 0-50000
  - [ ] Motion: 0 or 1
  - [ ] DT: 0-100%
  - [ ] Pot: -50 to +50%
  - [ ] Final: 0-100%

### Learning Test
- [ ] Set pot to center position (Pot: ~0%)
- [ ] Note DT prediction value
- [ ] Turn pot by >5% (e.g., to +15%)
- [ ] Verify message appears:
  ```
  → User adjustment detected! Collecting training sample...
  ```
- [ ] Check sample counter increases (visible in logs after retraining)

**✓ Phase 5 Complete** when all tests pass and learning triggers correctly.

---

## 🎓 Phase 6: Calibration (Optional but Recommended)

### LDR Calibration
- [ ] Obtain lux meter or calibration app
- [ ] Measure actual lux in various conditions
- [ ] Compare to ESP32 readings
- [ ] If needed, adjust formula in `readLux()`:
  ```cpp
  float lux = pow(10, (log10(resistance / 1000.0) - 1.5) / -0.7);
  //                                                 ↑      ↑
  //                          Adjust these coefficients for your LDR
  ```
- [ ] Re-upload sketch after changes
- [ ] Verify improved accuracy

### Potentiometer Calibration
- [ ] Set pot to center position
- [ ] Check Serial shows Pot: ~0% (±2%)
- [ ] If not centered, check wiring or adjust threshold
- [ ] Turn fully clockwise → should show ~+50%
- [ ] Turn fully counter-clockwise → should show ~-50%
- [ ] If reversed, swap pot's outer pins

### RTC Time Setting
- [ ] Check current time in Serial Monitor
- [ ] If incorrect, set manually in `setup()`:
  ```cpp
  rtc.adjust(DateTime(2026, 5, 25, 22, 30, 0));  // Y, M, D, H, M, S
  ```
- [ ] Re-upload sketch
- [ ] Verify time is now correct

**✓ Phase 6 Complete** when all sensors show accurate readings.

---

## 🚀 Phase 7: Long-Term Monitoring

### First Week Monitoring
- [ ] Day 1: Note baseline performance
- [ ] Day 2-3: Make 3-5 adjustments per day
- [ ] Day 4: Verify first retraining occurs (50 samples)
- [ ] Day 5-7: Compare predictions before/after retraining
- [ ] Week 1 end: Verify adaptation to your preferences

### Performance Metrics to Track
- [ ] Frequency of manual adjustments (should decrease)
- [ ] Magnitude of adjustments (should decrease)
- [ ] User satisfaction with automatic brightness
- [ ] Number of retraining cycles completed

### Optional Enhancements
- [ ] Add WiFi for remote monitoring
- [ ] Log data to SD card for analysis
- [ ] Create web interface for configuration
- [ ] Implement multiple lighting zones
- [ ] Upgrade to Random Forest model

**✓ Phase 7 Complete** when system adapts to your preferences reliably.

---

## 🎉 Success Criteria Summary

Your system is working correctly when:

✅ **Automatic Mode**
- LED adjusts based on ambient light
- Motion detection triggers appropriate brightness
- Time of day influences decisions
- No manual intervention needed 80%+ of the time

✅ **Manual Override**
- Potentiometer immediately affects brightness
- Adjustment is additive to ML prediction
- Full range -50% to +50% works smoothly

✅ **Learning Mode**
- Adjustments >5% trigger data collection
- System retrains after 50 samples
- Predictions improve over time
- Model persists across reboots

✅ **Reliability**
- Runs continuously without crashes
- SPIFFS operations succeed
- Serial output is clean and informative
- LED behavior is predictable

---

## 📞 Troubleshooting Quick Reference

| Issue | Quick Fix | See |
|-------|-----------|-----|
| Model not loading | Re-upload SPIFFS data | Phase 4 |
| Wrong predictions | Calibrate LDR | Phase 6 |
| LED not working | Check MOSFET wiring | Phase 2 |
| Time incorrect | Set RTC manually | Phase 6 |
| Erratic readings | Add capacitor to LDR | ESP32_SmartLight/README.md |
| SPIFFS full | Clear learning data | ESP32_SmartLight/README.md |
| Serial garbage | Check baud rate (115200) | Phase 5 |
| ESP32 won't boot | Check power supply | Phase 2 |

**For detailed troubleshooting**: See `ESP32_SmartLight/README.md` section 🔧

---

## 📚 Documentation Reference

| Document | When to Use |
|----------|-------------|
| QUICKSTART.md | First-time setup (this checklist expands on it) |
| ESP32_SmartLight/README.md | Hardware wiring, detailed troubleshooting |
| README.md | System architecture, features overview |
| PROJECT_SUMMARY.md | What was built, next steps |
| complete_workflow_example.py | Python export code examples |

---

## 🎓 Completion Status

Track your overall progress:

- [ ] Phase 1: Model Training (Python) - COMPLETE
- [ ] Phase 2: Hardware Assembly - COMPLETE
- [ ] Phase 3: Software Setup - COMPLETE
- [ ] Phase 4: File Preparation & Upload - COMPLETE
- [ ] Phase 5: Testing & Verification - COMPLETE
- [ ] Phase 6: Calibration (Optional) - COMPLETE
- [ ] Phase 7: Long-Term Monitoring - ONGOING

**🎊 ALL PHASES COMPLETE = PRODUCTION READY! 🎊**

---

## 💡 Pro Tips

1. **Keep Serial Monitor open** during first few days to understand behavior
2. **Document your calibration values** in case you need to reflash
3. **Start with center pot position** to verify ML works before adjusting
4. **Make intentional adjustments** (not random) for better learning
5. **Be patient with learning** - 50 samples takes time but produces good results

---

## 🚀 Next Steps After Completion

1. **Share your results** - Document what you learned
2. **Experiment with parameters** - Tune for your specific environment
3. **Extend functionality** - Add WiFi, web interface, or additional sensors
4. **Improve the model** - Try Random Forest or XGBoost
5. **Deploy to multiple rooms** - Scale your solution

---

**Congratulations on building an intelligent edge ML system! 🎉💡🤖**

*Questions? Start with the document that best matches your current phase.*
