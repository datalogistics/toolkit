lodnwebdir = @LODN_WEB@

dist_lodnweb_DATA = @JAR_FILES@

 
Exnode.jar: 
	@echo "Signing $@"
	@JARSIGNER_BIN@ -keystore "@KEYSTORE@" -storepass "@KEYSTORE_PASSWD@" -signedjar $@ $@.unsigned "@KEYSTORE_ALIAS@"

IBP.jar:  
	@echo "Signing $@"
	@JARSIGNER_BIN@ -keystore "@KEYSTORE@" -storepass "@KEYSTORE_PASSWD@" -signedjar $@ $@.unsigned "@KEYSTORE_ALIAS@"
	
LBone.jar: 
	@echo "Signing $@"
	@JARSIGNER_BIN@ -keystore "@KEYSTORE@" -storepass "@KEYSTORE_PASSWD@" -signedjar $@ $@.unsigned "@KEYSTORE_ALIAS@"

LoDN.jar: 
	@echo "Signing $@"
	@JARSIGNER_BIN@ -keystore "@KEYSTORE@" -storepass "@KEYSTORE_PASSWD@" -signedjar $@ $@.unsigned "@KEYSTORE_ALIAS@"

LoDNClient.jar: 
	@echo "Signing $@"
	@JARSIGNER_BIN@ -keystore "@KEYSTORE@" -storepass "@KEYSTORE_PASSWD@" -signedjar $@ $@.unsigned "@KEYSTORE_ALIAS@"

LoDNHelp.jar: 
	@echo "Signing $@"
	@JARSIGNER_BIN@ -keystore "@KEYSTORE@" -storepass "@KEYSTORE_PASSWD@" -signedjar $@ $@.unsigned "@KEYSTORE_ALIAS@"

LoDNPublisher.jar: 
	@echo "Signing $@"
	@JARSIGNER_BIN@ -keystore "@KEYSTORE@" -storepass "@KEYSTORE_PASSWD@" -signedjar $@ $@.unsigned "@KEYSTORE_ALIAS@"

cli.jar: 
	@echo "Signing $@"
	@JARSIGNER_BIN@ -keystore "@KEYSTORE@" -storepass "@KEYSTORE_PASSWD@" -signedjar $@ $@.unsigned "@KEYSTORE_ALIAS@"

forms-1.0.4.jar: 
	@echo "Signing $@"
	@JARSIGNER_BIN@ -keystore "@KEYSTORE@" -storepass "@KEYSTORE_PASSWD@" -signedjar $@ $@.unsigned "@KEYSTORE_ALIAS@"

jhall.jar: 
	@echo "Signing $@"
	@JARSIGNER_BIN@ -keystore "@KEYSTORE@" -storepass "@KEYSTORE_PASSWD@" -signedjar $@ $@.unsigned "@KEYSTORE_ALIAS@"

install-data-hook:
	chmod o-rx @LODN_WEB@/*.jar

clean: 
	@echo "Removing signed jars"
	rm -rf *jar
	
