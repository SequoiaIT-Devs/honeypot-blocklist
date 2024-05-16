Name:           honeypot-blocklist
Version:        1.0
Release:        1%{?dist}
Summary:        Honeypot Blocklist Service

License:        MIT License
URL:            http://sequoiaheightsms.com
Source0:        fetch_blocked_ips.cpp
Source1:        sync_blocklist.cpp
Source2:        update_blocklist.cpp
Source3:        honeypot-blocklist.service
Source4:        honeypot-blocklist.timer

BuildRequires:  gcc
Requires:       systemd

%description
Honeypot Blocklist Service to block and sync IPs using firewalld and fail2ban.

%prep
%setup -c -T
cp %{SOURCE0} .
cp %{SOURCE1} .
cp %{SOURCE2} .

%build
g++ -g -o fetch_blocked_ips fetch_blocked_ips.cpp
g++ -g -o sync_blocklist sync_blocklist.cpp
g++ -g -o update_blocklist update_blocklist.cpp

%install
install -Dm755 fetch_blocked_ips %{buildroot}/usr/local/bin/fetch_blocked_ips
install -Dm755 sync_blocklist %{buildroot}/usr/local/bin/sync_blocklist
install -Dm755 update_blocklist %{buildroot}/usr/local/bin/update_blocklist

install -Dm644 %{SOURCE3} %{buildroot}/etc/systemd/system/honeypot-blocklist.service
install -Dm644 %{SOURCE4} %{buildroot}/etc/systemd/system/honeypot-blocklist.timer

%post
systemctl daemon-reload
systemctl enable honeypot-blocklist.timer
systemctl start honeypot-blocklist.timer

%preun
if [ $1 -eq 0 ]; then
    systemctl stop honeypot-blocklist.timer
    systemctl disable honeypot-blocklist.timer
fi

%postun
systemctl daemon-reload

%files
/usr/local/bin/fetch_blocked_ips
/usr/local/bin/sync_blocklist
/usr/local/bin/update_blocklist
/etc/systemd/system/honeypot-blocklist.service
/etc/systemd/system/honeypot-blocklist.timer

%changelog
* Wed May 15 2024 Sequoia Heights MS <info@sequoiaheightsms.com> - 1.0-1
- Initial package
