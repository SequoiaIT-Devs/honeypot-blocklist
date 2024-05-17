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

// Function to check if an IP already exists in the blocklist file
bool ipExistsInBlocklist(const std::string& blocklistFile, const std::string& ip) {
    std::ifstream file(blocklistFile);
    if (!file.is_open()) {
        throw std::runtime_error("Error opening file for reading: " + blocklistFile);
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line == ip) {
            return true; // IP already exists in the blocklist
        }
    }
    return false;
}

// Function to add an IP to the local blocklist file and commit to GitHub
void addIPToBlocklist(const std::string& blocklistFile, const std::string& ip) {
    if (ipExistsInBlocklist(blocklistFile, ip)) {
        std::cout << "IP " << ip << " already exists in blocklist. Skipping." << std::endl;
        return;
    }

    std::ofstream file(blocklistFile, std::ios::app);
    if (!file.is_open()) {
        throw std::runtime_error("Error opening file for writing: " + blocklistFile);
    }
    file << ip << std::endl;
    file.close();

    std::string command = "git add \"" + blocklistFile + "\" && git commit -m \"Add " + ip + " to blocklist\"";
    if (execWithResult(command) != 0) {
        throw std::runtime_error("Error committing IP to Git repository: " + ip);
    }
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

int main() {
    try {
        const std::string repoPath = "/root/honeypot-blocklist";
        const std::string blocklistFile = repoPath + "/Unauthorized Access Blocklist";

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
            addIPToBlocklist(blocklistFile, ip);
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

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        logMessage("Error: " + std::string(e.what()));
        return 1;
    }

    return 0;
}
