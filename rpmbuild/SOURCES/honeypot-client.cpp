#include <iostream>
#include <fstream>
#include <set>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <sstream>
#include <filesystem>
#include <sqlite3.h>

namespace fs = std::filesystem;

std::ofstream logFile("/var/log/honeypot-client.log", std::ios::app);

// Function to log messages to both console and log file
void log(const std::string& message) {
    std::cout << message << std::endl;
    logFile << message << std::endl;
}

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
    
    const char* createAppliedTableSQL = 
        "CREATE TABLE IF NOT EXISTS AppliedBlocklist ("
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
    rc = sqlite3_exec(db, createAppliedTableSQL, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string error = "SQL error: ";
        error += errMsg;
        sqlite3_free(errMsg);
        throw std::runtime_error(error);
    }
}

// Function to check if an IP already exists in the AppliedBlocklist table
bool ipExistsInAppliedBlocklist(sqlite3* db, const std::string& ip) {
    const char* selectSQL = "SELECT 1 FROM AppliedBlocklist WHERE ip = ?;";
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

// Function to add an IP to the AppliedBlocklist table
void addIPToAppliedBlocklist(sqlite3* db, const std::string& ip) {
    const char* insertSQL = "INSERT INTO AppliedBlocklist (ip) VALUES (?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, insertSQL, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement");
    }

    sqlite3_bind_text(stmt, 1, ip.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        throw std::runtime_error("Failed to insert IP into AppliedBlocklist");
    }
}

// Function to determine if firewalld is active
bool isFirewalldActive() {
    std::string output = exec("systemctl is-active firewalld");
    return (output.find("active") != std::string::npos);
}

// Function to determine if ufw is active
bool isUfwActive() {
    std::string output = exec("ufw status");
    return (output.find("Status: active") != std::string::npos);
}

// Function to sync and apply the blocklist
void syncBlocklist() {
    const std::string repoPath = "/root/honeypot-blocklist";
    const std::string dbPath = repoPath + "/blocklist.db";

    // Sync the GitHub repository
    std::string command = "cd " + repoPath + " && git pull origin main";
    if (std::system(command.c_str()) != 0) {
        throw std::runtime_error("Error syncing GitHub repository");
    }
    log("GitHub repository synced successfully.");

    // Open the SQLite database
    sqlite3* db = nullptr;
    int rc = sqlite3_open(dbPath.c_str(), &db);
    if (rc) {
        throw std::runtime_error("Can't open database: " + std::string(sqlite3_errmsg(db)));
    }
    initializeDatabase(db);

    // Determine which firewall is active and apply IPs accordingly
    bool firewalldActive = isFirewalldActive();
    bool ufwActive = isUfwActive();

    if (!firewalldActive && !ufwActive) {
        throw std::runtime_error("Neither firewalld nor ufw is active.");
    }

    const char* selectSQL = "SELECT ip FROM Blocklist;";
    sqlite3_stmt* stmt = nullptr;
    rc = sqlite3_prepare_v2(db, selectSQL, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement");
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string ip = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (!ip.empty() && !ipExistsInAppliedBlocklist(db, ip)) {
            if (firewalldActive) {
                std::string firewallCommand = "firewall-cmd --permanent --add-rich-rule='rule family=\"ipv4\" source address=\"" + ip + "\" reject'";
                exec(firewallCommand.c_str());
                log("IP " + ip + " added to firewalld.");
            } else if (ufwActive) {
                std::string ufwCommand = "ufw deny from " + ip;
                exec(ufwCommand.c_str());
                log("IP " + ip + " added to ufw.");
            }
            addIPToAppliedBlocklist(db, ip);
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    if (firewalldActive) {
        exec("firewall-cmd --reload");
        log("Firewalld reloaded.");
    } else if (ufwActive) {
        exec("ufw reload");
        log("Ufw reloaded.");
    }
}

int main() {
    try {
        syncBlocklist();
        log("Blocklist has been synced and applied.");
    } catch (const std::exception& e) {
        log("Error: " + std::string(e.what()));
        return 1;
    }
    return 0;
}
