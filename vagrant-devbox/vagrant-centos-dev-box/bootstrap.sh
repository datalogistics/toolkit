#!/usr/bin/env bash

yum check-update
sudo bash -c "yum -y install libconfig-devel libxml2-devel libdb-devel libc6-devel vim pkg-config cmake openssl-devel libtool gawk bison byacc automake m4 autoconf autotools-devel libevent-devel apr-devel apr-util-devel openssl libconfig9 libxml2 libdb-devel jansson-devel git libcurl-devel libjansson-devel python-setuptools python-argparse python-devel mongodb swig python-pip python-zmq python-psutil python-m2crypto mongodb-server python-netifaces"

# some housekeeping
cd /usr/include
ln -s apr-1.0 apr-1


# Pulling up repos
cd /home/vagrant/
sudo -u vagrant git clone git@github.com:datalogistics/ibp_server.git
sudo -u vagrant git clone git@github.com:datalogistics/toolkit.git
sudo -u vagrant git clone git@github.com:periscope-ps/libunis-c.git
sudo -u vagrant git clone git@gitlab.crest.iu.edu:damsl/xsp.git
sudo -u vagrant git clone git@gitlab.crest.iu.edu:damsl/phoebus.git
sudo -u vagrant git clone git@gitlab.crest.iu.edu:damsl/idms.git
sudo -u vagrant git clone git@github.com:periscope-ps/blipp.git
sudo -u vagrant git clone git@github.com:periscope-ps/unis.git
sudo -u vagrant git clone git@gitlab.crest.iu.edu:damsl/idms.git


# lets compile stuff
## libunis-c
cd /home/vagrant/libunis-c/
sudo -u vagrant ./configure
sudo -u vagrant make
make install

#xsp
cd /home/vagrant/xsp
sudo -u vagrant ./autogen.sh
sudo -u vagrant ./configure
sudo -u vagrant make
make install

#phoebus
cd /home/vagrant/phoebus
sudo -u vagrant ./autogen.sh
sudo -u vagrant ./configure --with-xsp=/home/vagrant/xsp
sudo -u vagrant make
make install

#ibp_server
cd /home/vagrant/ibp_server
sudo -u vagrant ./bootstrap
sudo -u vagrant make
make install

#unis
cd /home/vagrant/unis
python ./setup.py install

#blipp
cd /home/vagrant/blipp
python ./setup.py install
