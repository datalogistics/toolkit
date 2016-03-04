%define default_release 1

Summary: OSiRIS Store Meta Package
Name: OSiRIS-store
Version: 1.0
Release: %{?release}%{!?release:%{default_release}}
License: N/A
Group: Utilities/Configuration

BuildRoot: %{_builddir}/%{name}-root
Packager: jayaajay <jayaajay@indiana.edu>
BuildArch: noarch

Requires : periscope ntp libwebsockets dlt-lors OSiRIS-Tools

%description 

Configures the OSiRIS Store for measurement store. This meta 
package installs the Periscope measurement store, DLT LoRS 
and OSiRIS-tools.

%prep

# %setup

%build

%clean

%install

%files

%post

%changelog

