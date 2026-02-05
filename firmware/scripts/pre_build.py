#!/usr/bin/env python3
"""
Pre-build script for PlatformIO
Copies config.json to the firmware filesystem image
"""

import os
import shutil
import sys

Import("env")

CONFIG_SOURCE = "config/config.json"
CONFIG_DEST = "data/config.json"

def copy_config():
    """Copy config file to data directory for filesystem."""
    if os.path.exists(CONFIG_SOURCE):
        os.makedirs(os.path.dirname(CONFIG_DEST), exist_ok=True)
        shutil.copy2(CONFIG_SOURCE, CONFIG_DEST)
        print(f"Copied {CONFIG_SOURCE} to {CONFIG_DEST}")
    else:
        print(f"Warning: {CONFIG_SOURCE} not found, using defaults")
        # Create default config
        os.makedirs(os.path.dirname(CONFIG_DEST), exist_ok=True)
        with open(CONFIG_DEST, "w") as f:
            f.write('{}')

copy_config()
