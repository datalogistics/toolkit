# Uncomment the following if compiling with java > 1.4.2
#FLAGS=-Xlint:unchecked  

JAVAC = javac
JAR   = jar

ALL: Exnode.jar.unsigned IBP.jar.unsigned LBone.jar.unsigned LoDN.jar.unsigned  LoDNPublisher.jar.unsigned cli.jar.unsigned LoDNHelp.jar.unsigned LoDNClient.jar.unsigned

classfiles:
	find ./ -name "*java" | xargs ${JAVAC} ${FLAGS}  -classpath ./src:forms-1.0.4.jar:jhall.jar:lstcp.jar

Exnode.jar.unsigned: classfiles
	cd src && \
	${JAR} cf ../Exnode.jar.unsigned \
	edu/utk/cs/loci/exnode/*.class 

IBP.jar.unsigned: classfiles
	cd src && \
	${JAR} cf ../IBP.jar.unsigned \
	edu/utk/cs/loci/ibp/* 

LBone.jar.unsigned: classfiles
	cd src && \
	${JAR} cf ../LBone.jar.unsigned \
	edu/utk/cs/loci/lbone/LboneServer.class 

LoDN.jar.unsigned: classfiles
	cd src && \
	${JAR} cf ../LoDN.jar.unsigned \
	com/bayanihancomputing/util/time/*.class \
	com/bayanihancomputing/util/stat/*.class \
	edu/utk/cs/loci/cli/*.class \
	edu/utk/cs/loci/ibp/*.class \
	edu/utk/cs/loci/exnode/*.class \
	edu/utk/cs/loci/transfer/*.class \
	edu/utk/cs/loci/lbone/*.class \
	edu/utk/cs/loci/lodnclient/*.class \
	edu/utk/cs/loci/lodnpublisher/*.class \
	edu/utk/cs/loci/exnode/*.class 

LoDNPublisher.jar.unsigned: classfiles
	cd src && \
	${JAR} cf ../LoDNPublisher.jar.unsigned \
	edu/utk/cs/loci/lodnpublisher/*.class \
	edu/utk/cs/loci/lodnpublisher/UploadAction.class \
	edu/utk/cs/loci/lodnpublisher/WindowAdapter.class \

LoDNClient.jar.unsigned: classfiles
	cd src && \
	${JAR} cf ../LoDNClient.jar.unsigned edu/utk/cs/loci/lodnclient/*.class

cli.jar.unsigned: classfiles
	cd src && \
	${JAR} cf ../cli.jar.unsigned \
	edu/utk/cs/loci/cli/*.class 

LoDNHelp.jar.unsigned:
	${JAR} cf LoDNHelp.jar.unsigned \
	lodnhelp/UploadClient.html \
	lodnhelp/UploadFig1.jpg \
	lodnhelp/UploadFig2.gif \
	lodnhelp/fig1.jpg \
	lodnhelp/fig2.gif \
	lodnhelp/fig3.jpg \
	lodnhelp/help.hs \
	lodnhelp/helpclient.html \
	lodnhelp/helpintro.html \
	lodnhelp/helpupload.hs \
	lodnhelp/map.jhm \
	lodnhelp/mapupload.jhm \
	lodnhelp/toc.xml \
	lodnhelp/tocupload.xml 

clean: 
	rm -rf Exnode.jar.unsigned IBP.jar.unsigned LBone.jar.unsigned LoDN.jar.unsigned  LoDNPublisher.jar.unsigned cli.jar.unsigned LoDNHelp.jar.unsigned LoDNClient.jar.unsigned
	find ./ -name "*.class" | xargs rm -rf
