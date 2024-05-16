#include <iostream>
#include <fstream>
#include <set>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <sstream>  // Add this header

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
    std::string jail = "sshd";
    std::string result = exec(("sudo fail2ban-client status " + jail + " | grep 'Banned IP list:'").c_str());

    size_t pos = result.find("Banned IP list:");
    if (pos == std::string::npos) {
        std::cerr << "No banned IPs found.\n";
        return 1;
    }
    std::string ips = result.substr(pos + strlen("Banned IP list:"));
    std::istringstream iss(ips);  // Correctly initialize the istringstream
    std::string ip;

    while (iss >> ip) {
        ip = trim(ip);
        if (!ip.empty() && exec(("sudo firewall-cmd --permanent --list-rich-rules | grep -q '" + ip + "'").c_str()).empty()) {
            std::cout << "Blocking IP: " << ip << std::endl;
            exec(("sudo firewall-cmd --permanent --add-rich-rule='rule family=\"ipv4\" source address=\"" + ip + "\" reject'").c_str());
        }
    }

    exec("sudo firewall-cmd --reload");
    std::cout << "All banned IPs from " << jail << " have been blocked." << std::endl;
    return 0;
}
