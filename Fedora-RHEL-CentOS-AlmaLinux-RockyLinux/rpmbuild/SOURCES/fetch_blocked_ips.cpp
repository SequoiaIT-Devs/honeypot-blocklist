#include <iostream>
#include <fstream>
#include <set>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <sstream>

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

// Function to trim leading and trailing whitespace
std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    size_t end = str.find_last_not_of(" \t\n\r");
    return (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
}

int main() {
    try {
        std::string jail = "sshd";
        
        // Fetch the list of banned IPs from Fail2ban using sed
        std::string result = exec(("sudo fail2ban-client status " + jail + " | grep 'Banned IP list:' | sed 's/.*Banned IP list:[ ]*//'").c_str());

        // Debugging: Print the raw output from fail2ban-client status
        std::cout << "Raw banned IPs output:" << std::endl;
        std::cout << result << std::endl;

        std::istringstream iss(result);
        std::string ip;

        std::set<std::string> ip_set;
        while (iss >> ip) {
            ip = trim(ip);
            if (!ip.empty()) {
                ip_set.insert(ip);
            }
        }

        // Loop through each banned IP and block it using firewalld
        for (const auto& ip : ip_set) {
            std::string checkCommand = "sudo firewall-cmd --permanent --list-rich-rules | grep -q '" + ip + "'";
            if (exec(checkCommand.c_str()).empty()) {
                std::cout << "Blocking IP: " << ip << std::endl;
                std::string blockCommand = "sudo firewall-cmd --permanent --add-rich-rule='rule family=\"ipv4\" source address=\"" + ip + "\" reject'";
                exec(blockCommand.c_str());
            } else {
                std::cout << "IP already blocked: " << ip << std::endl;
            }
        }

        // Reload firewalld to apply changes
        exec("sudo firewall-cmd --reload");
        std::cout << "All banned IPs from " << jail << " have been blocked." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

