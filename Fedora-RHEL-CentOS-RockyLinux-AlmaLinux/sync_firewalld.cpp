#include <iostream>
#include <fstream>
#include <set>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <cstring>  // Include the <cstring> header for strlen

namespace fs = std::filesystem;

// Function to read IPs from a local file
std::set<std::string> getIPsFromFile(const std::string& filename) {
    std::set<std::string> ips;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return ips;
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
    FILE* pipe = popen("sudo firewall-cmd --list-all", "r");
    if (!pipe) {
        std::cerr << "Error executing firewall-cmd" << std::endl;
        return ips;
    }

    char buffer[128];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);

    // Extract IPs from the output
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
    std::string command = "sudo firewall-cmd --permanent --add-rich-rule='rule family=\"ipv4\" source address=\"" + ip + "\" reject'";
    std::system(command.c_str());
    std::system("sudo firewall-cmd --reload");
}

// Function to add an IP to the local file
void addIPToFile(const std::string& filename, const std::string& ip) {
    std::ofstream file;
    file.open(filename, std::ios::app);
    if (file.is_open()) {
        file << ip << std::endl;
        file.close();
    } else {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
    }
}

// Function to pull the latest blocklist from the Git repository
void pullGitRepo(const std::string& repoPath) {
    std::string command = "cd " + repoPath + " && git checkout community && git pull origin community";
    std::system(command.c_str());
}

// Function to push updates to the Git repository
void pushGitRepo(const std::string& repoPath, const std::string& commitMessage) {
    std::string command = "cd " + repoPath + " && git add . && git commit -m \"" + commitMessage + "\" && git push origin community";
    std::system(command.c_str());
}

int main() {
    const std::string repoPath = "/root/honeypot-blocklist"; // Path to your cloned Git repository
    const std::string filename = repoPath + "/Unauthorized Access Blocklist";

    // Check if the blocklist file exists
    if (!fs::exists(filename)) {
        std::cout << "Blocklist file not found. Pulling from Git repository..." << std::endl;
        pullGitRepo(repoPath);
    }

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

    // If there were updates, push changes back to the Git repository
    if (updated) {
        std::cout << "Changes detected. Pushing updates to Git repository..." << std::endl;
        pushGitRepo(repoPath, "Sync updates between Firewalld and blocklist");
    } else {
        std::cout << "No changes detected." << std::endl;
    }

    std::cout << "Synchronization complete." << std::endl;
    return 0;
}
