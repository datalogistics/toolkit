SUBDIRS = cgi jars etc web web tools trash warmer

install-data-hook:
	@echo "Installing the content directory"
	@INSTALL@ -m 750 -d @LODN_CONTENT_DIR@
	@INSTALL@ -m 600 warmer/master_warmer @LODN_CONTENT_DIR@/.warmer
	@echo "Installing the trash directory"
	@INSTALL@ -m 750 -d @LODN_TRASH@
	ln -s @LODN_WEB@ @LODN_WEB@/../lodn
