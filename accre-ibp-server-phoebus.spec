Name: accre-ibp-server
Version: 1.0.0pre
Release: 1%{?dist}
Summary: Internet Backplane Protocol (IBP) Server

Group: Applications/System
License: ACCRE
URL: http://www.reddnet.org/
#Source0: http://www.lstore.org/pwiki/uploads/Download/ibp_server-accre.tgz
Source0: %{name}-%{version}.tar.gz
BuildRoot: %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

BuildRequires:	cmake protobuf-c-devel apr-devel apr-util-devel openssl-devel czmq-devel hwloc-devel uuid-devel fuse-devel libattr-devel gcc gcc-c++ openssl-devel
Requires: czmq fuse openssl libxsp-client

%description
The Internet Backplane Protocol (IBP) Server handles exposes storage 
to the network via IBP. It is an integral part of various distributed
storage technologies (ex http://www.reddnet.org)

%prep
%setup -n ibp_server

%build
./bootstrap
cmake -DCMAKE_INSTALL_PREFIX:PATH=%{buildroot} .
make all

%install
install -d ${RPM_BUILD_ROOT}/bin
install -d ${RPM_BUILD_ROOT}/etc
install -m 755 ibp_server ibp_attach_rid ibp_detach_rid ibp_rescan ${RPM_BUILD_ROOT}/bin
install -m 755 get_alloc get_config get_corrupt get_version ${RPM_BUILD_ROOT}/bin
install -m 755 print_alog read_alloc repair_history ${RPM_BUILD_ROOT}/bin
install -m 755 date_spacefree chksum_test expire_list mkfs.resource ${RPM_BUILD_ROOT}/bin
install -m 644 ibp.cfg ${RPM_BUILD_ROOT}/etc

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
/bin/*
/etc/ibp.cfg

%changelog
