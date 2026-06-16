import os

root_dir = "../source_pictures"

for dirpath, dirnames, filenames in os.walk(root_dir, topdown=False):
    # Rename files first
    for name in filenames:
        if ' ' in name:
            old_path = os.path.join(dirpath, name)
            new_name = name.replace(' ', '_')
            new_path = os.path.join(dirpath, new_name)
            os.rename(old_path, new_path)
            print(f"Renamed file: {old_path} -> {new_path}")

    # Then rename directories (bottom-up so children are renamed first)
    for name in dirnames:
        if ' ' in name:
            old_path = os.path.join(dirpath, name)
            new_name = name.replace(' ', '_')
            new_path = os.path.join(dirpath, new_name)
            os.rename(old_path, new_path)
            print(f"Renamed directory: {old_path} -> {new_path}")
