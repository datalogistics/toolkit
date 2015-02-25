CC = gcc 
CFLAGS = -g -DMULTIPLICITY -Dbool=char -DHAS_BOOL
CDIR = /usr/local/include


ifeq ($(GTYPE),sparc-sun-solaris2.7)	    	# For Sparc machines
  PERLDIR = /mdx/usr/local/lib/perl5/5.00503/sun4-solaris/CORE
  PERLLDFLAGS = -lperl -lsocket -lnsl -ldl -lm -lc -lcrypt
else						# For Intel machines
  PERLDIR = /usr/lib/perl5/5.6.0/i386-linux/
  PERLLDFLAGS = -lperl -lnsl -ldl -lm -lc -lcrypt
endif

PERLCFLAGS = -O2 -Dbool=char -DHAS_BOOL -DMULTIPLICITY
PERLINCLUDES = -I$(CDIR) -I$(PERLDIR) -I/usr/lib/perl5/5.6.0/i386-linux/CORE
PERLEXTUTILSC = `perl -MExtUtils::Embed -e ccopts`
PERLEXTUTILSO = `perl -MExtUtils::Embed -e ldopts`


EXECUTABLES = ngc

ARGS0 = interp.o
ARGS1 = showtime.o
ARGS2 = perlxsi.o ngc.o call_ngc.o

all: $(EXECUTABLES)

clean:
	rm -f core *.o $(EXECUTABLES)

.SUFFIXES: .c .o
.c.o:
	$(CC) $(PERLINCLUDES)  -c $*.c $(PERLEXTUTILSC) 
	
#interp: $(ARGS0)
#	$(CC) $(PERLCFLAGS) $(PERLINCLUDES) -L$(PERLDIR) -o interp $(ARGS0) $(PERLLDFLAGS)
	
interp: $(ARGS0)
	$(CC) -o interp $(ARGS0) $(PERLEXTUTILSO)
	
showtime: $(ARGS1)
	$(CC) -o showtime $(ARGS1) $(PERLEXTUTILSO)
	
ngc: $(ARGS2)
	$(CC) $(CFLAGS) $(PERLINCLUDES) -o ngc $(ARGS2) $(PERLEXTUTILSO)
