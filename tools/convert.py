#!/usr/bin/env python3

from PIL import Image, ImageOps, ImageEnhance, ImageFilter
import sys
import os
import traceback
import numpy as np

# Screen color palette
palette_colors = [
    [0,   0,   0  ],
    [255, 255, 255],
    [67,  138, 28 ],
    [100, 64,  255],
    [191, 0,   0  ],
    [255, 243, 56 ],
    [232, 126, 0  ],
    [194, 164, 244]
]

# Resolution of the e-paper display in pixels (w, h)
screen_resolution = (600, 480)

def process_image(input_file):
    # Dict to store rotations corresponding to different EXIF rotation tag values
    rotations = { 3: 180, 6: 270, 8: 90 }
    
    # Create a palette with 256 slots, padded with black (0, 0, 0) after the 8 colors
    palette = []
    for color in palette_colors:
        palette.extend(color)
    palette.extend([0, 0, 0] * (256 - len(palette_colors)))

    # Step 2: Load 'input_image.jpg'
    input_image = Image.open(input_file)

    # Try to get the EXIF data and find the orientation tag
    exif_data = input_image._getexif()

    if exif_data:
        for tag, value in exif_data.items():            
            if tag == 274 and value in rotations: # 274 is the EXIF orientation tag
                input_image = input_image.rotate(rotations[value], expand=True)
                break

    input_width, input_height = input_image.size

    # Rotate image 90 degrees if needed in a way
    # the lower edge will face the kickstand
    if input_height > input_width:
        #print(f"{input_file} is vertical")
        input_image = input_image.rotate(90, expand=True)

    input_width, input_height = input_image.size

    input_image = input_image.convert("RGB")

    canvas_width, canvas_height = screen_resolution    
    
    # Enhance colors and sharpness
    input_image = ImageOps.autocontrast(input_image)
    #input_image = ImageEnhance.Sharpness(input_image).enhance(2.0)
    input_image = input_image.filter(ImageFilter.BoxBlur(radius=0.3))
    input_image = ImageEnhance.Color(input_image).enhance(1.0)
    #input_image = input_image.filter(ImageFilter.MedianFilter(size=3))
    input_image = adjust_gamma(input_image, gamma=1.3)  # Lower gamma brightens shadows

    # The image will be scaled in a way that fills the screen, even if a part
    # gets cropped
    scale = max(canvas_width / input_width, canvas_height / input_height)
    new_width = int(input_width * scale)
    new_height = int(input_height * scale)
    resized_image = input_image.resize((new_width, new_height), Image.LANCZOS)

    # Crop the resized image to fit exactly on the canvas
    left = (new_width - canvas_width) // 2
    upper = (new_height - canvas_height) // 2
    right = left + canvas_width
    lower = upper + canvas_height
    cropped_image = resized_image.crop((left, upper, right, lower))

    # Apply Floyd-Steinberg dithering using the 8-color palette
    palette_image = Image.new("P", (1, 1))
    palette_image.putpalette(palette, rawmode='RGB')
    dithered_image = cropped_image.quantize(palette=palette_image, method=Image.FLOYDSTEINBERG)
    
    return dithered_image

def adjust_gamma(image, gamma=1.0):
    inv_gamma = 1.0 / gamma
    lut = [int((i / 255.0) ** inv_gamma * 255) for i in range(256)] * 3
    return image.point(lut)

def normalize_brightness(image, target_brightness = 130):
    current_brightness = calculate_brightness(image)
    if current_brightness == 0:
        return image  # Avoid division by zero
    factor = target_brightness / current_brightness
    enhancer = ImageEnhance.Brightness(image)
    return enhancer.enhance(factor)

def create_binary_output(image, output_file):
    header = "PTG".encode("ascii")
    filler = [0x0 for _ in range(16 - len(header))]
    buffer = bytearray()
    buffer.extend(header)
    buffer.extend(filler)
    pixels = image.getdata()

    for y in range(image.height):
        for x in range(0, image.width, 2):
            # Get palette index for two adjacent pixels
            pixel1 = pixels[x + y*image.width]
            pixel2 = pixels[x + 1 + y*image.width] if x + 1 < image.width else 0  # Use 0 for padding if out of bounds

            assert(pixel1 < len(palette_colors) and pixel2 < len(palette_colors))

            # Linear 4bpp
            byte = (pixel1 << 4) | pixel2            
            buffer.append(byte)

    # Get the directory part of the file path
    directory = os.path.dirname(output_file)
    
    # Create the directory if it doesn't exist
    if directory and not os.path.exists(directory):
        os.makedirs(directory)
    
    # Write the buffer to a binary file
    with open(output_file, "wb") as f:
        f.write(buffer)

def calculate_brightness(image):
    grayscale = image.convert("L")  # Convert to grayscale
    np_img = np.array(grayscale)
    return np.mean(np_img)  # Average brightness

import os

def truncate_filename(filename, max_bytes=24):
    name, ext = os.path.splitext(filename)
    ext_bytes = len(ext.encode('utf-8'))
    max_name_bytes = max_bytes - ext_bytes
    name_bytes = name.encode('utf-8')

    if len(name_bytes) > max_name_bytes:
        # Truncate safely without breaking multibyte characters
        truncated = b""
        for ch in name:
            bch = ch.encode('utf-8')
            if len(truncated) + len(bch) > max_name_bytes:
                break
            truncated += bch
        name = truncated.decode('utf-8')

    return name + ext

def main():
    if len(sys.argv) != 3:
        print("Usage: python3 convert.py <input_image.jpg> <output_file.bin>")
        sys.exit(1)

    input_file, output_file = sys.argv[1], sys.argv[2]

    # Process the image (handle rotation based on EXIF)
    image = process_image(input_file)

    # Save the final result
    image.save(f"converted_pictures/{os.path.basename(input_file)}_output_.png")

    # Create the binary output
    create_binary_output(image, output_file)

if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        traceback.print_exc()
        exit(e.__hash__())
    exit(0)
