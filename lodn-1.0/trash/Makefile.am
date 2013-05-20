bin_PROGRAMS = LoDN_Free_Exnode
LoDN_Free_Exnode_SOURCES = LoDN_Free_Exnode.c
INCLUDES = -I@LORS_INCLUDE_DIR@ @LIBXML_INCLUDE_DIR@
LoDN_Free_Exnode_LDADD = -libp -lexnode -lxml2 -lfdr -lpthread
LoDN_Free_Exnode_LDFLAGS = -L@LORS_LIB_DIR@  -Wl,-rpath -Wl,@LORS_LIB_DIR@  \
                            @LIBXML_LIB_DIR@


bindir = @LODN_TOOLS@

lodntoolsdir = @LODN_TOOLS@

dist_lodntools_SCRIPTS = free_trash.pl

install-data-hook:
	chmod o-rwx @LODN_TOOLS@/*
	crontab -l > cronfile || touch cronfile
	echo "0 3 * * * @LODN_TOOLS@/free_trash.pl @LODN_TRASH@" >> cronfile
	crontab cronfile
	rm -rf cronfile

clean:
	rm -rf free_trash.pl LoDN_Free_Exnode *.o cronfile
