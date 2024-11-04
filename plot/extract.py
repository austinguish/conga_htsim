import re
import os
import glob

def extract_fct(input_file):
    # Create output filename by adding '_fct' before the extension
    base, ext = os.path.splitext(input_file)
    output_file = f"{base}_fct{ext}"

    fct_values = []

    # Read the input file and extract FCT values
    with open(input_file, 'r') as f:
        for line in f:
            if line.startswith('Flow src'):
                match = re.search(r'fct (\d+\.?\d*)', line)
                if match:
                    fct_values.append(float(match.group(1)))

    # Write FCT values to output file
    with open(output_file, 'w') as f:
        for fct in fct_values:
            f.write(f"{fct}\n")

    return len(fct_values)

def main():
    # Process all txt files in current directory
    for input_file in glob.glob("*.txt"):
        if not input_file.endswith('_fct.txt'):  # Skip previously generated FCT files
            num_values = extract_fct(input_file)
            print(f"Processed {input_file}: extracted {num_values} FCT values")

if __name__ == "__main__":
    main()