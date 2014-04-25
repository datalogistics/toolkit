#!/bin/bash
curl -o /etc/yum.repos.d/jenkins.repo http://pkg.jenkins-ci.org/redhat/jenkins.repo
curl -o epel.rpm http://dl.fedoraproject.org/pub/epel/6/x86_64/epel-release-6-8.noarch.rpm
rpm -Uvh epel.rpm 
rpm --import http://pkg.jenkins-ci.org/redhat/jenkins-ci.org.key
JENKINS_MASTER_DEPS="docker-io java-1.7.0-openjdk jenkins nano"
DLT_DEVELOPMENT_DEPS="git gcc libcurl-devel jansson-devel autoconf213 openssl-devel rpm-build rpmlint rpmdevtools make cmake28 mock"
yum -y install ${JENKINS_MASTER_DEPS} ${DLT_DEVELOPMENT_DEPS}
newgrp mock && usermod -a -G mock vagrant && newgrp mock
rm -rf /var/lib/jenkins
git clone https://github.com/datalogistics/CI.git /var/lib/jenkins
mkdir /var/lib/jenkins/.ssh
cp /vagrant/id_rsa* /var/lib/jenkins/.ssh || true
chown -R jenkins:jenkins /var/lib/jenkins
chmod 700 /var/lib/jenkins/.ssh
chmod 600 /var/lib/jenkins/.ssh/*
sudo -u jenkins ssh-keyscan -H github.com >> /var/lib/jenkins/.ssh/known_hosts
chown jenkins:jenkins /var/lib/jenkins/.ssh/known_hosts
chmod 600 /var/lib/jenkins/.ssh/known_hosts
service jenkins start
chkconfig jenkins on
service docker start
chkconfig docker on
