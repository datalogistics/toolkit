#!/bin/sh

JARS="/home/jp/lodn/web/Exnode.jar:/home/jp/lodn/web/LBone.jar:/home/jp/lodn/web/IBP.jar:."

if [ $# -ne 1 ]; then
  echo "Usage: $0 filename";
  exit 1;
fi

/data/jdk/bin/java -cp /tmp/cli.jar:${JARS} edu.utk.cs.loci.cli.LogisticalUpload $1

exit 0