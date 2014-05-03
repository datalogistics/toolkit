#!/usr/bin/env bash

sudo apt-get update
sudo bash -c "DEBIAN_FRONTEND=noninteractive apt-get install -q -y libconfig-dev libxml2-dev libdb-dev libc6-dev build-essential vim pkg-config cmake libssl-dev libtool gawk bison byacc automake m4 autoconf autotools-dev libevent-dev libaprutil1-dev libapr1-dev openssl libprotobuf-c0 libconfig9 libxml2 libdb-dev libjansson4 git libcurl4-openssl-dev libjansson-dev python-setuptools python-argparse python-dev mongodb swig python-pip python-zmq python-psutil python-m2crypto"

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
sudo -u vagrant ./configure -with-xsp=/home/vagrant/xsp
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
