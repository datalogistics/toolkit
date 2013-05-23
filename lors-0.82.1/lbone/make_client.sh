#!/bin/sh

UNAME=`uname`
mkdir -p obj/$UNAME
# CLIENT
cd ibp/client 
sh ./client_install.sh
cd ../../
ranlib ./ibp/client/lib/$UNAME/IBPClientLib.a

mkdir -p  client/bin/$UNAME
#cd client/src
#make
#cd ../../

#./configure
#make
