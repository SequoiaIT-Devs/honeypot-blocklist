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

// Function to generate a new SSH key pair
void generateSSHKeyPair(const std::string& sshKeyPath, const std::string& sshPublicKeyPath) {
    std::string command = "ssh-keygen -t rsa -b 4096 -f " + sshKeyPath + " -N \"\"";
    if (std::system(command.c_str()) != 0) {
        throw std::runtime_error("Error generating SSH key pair");
    }
    std::cout << "A new SSH key pair has been generated.\n";
    std::cout << "Please email the public key to robert.romero@sequoiaheightsms.com to be added as a contributor:\n";
    std::ifstream pubkeyFile(sshPublicKeyPath);
    std::string pubkey((std::istreambuf_iterator<char>(pubkeyFile)), std::istreambuf_iterator<char>());
    std::cout << pubkey << std::endl;
}

// Function to set up GitHub repository and SSH configuration using temporary keys
void setupGitHub() {
    const std::string repoPath = "/root/honeypot-blocklist";
    const std::string tempDir = "/tmp/honeypot-ssh";
    const std::string sshKeyPath = tempDir + "/id_rsa";
    const std::string sshPublicKeyPath = sshKeyPath + ".pub";
    const std::string sshConfigPath = tempDir + "/config";

    // Create temporary directory
    fs::create_directories(tempDir);

    // Generate SSH key pair
    if (!fs::exists(sshKeyPath) || !fs::exists(sshPublicKeyPath)) {
        generateSSHKeyPair(sshKeyPath, sshPublicKeyPath);
        std::cout << "Setup is complete. Please add the public key to GitHub and rerun the program." << std::endl;
        exit(0);
    }

    // Set up SSH config
    std::ofstream sshConfigFile(sshConfigPath);
    if (!sshConfigFile.is_open()) {
        throw std::runtime_error("Error opening SSH config file: " + sshConfigPath);
    }
    sshConfigFile << "Host github.com\n";
    sshConfigFile << "  IdentityFile " << sshKeyPath << "\n";
    sshConfigFile << "  StrictHostKeyChecking no\n";
    sshConfigFile.close();

    // Set GIT_SSH_COMMAND to use the custom SSH config
    std::string gitSshCommand = "GIT_SSH_COMMAND='ssh -F " + sshConfigPath + "'";

    // Clone the repository if it doesn't exist
    if (!fs::exists(repoPath)) {
        exec((gitSshCommand + " git clone https://github.com/sequoiaheightsms/honeypot-blocklist.git " + repoPath).c_str());
    }

    // Set up the remote URL to use SSH
    exec((gitSshCommand + " cd " + repoPath + " && git remote set-url origin git@github.com:sequoiaheightsms/honeypot-blocklist.git").c_str());
    exec((gitSshCommand + " cd " + repoPath + " && git remote -v").c_str());

    // Verify the SSH connection
    exec(("ssh -F " + sshConfigPath + " -T git@github.com").c_str());

    // Configure Git user settings
    std::string gitConfigCommand = "cd " + repoPath + " && git config --local user.name \"Sequoia Heights MS\" && git config --local user.email \"robert.romero@sequoiaheightsms.com\"";
    exec(gitConfigCommand.c_str());

    // Clean up temporary files
    fs::remove_all(tempDir);
}

// Function to set up Fail2ban configuration
void setupFail2ban() {
    const std::string fail2banConfigPath = "/etc/fail2ban/jail.local";
    std::ofstream fail2banConfigFile(fail2banConfigPath);
    if (!fail2banConfigFile.is_open()) {
        throw std::runtime_error("Error opening Fail2ban config file: " + fail2banConfigPath);
    }
    fail2banConfigFile << "[sshd]\n";
    fail2banConfigFile << "enabled  = true\n";
    fail2banConfigFile << "port     = ssh\n";
    fail2banConfigFile << "logpath  = %(sshd_log)s\n";
    fail2banConfigFile << "backend  = %(sshd_backend)s\n";
    fail2banConfigFile << "maxretry = 2\n";
    fail2banConfigFile << "bantime  = 2h\n";
    fail2banConfigFile.close();

    // Restart Fail2ban service to apply changes
    exec("systemctl restart fail2ban");
    std::cout << "Fail2ban has been configured and restarted." << std::endl;
}

// Function to trim leading and trailing whitespace from a string
std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    size_t end = str.find_last_not_of(" \t\n\r");
    return (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
}

// Function to add an IP to the local blocklist file and commit to GitHub
void addIPToBlocklist(const std::string& blocklistFile, const std::string& ip) {
    std::ofstream file(blocklistFile, std::ios::app);
    if (!file.is_open()) {
        throw std::runtime_error("Error opening file for writing: " + blocklistFile);
    }
    file << ip << std::endl;
    file.close();

    const std::string tempDir = "/tmp/honeypot-ssh";
    const std::string sshConfigPath = tempDir + "/config";
    std::string gitSshCommand = "GIT_SSH_COMMAND='ssh -F " + sshConfigPath + "'";

    std::string command = gitSshCommand + " cd /root/honeypot-blocklist && git add \"" + blocklistFile + "\" && git commit -m \"Add " + ip + " to blocklist\" && git push origin main";
    if (std::system(command.c_str()) != 0) {
        throw std::runtime_error("Error committing IP to Git repository: " + ip);
    }
}

int main() {
    try {
        setupGitHub();
        setupFail2ban();

        std::string jail = "sshd";
        const std::string repoPath = "/root/honeypot-blocklist";
        const std::string blocklistFile = repoPath + "/Unauthorized Access Blocklist";

        // Fetch the list of banned IPs from Fail2ban using sed
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

        // Add IPs to the blocklist and commit to GitHub
        for (const auto& ip : ip_set) {
            addIPToBlocklist(blocklistFile, ip);
        }

        std::cout << "All banned IPs from " << jail << " have been added to the blocklist and committed to GitHub." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
