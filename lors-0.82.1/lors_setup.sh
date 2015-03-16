#!/bin/sh

INSTALLDIR=/home/vagrant/usr

echo "Are you located in the US? (Y/n): "
read US
if test -z "$US" || test "x$US" = xy || test "x$US" = xY ; then
    echo  "Please enter your US zip code: "
    read ZIP
    if test -z "$ZIP"; then
        echo "you did not enter a zipcode.. using 37919"
        ZIP=37919
    fi
    LOCATION="zip= $ZIP"
else
    echo  "Please enter your two letter Country Code: "
    read COUNTRY
    if test -z "$COUNTRY"; then
        echo "You entered no response.. using 'US'"
        COUNTRY=US
    fi
    LOCATION="country= $COUNTRY"
fi

echo ""
echo "The LoRS Tools support various encryption and compression schemes."
echo "Defaults can be set in your .xndrc file, or you may override them"
echo "from the command line."
echo ""
echo "You should disable encryption ONLY if you do not care about the"
echo "privacy of the data you will be storing in IBP."
echo ""
echo "The consequence of the end to end services, generally, is slower throughput,"
echo "as more processing will have to occur before your data can be transfered"
echo "over the network."
echo ""
echo "1 - Checksum, XOR"
echo "2 - Checksum, DES"
echo "3 - Checksum, Compression, XOR"
echo "4 - Checksum, Compression, DES"
echo "5 - None"
echo "6 - Checksum, AES (incompatible with LoRS 0.81 and earlier)"
echo "7 - Checksum, Compression, AES (incompatible with LoRS 0.81 and earlier)"
echo ""
echo "Pick a number which represents your choice:"
read E2E
case $E2E in
    1)
        E2E_ORDER="checksum xor_encrypt"
        ;;
    2)
        E2E_ORDER="checksum des_encrypt"
        ;;
    3)
        E2E_ORDER="checksum zlib_compress xor_encrypt"
        ;;
    4)
        E2E_ORDER="checksum zlib_compress des_encrypt"
        ;;
    5)
        E2E_ORDER="none"
        ;;
    6)
        E2E_ORDER="checksum aes_encrypt"
        ;;
    7)
        E2E_ORDER="checksum zlib_compress aes_encrypt"
        ;;
    *)
        echo "Your choice was not recognized as a valid option."
        echo "We have chosen 1, Checksum, XOR."
        E2E_ORDER="checksum xor_encrypt"
        ;;
esac

echo ""
echo "Running lbone_proximity"

UNAME=`uname`
PWD=`pwd`
RESOLUTION_FILE=${HOME}/.resolution.txt

if test -x $INSTALLDIR/bin/lbone_resolution ; then
    echo "Writing ~/.resolution.txt cache file (be patient)..."
    $INSTALLDIR/bin/lbone_resolution -getcache -m -1 -l "$LOCATION"  > $RESOLUTION_FILE 2> /dev/null
else
   echo "Cannot find executable installation of 'lbone_resolution'."
   echo "Please run 'make install' prior to running lors_setup.sh."
   exit
fi

echo ""
echo "Writing ~/.xndrc configuration file..."

cat << EOF_XND > $HOME/.xndrc
######################################################################
#
#  This file contains defaults for the LoRS command line tools.
#
#  These defaults can be overriden by specifying parameters on
#  the command line of each tool.
# 
#  See loci.cs.utk.edu/lbone/lbone_api.html for details on how to 
#  specify location options. 
#
######################################################################

#  The LBONE_SERVER is required. 
#  You may specify as many lbone servers as are available.  If one should
#  fail, the tools will fail over to the next.

LBONE_SERVER     dsj.sinrg.cs.utk.edu 6767
LBONE_SERVER     vertex.cs.utk.edu 6767
LBONE_SERVER     galapagos.cs.utk.edu 6767
LBONE_SERVER     adder.cs.utk.edu 6767
   

RESOLUTION_FILE $RESOLUTION_FILE

#  If Location is left empty, the tools will default to random depots. 
LOCATION                    $LOCATION

#  Set DURATION to be the number of days that you want the new 
#  storage to exist when you use lors_upload, lors_augment. 
#  You may specify partial days using the modifiers 'm', 'h', 'd' for 
#  Minutes, Hours, and Days respectively. The default (if unspecified) 
#  is 1 day.
DURATION 1d

#  IBP information. STORAGE_TYPE may be either HARD or SOFT 
#  and it defaults to HARD. See the IBP website for details.
STORAGE_TYPE                HARD

# Internal diagnostic messages.
VERBOSE                1

#  Buffer size. Upload and Download require a buffer as they move data
#  to and from depots. You can specify values using K for kilobytes or 
#  M for megabytes. The default buffer size is 48M. 
#
MAX_INTERNAL_BUFFER    64M

#  LORS View demo output is now selectable from the command line and xndrc
#  -D from the command line will toggle the value in xndrc. 1 is demo on, 0 is
#  demo off.
DEMO 0

######################################################################
#
#  UPLOAD/AUGMENT - for xnd_upload and xnd_augment only
# 
######################################################################

#  Use FRAGMENTS_PER_FILE to break a file into a specific number of
#  pieces. Use DATA_BLOCKSIZE to store the file in blocks of this size.
#  DATA_BLOCKSIZE is the preferred choice.
#  You may specify either FRAGMENTS_PER_FILE or DATABLOCK_SIZE. 
#  You may use K for kilobytes or M for megabytes. The default is bytes.

DATA_BLOCKSIZE       5M
# FRAGMENTS_PER_FILE   -1

#  End to end conditioning is necessary to guarantee data security and 
#  integrity while operating on an unreliable, untrusted storage medium.
#  The default is to MD5 checksum and DES encryption.
#  The E2E_BLOCKSIZE specifies the size of end to end conditioning on a
#  Mapping (as defined by either Fragment size or Data blocksize.)
E2E_BLOCKSIZE        512K
E2E_ORDER            $E2E_ORDER

#  Use COPIES to set how many copies of the file that you want to 
#  create. COPIES defaults to 1. 
COPIES               3

TIMEOUT     2600
#  Use THREADS to specify the maximum number of threads to use.
#  This is regularly calculated automatically by the tools, to use as many
#  threads as necessary.  This is easily not ideal.
#
THREADS  8

# 
#  Use MAXDEPOTS to specify the maximum number of IBP depots to use.
# 
MAXDEPOTS 6

#  
#  Use DEPOT to specify the specific depot to be used.
#  If you have local depots which are not registered in any lbone server, you
#  may use them with the LoRS tools in this way.
# 
#  DEPOT   fretless.cs.utk.edu  6714
#  DEPOT   ovation.cs.utk.edu   6714


EOF_XND

sleep 1
echo ""
echo "Success!"
echo "(press enter to exit)"
read END

