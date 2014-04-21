Name: libunis-c
Version: 1.0.0pre
Release: 1%{?dist}
Summary: Unis C library

Group: Applications/System
License: ACCRE
URL: http://www.reddnet.org/
#Source0: https://github.com/periscope-ps/libunis-c/archive/master.tar.gz
Source0: %{name}.tar.gz
BuildRoot: %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

BuildRequires:	cmake curl-devel jansson-devel sed
Requires: jansson curl

%description
Unis c bindings

%prep
%setup -n libunis-c

%build
./configure --prefix=%{buildroot}
make

%install
make install
sed -i "s@libdir='.*'@libdir='/lib'@g" $RPM_BUILD_ROOT/lib/*.la

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
/lib/*
/include/unis_registration.h

%changelog
