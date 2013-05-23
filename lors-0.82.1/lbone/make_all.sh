#!/bin/sh

UNAME=`uname`
mkdir -p obj/$UNAME
# CLIENT
cd ibp/client 
sh ./client_install.sh
cd ../../
ranlib ./ibp/client/lib/$UNAME/IBPClientLib.a

cd nws
make clean
rm -f config.status config.cache
./configure && make lib
cd Sensor/ExpMemory
make memory.o
ar -q ../../libnws.a memory.o
cd ../../
ranlib ./libnws.a

mkdir -p Library/$UNAME/
cp libnws.a Library/$UNAME
cd ..

cd ecgi
make
ranlib libecgi.a
cd ..

./configure --enable-server
make
