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

Requires : libxsp-client = 1.0-10 phoebus-client = 1.0-10 periscope = 0.2.dev-1 blipp = 0.1.dev-13 accre-ibp-server = 1.0-11 ntp libwebsockets dlt-lors = 1.0-1 dlt-tools = 0.1.dev-2

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
* Mon Mar 14 2016 <jayaajay@indiana.edu> dlt_1.0.spec
- Moved the dlt.spec to dlt_1.0.spec and updated the version numbers in 
  requires field.
