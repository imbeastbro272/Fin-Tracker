"""
Export Decision Tree Model to ESP32-Compatible JSON Format
============================================================
This script extracts the trained decision tree structure and converts it
to a lightweight JSON format that can be loaded on ESP32.

Run this AFTER training your model in the main training script.
"""

import json
import numpy as np
from sklearn.tree import DecisionTreeRegressor

def export_tree_to_json(model, feature_names, output_file='tree_model.json'):
    """
    Export sklearn DecisionTreeRegressor to JSON format.
    
    Args:
        model: Trained sklearn DecisionTreeRegressor
        feature_names: List of feature names
        output_file: Output JSON file path
    """
    tree = model.tree_
    
    def recurse(node_id, depth=0):
        """Recursively build tree structure."""
        if tree.feature[node_id] != -2:  # Not a leaf node
            feature_idx = tree.feature[node_id]
            threshold = float(tree.threshold[node_id])
            
            return {
                'type': 'split',
                'feature_idx': int(feature_idx),
                'feature_name': feature_names[feature_idx],
                'threshold': threshold,
                'left': recurse(tree.children_left[node_id], depth + 1),
                'right': recurse(tree.children_right[node_id], depth + 1)
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
    
    print(f"✓ Model exported to {output_file}")
    print(f"  - Features: {len(feature_names)}")
    print(f"  - Max Depth: {model.get_depth()}")
    print(f"  - Leaves: {model.get_n_leaves()}")
    print(f"  - File Size: {len(json.dumps(tree_structure))} bytes")
    
    return tree_structure


def export_preprocessing_params(le, max_lux, output_file='preprocessing.json'):
    """
    Export preprocessing parameters needed for ESP32.
    
    Args:
        le: LabelEncoder for time of day
        max_lux: Maximum lux value for normalization
        output_file: Output JSON file path
    """
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
    print(f"  - Time Encodings: {params['time_of_day_encoding']}")
    
    return params


# Example usage after training your model:
if __name__ == "__main__":
    print("=" * 80)
    print("MODEL EXPORT FOR ESP32")
    print("=" * 80)
    print("\nIMPORTANT: Run this AFTER training your model in the main script.")
    print("Add this code at the end of your training script:\n")
    print("""
# After training is complete, export the model:
from export_model_to_esp32 import export_tree_to_json, export_preprocessing_params

# Export decision tree structure
export_tree_to_json(
    best_model, 
    features_final, 
    'tree_model.json'
)

# Export preprocessing parameters
export_preprocessing_params(
    le, 
    MAX_LUX, 
    'preprocessing.json'
)

print("\\n✓ Model export complete! Transfer files to ESP32:")
print("  1. tree_model.json")
print("  2. preprocessing.json")
    """)
    print("\n" + "=" * 80)
