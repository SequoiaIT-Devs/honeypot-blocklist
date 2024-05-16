#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <sstream>
#include <set>
#include <cstring>
#include <ctime>
#include <thread>
#include <chrono>

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

// Function to check if there are changes to commit
bool hasChangesToCommit(const std::string& repoPath) {
    std::string command = "cd " + repoPath + " && git status --porcelain";
    std::string output = exec(command.c_str());
    return !output.empty();
}

// Function to pull the latest blocklist from the Git repository
void pullGitRepo(const std::string& repoPath) {
    std::string command = "cd " + repoPath + " && git reset --hard && git pull --rebase origin main";
    if (execWithResult(command) != 0) {
        throw std::runtime_error("Error pulling from Git repository");
    }
}

// Function to push updates to the Git repository with retries
void pushGitRepo(const std::string& repoPath, const std::string& commitMessage) {
    if (!hasChangesToCommit(repoPath)) {
        std::cout << "No changes to commit." << std::endl;
        return;
    }

    std::string command = "cd " + repoPath + " && git add . && git commit -m \"" + commitMessage + "\"";
    if (execWithResult(command) != 0) {
        throw std::runtime_error("Error committing to Git repository");
    }

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

int main() {
    const std::string repoPath = "/root/honeypot-blocklist";
    const std::string blocklistFile = repoPath + "/Unauthorized Access Blocklist";

    if (!fs::exists(repoPath)) {
        std::cerr << "Repository path not found." << std::endl;
        return 1;
    }

    pullGitRepo(repoPath);

    std::string tempFile = "/tmp/honeypot_blocklist.tmp";
    exec(("curl -s https://raw.githubusercontent.com/sequoiaheightsms/honeypot-blocklist/main/Unauthorized%20Access%20Blocklist -o " + tempFile).c_str());

    std::ifstream temp(tempFile);
    std::ifstream blocklist(blocklistFile);
    if (!temp.is_open() || !blocklist.is_open()) {
        throw std::runtime_error("Error opening temporary or blocklist file");
    }

    std::set<std::string> tempIPs;
    std::set<std::string> blocklistIPs;

    std::string ip;
    while (std::getline(temp, ip)) {
        if (!ip.empty()) {
            tempIPs.insert(ip);
        }
    }
    while (std::getline(blocklist, ip)) {
        if (!ip.empty()) {
            blocklistIPs.insert(ip);
        }
    }

    temp.close();
    blocklist.close();

    std::ofstream blocklistOut(blocklistFile, std::ios::app); // Open in append mode to avoid deletion
    for (const auto& ip : tempIPs) {
        if (blocklistIPs.find(ip) == blocklistIPs.end()) {
            blocklistOut << ip << std::endl;
        }
    }
    blocklistOut.close();

    // Create a unique commit message using the current time
    std::time_t now = std::time(nullptr);
    std::string commitMessage = "Sync updates between GitHub and local blocklist - " + std::to_string(now);

    pushGitRepo(repoPath, commitMessage);

    exec(("rm " + tempFile).c_str());

    std::cout << "Blocklist update complete." << std::endl;
    return 0;
}




