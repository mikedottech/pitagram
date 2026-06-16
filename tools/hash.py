#!/usr/bin/env python3

import os
import hashlib
import shutil
import sys

def hash_filename(filename):
    """Generate an 8-character hash for the given filename."""
    return hashlib.md5(filename.encode()).hexdigest()[:8]

def copy_file_with_hash(input_file, output_dir):
    """Copy a file to the output directory with its hash as the new filename."""
    if not os.path.isfile(input_file):
        print(f"Error: {input_file} is not a valid file.")
        return
    
    # Generate the hash from the filename
    filename = os.path.basename(input_file)
    hash_name = hash_filename(filename)
    ext = os.path.splitext(filename)[1]  # Preserve the original file extension

    # Create the output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)

    # Construct the destination path
    destination = os.path.join(output_dir, f"{hash_name}{ext}")
    
    # Copy the file
    shutil.copy2(input_file, destination)
    print(f"Copied {input_file} to {destination}")

# Example usage
if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python script.py <path_to_file> <output_directory>")
    else:
        input_file = sys.argv[1]
        output_dir = sys.argv[2]
        copy_file_with_hash(input_file, output_dir)
