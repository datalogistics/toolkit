%define default_release 1

Summary: Data Logistics Toolkit Meta Package
Name: dlt
Version: 1.0
Release: %{?release}%{!?release:%{default_release}}
License: N/A
Group: Utilities/Configuration

BuildRoot: %{_builddir}/%{name}-root
Packager: Ezra Kissel <ezkissel@indiana.edu>
BuildArch: noarch

Requires: libxsp-client = 1.0-10%{dist} phoebus-client = 1.0-10%{dist} periscope = 0.2.dev-1 blipp = 0.1.dev-12 accre-ibp-server = 1.0-11%{dist} ntp libwebsockets dlt-lors = 1.0-1%{dist} dlt-tools = 0.1.dev-2 dlt-installer = 1.0-1%{dist}

%description 

Configures the machine as a DLT node.  This meta package installs the
ACCRE IBP server, XSP clients, a Periscope measurement store, the
BLiPP monitoring agent, DLT LoRS and tools and the Global Package Installer.

%prep

# %setup

%build

%clean

%install

%files

%post

%changelog
* Tue Mar 15 2016 <jayaajay@indiana.edu> dlt_1.0.spec
 - Added the versions for 1.0 pin release in requires field.
