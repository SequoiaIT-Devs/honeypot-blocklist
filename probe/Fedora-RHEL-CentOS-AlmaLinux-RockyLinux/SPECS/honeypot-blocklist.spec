Name:           honeypot-blocklist
Version:        1.3
Release:        1%{?dist}
Summary:        Honeypot Blocklist Service

License:        MIT License
URL:            http://sequoiaheightsms.com
Source0:        fetch_blocked_ips.cpp
Source1:        sync_blocklist.cpp
Source2:        fetch_blocked_ips.service
Source3:        fetch_blocked_ips.timer
Source4:        sync_blocklist.service
Source5:        sync_blocklist.timer

BuildRequires:  gcc
Requires:       systemd

%description
Honeypot Blocklist Service to block and sync IPs using firewalld and fail2ban.

%package probe
Summary:        Honeypot Blocklist Probe
Requires:       systemd fail2ban
%description probe
Honeypot Blocklist Probe to collect IPs from fail2ban and upload them to GitHub.

%package client
Summary:        Honeypot Blocklist Client
Requires:       systemd
%description client
Honeypot Blocklist Client to sync IPs from GitHub and apply them to firewalld.

%prep
%setup -c -T
cp %{SOURCE0} .
cp %{SOURCE1} .

%build
g++ -g -o fetch_blocked_ips fetch_blocked_ips.cpp
g++ -g -o sync_blocklist sync_blocklist.cpp

%install
# Install files for the probe package
install -Dm755 fetch_blocked_ips %{buildroot}/usr/local/bin/fetch_blocked_ips
install -Dm644 %{SOURCE2} %{buildroot}/etc/systemd/system/fetch_blocked_ips.service
install -Dm644 %{SOURCE3} %{buildroot}/etc/systemd/system/fetch_blocked_ips.timer

# Install files for the client package
install -Dm755 sync_blocklist %{buildroot}/usr/local/bin/sync_blocklist
install -Dm644 %{SOURCE4} %{buildroot}/etc/systemd/system/sync_blocklist.service
install -Dm644 %{SOURCE5} %{buildroot}/etc/systemd/system/sync_blocklist.timer

%post probe
systemctl daemon-reload
systemctl enable fetch_blocked_ips.timer
systemctl start fetch_blocked_ips.timer

%preun probe
if [ $1 -eq 0 ]; then
    systemctl stop fetch_blocked_ips.timer
    systemctl disable fetch_blocked_ips.timer
fi

%postun probe
systemctl daemon-reload

%post client
systemctl daemon-reload
systemctl enable sync_blocklist.timer
systemctl start sync_blocklist.timer

%preun client
if [ $1 -eq 0 ]; then
    systemctl stop sync_blocklist.timer
    systemctl disable sync_blocklist.timer
fi

%postun client
systemctl daemon-reload

%files probe
/usr/local/bin/fetch_blocked_ips
/etc/systemd/system/fetch_blocked_ips.service
/etc/systemd/system/fetch_blocked_ips.timer

%files client
/usr/local/bin/sync_blocklist
/etc/systemd/system/sync_blocklist.service
/etc/systemd/system/sync_blocklist.timer

%changelog
* Thu May 16 2024 Sequoia Heights MS <info@sequoiaheightsms.com> - 1.3-1
- Split into probe and client packages
- Probe: collects IPs from fail2ban and uploads to GitHub
- Client: syncs IPs from GitHub and applies to firewalld
* Wed May 15 2024 Sequoia Heights MS <info@sequoiaheightsms.com> - 1.0-1
- Initial package