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
    } else {
        std::cout << "IP " << ip << " is already in the blocklist." << std::endl;
    }
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

    // Read the blocklist file
    std::ifstream inputFile(blocklistFile);
    if (!inputFile.is_open()) {
        throw std::runtime_error("Error opening blocklist file: " + blocklistFile);
    }

    // Apply IPs to firewalld
    std::string line;
    while (std::getline(inputFile, line)) {
        std::string ip = trim(line);
        if (!ip.empty()) {
            std::string firewallCommand = "firewall-cmd --permanent --add-rich-rule='rule family=\"ipv4\" source address=\"" + ip + "\" reject'";
            exec(firewallCommand.c_str());
        }
    }
    inputFile.close();

    // Reload firewalld
    exec("firewall-cmd --reload");
}

int main() {
    try {
        syncBlocklist();
        std::cout << "Blocklist has been synced and applied." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
