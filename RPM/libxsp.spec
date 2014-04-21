Name: libxsp
Version: 1.0
Release: 8
Summary: XSP RPM

Group: Application/Network
License: GPL
URL: http://damsl.cs.indiana.edu/projects/xsp
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}
BuildRequires: libconfig-devel
Requires: libxsp-common, libxsp-client, libconfig

Packager: Ezra Kissel <ezkissel@indiana.edu>
Vendor: Distributed and Metasystems Lab (DAMSL), Indiana University
Provides: xsp

%define __os_install_post /usr/lib/rpm/brp-compress

%description
The eXtensible Session Protocol RPM packages.

%files

%package client
Summary: XSP client library
Group: Application/Network
Requires: libconfig
%description client
This package contains the client libraries and test applications for
interacting with XSP services.


%package common
Summary: XSP common library
Group: Application/Network
Requires: libconfig
%description common
The eXtensible Session Protocol common library.


%package xspd
Summary: XSP Daemon
Group: Application/Network
Requires: libconfig, libxsp-common
%description xspd
This package contains a basic daemon for libxsp.

%prep
rm -rf $RPM_BUILD_ROOT
%setup -q


%build
#./configure \
#--prefix=%{_prefix} \
#--exec-prefix=%{_exec_prefix} \
#--libexecdir=%{_exec_prefix} \
#--sysconfdir=%{_sysconfdir} \
#--datadir=%{_datadir} \
#make %{?_smp_mflags}
%configure --without-mysql --without-sqlite --enable-oscars
make -j 2

%install
%makeinstall
install -d ${RPM_BUILD_ROOT}/etc/xspd
install -d ${RPM_BUILD_ROOT}/etc/init.d
install -m 664 scripts/xspd.conf ${RPM_BUILD_ROOT}/etc/xspd
install -m 755 scripts/xspd.init ${RPM_BUILD_ROOT}/etc/init.d/xspd

%clean
rm -rf $RPM_BUILD_ROOT

#%post xspd
#/sbin/chkconfig --add xspd
#/usr/sbin/useradd -r -s /bin/nologin -d /tmp xspd || :
#touch /var/log/xspd.log
#chown xspd:xspd /var/log/xspd.log

%preun xspd
if [ $1 -eq 0 ]; then
    /sbin/chkconfig --del xspd
    /sbin/service xspd stop
fi

%postun xspd
/usr/sbin/userdel xspd || :
if [ $1 -ge 1 ]; then
    /sbin/service xspd stop
fi

%files common
%defattr(-,root,root)
%{_libdir}/libxsp_common*
%{_libdir}/libxsp/*

%files client
%defattr(-,root,root)
%{_includedir}/libxsp_client.h
%{_includedir}/libxsp_client_common.h
# RPM build errors:
#    File not found: /home/vagrant/BUILDROOT/libxsp-1.0-20140421031643.x86_64/usr/include/config.h
#%{_includedir}/config.h
%{_includedir}/xsp-proto.h
%{_includedir}/option_types.h
%defattr(775,root,root)
%{_bindir}/*
%{_libdir}/libxsp_client*
%{_libdir}/libxsp_wrapper*
%{_libdir}/libxsp_simplewrapper*

%files xspd
%defattr(-,root,root)
%config(noreplace)/etc/xspd/*
%defattr(775,root,root)
/etc/init.d/xspd
%{_sbindir}/xspd
