FROM tianon/centos:6.5
RUN curl -o epel.rpm http://dl.fedoraproject.org/pub/epel/6/x86_64/epel-release-6-8.noarch.rpm ; rpm -Uvh epel.rpm ;yum install -y java-1.7.0-openjdk git gcc libcurl-devel jansson-devel autoconf213 openssl-devel rpm-build rpmlint rpmdevtools make openssh-server
EXPOSE 22
CMD /usr/sbin/sshd -D
