#!/bin/bash
#SBATCH -J conga           # Job name
#SBATCH -o job.%j.out      # Name of stdout output file (%j expands to jobId)
#SBATCH -N 1               # Total number of nodes requested
#SBATCH -t 02:30:00        # Run time (hh:mm:ss) - 1.5 hours
#SBATCH -p mi2104x         # Desired partition
#SBATCH --mail-user=jiang357@wisc.edu
#SBATCH --mail-type=BEGIN
#SBATCH --mail-type=END
#SBATCH --mail-type=FAIL

# Array of load values to test
loads=(10 20 30 40 50 60 70 80 90 100)

# Run experiments for each load value
for load in "${loads[@]}"; do
    # ECMP test
    echo "Running ECMP test with load=${load}"
    ./htsim --expt=1 --flowgen="ecmp" --duration=50 --load=${load} > ecmp_50ms_100KB_load${load}.txt

    # CONGA test
    echo "Running CONGA test with load=${load}"
    ./htsim --expt=2 --flowgen="conga" --duration=50 --load=${load} > conga_50ms_100KB_load${load}.txt
done

echo "All experiments completed!"