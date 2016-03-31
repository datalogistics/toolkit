%define default_release 0

Summary: Data Logistics Repository Package
Name: data-logistics-release
Version: 2
Release: %{?release}%{!?release:%{default_release}}
License: none
Group: System Environment/Base
Source: %{name}.tar.gz
BuildArch: noarch
BuildRoot: %{_builddir}/%{name}-build
Packager: Ezra Kissel <ezkissel@indiana.edu>
BuildArch: noarch

%description 

This package contains the Data Logistics repository GPG key as well as
configuration for yum.

%prep
%autosetup -n %{name}

%build

%install
ls -al 
mkdir -p $RPM_BUILD_ROOT/etc/yum.repos.d
mkdir -p $RPM_BUILD_ROOT/etc/pki/rpm-gpg
install -m 644 data-logistics.repo $RPM_BUILD_ROOT/etc/yum.repos.d/
install -m 644 RPM-GPG-KEY-data-logistics $RPM_BUILD_ROOT/etc/pki/rpm-gpg/

%files
%defattr(-,root,root)
/etc/yum.repos.d/data-logistics.repo
/etc/pki/rpm-gpg/RPM-GPG-KEY-data-logistics

%post

%clean
rm -rf $RPM_BUILD_ROOT
rm -rf %{_tmppath}/%{name}
rm -rf %{_topdir}/BUILD/%{name}

%changelog

