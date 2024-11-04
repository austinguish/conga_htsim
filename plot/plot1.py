import glob
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

def extract_load_from_filename(filename):
    # Extract load value from filename like "ecmp_50ms_100KB_load50_fct.txt"
    try:
        # Split filename by 'load' and get the number before '_fct.txt'
        load_str = filename.split('load')[1].split('_fct.txt')[0]
        return int(load_str)
    except:
        return None

def process_file(filename):
    # Simply read FCT values directly from file, one value per line
    try:
        with open(filename, 'r') as f:
            fct_values = [float(line.strip()) for line in f if line.strip()]
        return np.mean(fct_values) if fct_values else 0
    except:
        return 0

def analyze_results():
    # Lists to store results
    results = []

    # Process both ECMP and CONGA files
    for pattern in ['ecmp_*_fct.txt', 'conga_*_fct.txt']:
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

    if not df.empty:
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