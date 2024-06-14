#!/bin/bash

# Specify the SQLite database file
db_file="/root/honeypot-blocklist/blocklist.db"

# Check if the database file exists
if [ ! -f "$db_file" ]; then
    echo "Error: Database file $db_file not found!"
    exit 1
fi

# Use sqlite3 to fetch the IP addresses from the Blocklist table
ip_list=$(sqlite3 "$db_file" "SELECT ip FROM Blocklist;")

# Check if the query was successful
if [ $? -ne 0 ]; then
    echo "Error: Failed to query the database!"
    exit 1
fi

# Use a hash map to detect duplicates
declare -A ip_map
duplicate_count=0

echo "Duplicate IP addresses found:"

# Read IPs from the query result
while IFS= read -r ip; do
    if [[ -n "$ip" ]]; then
        if [[ -n "${ip_map[$ip]}" ]]; then
            echo "$ip"
            duplicate_count=$((duplicate_count + 1))
        else
            ip_map["$ip"]=1
        fi
    fi
done <<< "$ip_list"

if [[ $duplicate_count -eq 0 ]]; then
    echo "No duplicates found."
fi

