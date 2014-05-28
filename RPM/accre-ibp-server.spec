%define default_release 1

Name: accre-ibp-server
Version: 1.0
Release: %{?release}%{!?release:%{default_release}}
Summary: Internet Backplane Protocol (IBP) Server

Group: Applications/System
License: ACCRE
URL: http://www.reddnet.org/
#Source0: http://www.lstore.org/pwiki/uploads/Download/ibp_server-accre.tgz
Source0: ibp_server.tar.gz
BuildRoot: %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

BuildRequires:	cmake apr-devel apr-util-devel openssl-devel czmq-devel hwloc-devel uuid-devel fuse-devel libattr-devel gcc gcc-c++ openssl-devel jansson-devel
Requires: czmq fuse openssl jansson libunis-c python-argparse

%description
The Internet Backplane Protocol (IBP) Server handles exposes storage 
to the network via IBP. It is an integral part of various distributed
storage technologies (ex http://www.reddnet.org). In case of bug or issue 
please report it to data-logistics@googlegroups.org.

%prep
%setup -n ibp_server

%build
./bootstrap
cmake -DCMAKE_INSTALL_PREFIX:PATH=%{buildroot} .
make

%install
chmod 755 misc/ibp-server
make install
install -d ${RPM_BUILD_ROOT}/bin
install -m 755 ibp_server ibp_attach_rid ibp_detach_rid ibp_rescan ${RPM_BUILD_ROOT}/bin
install -m 755 get_alloc get_config get_corrupt get_version ${RPM_BUILD_ROOT}/bin
install -m 755 print_alog read_alloc repair_history ${RPM_BUILD_ROOT}/bin
install -m 755 date_spacefree chksum_test expire_list mkfs.resource ${RPM_BUILD_ROOT}/bin
install -m 755 misc/ibp_configure.py ${RPM_BUILD_ROOT}/bin

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
/bin/*
/etc/ibp.cfg
%attr(755,root,root) /etc/init.d/ibp-server

%changelog
* Tue May 27 2014 Akshay Dorwat <adorwat@indiana.edu> 1.1-accre-ibp-server 
- Updated the permissions for /etc/init.d/ibp-server script.
