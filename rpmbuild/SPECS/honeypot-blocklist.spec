Name:           honeypot-blocklist
Version:        1.4
Release:        1%{?dist}
Summary:        Honeypot Blocklist Service

License:        MIT License
URL:            http://sequoiaheightsms.com
Source0:        honeypot-probe.cpp
Source1:        honeypot-client.cpp
Source2:        honeypot-probe.service
Source3:        honeypot-probe.timer
Source4:        honeypot-client.service
Source5:        honeypot-client.timer
Source6:        honeypot-probe.logrotate
Source7:        honeypot-client.logrotate

BuildRequires:  gcc sqlite-devel
Requires:       systemd

%description
Honeypot Blocklist Service to block and sync IPs using firewalld/ufw and fail2ban.

%package probe
Summary:        Honeypot Blocklist Probe
Requires:       systemd fail2ban logrotate
%description probe
Honeypot Blocklist Probe will collect IPs from fail2ban and upload them to GitHub.

%package client
Summary:        Honeypot Blocklist Client
Requires:       systemd logrotate
%description client
Honeypot Blocklist Client will sync IPs from GitHub and apply them to firewalld or ufw.

%prep
%setup -c -T
cp %{SOURCE0} .
cp %{SOURCE1} .

%build
g++ -g -o honeypot-probe honeypot-probe.cpp -lsqlite3
g++ -g -o honeypot-client honeypot-client.cpp -lsqlite3

%install
# Install files for the probe package
install -Dm755 honeypot-probe %{buildroot}/usr/local/bin/honeypot-probe
install -Dm644 %{SOURCE2} %{buildroot}/etc/systemd/system/honeypot-probe.service
install -Dm644 %{SOURCE3} %{buildroot}/etc/systemd/system/honeypot-probe.timer
install -Dm644 %{SOURCE6} %{buildroot}/etc/logrotate.d/honeypot-probe

# Install files for the client package
install -Dm755 honeypot-client %{buildroot}/usr/local/bin/honeypot-client
install -Dm644 %{SOURCE4} %{buildroot}/etc/systemd/system/honeypot-client.service
install -Dm644 %{SOURCE5} %{buildroot}/etc/systemd/system/honeypot-client.timer
install -Dm644 %{SOURCE7} %{buildroot}/etc/logrotate.d/honeypot-client

%post probe
systemctl daemon-reload
systemctl enable honeypot-probe.timer
systemctl start honeypot-probe.timer

%preun probe
if [ $1 -eq 0 ]; then
    if systemctl is-active --quiet honeypot-probe.timer; then
        systemctl stop honeypot-probe.timer
    fi
    if systemctl is-enabled --quiet honeypot-probe.timer; then
        systemctl disable honeypot-probe.timer
    fi
fi

%postun probe
systemctl daemon-reload

%post client
systemctl daemon-reload
systemctl enable honeypot-client.timer
systemctl start honeypot-client.timer
# Initialize the SQLite database
/usr/bin/sqlite3 /root/honeypot-blocklist/blocklist.db "DROP TABLE IF EXISTS AppliedBlocklist;"
%preun client
if [ $1 -eq 0 ]; then
    if systemctl is-active --quiet honeypot-client.timer; then
        systemctl stop honeypot-client.timer
    fi
    if systemctl is-enabled --quiet honeypot-client.timer; then
        systemctl disable honeypot-client.timer
    fi
fi

%postun client
systemctl daemon-reload

%files probe
/usr/local/bin/honeypot-probe
/etc/systemd/system/honeypot-probe.service
/etc/systemd/system/honeypot-probe.timer
/etc/logrotate.d/honeypot-probe

%files client
/usr/local/bin/honeypot-client
/etc/systemd/system/honeypot-client.service
/etc/systemd/system/honeypot-client.timer
/etc/logrotate.d/honeypot-client

%changelog
* Fri Jun 14 2024 Sequoia Heights MS <info@sequoiaheightsms.com> - 1.4-1
- Switched from single file storage to SQLite database for IP blocklist
- Added database initialization and migration functions
- Added table to track applied IPs to avoid reapplying them
* Thu May 16 2024 Sequoia Heights MS <info@sequoiaheightsms.com> - 1.3-2
- Bug Fix: Explicitly check if the IP already exists in the blocklist file before adding it
* Thu May 16 2024 Sequoia Heights MS <info@sequoiaheightsms.com> - 1.3-1
- Split into probe and client packages
- Probe: collects IPs from fail2ban and uploads to GitHub
- Client: syncs IPs from GitHub and applies to firewalld
- Probe generates SSH key labeled as id_rsa_probe to be added to be able to contribute to list
- Added log rotation for probe and client logs
* Wed May 15 2024 Sequoia Heights MS <info@sequoiaheightsms.com> - 1.0-1
- Initial package
