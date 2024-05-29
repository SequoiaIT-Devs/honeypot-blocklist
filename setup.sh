#!/bin/bash
# 5/29: Initial Test Deployment.
# Known Issues: The assumption repo has already been forked.

# Function to display help message
show_help() {
    echo "Usage: $0 [--probe | --client]"
    echo ""
    echo "Options:"
    echo "  --help      Show this help message and exit"
    echo "  --probe     Setup probe environment"
    echo "  --client    Setup client environment"
}

# Check if the script is run with sudo
if [ "$(id -u)" -ne 0 ]; then
    echo "This script must be run as root."
    exit 1
fi

# Function to determine the OS version
determine_os_version() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        if [[ "$ID" == "centos" || "$ID" == "rhel" || "$ID" == "fedora" || "$ID" == "almalinux" || "$ID" == "rocky" ]]; then
            if [[ "$VERSION_ID" =~ ^8 ]]; then
                echo "el8"
            elif [[ "$VERSION_ID" =~ ^9 ]]; then
                echo "el9"
            else
                echo "Unsupported OS version: $VERSION_ID"
                exit 1
            fi
        else
            echo "Unsupported OS: $ID"
            exit 1
        fi
    else
        echo "Cannot determine OS version."
        exit 1
    fi
}

# Function to setup probe environment
setup_probe() {
    local os_version=$(determine_os_version)

    # Prompt for Git user details
    read -p "Enter your Git name: " git_name
    read -p "Enter your Git email: " git_email
    read -p "Enter your GitHub username: " github_username

    # Configure Git
    git config --global user.name "$git_name"
    git config --global user.email "$git_email"

    # Check and install fail2ban if not installed
    if ! command -v fail2ban-server &> /dev/null; then
        echo "Fail2Ban is not installed. Installing..."
        if command -v yum &> /dev/null; then
            yum install -y fail2ban
        elif command -v dnf &> /dev/null; then
            dnf install -y fail2ban
        else
            echo "Neither yum nor dnf is available for package installation."
            exit 1
        fi
    fi

    # Configure Fail2Ban
    echo "Configuring Fail2Ban..."
    cat <<EOL > /etc/fail2ban/jail.local
[sshd]
enabled  = true
port     = ssh
logpath  = %(sshd_log)s
backend  = %(sshd_backend)s
maxretry = 2
bantime  = 2h
EOL

    systemctl restart fail2ban

    # Create SSH key if not exists
    if [ ! -f ~/.ssh/id_rsa_probe ]; then
        echo "Creating SSH key for probe..."
        ssh-keygen -t rsa -b 4096 -f ~/.ssh/id_rsa_probe -N ""
        echo "Public key to add to GitHub:"
        cat ~/.ssh/id_rsa_probe.pub
    fi

    # Create SSH config
    echo "Creating SSH config..."
    cat <<EOL >> ~/.ssh/config

Host github.com-honeypot-probe
  HostName github.com
  IdentityFile ~/.ssh/id_rsa_probe
  StrictHostKeyChecking no
EOL

    # Set git remote URL to user's fork
    echo "Configuring git remote URL..."
    git remote set-url origin git@github.com-honeypot-probe:$github_username/honeypot-blocklist.git

    # Install honeypot-blocklist-probe RPM
    echo "Installing honeypot-blocklist-probe RPM..."
    if command -v yum &> /dev/null; then
        yum install -y RPMS/x86_64/honeypot-blocklist-probe-*.$os_version.rpm
    elif command -v dnf &> /dev/null; then
        dnf install -y RPMS/x86_64/honeypot-blocklist-probe-*.$os_version.rpm
    else
        echo "Neither yum nor dnf is available for package installation."
        exit 1
    fi

    # Restart honeypot-probe service
    systemctl restart honeypot-probe
}

# Function to set up client environment
setup_client() {
    local os_version=$(determine_os_version)

    # Ensure firewalld is installed and active
    echo "Ensuring firewall is installed and active..."
    if ! command -v firewall-cmd &> /dev/null; then
        if command -v yum &> /dev/null; then
            yum install -y firewalld
        elif command -v dnf &> /dev/null; then
            dnf install -y firewalld
        else
            echo "Neither yum nor dnf is available for package installation."
            exit 1
        fi
    fi
    systemctl enable --now firewalld

    # Install honeypot-blocklist-client RPM
    echo "Installing honeypot-blocklist-client RPM..."
    if command -v yum &> /dev/null; then
        yum install -y RPMS/x86_64/honeypot-blocklist-client-*.$os_version.rpm
    elif command -v dnf &> /dev/null; then
        dnf install -y RPMS/x86_64/honeypot-blocklist-client-*.$os_version.rpm
    else
        echo "Neither yum nor dnf is available for package installation."
        exit 1
    fi
}

# Main script logic
case "$1" in
    --help)
        show_help
        ;;
    --probe)
        setup_probe
        ;;
    --client)
        setup_client
        ;;
    *)
        echo "Invalid option: $1"
        show_help
        exit 1
        ;;
esac
