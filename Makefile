# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Miguel Angel Exposito Sanchez

# Directories
SRC_DIR := source_pictures
OUT_DIR := converted_pictures
OUT_DIR_HASHED := hashed_pictures

# File extensions
SRC_EXT := .jpg .jpeg .png
OUT_EXT := .bin

# Find all JPG and PNG files in the source directory
SRC_FILES := $(foreach ext,$(SRC_EXT),$(wildcard $(SRC_DIR)/*$(ext)))

# Generate corresponding output files in the output directory
OUT_FILES := $(patsubst $(SRC_DIR)/%.jpg, $(OUT_DIR)/%${OUT_EXT}, $(SRC_FILES)) $(patsubst $(SRC_DIR)/%.png, $(OUT_DIR)/%${OUT_EXT}, $(SRC_FILES)) $(patsubst $(SRC_DIR)/%.jpeg, $(OUT_DIR)/%${OUT_EXT}, $(SRC_FILES))
OUT_HASHED_FILES := $(patsubst $(OUT_DIR)/%.bin, $(OUT_DIR_HASHED)/%${OUT_EXT}, $(OUT_FILES))


# Ensure output directory exists
$(shell mkdir -p $(OUT_DIR))
$(shell mkdir -p $(OUT_DIR_HASHED))

# Pattern rule to convert source files to .bin files
$(OUT_DIR)/%.bin: $(SRC_DIR)/%.jpg
	@echo "$< -> $@"
	@./tools/convert.py $< $@
	
$(OUT_DIR)/%.bin: $(SRC_DIR)/%.jpeg
	@echo "$< -> $@"
	@./tools/convert.py $< $@

$(OUT_DIR)/%.bin: $(SRC_DIR)/%.png
	@echo "$< -> $@"
	@./tools/convert.py $< $@

${OUT_DIR_HASHED}/%.bin: $(OUT_DIR)/%.bin
	@echo "Hashing $<"
	@./tools/hash.py $< ${OUT_DIR_HASHED}


# Default target to convert all files
all: $(OUT_FILES)
hashed: ${OUT_HASHED_FILES}

# Clean up the output files
clean:
	rm -f $(OUT_DIR)/*${OUT_EXT}
	rm -f ${OUT_DIR_HASHED}/*${OUT_EXT}
