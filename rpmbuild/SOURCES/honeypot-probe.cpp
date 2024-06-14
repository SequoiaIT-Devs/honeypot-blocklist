#include <iostream>
#include <fstream>
#include <set>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <sstream>
#include <ctime>
#include <unistd.h>
#include <filesystem>
#include <sqlite3.h>

// Function to execute a shell command and return the output
std::string exec(const char* cmd) {
    char buffer[128];
    std::string result = "";
    FILE* pipe = popen(cmd, "r");
    if (!pipe) throw std::runtime_error("popen() failed!");
    try {
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
    } catch (...) {
        pclose(pipe);
        throw;
    }
    pclose(pipe);
    return result;
}

// Function to execute a shell command and return the result code
int execWithResult(const std::string& cmd) {
    int result = std::system(cmd.c_str());
    if (result != 0) {
        std::cerr << "Command failed with exit code " << result << ": " << cmd << std::endl;
    }
    return result;
}

// Function to log messages with timestamps
void logMessage(const std::string& message) {
    std::ofstream logFile("/var/log/honeypot-probe.log", std::ios::app);
    if (!logFile.is_open()) {
        throw std::runtime_error("Error opening log file for writing");
    }

    std::time_t now = std::time(nullptr);
    char timestamp[20];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    logFile << "[" << timestamp << "] " << message << std::endl;
    logFile.close();
}

// Function to trim leading and trailing whitespace from a string
std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    size_t end = str.find_last_not_of(" \t\n\r");
    return (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
}

// Function to initialize the SQLite database
void initializeDatabase(sqlite3* db) {
    const char* createTableSQL = 
        "CREATE TABLE IF NOT EXISTS Blocklist ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "ip TEXT NOT NULL UNIQUE);";
    
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, createTableSQL, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string error = "SQL error: ";
        error += errMsg;
        sqlite3_free(errMsg);
        throw std::runtime_error(error);
    }
}

// Function to check if an IP already exists in the database
bool ipExistsInDatabase(sqlite3* db, const std::string& ip) {
    const char* selectSQL = "SELECT 1 FROM Blocklist WHERE ip = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, selectSQL, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement");
    }

    sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    bool exists = (rc == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}

// Function to add an IP to the database and commit to GitHub
void addIPToDatabase(sqlite3* db, const std::string& ip) {
    if (ipExistsInDatabase(db, ip)) {
        std::cout << "IP " << ip << " already exists in blocklist. Skipping." << std::endl;
        return;
    }

    const char* insertSQL = "INSERT INTO Blocklist (ip) VALUES (?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, insertSQL, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement");
    }

    sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        throw std::runtime_error("Failed to insert IP into database");
    }

    std::string command = "git add \"blocklist.db\" && git commit -m \"Add " + ip + " to blocklist\"";
    if (execWithResult(command) != 0) {
        throw std::runtime_error("Error committing IP to Git repository: " + ip);
    }
}

// Function to upgrade from single file to database
void upgradeBlocklistToDatabase(const std::string& blocklistFile, sqlite3* db) {
    std::ifstream file(blocklistFile);
    if (!file.is_open()) {
        throw std::runtime_error("Error opening file for reading: " + blocklistFile);
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (!line.empty()) {
            addIPToDatabase(db, line);
        }
    }

    // Optionally remove the old blocklist file after upgrade
    std::filesystem::remove(blocklistFile);
}

int main(int argc, char* argv[]) {
    try {
        const std::string repoPath = "/root/honeypot-blocklist";
        const std::string blocklistFile = repoPath + "/Unauthorized Access Blocklist";
        const std::string dbPath = repoPath + "/blocklist.db";

        // Ensure the repository path exists
        if (!std::filesystem::exists(repoPath)) {
            std::cerr << "Repository path not found." << std::endl;
            return 1;
        }

        // Change to the local repository directory
        if (chdir(repoPath.c_str()) != 0) {
            throw std::runtime_error("Failed to change directory to " + repoPath);
        }

        // Pull the latest changes from the Git repository
        logMessage("Pulling the latest changes from the Git repository");
        std::string pullCommand = "cd " + repoPath + " && git reset --hard && git pull --rebase origin main";
        if (execWithResult(pullCommand) != 0) {
            throw std::runtime_error("Error pulling from Git repository");
        }

        // Open the SQLite database
        sqlite3* db = nullptr;
        int rc = sqlite3_open(dbPath.c_str(), &db);
        if (rc) {
            throw std::runtime_error("Can't open database: " + std::string(sqlite3_errmsg(db)));
        }
        initializeDatabase(db);

        // Check if the --upgrade flag is provided
        if (argc > 1 && std::strcmp(argv[1], "--upgrade") == 0) {
            logMessage("Upgrading blocklist to database");
            upgradeBlocklistToDatabase(blocklistFile, db);
            logMessage("Blocklist upgraded to database successfully");
            sqlite3_close(db);
            return 0;
        }

        std::string jail = "sshd";
        
        // Fetch the list of banned IPs from Fail2ban using sed
        logMessage("Fetching the list of banned IPs from Fail2ban");
        std::string result = exec(("sudo fail2ban-client status " + jail + " | grep 'Banned IP list:' | sed 's/.*Banned IP list:[ ]*//'").c_str());

        // Debugging: Print the raw output from fail2ban-client status
        std::cout << "Raw banned IPs output:" << std::endl;
        std::cout << result << std::endl;

        // Convert the result to a stringstream
        std::istringstream iss(result);
        std::string ip;
        
        // Use a set to store IPs and avoid duplicates
        std::set<std::string> ip_set;
        while (iss >> ip) {
            ip = trim(ip);
            if (!ip.empty()) {
                ip_set.insert(ip);
            }
        }

        bool updated = false;

        // Loop through each banned IP and commit it to GitHub
        for (const auto& ip : ip_set) {
            logMessage("Processing IP: " + ip);
            std::cout << "Adding IP to blocklist: " << ip << std::endl;
            addIPToDatabase(db, ip);
            updated = true;
        }

        // Push the changes to the remote repository if there were updates
        if (updated) {
            logMessage("Pushing changes to the remote Git repository");
            std::string pushCommand = "cd " + repoPath + " && git push origin main";
            if (execWithResult(pushCommand) != 0) {
                throw std::runtime_error("Error pushing to Git repository");
            }
        }

        logMessage("All banned IPs from " + jail + " have been added to the blocklist.");
        sqlite3_close(db);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        logMessage("Error: " + std::string(e.what()));
        return 1;
    }

    return 0;
}
