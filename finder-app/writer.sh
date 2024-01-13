#!/bin/sh
# Writer script for assignment 1
# Author: Bassel Sherif

# Check if the correct number of arguments is provided
if [ "$#" -ne 2 ]; then
    echo "Error: Please provide two arguments - full path to the file and string to be written."
    exit 1
fi

# Extract arguments
writefile="$1"
writestr="$2"

# Create directory if it does not exist
mkdir -p "$(dirname "$writefile")"

# Write the content to the file
echo "$writestr" > "$writefile"

# Check if the file was created successfully
if [ $? -ne 0 ]; then
    echo "Error: Could not create the file!"
    exit 1
fi

exit 0
