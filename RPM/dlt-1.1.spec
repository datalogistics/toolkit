%define default_release 1

Summary: Data Logistics Toolkit Meta Package
Name: dlt
Version: 1.1
Release: %{?release}%{!?release:%{default_release}}
License: N/A
Group: Utilities/Configuration

BuildRoot: %{_builddir}/%{name}-root
Packager: Jayashree Candadai <jayaajay@indiana.edu>
BuildArch: noarch

Requires : libxsp-client >= 1.0-10 phoebus-client >=1.0-10 periscope >= 0.3.dev-1 blipp >= 0.1.dev-14 accre-ibp-server >= 1.0-12 ntp libwebsockets dlt-lors >= 1.0-1 dlt-tools >= 0.1.dev-3

%description 

Configures the machine as a DLT node.  This meta package installs the
ACCRE IBP server, XSP clients, a Periscope measurement store, and the
BLiPP monitoring agent, DLT LoRS and tools.

%prep

# %setup

%build

%clean

%install

%files

%post

%changelog
