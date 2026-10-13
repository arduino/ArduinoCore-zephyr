#!/usr/bin/env python3
import sys
import re
import shlex

def extract_squash_args(boards_txt_path, target_board, platform_path, variant_path, bootloader_file, build_variant):
    try:
        with open(boards_txt_path, 'r') as f:
            lines = f.readlines()
    except FileNotFoundError:
        sys.exit(0) # Fail silently if not found

    # Regex to find: <board_prefix>.build.zsk_args.squash=...
    pattern = re.compile(rf"^{target_board}\.build\.zsk_args\.squash=(.+)$")
    
    for line in lines:
        match = pattern.match(line.strip())
        if match:
            raw_args = match.group(1)
            
            # Substitute the Arduino IDE placeholders with actual shell variables
            raw_args = raw_args.replace('{runtime.platform.path}', platform_path)
            raw_args = raw_args.replace('{build.variant.path}', variant_path)
            raw_args = raw_args.replace('{bootloader.file}', bootloader_file)
            raw_args = raw_args.replace('{build.variant}', build_variant)
            
            # Print securely formatted arguments string
            print(" ".join(shlex.split(raw_args)))
            sys.exit(0)

    # Return nothing if property is not found
    sys.exit(0)

if __name__ == "__main__":
    if len(sys.argv) != 7:
        sys.exit(1)
    
    extract_squash_args(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5], sys.argv[6])