#!/usr/bin/env bash

yum-config-manager --add-repo http://www.data-logistics.org/yum/damsl-centos-6.repo
rpm --import http://www.data-logistics.org/yum/RPM-GPG-KEY-damsl-6
rpm --import http://www.data-logistics.org/yum/RPM-GPG-KEY-EPEL-6
yum check-update
yum -y install dlt xsp phoebus
