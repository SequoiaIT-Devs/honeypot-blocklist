#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <sstream>
#include <set>
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

// Function to pull the latest blocklist from the Git repository
void pullGitRepo(const std::string& repoPath) {
    std::string command = "cd " + repoPath + " && git reset --hard && git pull --rebase origin main";
    if (std::system(command.c_str()) != 0) {
        throw std::runtime_error("Error pulling from Git repository");
    }
}

// Function to push updates to the Git repository
void pushGitRepo(const std::string& repoPath, const std::string& commitMessage) {
    std::string command = "cd " + repoPath + " && git add . && git commit -m \"" + commitMessage + "\" && git push origin main";
    if (std::system(command.c_str()) != 0) {
        throw std::runtime_error("Error pushing to Git repository");
    }
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

    pushGitRepo(repoPath, "Sync updates between GitHub and local blocklist");

    exec(("rm " + tempFile).c_str());

    std::cout << "Blocklist update complete." << std::endl;
    return 0;
}

