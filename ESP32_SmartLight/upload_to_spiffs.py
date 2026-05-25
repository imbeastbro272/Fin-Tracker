#!/usr/bin/env python3
"""
Utility script to prepare and upload model files to ESP32 SPIFFS
================================================================
This script helps automate the SPIFFS upload process by:
1. Validating JSON files
2. Checking file sizes
3. Providing upload instructions

Usage:
    python upload_to_spiffs.py
"""

import json
import os
import sys

def validate_json_file(filepath):
    """Validate JSON file can be parsed."""
    try:
        with open(filepath, 'r') as f:
            data = json.load(f)
        return True, data
    except FileNotFoundError:
        return False, f"File not found: {filepath}"
    except json.JSONDecodeError as e:
        return False, f"Invalid JSON: {e}"

def get_file_size(filepath):
    """Get file size in bytes."""
    try:
        return os.path.getsize(filepath)
    except:
        return 0

def main():
    print("=" * 80)
    print("ESP32 SPIFFS Upload Preparation Tool")
    print("=" * 80)
    
    # Check if we're in the right directory
    sketch_file = "ESP32_SmartLight.ino"
    if not os.path.exists(sketch_file):
        print(f"\n✗ Error: {sketch_file} not found!")
        print("  Please run this script from the ESP32_SmartLight directory.")
        sys.exit(1)
    
    print("\n[Step 1] Checking for required files...")
    print("-" * 80)
    
    required_files = [
        '../tree_model.json',
        '../preprocessing.json'
    ]
    
    all_files_exist = True
    total_size = 0
    
    for filepath in required_files:
        filename = os.path.basename(filepath)
        if os.path.exists(filepath):
            valid, data = validate_json_file(filepath)
            size = get_file_size(filepath)
            total_size += size
            
            if valid:
                print(f"  ✓ {filename:<25} [{size:>6} bytes] Valid JSON")
                
                # Show some metadata
                if filename == 'tree_model.json':
                    print(f"    - Max Depth: {data.get('max_depth', 'N/A')}")
                    print(f"    - Leaves: {data.get('n_leaves', 'N/A')}")
                elif filename == 'preprocessing.json':
                    print(f"    - Max Lux: {data.get('max_lux', 'N/A')}")
            else:
                print(f"  ✗ {filename:<25} INVALID: {data}")
                all_files_exist = False
        else:
            print(f"  ✗ {filename:<25} NOT FOUND")
            all_files_exist = False
    
    if not all_files_exist:
        print("\n✗ Missing or invalid files! Please run export_model_to_esp32.py first.")
        sys.exit(1)
    
    print(f"\n  Total size: {total_size} bytes ({total_size/1024:.2f} KB)")
    
    # Create data directory
    print("\n[Step 2] Creating data directory...")
    print("-" * 80)
    
    data_dir = "data"
    if not os.path.exists(data_dir):
        os.makedirs(data_dir)
        print(f"  ✓ Created '{data_dir}/' directory")
    else:
        print(f"  ✓ '{data_dir}/' directory already exists")
    
    # Copy files
    print("\n[Step 3] Copying files to data directory...")
    print("-" * 80)
    
    import shutil
    
    for src in required_files:
        dst = os.path.join(data_dir, os.path.basename(src))
        try:
            shutil.copy2(src, dst)
            print(f"  ✓ Copied {os.path.basename(src)} → {dst}")
        except Exception as e:
            print(f"  ✗ Failed to copy {os.path.basename(src)}: {e}")
            sys.exit(1)
    
    # Verify SPIFFS capacity
    print("\n[Step 4] Checking SPIFFS capacity...")
    print("-" * 80)
    
    spiffs_size = 1024 * 1024  # 1MB typical SPIFFS partition
    usage_percent = (total_size / spiffs_size) * 100
    
    print(f"  SPIFFS size: {spiffs_size / 1024:.0f} KB (typical)")
    print(f"  Model files: {total_size / 1024:.2f} KB")
    print(f"  Usage: {usage_percent:.1f}%")
    
    if usage_percent > 50:
        print("  ⚠ Warning: Model files use >50% of SPIFFS")
        print("    Consider using a larger partition scheme in Arduino IDE")
    else:
        print("  ✓ Sufficient space available")
    
    # Provide upload instructions
    print("\n[Step 5] Upload instructions")
    print("-" * 80)
    print("""
  1. Open ESP32_SmartLight.ino in Arduino IDE

  2. Install ESP32 Filesystem Uploader:
     - Download: https://github.com/me-no-dev/arduino-esp32fs-plugin
     - Extract to: <Arduino_IDE_folder>/tools/ESP32FS/tool/
     - Restart Arduino IDE

  3. Configure board settings:
     Tools → Board → "ESP32 Dev Module"
     Tools → Partition Scheme → "Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)"
     Tools → Port → <your ESP32 port>

  4. Upload SPIFFS data:
     Tools → ESP32 Sketch Data Upload
     (Wait for "SPIFFS Image Uploaded" message)

  5. Upload sketch:
     Sketch → Upload
     (Or press Ctrl+U)

  6. Open Serial Monitor (115200 baud) to verify
    """)
    
    print("=" * 80)
    print("✓ Preparation complete! Ready for SPIFFS upload.")
    print("=" * 80)
    
    # List data directory contents
    print("\nFiles in data/ directory:")
    for item in os.listdir(data_dir):
        full_path = os.path.join(data_dir, item)
        size = get_file_size(full_path)
        print(f"  - {item:<30} {size:>8} bytes")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\n✗ Interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n✗ Unexpected error: {e}")
        sys.exit(1)
