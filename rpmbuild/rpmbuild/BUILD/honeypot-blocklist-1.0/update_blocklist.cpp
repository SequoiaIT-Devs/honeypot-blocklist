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
#include <ctime>
#include <thread>
#include <chrono>
#include <unistd.h>

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

// Function to execute a shell command and return the result code
int execWithResult(const std::string& cmd) {
    int result = std::system(cmd.c_str());
    if (result != 0) {
        std::cerr << "Command failed with exit code " << result << ": " << cmd << std::endl;
    }
    return result;
}

// Function to pull the latest blocklist from the Git repository
void pullGitRepo(const std::string& repoPath) {
    std::string command = "cd " + repoPath + " && git reset --hard && git pull --rebase origin main";
    if (execWithResult(command) != 0) {
        throw std::runtime_error("Error pulling from Git repository");
    }
}

// Function to push updates to the Git repository with retries
void pushGitRepo(const std::string& repoPath) {
    // Retry mechanism for pushing changes
    int maxRetries = 3;
    for (int attempt = 1; attempt <= maxRetries; ++attempt) {
        std::string pushCommand = "cd " + repoPath + " && git push origin main";
        int result = execWithResult(pushCommand);
        if (result == 0) {
            return; // Push successful
        }
        std::cerr << "Attempt " << attempt << " to push changes failed. Retrying..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2)); // Sleep before retrying
    }
    throw std::runtime_error("Error pushing to Git repository after multiple attempts");
}

// Function to add an IP to the local blocklist file and commit to GitHub
void addIPToBlocklist(const std::string& blocklistFile, const std::string& ip) {
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

int main() {
    try {
        const std::string repoPath = "/root/honeypot-blocklist";
        const std::string blocklistFile = repoPath + "/Unauthorized Access Blocklist";
        const std::string tempFile = "/tmp/honeypot_blocklist.tmp";
        const std::string githubRepoUrl = "https://raw.githubusercontent.com/sequoiaheightsms/honeypot-blocklist/main/Unauthorized%20Access%20Blocklist";

        // Ensure the repository path exists
        if (!fs::exists(repoPath)) {
            std::cerr << "Repository path not found." << std::endl;
            return 1;
        }

        // Change to the local repository directory
        if (chdir(repoPath.c_str()) != 0) {
            throw std::runtime_error("Failed to change directory to " + repoPath);
        }

        // Pull the latest changes from the Git repository
        pullGitRepo(repoPath);

        // Download the current blocklist from the remote repository
        exec(("curl -s " + githubRepoUrl + " -o " + tempFile).c_str());

        // Open the temporary file
        std::ifstream temp(tempFile);
        if (!temp.is_open()) {
            throw std::runtime_error("Error opening temporary file: " + tempFile);
        }

        // Read IPs from the temporary file into a set
        std::set<std::string> tempIPs;
        std::string ip;
        while (std::getline(temp, ip)) {
            if (!ip.empty()) {
                tempIPs.insert(ip);
            }
        }
        temp.close();

        // Extract IPs from firewalld rich rules and check against blocklist
        std::string firewallOutput = exec("sudo firewall-cmd --list-all");
        std::istringstream iss(firewallOutput);
        std::set<std::string> firewalldIPs;
        std::string line;
        while (std::getline(iss, line)) {
            size_t pos = line.find("source address=\"");
            if (pos != std::string::npos) {
                pos += strlen("source address=\"");
                size_t end = line.find("\"", pos);
                if (end != std::string::npos) {
                    firewalldIPs.insert(line.substr(pos, end - pos));
                }
            }
        }

        bool updated = false;

        // Add new IPs from firewalld to the blocklist file and commit to GitHub
        for (const auto& ip : firewalldIPs) {
            if (tempIPs.find(ip) == tempIPs.end()) {
                std::cout << ip << " not found in blocklist. Adding..." << std::endl;
                addIPToBlocklist(blocklistFile, ip);
                updated = true;
            } else {
                std::cout << ip << " already exists in blocklist." << std::endl;
            }
        }

        // Push the changes to the remote repository if there were updates
        if (updated) {
            pushGitRepo(repoPath);
        }

        // Clean up temporary file
        exec(("rm " + tempFile).c_str());

        std::cout << "Blocklist update complete." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}





