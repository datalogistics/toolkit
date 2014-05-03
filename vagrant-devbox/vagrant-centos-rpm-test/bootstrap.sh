#!/usr/bin/env bash

yum-config-manager --add-repo http://damsl.cs.indiana.edu/yum/damsl-centos-6.repo
rpm --import http://damsl.cs.indiana.edu/yum/RPM-GPG-KEY-damsl-6
yum check-update
yum -y install dlt xsp phoebus
