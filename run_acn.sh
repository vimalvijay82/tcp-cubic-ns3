#!/bin/bash

# Function to plot graph
plot_graph() {
    local DATA_FILE=$1
    local OUTPUT_FILE=$2
    local TITLE=$3

    gnuplot -e "set terminal pngcairo enhanced font 'arial,10' size 600,400; set output '$OUTPUT_FILE'; set xlabel 'X-axis'; set ylabel 'Y-axis'; set title '$TITLE'; plot '$DATA_FILE' with lines title 'Data Line'"
}

# Array of file names for wired simulation
WIRED_FILES=("wired.cwnd" "wired.ssthresh" "wired.throughput")
WIRELESS_FILES=("wireless.cwnd" "wireless.ssthresh" "wireless.throughput")

# Function to run simulation
run_simulation() {
    local SIMULATION_TYPE=$1
    local FILES_ARRAY=("${@:2}")

    # Run simulation
    echo "Running $SIMULATION_TYPE simulation"
    ./ns3 run "scratch/$SIMULATION_TYPE.cc"

    # Process files
    for FILE in "${FILES_ARRAY[@]}"; do
        FILE_PATH="./outputs/$FILE"
        
        # Remove first line from output file if it contains "ssthresh"
        if [[ $FILE =~ ssthresh ]]; then
            sed 1d "$FILE_PATH" > "$FILE_PATH.tmp" && mv "$FILE_PATH.tmp" "$FILE_PATH"
        fi

        # Plot graph for each file
        echo "Plotting $SIMULATION_TYPE $FILE"
        plot_graph "$FILE_PATH" "$FILE_PATH.png" "$FILE"
    done
}

# Run wired simulation
run_simulation "wired" "${WIRED_FILES[@]}"

run_simulation "wireless" "${WIRELESS_FILES[@]}"


echo "Complete!"
