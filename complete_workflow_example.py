"""
Complete End-to-End Workflow Example
=====================================
This script demonstrates the complete workflow from training to deployment:
1. Train Decision Tree model (simplified version of your main script)
2. Export model to ESP32-compatible format
3. Generate deployment instructions

Add this to the END of your main training script to automatically export.
"""

import pandas as pd
import numpy as np
import math
import json
from sklearn.tree import DecisionTreeRegressor
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import LabelEncoder

# ============================================================================
# PART 1: TRAINING (SIMPLIFIED - USE YOUR FULL SCRIPT)
# ============================================================================

print("=" * 80)
print("COMPLETE WORKFLOW: TRAINING → EXPORT → DEPLOYMENT")
print("=" * 80)

# Load your trained model (this assumes you've already trained it)
# In your actual script, you would already have these variables:
# - best_model: Your trained DecisionTreeRegressor
# - features_final: List of feature names
# - le: LabelEncoder for time of day
# - MAX_LUX: Maximum lux value from dataset

# ============================================================================
# PART 2: EXPORT MODEL TO ESP32 FORMAT
# ============================================================================

def export_tree_to_json(model, feature_names, output_file='tree_model.json'):
    """Export sklearn DecisionTreeRegressor to JSON format."""
    tree = model.tree_
    
    def recurse(node_id):
        """Recursively build tree structure."""
        if tree.feature[node_id] != -2:  # Not a leaf node
            feature_idx = tree.feature[node_id]
            threshold = float(tree.threshold[node_id])
            
            return {
                'type': 'split',
                'feature_idx': int(feature_idx),
                'feature_name': feature_names[feature_idx],
                'threshold': threshold,
                'left': recurse(tree.children_left[node_id]),
                'right': recurse(tree.children_right[node_id])
            }
        else:  # Leaf node
            value = float(tree.value[node_id][0][0])
            return {
                'type': 'leaf',
                'value': value
            }
    
    tree_structure = {
        'model_type': 'decision_tree_regressor',
        'n_features': len(feature_names),
        'feature_names': feature_names,
        'max_depth': int(model.get_depth()),
        'n_leaves': int(model.get_n_leaves()),
        'tree': recurse(0)
    }
    
    with open(output_file, 'w') as f:
        json.dump(tree_structure, f, indent=2)
    
    file_size = len(json.dumps(tree_structure))
    
    print(f"\n✓ Model exported to {output_file}")
    print(f"  - Features: {len(feature_names)}")
    print(f"  - Max Depth: {model.get_depth()}")
    print(f"  - Leaves: {model.get_n_leaves()}")
    print(f"  - File Size: {file_size:,} bytes ({file_size/1024:.2f} KB)")
    
    return tree_structure


def export_preprocessing_params(le, max_lux, output_file='preprocessing.json'):
    """Export preprocessing parameters needed for ESP32."""
    params = {
        'max_lux': float(max_lux),
        'time_of_day_encoding': {
            tod: int(encoded) 
            for tod, encoded in zip(le.classes_, le.transform(le.classes_))
        }
    }
    
    with open(output_file, 'w') as f:
        json.dump(params, f, indent=2)
    
    print(f"\n✓ Preprocessing parameters exported to {output_file}")
    print(f"  - Max Lux: {max_lux}")
    print(f"  - Time Encodings:")
    for tod, enc in params['time_of_day_encoding'].items():
        print(f"    • {tod:<20} → {enc}")
    
    return params


def generate_test_predictions(model, feature_names, le, max_lux):
    """Generate test predictions to verify on ESP32."""
    
    print("\n" + "=" * 80)
    print("TEST PREDICTIONS (Verify these match on ESP32)")
    print("=" * 80)
    
    test_scenarios = [
        {'lux': 1100, 'motion': 1, 'hour': 22, 'tod': 'Night', 'desc': 'Night + Bright'},
        {'lux': 0, 'motion': 1, 'hour': 22, 'tod': 'Night', 'desc': 'Night + Dark + Motion'},
        {'lux': 0, 'motion': 0, 'hour': 22, 'tod': 'Night', 'desc': 'Night + Dark + No Motion'},
        {'lux': 7000, 'motion': 1, 'hour': 10, 'tod': 'Morning', 'desc': 'Morning + Bright'},
        {'lux': 200, 'motion': 1, 'hour': 10, 'tod': 'Morning', 'desc': 'Morning + Dim'},
        {'lux': 5000, 'motion': 1, 'hour': 19, 'tod': 'Evening', 'desc': 'Evening + Bright'},
    ]
    
    predictions = []
    
    print("\nFormat: [Scenario] → Predicted Brightness")
    print("-" * 80)
    
    for scenario in test_scenarios:
        lux_norm = scenario['lux'] / max_lux
        motion = scenario['motion']
        hour_sin = math.sin(2 * math.pi * scenario['hour'] / 24)
        hour_cos = math.cos(2 * math.pi * scenario['hour'] / 24)
        time_enc = le.transform([scenario['tod']])[0]
        effective_need = (1 - lux_norm) * motion
        
        features = [[lux_norm, motion, hour_sin, hour_cos, time_enc, effective_need]]
        prediction = model.predict(features)[0]
        
        predictions.append({
            'scenario': scenario,
            'prediction': prediction
        })
        
        print(f"{scenario['desc']:<35} → {prediction:>6.2f}%")
        print(f"  (lux={scenario['lux']}, motion={motion}, hour={scenario['hour']})")
    
    # Save test cases to file
    with open('test_predictions.json', 'w') as f:
        json.dump(predictions, f, indent=2)
    
    print(f"\n✓ Test predictions saved to test_predictions.json")
    
    return predictions


def generate_arduino_test_code(predictions):
    """Generate Arduino test code to verify predictions."""
    
    print("\n" + "=" * 80)
    print("ARDUINO TEST CODE (Add to setup() function for verification)")
    print("=" * 80)
    
    print("""
void runModelTests() {
    Serial.println("\\n" + String('=', 80));
    Serial.println("MODEL VERIFICATION TESTS");
    Serial.println(String('=', 80));
    
    struct TestCase {
        const char* name;
        float lux;
        int motion;
        int hour;
        const char* tod;
        float expected;
    };
    
    TestCase tests[] = {
""")
    
    for pred in predictions:
        s = pred['scenario']
        p = pred['prediction']
        print(f'        {{"{s["desc"]}", {s["lux"]}, {s["motion"]}, {s["hour"]}, "{s["tod"]}", {p:.2f}}},')
    
    print("""    };
    
    int numTests = sizeof(tests) / sizeof(TestCase);
    int passed = 0;
    
    for (int i = 0; i < numTests; i++) {
        TestCase& test = tests[i];
        
        // Compute features
        float lux_norm = test.lux / maxLux;
        float hour_sin = sin(2.0 * PI * test.hour / 24.0);
        float hour_cos = cos(2.0 * PI * test.hour / 24.0);
        int time_enc = getTimeEncoding(String(test.tod));
        float effective_need = (1.0 - lux_norm) * test.motion;
        
        // Predict
        float prediction = predictBrightness(lux_norm, test.motion, hour_sin, 
                                            hour_cos, time_enc, effective_need);
        
        // Check
        float error = abs(prediction - test.expected);
        bool ok = error < 1.0;  // Allow 1% tolerance for floating point
        
        if (ok) {
            passed++;
            Serial.printf("  ✓ Test %d: %s\\n", i+1, test.name);
        } else {
            Serial.printf("  ✗ Test %d: %s\\n", i+1, test.name);
            Serial.printf("    Expected: %.2f%%, Got: %.2f%% (error: %.2f%%)\\n", 
                         test.expected, prediction, error);
        }
    }
    
    Serial.println(String('-', 80));
    Serial.printf("Result: %d/%d tests passed\\n", passed, numTests);
    
    if (passed == numTests) {
        Serial.println("✓ ALL TESTS PASSED! Model verified successfully.");
    } else {
        Serial.println("✗ SOME TESTS FAILED! Check model export.");
    }
    
    Serial.println(String('=', 80));
}
""")


def print_deployment_instructions():
    """Print step-by-step deployment instructions."""
    
    print("\n" + "=" * 80)
    print("DEPLOYMENT INSTRUCTIONS")
    print("=" * 80)
    
    print("""
📋 CHECKLIST - Complete these steps in order:

□ Step 1: Verify Export Files
  ├─ Check tree_model.json exists
  ├─ Check preprocessing.json exists
  └─ Run: python ESP32_SmartLight/upload_to_spiffs.py

□ Step 2: Hardware Setup
  ├─ Wire LDR to GPIO34 (with 10KΩ pulldown)
  ├─ Wire PIR sensor to GPIO23
  ├─ Wire potentiometer to GPIO35
  ├─ Wire LED PWM to GPIO16 (through MOSFET)
  ├─ Connect RTC module (SDA=GPIO21, SCL=GPIO22)
  └─ See ESP32_SmartLight/README.md for wiring diagram

□ Step 3: Arduino IDE Setup
  ├─ Install ESP32 board support
  ├─ Install ArduinoJson library (v6+)
  ├─ Install RTClib library
  └─ Install ESP32 Filesystem Uploader plugin

□ Step 4: Upload to ESP32
  ├─ Open ESP32_SmartLight.ino
  ├─ Select board: "ESP32 Dev Module"
  ├─ Select partition: "Default 4MB with spiffs"
  ├─ Upload SPIFFS data: Tools → ESP32 Sketch Data Upload
  └─ Upload sketch: Ctrl+U

□ Step 5: Test & Verify
  ├─ Open Serial Monitor (115200 baud)
  ├─ Verify model loaded successfully
  ├─ Run built-in tests (add runModelTests() to setup)
  ├─ Test automatic brightness control
  └─ Test manual adjustment with potentiometer

□ Step 6: Monitor Learning
  ├─ Adjust potentiometer when brightness isn't ideal
  ├─ System collects training samples automatically
  └─ Model retrains after 50 adjustments

═══════════════════════════════════════════════════════════════════════════════

📁 FILES GENERATED:
  ✓ tree_model.json         - Decision tree structure for ESP32
  ✓ preprocessing.json      - Normalization parameters
  ✓ test_predictions.json   - Test cases for verification

📂 PROJECT STRUCTURE:
  ESP32_SmartLight/
  ├── ESP32_SmartLight.ino       - Main Arduino sketch
  ├── README.md                  - Complete documentation
  ├── upload_to_spiffs.py        - SPIFFS preparation tool
  └── data/                      - Files to upload to SPIFFS
      ├── tree_model.json
      └── preprocessing.json

═══════════════════════════════════════════════════════════════════════════════

🎯 QUICK START:
  1. cd ESP32_SmartLight
  2. python upload_to_spiffs.py
  3. Open ESP32_SmartLight.ino in Arduino IDE
  4. Tools → ESP32 Sketch Data Upload
  5. Upload sketch (Ctrl+U)
  6. Open Serial Monitor (Ctrl+Shift+M)

═══════════════════════════════════════════════════════════════════════════════

📖 DOCUMENTATION:
  Full guide: ESP32_SmartLight/README.md
  - Wiring diagrams
  - Calibration procedures
  - Troubleshooting
  - Advanced configuration

═══════════════════════════════════════════════════════════════════════════════
""")


# ============================================================================
# EXAMPLE: ADD THIS TO YOUR TRAINING SCRIPT
# ============================================================================

if __name__ == "__main__":
    print("""
╔══════════════════════════════════════════════════════════════════════════════╗
║                                                                              ║
║  ADD THIS CODE TO THE END OF YOUR TRAINING SCRIPT                           ║
║                                                                              ║
╚══════════════════════════════════════════════════════════════════════════════╝

# After training completes and you have best_model, features_final, le, MAX_LUX:

print("\\n" + "=" * 80)
print("EXPORTING MODEL FOR ESP32 DEPLOYMENT")
print("=" * 80)

# Export decision tree structure
export_tree_to_json(best_model, features_final, 'tree_model.json')

# Export preprocessing parameters
export_preprocessing_params(le, MAX_LUX, 'preprocessing.json')

# Generate test predictions for verification
predictions = generate_test_predictions(best_model, features_final, le, MAX_LUX)

# Generate Arduino test code
generate_arduino_test_code(predictions)

# Print deployment instructions
print_deployment_instructions()

print("\\n✓ Model export complete!")
print("  Next: Follow deployment instructions above")

═══════════════════════════════════════════════════════════════════════════════
""")
