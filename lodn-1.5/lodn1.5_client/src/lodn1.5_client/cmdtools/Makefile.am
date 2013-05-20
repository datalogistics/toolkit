bin_PROGRAMS = test_session lodn_mkdir lodn_rmdir lodn_ls lodn_importExnode \
               lodn_exportExnode lodn_stat lodn_unlink lodn_xattr \
               #lodn_cp

#get_status_SOURCES         = get_status.c
test_session_SOURCES        = test_session.c
lodn_mkdir_SOURCES          = lodn_mkdir.c
lodn_rmdir_SOURCES          = lodn_rmdir.c
lodn_ls_SOURCES             = lodn_ls.c
lodn_importExnode_SOURCES   = lodn_importExnode.c
lodn_exportExnode_SOURCES   = lodn_exportExnode.c
lodn_stat_SOURCES           = lodn_stat.c
lodn_unlink_SOURCES         = lodn_unlink.c
lodn_xattr_SOURCES          = lodn_xattr.c
lodn_cp_SOURCES             = lodn_cp.c

INCLUDE = -I../include
LDADD   = -L../src -llodn 

if SSL
    LDADD += -lssl -lcrypto
endif


#get_status_INCLUDE    = -I../include
#test_session_INCLUDE = -I../include
#test_mkdir_INCLUDE = -I../include
#get_status_LDADD   = -L../src -llodn -lssl -lcrypto
#test_session_LDADD = -L../src -llodn -lssl -lcrypto
#test_mkdir_LDADD = -L../src -llodn -lssl -lcrypto