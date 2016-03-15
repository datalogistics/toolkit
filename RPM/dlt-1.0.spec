%define default_release 1

Summary: Data Logistics Toolkit Meta Package
Name: dlt
Version: 1.0
Release: %{?release}%{!?release:%{default_release}}
License: N/A
Group: Utilities/Configuration

BuildRoot: %{_builddir}/%{name}-root
Packager: Jayashree Candadai <jayaajay@indiana.edu>
BuildArch: noarch

Requires: libxsp-client = 1.0-10%{dist} phoebus-client = 1.0-10%{dist} periscope = 0.2.dev-1 blipp = 0.1.dev-13 accre-ibp-server = 1.0-11%{dist} ntp libwebsockets dlt-lors = 1.0-1%{dist} dlt-tools = 0.1.dev-2

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
