
JARPATH=/home/jp/lodn/web
JARS="${JARPATH}/Exnode.jar:${JARPATH}/LBone.jar:${JARPATH}/IBP.jar:."
JAVABINPATH=/ibp/jdk/bin

all: javac jar

javac:
	${JAVABINPATH}/javac -classpath ${JARS} LogisticalDownload.java
	${JAVABINPATH}/javac -classpath ${JARS} LogisticalUpload.java


jar:
	(cd /home/jp/lodn/cli ; ${JAVABINPATH}/jar cMf ${JARPATH}/cli.jar `find . -name "*.class"`)


