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

// Function to add an IP to the local blocklist file and commit to GitHub
void addIPToFile(const std::string& blocklistFile, const std::string& ip) {
    // Read current IPs from blocklist
    std::ifstream inputFile(blocklistFile);
    std::set<std::string> currentIPs;
    std::string line;
    while (std::getline(inputFile, line)) {
        currentIPs.insert(trim(line));
    }
    inputFile.close();

    // Check if the IP is already in the set
    if (currentIPs.find(ip) == currentIPs.end()) {
        std::ofstream outputFile(blocklistFile, std::ios::app);
        if (!outputFile.is_open()) {
            throw std::runtime_error("Error opening file for writing: " + blocklistFile);
        }
        outputFile << ip << std::endl;
        outputFile.close();
        log("IP " + ip + " added to blocklist.");
    } else {
        log("IP " + ip + " is already in the blocklist.");
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
    const std::string blocklistFile = repoPath + "/Unauthorized Access Blocklist";

    // Sync the GitHub repository
    std::string command = "cd " + repoPath + " && git pull origin main";
    if (std::system(command.c_str()) != 0) {
        throw std::runtime_error("Error syncing GitHub repository");
    }
    log("GitHub repository synced successfully.");

    // Read the blocklist file
    std::ifstream inputFile(blocklistFile);
    if (!inputFile.is_open()) {
        throw std::runtime_error("Error opening blocklist file: " + blocklistFile);
    }

    // Determine which firewall is active and apply IPs accordingly
    bool firewalldActive = isFirewalldActive();
    bool ufwActive = isUfwActive();

    if (!firewalldActive && !ufwActive) {
        throw std::runtime_error("Neither firewalld nor ufw is active.");
    }

    std::string line;
    while (std::getline(inputFile, line)) {
        std::string ip = trim(line);
        if (!ip.empty()) {
            if (firewalldActive) {
                std::string firewallCommand = "firewall-cmd --permanent --add-rich-rule='rule family=\"ipv4\" source address=\"" + ip + "\" reject'";
                exec(firewallCommand.c_str());
                log("IP " + ip + " added to firewalld.");
            } else if (ufwActive) {
                std::string ufwCommand = "ufw deny from " + ip;
                exec(ufwCommand.c_str());
                log("IP " + ip + " added to ufw.");
            }
        }
    }
    inputFile.close();

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
