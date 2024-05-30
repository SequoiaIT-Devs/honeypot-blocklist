#!/bin/bash
# 
# Specify the file containing the IP addresses
file="Unauthorized Access Blocklist"

# Check if the file exists
if [ ! -f "$file" ]; then
    echo "Error: File $file not found!"
    exit 1
fi

# Use a hash map to detect duplicates
declare -A ip_map
duplicate_count=0

echo "Duplicate IP addresses found:"

while read -r line; do
    ip=$(echo "$line" | awk '{print $1}')
    if [[ -n "$ip" ]]; then
        if [[ -n "${ip_map[$ip]}" ]]; then
            echo "$ip"
            duplicate_count=$((duplicate_count + 1))
        else
            ip_map["$ip"]=1
        fi
    fi
done < "$file"

if [[ $duplicate_count -eq 0 ]]; then
    echo "No duplicates found."
fi
