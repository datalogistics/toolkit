#!/bin/sh


#
# Please set the following variables to your keystore setting
#

# JAVA KEYSTORE
keystore="keystore"

# Keystore alias
alias="self" 

# Keystore password
storepass="password"

# Key password
keypass="password"



mkdir signed_jars

cp forms-1.0.4.jar forms-1.0.4.jar.unsigned
cp jhall.jar jhall.jar.unsigned
cp lstcp.jar lstcp.jar.unsigned

for unsigned_jar in *.unsigned; do

    signed_jar=""
    last=""
    i=2
    while [ "x$signed_jar" != "x$unsigned_jar" ]; do
       last=$signed_jar
       signed_jar=`echo $unsigned_jar | cut -d'.' -f1-$i`
       i=`expr $i + 1`
    done

    signed_jar=$last; 

    echo $signed_jar

    jarsigner -keystore $keystore -storepass $storepass -keypass $keypass -signedjar "signed_jars/$signed_jar" $unsigned_jar $alias

done

rm -f forms-1.0.4.jar.unsigned
rm -f jhall.jar.unsigned
rm -f lstcp.jar.unsigned

