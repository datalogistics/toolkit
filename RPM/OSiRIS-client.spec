%define default_release 1

Summary: OSiRIS Client Meta Package
Name: OSiRIS-client
Version: 1.0
Release: %{?release}%{!?release:%{default_release}}
License: N/A
Group: Utilities/Configuration

BuildRoot: %{_builddir}/%{name}-root
Packager: Jayashree Candadai <jayaajay@indiana.edu>
BuildArch: noarch

Requires : blipp iperf

%description 

Configures the client tools of OSiRIS. This meta package 
installs the BLiPP monitoring agent and iperf.

%prep

# %setup

%build

%clean

%install

%files

%post

%changelog

