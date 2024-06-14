#!/bin/bash

# Path to the SQLite database
db_path="/root/honeypot-blocklist/blocklist.db"

# Check if the database file exists
if [ ! -f "$db_path" ]; then
    echo "Error: Database file $db_path not found!"
    exit 1
fi

# Execute the SQL command to count the number of IPs in the Blocklist table
ip_count=$(sqlite3 "$db_path" "SELECT COUNT(*) FROM Blocklist;")

# Check if the SQL command was successful
if [ $? -ne 0 ]; then
    echo "Error: Failed to query the database!"
    exit 1
fi

# Output the count of IPs
echo "Number of IPs in the Blocklist table: $ip_count"
