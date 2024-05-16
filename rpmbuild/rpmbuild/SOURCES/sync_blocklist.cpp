#include <iostream>
#include <fstream>
#include <set>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <sstream>
#include <cstring>

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

// Function to read IPs from a local file
std::set<std::string> getIPsFromFile(const std::string& filename) {
    std::set<std::string> ips;
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Error opening file: " + filename);
    }
    
    std::string ip;
    while (std::getline(file, ip)) {
        if (!ip.empty()) {
            ips.insert(ip);
        }
    }
    file.close();
    return ips;
}

// Function to get IPs currently blocked by Firewalld
std::set<std::string> getIPsFromFirewalld() {
    std::set<std::string> ips;
    std::string result = exec("sudo firewall-cmd --list-all");
    size_t pos = 0;
    while ((pos = result.find("source address=\"", pos)) != std::string::npos) {
        pos += strlen("source address=\"");
        size_t end = result.find("\"", pos);
        ips.insert(result.substr(pos, end - pos));
        pos = end;
    }
    return ips;
}

// Function to add an IP to Firewalld
void addIPToFirewalld(const std::string& ip) {
    // Check if the IP is already blocked
    std::string checkCommand = "sudo firewall-cmd --permanent --list-rich-rules | grep -q '" + ip + "'";
    if (!exec(checkCommand.c_str()).empty()) {
        std::cout << "IP already blocked: " << ip << std::endl;
    } else {
        // Add the IP to Firewalld's blocked list
        exec(("sudo firewall-cmd --permanent --add-rich-rule='rule family=\"ipv4\" source address=\"" + ip + "\" reject'").c_str());
        // Reload Firewalld to apply changes
        exec("sudo firewall-cmd --reload");
    }
}

// Function to add an IP to the local file, ensuring no duplicates
void addIPToFile(const std::string& filename, const std::string& ip) {
    // Read current IPs from the file
    std::set<std::string> currentIPs = getIPsFromFile(filename);
    // Check if the IP is already in the file
    if (currentIPs.find(ip) == currentIPs.end()) {
        // Append the IP to the file if it is not already present
        std::ofstream file;
        file.open(filename, std::ios::app);
        if (!file.is_open()) {
            throw std::runtime_error("Error opening file for writing: " + filename);
        }
        file << ip << std::endl;
        file.close();
        std::cout << "IP added to file: " << ip << std::endl;
    } else {
        std::cout << "IP already exists in file: " << ip << std::endl;
    }
}

int main() {
    try {
        const std::string repoPath = "/root/honeypot-blocklist";
        const std::string filename = repoPath + "/Unauthorized Access Blocklist";

        // Check if the blocklist file exists
        if (!fs::exists(filename)) {
            std::cerr << "Blocklist file not found." << std::endl;
            return 1;
        }

        // Get IPs from the blocklist file and Firewalld
        std::set<std::string> fileIPs = getIPsFromFile(filename);
        std::set<std::string> firewalldIPs = getIPsFromFirewalld();
        bool updated = false;

        // Add IPs from the file to Firewalld if they are missing
        for (const auto& ip : fileIPs) {
            if (firewalldIPs.find(ip) == firewalldIPs.end()) {
                addIPToFirewalld(ip);
                updated = true;
            }
        }

        // Add IPs from Firewalld to the file if they are missing
        for (const auto& ip : firewalldIPs) {
            if (fileIPs.find(ip) == fileIPs.end()) {
                addIPToFile(filename, ip);
                updated = true;
            }
        }

        // Print status message if updates were made
        if (updated) {
            std::cout << "Changes detected. Syncing updates..." << std::endl;
        } else {
            std::cout << "No changes detected." << std::endl;
        }

        std::cout << "Synchronization complete." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

