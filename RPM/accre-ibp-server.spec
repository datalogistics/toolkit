Name:	accre-ibp-server	
Version: 1.0.0pre
Release:	1%{?dist}
Summary: Internet Backplane Protocol (IBP) Server	

Group:	Applications/System
License: ACCRE
URL:	http://www.reddnet.org/
Source0:	http://www.lstore.org/pwiki/uploads/Download/ibp_server-accre.tgz
BuildRoot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

BuildRequires:	cmake protobuf-c-devel apr-devel apr-util-devel openssl-devel czmq-devel hwloc-devel uuid-devel fuse-devel libattr-devel gcc gcc-c++ openssl-devel 
Requires: czmq fuse openssl

%description
The Internet Backplane Protocol (IBP) Server handles exposes storage 
to the network via IBP. It is an integral part of various distributed
storage technologies (ex http://www.reddnet.org)

%prep
%setup -n ibp_server


%build
cmake -DCMAKE_INSTALL_PREFIX:PATH=%{buildroot} .
make all

%install
rm -rf %{buildroot}
make install 


%clean
rm -rf %{buildroot}


%files
%defattr(-,root,root,-)
%doc
/bin/*


%changelog
