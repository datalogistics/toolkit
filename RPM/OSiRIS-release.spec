%define default_release 0

Summary: OSiRIS Repository Package
Name: OSiRIS-release
Version: 1
Release: %{?release}%{!?release:%{default_release}}
License: none
Group: System Environment/Base
Source: %{name}.tar.gz
BuildRoot: %{_builddir}/%{name}-build
Packager: Jayashree Candadai <jayaajay@indiana.edu>
BuildArch: noarch

%description 

This package contains the OSiRIS repository GPG key as well as
configuration for yum.

%prep
%autosetup -n %{name}

%build

%install
ls -al 
mkdir -p $RPM_BUILD_ROOT/etc/yum.repos.d
mkdir -p $RPM_BUILD_ROOT/etc/pki/rpm-gpg
install -m 644 OSiRIS.repo $RPM_BUILD_ROOT/etc/yum.repos.d/
install -m 644 RPM-GPG-KEY-OSiRIS $RPM_BUILD_ROOT/etc/pki/rpm-gpg/

%files
%defattr(-,root,root)
/etc/yum.repos.d/OSiRIS.repo
/etc/pki/rpm-gpg/RPM-GPG-KEY-OSiRIS

%post

%clean
rm -rf $RPM_BUILD_ROOT
rm -rf %{_tmppath}/%{name}
rm -rf %{_topdir}/BUILD/%{name}

%changelog

