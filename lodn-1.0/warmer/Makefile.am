SUBDIRS = fdr ibp libexnode lbone

bin_PROGRAMS = LoDNWarmer LoDNWarmerUpdater warmer_generator ls_mappings 
LoDNWarmer_SOURCES = LoDNAuxillaryFunctions.c LoDNDepotMgmt.c \
					LoDNExnode.c LoDNLogging.c LoDNMsgQueue.c \
					LoDNSerializedWarmer.c LoDNFreeMappings.c LoDNThreads.c \
					LoDNWarmer.c \
					LoDNAuxillaryFunctions.h LoDNFreeMappings.h \
					LoDNDepotMgmt.h LoDNExnode.h LoDNLogging.h LoDNMsgQueue.h \
					LoDNSerializedWarmer.h LoDNThreads.h
LoDNWarmerUpdater_SOURCES = LoDNWarmerUpdater.c
warmer_generator_SOURCES = warmer_generator.c
ls_mappings_SOURCES = ls_mappings.c

# Compile with phoebus if required
if PHOEBUS
    LoDNWarmer_SOURCES       += phoebus.c phoebus.h
    phoebus_route_ctl_SOURCES = phoebus.c phoebus.h phoebus_route_ctl.c 
    bin_PROGRAMS             += phoebus_route_ctl
endif
    
INCLUDES = -I./lbone/local/include -I./ibp/local/include -I./libexnode/local/include -I./fdr/local/include @LIBXML_INCLUDE_DIR@
LoDNWarmer_CFLAGS = -D_RANDOM_LINK_VALUE
LoDNWarmer_LDADD = -libp -lexnode -lxml2 -lfdr -lpthread @ICONV_LIB@ ${LDADD}
LoDNWarmer_LDFLAGS = -L./ibp/local/lib -Wl,-rpath -Wl,./ibp/local/lib -L./libexnode/local/lib -Wl,-rpath -Wl,./libexnode/local/lib -L./fdr/local/lib -Wl,-rpath -Wl,./fdr/local/lib  @LIBXML_LIB_DIR@ @ICONV_LIB_DIR@  ${LDFLAGS}
LoDNWarmerUpdater_LDADD = -libp -lexnode -lxml2 -lfdr -lpthread  @ICONV_LIB@ ${LDADD}
LoDNWarmerUpdater_LDFLAGS = -L./ibp/local/lib -Wl,-rpath -Wl,./ibp/local/lib  -L./libexnode/local/lib -Wl,-rpath -Wl,./libexnode/local/lib -L./fdr/local/lib -Wl,-rpath -Wl,./fdr/local/lib @LIBXML_LIB_DIR@ @ICONV_LIB_DIR@ ${LDFLAGS}
warmer_generator_LDADD = -llbone -libp -lfdr -lpthread  @ICONV_LIB@ ${LDADD}
warmer_generator_LDFLAGS = -L./lbone/local/lib -Wl,-rpath -Wl,./lbone/local/lib -L./ibp/local/lib -Wl,-rpath -Wl,./ibp/local/lib   -L./fdr/local/lib -Wl,-rpath -Wl,./fdr/local/lib @ICONV_LIB_DIR@ ${LDFLAGS}
ls_mappings_LDADD = -libp -lexnode -lxml2 -lfdr -lpthread  @ICONV_LIB@ ${LDADD}
ls_mappings_LDFLAGS = -L./ibp/local/lib -Wl,-rpath -Wl,./ibp/local/lib -L./libexnode/local/lib -Wl,-rpath -Wl,./libexnode/local/lib -L./fdr/local/lib -Wl,-rpath -Wl,./fdr/local/lib  @LIBXML_LIB_DIR@ @ICONV_LIB_DIR@ ${LDFLAGS}


bindir = @LODN_TOOLS@

lodntoolsdir = @LODN_TOOLS@

dist_lodntools_SCRIPTS = checkWarmer.sh 

install-data-hook:
	chmod o-rwx @LODN_TOOLS@/*
	echo "Generating Master Warmer"
	@LODN_TOOLS@/warmer_generator -v -l vertex.cs.utk.edu:6767 -g 3 -o master_warmer 
	crontab -l > cronfile
	echo "Putting checkWarmer into crontab"
	echo "0-59/5 * * * * @LODN_TOOLS@/checkWarmer.sh" >> cronfile
	crontab cronfile
	rm -rf cronfile blankfile
	if [ "x@WARMER_LOGFILE_DIR@" != "x" ]; then  install -d @WARMER_LOGFILE_DIR@;  touch @WARMER_LOGFILE@; chmod o-rwx @WARMER_LOGFILE@; fi

clean:
	rm -rf checkWarmer.sh warmerGenerator.sh LoDNWarmer LoDNWarmerUpdater warmer_generator *.o cronfile
