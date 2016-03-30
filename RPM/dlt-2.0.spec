%define default_release 1

Summary: Data Logistics Toolkit Meta Package
Name: dlt
Version: 2.0
Release: %{?release}%{!?release:%{default_release}}
License: N/A
Group: Utilities/Configuration

BuildRoot: %{_builddir}/%{name}-root
Packager: Jayashree Candadai <jayaajay@indiana.edu>
BuildArch: noarch

Requires : libxsp-client >= 2.0-1 phoebus-client >= 2.0-1 periscope >= 2.0.dev-1 blipp >= 2.0.dev-1 accre-ibp-server >= 2.0-1 ntp libwebsockets dlt-lors >= 2.0-1 dlt-tools >= 2.0.dev-1

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
