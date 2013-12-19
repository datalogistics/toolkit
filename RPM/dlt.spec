Summary: Data Logistics Toolkit Meta Package
Name: dlt
Version: 1.0
Release: 1
License: N/A
Group: Utilities/Configuration

BuildRoot: %{_builddir}/%{name}-root
Packager: Ezra Kissel <ezkissel@indiana.edu>
BuildArch: noarch

Requires : libxsp-client phoebus-client periscope blipp accre-ibp-server

%description 

Configures the machine as a DLT node.  This meta package installs the
ACCRE IBP server, XSP clients, a Periscope measurement store, and the
BLiPP monitoring agent.

%prep

# %setup

%build

%clean

%install

%files

%post

%changelog

