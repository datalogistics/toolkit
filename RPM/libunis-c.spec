Name:		libunis-c
Version:1.0	
Release:	1%{?dist}
Summary:UNIS C library.	

Group:	Library/System
License:	http://www.apache.org/licenses/LICENSE-2.0
URL:	https://github.com/periscope-ps/periscope
Source0:	%{name}.tar.gz
BuildRoot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

BuildRequires:	cmake curl-devel jansson-devel sed
Requires:	 jansson curl

%description
UNIS C library. Currently only used to register C services.

%prep
%setup -n libunis-c


%build
./configure --prefix=%{buildroot}
make

%install
rm -rf %{buildroot}
make install
sed -i "s@libdir='.*'@libdir='/lib'@g" $RPM_BUILD_ROOT/lib/*.la


%clean
rm -rf %{buildroot}


%files
%defattr(-,root,root,-)
/lib/*
/include/unis_registration.h
/include/libunis_c_log.h



%changelog
* Fri Apr 25 2014 Amey Jahagirdar <ajahagir@indiana.com>
- Basic reusable library for c services
- Added pluggable log function callback registration to get realtime feedback
