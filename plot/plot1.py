import glob
import re
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

def extract_load_from_filename(filename):
    # Extract load value from filename like "ecmp_50ms_100KB_load50.txt"
    match = re.search(r'load(\d+)', filename)
    return int(match.group(1)) if match else None

def process_file(filename):
    fct_values = []
    with open(filename, 'r') as f:
        for line in f:
            if line.startswith('Flow src'):
                # Extract FCT using regex
                match = re.search(r'fct (\d+\.?\d*)', line)
                if match:
                    fct = float(match.group(1))
                    fct_values.append(fct)
    return np.mean(fct_values) if fct_values else 0

def analyze_results():
    # Lists to store results
    results = []

    # Process both ECMP and CONGA files
    for pattern in ['ecmp_*.txt', 'conga_*.txt']:
        for filename in glob.glob(pattern):
            load = extract_load_from_filename(filename)
            if load is not None:
                avg_fct = process_file(filename)
                algorithm = 'ECMP' if 'ecmp' in filename else 'CONGA'
                results.append({
                    'Algorithm': algorithm,
                    'Load': load,
                    'Average FCT': avg_fct
                })

    # Convert to DataFrame
    df = pd.DataFrame(results)

    # Create the plot
    plt.figure(figsize=(10, 6))
    sns.set_style("whitegrid")

    # Plot lines for both algorithms
    sns.lineplot(data=df, x='Load', y='Average FCT', hue='Algorithm', marker='o')

    plt.title('Average Flow Completion Time vs Load')
    plt.xlabel('Load (%)')
    plt.ylabel('Average Flow Completion Time (ms)')

    # Save the plot
    plt.savefig('fct_comparison.png')
    plt.close()

    # Save the data to CSV
    df.to_csv('fct_results.csv', index=False)

    return df

if __name__ == "__main__":
    results_df = analyze_results()
    print("\nResults Summary:")
    print(results_df)