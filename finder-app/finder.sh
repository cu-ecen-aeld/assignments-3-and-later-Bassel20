#!/bin/sh
# Finder script for assignment 1
# Author: Bassel Sherif

# Check if the correct number of arguments is provided
if [ "$#" -ne 2 ]; then
    echo "Error: Please provide two arguments - directory path and string to be searched for."
    exit 1
fi

# Extract arguments
filesdir="$1"
searchstr="$2"

if [ ! -d "$filesdir" ]; then
    echo "Directory does not exist."
    exit 1
fi

# Find matching lines and count them
matching_lines=$(grep -r "$searchstr" "$filesdir" | wc -l)

# Count the number of files in the directory 
num_files=$(find "$filesdir" -type f | wc -l)

# Print the result
echo "The number of files are $num_files and the number of matching lines are $matching_lines"

exit 0
