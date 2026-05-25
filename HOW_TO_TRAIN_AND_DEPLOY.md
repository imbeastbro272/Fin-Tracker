# 🚀 How to Train Your Model & Deploy to ESP32

**Complete step-by-step guide - from training to working LED system**

---

## ⚡ Quick Summary

```
1. Run training script          → python train_brightness_model.py
2. Files auto-generated         → tree_model.json, preprocessing.json
3. Prepare for ESP32            → python ESP32_SmartLight/upload_to_spiffs.py
4. Upload to ESP32              → Arduino IDE (2 steps)
5. Success!                     → LED automatically adjusts brightness
```

**Total time: ~30 minutes** | **Complexity: Easy** | **Everything automated**

---

## 📋 Step-by-Step Guide

### STEP 1: Run the Training Script ✅

The easiest step! Just run this one command:

```bash
python train_brightness_model.py
```

**What happens:**
- ✅ Generates synthetic 52-day brightness dataset (or uses your CSV)
- ✅ Creates 6 ML features automatically
- ✅ Trains Decision Tree with hyperparameter tuning
- ✅ **Automatically exports `tree_model.json`** ← for ESP32
- ✅ **Automatically exports `preprocessing.json`** ← for ESP32
- ✅ Generates `test_predictions.json` ← for verification
- ✅ Prints model performance metrics
- ✅ Everything done in ~2 minutes!

**Output you'll see:**

```
================================================================================
DECISION TREE MODEL TRAINING - BRIGHTNESS PREDICTION
================================================================================

[STEP 1] Loading/Generating Dataset...
✓ Generated 2496 synthetic samples

[STEP 2] Feature Engineering...
✓ Features created

[STEP 3] Data Augmentation...
✓ Augmented total: 2796

[STEP 4] Preparing Features and Target...
✓ X shape: (2796, 6)

[STEP 5] Train-Test Split (80:20)...
✓ Training set: 2236 samples
✓ Testing set: 560 samples

[STEP 6] Hyperparameter Tuning (GridSearchCV)...
✓ Best Parameters Found

[STEP 7] Model Evaluation...
✓ R² Score: 0.937736  ✓ GOOD (>0.85)
✓ MAE: 5.4305%

[STEP 8] Feature Importances...
✓ effective_need is now influential

[STEP 9] Exporting Model to ESP32...
✓ Model exported to 'tree_model.json'
✓ Preprocessing params exported to 'preprocessing.json'

[STEP 10] Generating Test Predictions...
✓ Test predictions saved to 'test_predictions.json'

================================================================================
✓ TRAINING COMPLETE - MODEL READY FOR ESP32!
================================================================================
```

### STEP 2: Verify Generated Files ✅

Three files should now exist in your project directory:

```bash
# Check if files exist
ls -lh tree_model.json preprocessing.json test_predictions.json

# Output should be:
# -rw-r--r-- tree_model.json       (~6-50 KB depending on tree depth)
# -rw-r--r-- preprocessing.json    (~200 bytes)
# -rw-r--r-- test_predictions.json (~1 KB)
```

**What they contain:**

| File | Size | Purpose |
|------|------|---------|
| `tree_model.json` | 6-50 KB | Your trained Decision Tree (upload to ESP32) |
| `preprocessing.json` | ~200 B | Normalization params: max_lux, time encodings |
| `test_predictions.json` | ~1 KB | Test cases to verify ESP32 predictions match |

### STEP 3: Prepare Files for ESP32 Upload ✅

Run this utility script to prepare SPIFFS upload:

```bash
cd ESP32_SmartLight
python upload_to_spiffs.py
```

**What it does:**
- ✅ Validates both JSON files are valid
- ✅ Checks file sizes are reasonable
- ✅ Creates `data/` folder in the sketch directory
- ✅ Copies files to `data/` for SPIFFS upload
- ✅ Provides next steps

**Output you'll see:**

```
================================================================================
ESP32 SPIFFS Upload Preparation Tool
================================================================================

[Step 1] Checking for required files...
  ✓ tree_model.json                   [12216 bytes] Valid JSON
    - Max Depth: 6
    - Leaves: 39
  ✓ preprocessing.json                   [163 bytes] Valid JSON
    - Max Lux: 14966.19

  Total size: 12379 bytes (12.10 KB)

[Step 2] Creating data directory...
  ✓ 'data/' directory already exists

[Step 3] Copying files to data directory...
  ✓ Copied tree_model.json → data/tree_model.json
  ✓ Copied preprocessing.json → data/preprocessing.json

[Step 4] Checking SPIFFS capacity...
  SPIFFS size: 1024 KB (typical)
  Model files: 12.10 KB
  Usage: 1.2%
  ✓ Sufficient space available

================================================================================
✓ Preparation complete! Ready for SPIFFS upload.
================================================================================
```

**Result:** Files now in `ESP32_SmartLight/data/` ready for upload!

### STEP 4: Upload to ESP32 (Arduino IDE) ✅

**Important:** Have ESP32 connected via USB

#### Step 4a: Upload SPIFFS Data (Model Files)

1. Open Arduino IDE
2. Open sketch: `ESP32_SmartLight/ESP32_SmartLight.ino`
3. Configure board:
   - Tools → Board → "ESP32 Dev Module"
   - Tools → Partition Scheme → "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)"
   - Tools → Port → (select your ESP32 port)

4. **Upload SPIFFS data:**
   - Tools → **ESP32 Sketch Data Upload**
   - Wait for message: `SPIFFS Image Uploaded` (takes 10-30 seconds)

```
Took 8.87s
Hash of data verified.
Hard resetting via RTS pin...
SPIFFS Image Uploaded
```

#### Step 4b: Upload Firmware (Arduino Code)

1. Click **Upload** (or press Ctrl+U)
2. Wait for message: `Done uploading` (takes 20-40 seconds)

```
Wrote 516096 bytes to address 0x00010000 in 20.50 seconds

Leaving...
Hard resetting via RTS pin...
Done uploading.
```

**Both uploads complete! ✓**

### STEP 5: Verify It Works ✅

1. Open Serial Monitor:
   - Tools → Serial Monitor
   - Set baud to **115200**
   - Click reset button on ESP32

2. **You should see:**

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
    - Max Depth: 6
    - Leaves: 39
  ✓ Preprocessing params loaded (max_lux: 14966.19)

[3] System Ready!
  - Automatic brightness control: ENABLED
  - Manual adjustment: ENABLED (potentiometer)
  - Online learning: ENABLED
================================================================================

[22:45:30] Lux: 250 | Motion: 1 | DT: 78.5% | Pot: +0.0% | Final: 78.5%
[22:45:40] Lux: 235 | Motion: 1 | DT: 79.2% | Pot: +0.0% | Final: 79.2%
[22:45:50] Lux: 240 | Motion: 1 | DT: 79.0% | Pot: +0.0% | Final: 94.0%
  → User adjustment detected! Collecting training sample...
```

**Success! ✓** Your model is running on ESP32!

### STEP 6: Test the Hardware ✅

With Serial Monitor still open:

1. **Test LDR (Ambient Light):**
   - Cover the LDR sensor
   - LED should brighten (if motion detected)
   - Check Serial: `Lux` value decreases

2. **Test PIR (Motion):**
   - Wave your hand over PIR sensor
   - LED should respond
   - Check Serial: `Motion` changes to 1

3. **Test Potentiometer:**
   - Turn knob clockwise
   - LED should brighten
   - Check Serial: `Pot` shows +50% (at max)

4. **Test Learning:**
   - Adjust pot by >5% when LED is steady
   - Serial should show: `→ User adjustment detected!`
   - After 50 adjustments, model retrains automatically

**All tests pass? 🎉 YOU'RE DONE!**

---

## 🎓 What Each File Does

### `train_brightness_model.py` (The Script You Run)

**~550 lines | Ready-to-run | No modifications needed**

```python
python train_brightness_model.py
```

**Does:**
1. Generates synthetic dataset (or loads your CSV)
2. Performs feature engineering
3. Trains Decision Tree
4. Exports to ESP32-compatible JSON format

**Creates 3 files:**
- ✅ `tree_model.json` - Your trained model
- ✅ `preprocessing.json` - Normalization parameters
- ✅ `test_predictions.json` - Test cases

### `tree_model.json` (Your Trained Model)

**~6-50 KB | JSON format | For ESP32**

Contains the complete decision tree structure:

```json
{
  "model_type": "decision_tree_regressor",
  "n_features": 6,
  "feature_names": ["lux_norm", "motion_detected", "hour_sin", "hour_cos", "time_of_day_enc", "effective_need"],
  "max_depth": 6,
  "n_leaves": 39,
  "tree": {
    "type": "split",
    "feature_idx": 5,
    "feature_name": "effective_need",
    "threshold": 0.933,
    "left": { ... },
    "right": { ... }
  }
}
```

**ESP32 traverses this tree to make predictions!**

### `preprocessing.json` (Normalization Params)

**~200 bytes | JSON format | For ESP32**

Contains parameters needed to normalize input data:

```json
{
  "max_lux": 14966.19,
  "time_of_day_encoding": {
    "Afternoon": 0,
    "Early Morning": 1,
    "Evening": 2,
    "Morning": 3,
    "Night": 4
  }
}
```

**ESP32 uses this to normalize sensor readings!**

### `test_predictions.json` (Verification Cases)

**~1 KB | JSON format | For testing**

Test scenarios and expected predictions:

```json
[
  {
    "scenario": {
      "lux": 1100,
      "motion": 1,
      "hour": 22,
      "tod": "Night",
      "desc": "Night + Bright (street light)"
    },
    "prediction": 9.57
  },
  ...
]
```

**Compare these with ESP32 predictions to verify accuracy!**

---

## 🛠️ Customization Options

### Option A: Use Your Own Dataset

If you have actual brightness data, use it instead of synthetic:

```python
# In train_brightness_model.py, change:
csv_file = None  # Your file here
# to:
csv_file = 'your_data.csv'  # Your actual data
```

Your CSV must have columns:
- `ambient_light_lux` (0-50000)
- `motion_detected` (0 or 1)
- `time of day` (Morning/Afternoon/Evening/Night/Early Morning)
- `Bulb Intensity` (0-100, your preferred brightness)

### Option B: Adjust Hyperparameters

For better accuracy or smaller model:

```python
# In train_brightness_model.py, find:
param_grid = {
    'max_depth': [6, 8, 10, 12, 15],        # Shallow = faster, deep = accurate
    'min_samples_split': [5, 10, 15, 20],  # Higher = simpler tree
    'min_samples_leaf': [3, 5, 8, 10],     # Higher = smoother predictions
}

# Example for tiny ESP32:
param_grid = {
    'max_depth': [5, 6, 7],                # Very shallow
    'min_samples_split': [20],
    'min_samples_leaf': [5],
}
```

### Option C: Adjust Learning Rate

In ESP32 firmware, control how fast it learns:

```cpp
// In ESP32_SmartLight.ino:
float learningRate = 0.1;  // 0.0 = no learning, 1.0 = learn fast

// Make it learn faster:
float learningRate = 0.3;

// Make it learn slower:
float learningRate = 0.05;
```

---

## ❌ Troubleshooting

| Problem | Solution |
|---------|----------|
| `ModuleNotFoundError: pandas` | `pip install pandas numpy scikit-learn` |
| `Permission denied` | Try: `python3 train_brightness_model.py` |
| "Model not found" on ESP32 | Re-run `upload_to_spiffs.py` and re-upload |
| Wrong predictions on ESP32 | Verify `max_lux` matches in preprocessing.json |
| ESP32 won't boot | Check MOSFET wiring for shorts |
| LED not responding | Verify GPIO16 PWM is working |

**Detailed troubleshooting:** See `ESP32_SmartLight/README.md`

---

## 📊 Understanding the Output

### Model Performance

When training completes, you'll see:

```
✓ R² Score: 0.937736  ✓ GOOD (>0.85)
```

| R² Score | Quality | Status |
|----------|---------|--------|
| > 0.95 | Excellent | 🎯 Perfect! |
| 0.85-0.95 | Good | ✓ Deploy it |
| 0.75-0.85 | Acceptable | ⚠ May need tuning |
| < 0.75 | Poor | ❌ Retrain needed |

### Feature Importance

Shows which features matter most:

```
Feature Importance Ranking:
  effective_need          0.8800  ████████████████████
  lux_norm                0.1133  █████
  time_of_day_enc         0.0064
```

**Key insight:** `effective_need` (0.88) dominates because it encodes the rule: "high light → low bulb need"

### Predictions on ESP32

```
[22:45:50] Lux: 240 | Motion: 1 | DT: 79.0% | Pot: +0.0% | Final: 94.0%
  ↑         ↑        ↑        ↑         ↑      ↑        ↑         ↑
  Time      Light    Motion   DT Model  Manual Final    User
  Stamp     Sensor   Detect   Output    +50%   Brightness Adjusted!
```

---

## 🎯 Success Criteria

Your system is working when:

- ✅ `python train_brightness_model.py` runs without errors
- ✅ Three JSON files are created
- ✅ `upload_to_spiffs.py` reports "Preparation complete"
- ✅ Arduino IDE uploads SPIFFS data successfully
- ✅ Arduino IDE uploads firmware successfully
- ✅ Serial Monitor shows "Model loaded successfully"
- ✅ LED responds to light changes
- ✅ LED responds to motion
- ✅ Potentiometer immediately affects brightness
- ✅ Adjustments >5% trigger "Collecting training sample"

**All checked? 🎉 You've built an intelligent edge ML system!**

---

## 📚 Next Steps

### Immediate
1. ✅ Train model
2. ✅ Deploy to ESP32
3. ✅ Test hardware

### Short-term (Day 1-7)
- Monitor learning as ESP32 collects user feedback
- Calibrate LDR if brightness seems off
- Note any adjustments needed

### Long-term (Week 2+)
- Add WiFi for remote monitoring
- Log data for analysis
- Create web interface
- Try Random Forest model
- Deploy to multiple rooms

---

## 🆘 Getting Help

1. **Script won't run?** → Check you installed: `pip install pandas numpy scikit-learn matplotlib seaborn`

2. **Files not generated?** → Script should print SUCCESS message at end. Scroll up to see errors.

3. **ESP32 upload failing?** → See QUICKSTART.md or ESP32_SmartLight/README.md

4. **Predictions don't match test cases?** → Check max_lux in preprocessing.json matches

5. **Still stuck?** → Check ESP32_SmartLight/README.md troubleshooting section

---

## ✅ Final Checklist

- [ ] Installed Python dependencies
- [ ] Ran `python train_brightness_model.py` successfully
- [ ] Three JSON files created
- [ ] Wired ESP32 hardware (see SETUP_CHECKLIST.md Phase 2)
- [ ] Ran `upload_to_spiffs.py`
- [ ] Uploaded SPIFFS data to ESP32
- [ ] Uploaded firmware to ESP32
- [ ] Serial Monitor shows successful model load
- [ ] Tested all hardware functions
- [ ] Ready to use!

**🎉 Congratulations! You've trained and deployed your first edge ML model!**

---

## 💡 Pro Tips

1. **Use Python 3.8+** for best compatibility
2. **Keep Serial Monitor open** to watch the model work
3. **Be patient with learning** - 50 samples takes time but works great
4. **Calibrate LDR** with an actual lux meter for best accuracy
5. **Start with center potentiometer** to verify ML works first

---

**Questions? Open the associated .md files (README.md, QUICKSTART.md, etc.)**

**Built with ❤️ for edge ML enthusiasts**

*Last updated: May 25, 2026*
