#!/bin/bash

# Create and activate a virtual environment in a temporary directory
VENV_DIR=$(mktemp -d)
python3 -m venv $VENV_DIR
source $VENV_DIR/bin/activate

# Install required packages
pip install -r requirements.txt
