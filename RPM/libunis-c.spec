%define default_release 2
Name: libunis-c
Version: 1.0
Release: %{?release}%{!?release:%{default_release}}
Summary: Unis C library

Group:	        Library/System
License:	http://www.apache.org/licenses/LICENSE-2.0
URL:	        https://github.com/periscope-ps/periscope
Source0:	%{name}.tar.gz
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}

BuildRequires:	cmake curl-devel jansson-devel sed
Requires:	jansson curl

%description
UNIS C library. Currently only used to register C services.

%prep
%setup -n libunis-c

%build
%configure
make

%install
%makeinstall

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%{_libdir}/*
%{_includedir}/unis_registration.h
%{_includedir}/unis_exnode.h
%{_includedir}/libunis_c_log.h

%changelog
* Sat Oct 11 2014 <ezkissel@indiana.edu> 1.0-2-libunis-c
- Adding location support.
* Fri Apr 25 2014 Amey Jahagirdar <ajahagir@indiana.com>
- Basic reusable library for c services
- Added pluggable log function callback registration to get realtime feedback
