#!/bin/bash

# Check if a filename was provided
if [ $# -ne 1 ]; then
  echo "Usage: $0 input_binary_file"
  exit 1
fi

input_file=$1

if [[ "$input_file" == *.asm ]]; then
  echo "Usage: $0 input_binary_file"
  echo "ERROR: Input file $input_file appears to be an ASM file, not a binary file."
  echo "Please provide a binary file instead."
  exit 1
fi

temp_asm_file=$(mktemp --suffix=.asm)
temp_obj_file=$(mktemp --suffix=.obj)

# Run decode8086.exe to generate ASM
./decode8086.exe "$input_file" > "$temp_asm_file"

# Check if decode8086 succeeded
if [ $? -ne 0 ]; then
  echo "Failed to generate ASM from $input_file"
  rm "$temp_asm_file"
  exit 1
fi

# Assemble the generated ASM with nasm
./nasm.exe -o "$temp_obj_file" "$temp_asm_file"

# Check if NASM succeeded
if [ $? -ne 0 ]; then
  echo "NASM assembly failed"
  rm "$temp_asm_file" "$temp_obj_file"
  exit 1
fi

# Compare the original binary with the nasm output
if cmp -s "$input_file" "$temp_obj_file"; then
  echo "SUCCESS: Correctly decoded ${input_file}"
else
  echo "ERROR: Files do not match"
  # Optional: Add detailed comparison
  # hexdump -C "$input_file" > input.hex
  # hexdump -C "$temp_obj_file" > output.hex
  # echo "Differences:"
  # diff input.hex output.hex
  # rm input.hex output.hex
fi

# Clean up
rm "$temp_asm_file" "$temp_obj_file"
