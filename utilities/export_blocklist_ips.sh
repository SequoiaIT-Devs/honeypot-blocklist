#!/bin/bash

# Path to the SQLite database
db_path="/root/honeypot-blocklist/blocklist.db"
# Path to the export file
export_file="/root/honeypot-blocklist/Unauthorized Access Blocklist"

# Check if the database file exists
if [ ! -f "$db_path" ]; then
    echo "Error: Database file $db_path not found!"
    exit 1
fi

# Execute the SQL command to export the IPs to the file
sqlite3 "$db_path" "SELECT ip FROM Blocklist;" > "$export_file"

# Check if the SQL command was successful
if [ $? -ne 0 ]; then
    echo "Error: Failed to export IPs from the database!"
    exit 1
fi

# Output success message
echo "IPs have been exported to $export_file"
