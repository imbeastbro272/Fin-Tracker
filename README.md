# ESP32 Smart Light with Embedded Machine Learning

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![ML: Decision Tree](https://img.shields.io/badge/ML-Decision%20Tree-green.svg)](https://scikit-learn.org/)

A complete end-to-end machine learning system that trains a Decision Tree model in Python and deploys it to ESP32 for real-time, on-device brightness control with online learning capabilities.

## 🌟 Key Features

- **Edge ML Inference**: Decision Tree model runs directly on ESP32 (sub-millisecond predictions)
- **Automatic Control**: Adjusts LED brightness based on ambient light, motion, and time
- **Manual Override**: Potentiometer provides -50% to +50% additive brightness adjustment
- **Online Learning**: Model retrains itself based on user adjustments (incremental learning)
- **No Cloud Dependency**: 100% offline operation after initial training
- **Low Resource Usage**: ~150KB memory, <1ms inference time
- **Persistent Storage**: Model and training data saved to SPIFFS flash memory

## 📊 System Architecture

```
┌──────────────────────────────────────────────────────────────────────────┐
│                           TRAINING PHASE (Python)                         │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│  Historical Data (CSV)  →  Feature Engineering  →  Decision Tree         │
│                                                      Training             │
│                                 ↓                                         │
│                    Model Export (JSON format)                             │
│                                                                           │
└─────────────────────────────────────┬────────────────────────────────────┘
                                      │
                                      ↓ Upload to SPIFFS
┌──────────────────────────────────────────────────────────────────────────┐
│                        INFERENCE PHASE (ESP32)                            │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│  Sensors (LDR, PIR, RTC)  →  Feature Computation  →  Tree Traversal      │
│                                                           ↓               │
│                                                    Base Brightness        │
│                                                           +               │
│  Potentiometer  ───────────────────────────────────→  Adjustment         │
│                                                           ↓               │
│                                                    Final Brightness       │
│                                                           ↓               │
│                                                       LED PWM             │
│                                                                           │
│  If |adjustment| > 5%:                                                    │
│    Store sample → Buffer → Retrain (after 50) → Update model → Save      │
│                                                                           │
└──────────────────────────────────────────────────────────────────────────┘
```

## 🎯 How It Works

### Training Phase (Python)

1. **Dataset**: Historical brightness preferences (52 days typical)
   - Features: `ambient_light_lux`, `motion_detected`, `time_of_day`, `hour_sin`, `hour_cos`
   - Target: `Bulb Intensity` (0-100%)

2. **Feature Engineering**: 
   - Creates `effective_need = (1 - lux_norm) × motion` to encode "bright light reduces bulb need"
   - Normalizes lux values to [0, 1]

3. **Model Training**: scikit-learn DecisionTreeRegressor
   - Hyperparameter tuning via GridSearchCV
   - Typical performance: R² > 0.95, MAE < 5%

4. **Export**: Convert tree structure to JSON for ESP32 consumption

### Inference Phase (ESP32)

1. **Sensor Reading** (every 10 seconds):
   - LDR → ambient light (lux)
   - PIR → motion detection
   - RTC → current time
   - Potentiometer → manual adjustment

2. **Feature Computation**:
   ```cpp
   lux_norm = lux / MAX_LUX
   hour_sin = sin(2π × hour / 24)
   hour_cos = cos(2π × hour / 24)
   effective_need = (1 - lux_norm) × motion
   ```

3. **Decision Tree Traversal**:
   - Walk tree nodes comparing features to thresholds
   - Return leaf value as predicted brightness

4. **Final Brightness**:
   ```
   final = constrain(dt_prediction + pot_adjustment, 0, 100)
   ```

5. **LED Control**: Convert to PWM (0-255) and output

### Online Learning (ESP32)

When user adjusts potentiometer by >5%:

1. **Sample Collection**: Store `[features, corrected_brightness]` in buffer
2. **Trigger Retraining**: After 50 samples collected
3. **Leaf Adjustment**: Simple incremental update (moving average)
   ```cpp
   new_value = current_value + learning_rate × (target - current_value)
   ```
4. **Persistence**: Save updated model to SPIFFS

This is a **simplified online learning** approach suitable for ESP32's constraints. For full retraining, consider cloud-based periodic updates.

## 📦 Project Contents

```
Fin-Tracker/
│
├── README.md                          # This file (project overview)
├── QUICKSTART.md                      # 30-minute setup guide
├── .gitignore                         # Git ignore rules
│
├── export_model_to_esp32.py          # Utility: Export sklearn model to JSON
├── complete_workflow_example.py       # Example: Full training → export workflow
│
└── ESP32_SmartLight/
    ├── ESP32_SmartLight.ino          # Main Arduino sketch (inference + learning)
    ├── README.md                      # Detailed hardware/software guide
    ├── upload_to_spiffs.py           # Utility: Prepare SPIFFS upload
    ├── platformio.ini                 # PlatformIO config (alternative to Arduino IDE)
    │
    └── data/                          # Created by upload_to_spiffs.py
        ├── tree_model.json            # Your trained model (generated)
        └── preprocessing.json         # Normalization parameters (generated)
```

## 🚀 Quick Start

**Total time: ~30 minutes**

### Option A: Fast Track

Follow the **[QUICKSTART.md](QUICKSTART.md)** guide for step-by-step instructions.

### Option B: Detailed Setup

1. **Train your model**:
   ```bash
   python your_training_script.py
   # Add export commands at the end (see complete_workflow_example.py)
   ```

2. **Prepare for ESP32**:
   ```bash
   cd ESP32_SmartLight
   python upload_to_spiffs.py
   ```

3. **Upload to ESP32**:
   - Open `ESP32_SmartLight.ino` in Arduino IDE
   - Tools → ESP32 Sketch Data Upload (uploads model files)
   - Sketch → Upload (uploads code)

4. **Monitor & Test**:
   - Open Serial Monitor (115200 baud)
   - Verify model loaded successfully
   - Test automatic and manual control

Full hardware wiring and troubleshooting: See **[ESP32_SmartLight/README.md](ESP32_SmartLight/README.md)**

## 🔧 Hardware Requirements

| Component | Approx Cost | Purpose |
|-----------|-------------|---------|
| ESP32 DevKit | $6 | Main controller (dual-core, WiFi/BT) |
| LDR (GL5528) | $1 | Ambient light sensing (0-50K lux) |
| PIR Sensor (HC-SR501) | $2 | Motion detection (adjustable sensitivity) |
| DS3231 RTC | $2 | Accurate timekeeping (battery backup) |
| 10KΩ Potentiometer | $1 | Manual brightness adjustment |
| LED Strip/Bulb | $5-15 | Light output (12V PWM compatible) |
| MOSFET (IRLZ44N) | $1 | High-current LED switching |
| Misc (resistors, wires) | $5 | LDR divider, connections |
| **Total** | **$23-33** | Complete system |

## 💻 Software Requirements

### Python (Training)
- Python 3.7+
- scikit-learn
- pandas, numpy
- matplotlib, seaborn

```bash
pip install scikit-learn pandas numpy matplotlib seaborn
```

### Arduino (Deployment)
- Arduino IDE 1.8.x or 2.x
- ESP32 Board Support (Espressif)
- ArduinoJson library (v6.21+)
- RTClib library (Adafruit)
- ESP32 Filesystem Uploader plugin

## 📊 Model Performance

| Metric | Training (Python) | Inference (ESP32) |
|--------|-------------------|-------------------|
| R² Score | 0.95+ | N/A (regression) |
| MAE | <5% | Verified via test cases |
| Inference Time | ~1ms (CPU) | <1ms (240MHz ESP32) |
| Memory Usage | ~50MB (Python) | ~150KB (ESP32) |
| Model Size | N/A | 20-60KB (depends on depth) |
| Training Time | 10-30s (GridSearch) | 2-5s (50 samples, leaf update) |

## 🎛️ Configuration Options

### Training Parameters (Python)

```python
# In your training script
param_grid = {
    'max_depth':        [6, 8, 10, 12, 15],     # Tree depth
    'min_samples_split': [5, 10, 15, 20],       # Min samples to split
    'min_samples_leaf':  [3, 5, 8, 10],         # Min samples per leaf
}
```

### Runtime Parameters (ESP32)

```cpp
// In ESP32_SmartLight.ino
#define LEARNING_SAMPLES 50        // Retrain after N adjustments
#define SAMPLE_INTERVAL  10000     // Sensor read interval (ms)
#define ADJUSTMENT_THRESHOLD 5.0   // Min adjustment to trigger learning (%)

float learningRate = 0.1;  // Incremental update rate (0.0-1.0)
```

## 🧪 Example Use Cases

1. **Home Automation**: Bedroom light that learns your preferences
2. **Office Lighting**: Adaptive desk lamp based on natural light
3. **Greenhouse**: Plant growth light with photoperiod control
4. **Security**: Motion-activated light with time-based intensity
5. **Energy Saving**: Smart outdoor light that minimizes waste

## 📈 Performance Optimization

### Reduce Model Size
- Limit `max_depth` during training (e.g., 8-10 instead of 15)
- Use `min_samples_leaf` to prune small leaves
- Result: Faster inference, smaller JSON file

### Faster Retraining
- Reduce `LEARNING_SAMPLES` (e.g., 25 instead of 50)
- Increase `learningRate` (e.g., 0.2 instead of 0.1)
- Trade-off: Less stable updates

### Better Predictions
- Calibrate LDR with actual lux meter
- Tune `ADJUSTMENT_THRESHOLD` to your sensitivity
- Collect diverse training data (all times, weather conditions)

## 🔐 Security & Privacy

- ✅ **100% Offline**: No data leaves your device
- ✅ **Local Storage**: All data in SPIFFS (not accessible externally)
- ✅ **No Telemetry**: No phone-home behavior
- ✅ **Open Source**: Full code transparency

Optional: Add WiFi for remote monitoring (with proper authentication!)

## 🐛 Troubleshooting

| Issue | Quick Fix |
|-------|-----------|
| Model not loading | Re-upload SPIFFS data |
| Wrong predictions | Check LDR calibration |
| LED not responding | Verify MOSFET wiring |
| Time incorrect | Set RTC manually |
| SPIFFS full | Delete old learning data |

Full troubleshooting guide: [ESP32_SmartLight/README.md](ESP32_SmartLight/README.md)

## 🚧 Limitations & Future Work

### Current Limitations
1. **Simplified online learning**: Leaf adjustment only (not full tree retraining)
2. **Fixed feature set**: Adding features requires recompiling
3. **Limited memory**: Max ~200 training samples in buffer
4. **No ensemble methods**: Single tree only (no Random Forest)

### Planned Improvements
- [ ] Full CART algorithm for on-device retraining
- [ ] Support for Random Forest / XGBoost
- [ ] WiFi-based remote configuration
- [ ] Mobile app for monitoring
- [ ] Cloud sync for federated learning
- [ ] Multi-zone lighting control

## 🤝 Contributing

Contributions welcome! Areas of interest:
- Implementing full decision tree training on ESP32
- Adding support for other ML models
- Creating mobile app
- Improving LDR calibration methods
- Adding more sensors (temperature, humidity)

## 📄 License

MIT License - Free for personal and commercial use

## 📚 References

- [scikit-learn Decision Trees](https://scikit-learn.org/stable/modules/tree.html)
- [ESP32 Technical Reference](https://www.espressif.com/en/products/socs/esp32)
- [ArduinoJson Documentation](https://arduinojson.org/)
- [Online Learning Overview](https://en.wikipedia.org/wiki/Online_machine_learning)

## 🙏 Acknowledgments

- Decision Tree implementation based on scikit-learn
- RTC support via Adafruit RTClib
- JSON parsing via ArduinoJson
- Inspired by edge ML research and TinyML movement

## 📞 Support

- 📖 Documentation: See [QUICKSTART.md](QUICKSTART.md) and [ESP32_SmartLight/README.md](ESP32_SmartLight/README.md)
- 🐛 Issues: Check troubleshooting sections first
- 💡 Questions: Review code comments (heavily documented)

---

**Built with ❤️ for edge ML enthusiasts**

*"Machine learning shouldn't require the cloud"*

---

### Project Stats

- **Lines of Code**: ~1,200 (Arduino) + ~500 (Python)
- **Documentation**: 3 comprehensive guides
- **Hardware Cost**: $25-40 total
- **Setup Time**: ~30 minutes
- **Inference Latency**: <1ms
- **Power Consumption**: ~150mA (ESP32 + sensors)

**Start building your intelligent lighting system today! 🚀**
