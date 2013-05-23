
#$ENV{LOCAL_OS} = "MSWIN32";

$| =1;
foreach $arg (@ARGV)
{
    $_ = $arg;
    if ( m/^--/ )
    {
        s/--//g;
        ($name, @val) = split("=", $_);
        $param{$name} = join("=", @val);
    }
}

if ( defined $param{"vizhost"} )
{
    $ENV{REMOTE_ADDR} = $param{"vizhost"};
}

if ( defined $param{"threads"} )
{
    if ( $param{"threads"} eq "all" ||
         $param{"threads"} eq "ALL" )
    {
        $param{"threads"} = -1;
    }

}

#print "REDUNDANCE: $param{redundance}\n";
#print "TPD: $param{tpd}\n";
#print "PROGRESS: $param{progress}\n";
#print "ALLARGS: @ARGV\n";

$datadir = `pwd`;
chomp $datadir;

open(CYG, ">>log.txt");
foreach $x (keys %param )
{
    print CYG "$x == $param{$x}\n";
}
close (CYG);
if ( $param{cmd} eq "upload" )
{
    do_upload();
} elsif ( $param{cmd} eq "augment" )
{
    do_augment();
} elsif ( $param{cmd} eq "trim" )
{
    do_trim();
} elsif ( $param{cmd} eq "download" )
{
    @ext = split (/\./, $param{inputfile});
    do_download('ign');
} elsif ( $param{cmd} eq "play" )
{
    @ext = split (/\./, $param{inputfile});
    print "extension $ext[-2]\n";

    do_play($ext[-2]);
} elsif ( $param{cmd} eq "list" )
{
    do_list();
} elsif ( $param{cmd} eq "refresh" )
{
    do_refresh();
} elsif ( $param{cmd} eq "route" )
{
    do_route();
} elsif ( $param{cmd} eq "preferences" )
{
    do_preferences();
} elsif ( $param{cmd} eq "nws" )
{
    do_nws();
} elsif ( $param{cmd} eq "mapconfig" )
{
    $hide = "";
    mkdir "$ENV{HOME}/.xndcommand";
    if ( $param{hidedepots} ) { $hide = "-hide"; } 

    print "cachefile : $param{cachefile}\n";
    print "configfile: $param{configfile}\n";
    print "map: $param{pwdroot}/$param{map}\n";
    if ( $param{cachefile} != 0 ) 
    {
        `wget -q -O - http://loci.cs.utk.edu/lors/cgi-bin/displaycfg.cgi > out.tmp`;
#print CYG "`perl '$param{pwdroot}/ldap_to_cfg.pl' -i out.tmp -image $param{map} $param{depotnames} > \"$param{configfile}\"`\n";
        `perl "$param{pwdroot}/ldap_to_cfg.pl" -i out.tmp -image "$param{pwdroot}/$param{map}" $param{depotnames} > "$param{configfile}"`;
    }
    exit;

} elsif ( $param{cmd} eq "ldapsearch" )
{
    $hide = "";
    $OS=$^O;
    mkdir "$ENV{HOME}/.xndcommand";
    if ( $param{hidedepots} )
    {
        $hide = "-hideoff";
    } 
    # TODO ; kill any other instances of lat_display.tcl before launching another.
    open(CYG, ">>log.txt");
    print "cachefile : $param{cachefile}\n";
    if ( $param{cachefile} != 0 ) 
    {
#`ldapsearch -x -w lbone -h galapagos.cs.utk.edu:6776 -b 'o=lbone' -LLL "(objectclass=depot)" > out.tmp`;
        `wget -q -O - http://loci.cs.utk.edu/lors/cgi-bin/displaycfg.cgi > out.tmp`;
        print CYG "`perl '$param{pwdroot}/ldap_to_cfg.pl' -i out.tmp -image '$param{pwdroot}/$param{map}' $param{depotnames} > \"$param{configfile}\"`\n";
        `perl "$param{pwdroot}/ldap_to_cfg.pl" -i out.tmp -image "$param{pwdroot}/$param{map}" $param{depotnames} > "$param{configfile}"`;
        if ( $OS eq "darwin" )
        {
            `"/Applications/Utilities/Wish Shell.app/Contents/MacOS/Wish Shell" "$param{pwdroot}/lat_display.tcl" -config "$param{configfile}" $hide &> /dev/null &`;
        } else {
            `wish "$param{pwdroot}/lat_display.tcl" -config "$param{configfile}" $hide &`;
        }
        print CYG "wish '$param{pwdroot}/lat_display.tcl' -config \"$param{configfile}\" $hide &";
    } else {
        if ( $OS eq "darwin" )
        {
            `"/Applications/Utilities/Wish Shell.app/Contents/MacOS/Wish Shell" "$param{pwdroot}/lat_display.tcl" -config "$param{configfile}" $hide &> /dev/null &`;
        } else {
            `wish "$param{pwdroot}/lat_display.tcl" -config "$param{configfile}" $hide &`;
        }
        print CYG "wish '$param{pwdroot}/lat_display.tcl' -config \"$param{configfile}\" $hide &";
    }
    close (CYG);
    exit;
} else 
{
    print "unrecognized command\n";
    foreach $i (@ARGV) 
    {
        print "$i : \n";
    }
}
exit;

sub report_error_viz 
{
    use Socket;
    my ($message) = @_;
    my ($remote,$port, $iaddr, $paddr, $proto, $line);

    $remote  = 'localhost';
    $port    = 5240;  # random port
    if ($port =~ /\D/) { $port = getservbyname($port, 'tcp') }
    die "No port" unless $port;
    $iaddr   = inet_aton($remote)               || die "no host: $remote";
    $paddr   = sockaddr_in($port, $iaddr);

    $proto   = getprotobyname('tcp');
    socket(SOCK, PF_INET, SOCK_STREAM, $proto)  || die "socket: $!";
    connect(SOCK, $paddr)    || die "connect: $!";
    $resp = <SOCK>;
    print SOCK "MESSAGE 1 ERROR: $message\n";
    close (SOCK)            || die "close: $!";
}

sub do_list
{
    $mediaPath = $param{inputfile};
    $logPath = "dat/log/$param{cmd}_$param{file}.log";
    
    chdir "$ENV{HOME}/.xndcommand/";
    $cmd = "lors_launch lors_ls $param{physical} -t $param{threads} \"$mediaPath\" ";
    print STDERR "$cmd\n";
    
    $ret = open(CMD, "$cmd |"); # || report_error_viz "Executable 'lors_ls' not found in PATH";
    print "$ret: ret\n";
    while( <CMD> ) { print; }
}
sub do_nws
{
    $cmd = "lors_launch lbone_resolution -H $param{lboneserver} -P 6767 $param{source} -lbone $param{mode} -m -1 $param{depot}";
    print STDERR "$cmd\n";
    `$cmd`;
}
sub do_route 
{
    $cmd = "lors_launch lors_route \"$param{inputfile}\" -f -b $param{blocksize} -x \"$param{xndrc}\"";
    print STDERR "$cmd\n";
    open(CMD, "$cmd |")|| report_error_viz "Executable 'lors_route' not found in PATH";;
    while( <CMD> ) { print; }
}
sub do_upload
{
    
    $mediaPath = $param{inputfile};
    $xndFile = $param{outputfile};
    $logPath = "dat/log/$param{cmd}_$param{file}.log";
    
    @sp = split(/:/, $param{lboneserver});
    if ($sp[1] == undef) 
    {
        $lbone = $sp[0];
        $port = 6767;
    } else 
    {
        $lbone = $sp[0];
        $port = $sp[1];
    }
    if ( $param{xndrc} ne "" ) 
    {
        $xndrc = "--xndrc=\"$param{xndrc}\" --depot-list";
    } else{
        $xndrc = "";
    }
    chdir "$ENV{HOME}/.xndcommand/";
    $cmd = "lors_launch lors_upload \"$mediaPath\" ".
           "-b $param{blocksize} -c $param{copies} " .
           "-t $param{threads} -H $lbone ".
           "-P $port -m $param{maxdepot} ".
           "-M $param{memory} ".
           "-E $param{e2e_blocksize} ".
           "-d $param{duration} ".
           "-l $param{location} ".
           "-T $param{timeout} ".
           "--$param{alloc_type} ".
           "-$param{e2eargs} -o \"$xndFile\" $xndrc ";
    open(CYG, ">>log.txt");
    print CYG "$cmd\n";
    close (CYG);
    print "$cmd\n";
    open(CMD, "$cmd |")|| report_error_viz "Executable 'lors_upload' not found in PATH";;;
    while( <CMD> )
    {
        print;
    }
}
sub do_augment 
{
    $inputfile = $param{inputfile};
    $outputfile = $param{outputfile};
#print "$outputfile \n";
    @path = split('/', $outputfile);
#    print "@path \n";
    $ofile = $path[-1];
    pop @path;
    $opath = join ('/', @path);
#    print $opath . "\n";

    if ( $param{xndrc} ne "" ) 
    {
        $xndrc = "--xndrc=\"$param{xndrc}\" --depot-list";
    } else{
        $xndrc = "";
    }
    if ( $param{mcopy} eq "mcopy" )
    {
        $param{mcopy} = "--mcopy";
    }
    if ( $param{balance} == 1 )
    {
        $param{balance} = "--balance";
    }
    if ( $param{savenew} == 1)
    {
        $param{op} = "-n";
    } else {
        $param{op} = "-o";
    }
    if ( $param{offset} != 0 )
    {
        $param{offset} = "-O $param{offset}";
    } else {
        $param{offset} = "";
    }
    if ( $param{len} != 0 )
    {
        $param{len} = "-L $param{len}";
    } else {
        $param{len} = "";
    }

    chdir "$ENV{HOME}/.xndcommand/";
    $cmd = "lors_launch lors_augment \"$inputfile\" $param{op} \"$opath/$ofile\" ".
           "-b $param{blocksize} -l $param{location} -c $param{copies} ".
           "-m $param{maxdepot} -t $param{threads} ".
           "-d $param{duration} -T $param{timeout} ".
           "--$param{alloc_type} $param{mcopy} ". 
           "$param{balance} $param{len} $param{offset} $xndrc ";
    print  "$cmd\n";
    open(CMD, "$cmd |")|| report_error_viz "Executable 'lors_augment' not found in PATH";;;;
    while( <CMD> ) { print; }
}

sub do_preferences
{
    $loc = $param{location};
    @sp = split(/:/, $param{lboneserver});
    if ($sp[1] == undef) 
    {
        $lbone = $sp[0];
        $port = 6767;
    } else 
    {
        $lbone = $sp[0];
        $port = $sp[1];
    }
# TODO: change this to a perl call.
    mkdir "$ENV{HOME}/.xndcommand";
    open (XND, ">$ENV{HOME}/.xndcommand/.xndrc");

    print XND <<EOF
RESOLUTION_FILE $ENV{HOME}/.xndcommand/resolution.txt
LBONE_SERVER $lbone $port
LOCATION $loc
STORAGE_TYPE $param{alloc_type}
DURATION $param{duration}
VERBOSE 0
THREADS $param{threads}
COPIES $param{copies}
MAX_INTERNAL_BUFFER $param{memory}
TIMEOUT $param{timeout}
DATA_BLOCKSIZE $param{blocksize}
MAXDEPOTS $param{maxdepot}
DEMO 1

GUI_VIZ_HOST $param{vizhost}
GUI_ADVANCED $param{advanced}
XND_DIRECTORY $param{xnddirectory}
EOF
;
    close(XND);
    # `lbone_resolution -getcache -m -1 -l "$loc" > "$ENV{HOME}/.xndcommand/resolution.txt"`;
    print "Done\n";
}

sub do_refresh
{
    $inputfile = $param{inputfile};
    $outputfile = $param{outputfile};
    
    chdir "$ENV{HOME}/.xndcommand/";
    $cmd = "lors_launch lors_refresh \"$inputfile\" $param{duration} ";
    print  "$cmd\n";
    `$cmd`;
}

sub do_trim
{
    $inputfile = $param{inputfile};
    $outputfile = $param{outputfile};
    
    chdir "$ENV{HOME}/.xndcommand/";
    $cmd = "lors_launch lors_trim \"$inputfile\" -o \"$outputfile\" ".
           "$param{segs} ";
    print  "$cmd\n";
    `$cmd`;
}

sub do_download
{
    my ($ext ) = @_;
    print "EXTENSION IS $ext\n";
    $inputfile = $param{inputfile};
    $outputfile = $param{outputfile};

    if ( $param{offset} != 0 )
    {
        $param{offset} = "-O $param{offset}";
    } else {
        $param{offset} = "";
    }
    if ( $param{len} != 0 )
    {
        $param{len} = "-L $param{len}";
    } else {
        $param{len} = "";
    }

    $args = "-q $param{prebuf} -C $param{cache} ".
            "-a $param{tpd} -r $param{redundance} ".
            "-p $param{progress} $param{len} $param{offset} --resume ";
 
    chdir "$ENV{HOME}/.xndcommand/";
    $cmd = "lors_launch lors_download \"$inputfile\" ".
           "-b \"$param{blocksize}\" " . 
           "-t $param{threads} $args -o \"$outputfile\"";
    print "$cmd\n";
    open(CMD, "$cmd |")|| report_error_viz "Executable 'lors_download' not found in PATH";
    while( <CMD> ) { print; }
    #`$cmd `;
}

sub do_play
{
    my ($ext ) = @_;
    my $home = $ENV{HOME};
    chomp($home);

    my $mplayer;
    chomp($mplayer = `which mplayer`);
    if ($mplayer eq "") {
        if (-f "/usr/bin/mplayer") { $mplayer = "/usr/bin/mplayer"; }
        elsif (-f "/usr/local/bin/mplayer") { $mplayer = "/usr/local/bin/mplayer"; }
        elsif (-f "$home/bin/mplayer") { $mplayer = "$home/bin/mplayer"; }
        elsif (-f "/sw/bin/mplayer") { $mplayer = "/sw/bin/mplayer"; }
        elsif (-f "../bin/mplayer") { $mplayer = "../bin/mplayer"; }
    }

    my $mpg123;
    chomp($mpg123 = `which mpg123`);
    if ($mpg123 eq "") {
        if (-f "/usr/bin/mpg123") { $mpg123 = "/usr/bin/mpg123"; }
        elsif (-f "/usr/local/bin/mpg123") { $mpg123 = "/usr/local/bin/mpg123"; }
        elsif (-f "$home/bin/mpg123") { $mpg123 = "$home/bin/mpg123"; }
        elsif (-f "/sw/bin/mpg123") { $mpg123 = "/sw/bin/mpg123"; }
        elsif (-f "../bin/mpg123") { $mpg123 = "../bin/mpg123"; }
    }
    print "EXTENSION IS $ext\n";
    $inputfile = $param{inputfile};
    $outputfile = $param{outputfile};

    $OS=$^O;
    print "$OS\n";
    if ( $param{offset} != 0 )
    {
        $param{offset} = "-O $param{offset}";
    } else {
        $param{offset} = "";
    }
    if ( $param{len} != 0 )
    {
        $param{len} = "-L $param{len}";
    } else {
        $param{len} = "";
    }
    $args = "-q $param{prebuf} -C $param{cache} ".
            "-a $param{tpd} -r $param{redundance} ".
            "-p $param{progress} $param{len} $param{offset} ";
    chdir "$ENV{HOME}/.xndcommand/";
    if ( $ext eq "wav" )
    {
        if ( "$OS" ne "MSWin32" )
        {
            $cmd = "lors_launch lors_download \"$inputfile\" ".
               "-b \"$param{blocksize}\" $args " . 
               "-t $param{threads} | sox -t ". 
               "wav  -r 44100 -c 2 -w - -t ossdsp /dev/dsp";
        }
    } elsif ( $ext eq "mp3" ) 
    {
        if ( $mpg123 ne "" )
        {
            $cmd = "lors_launch lors_download \"$inputfile\" ".
                   "-M \"$param{blocksize}\" -b \"$param{blocksize}\" ".
                   "-t $param{threads} $args ".
                   "| $mpg123 -"; 
        } else {
            $cmd = "lors_launch lors_download \"$inputfile\" ".
                   "-b \"$param{blocksize}\" " . 
                   "-t $param{threads} $args -o \"$outputfile\"";
        }

    } elsif ( $ext eq "mpg" || $ext eq "mpeg" || $ext eq "avi" || 
              $ext eq "mov" || $ext eq "mp4") 
    {
        if ($mplayer ne "") {
            $cmd = "lors_launch lors_download \"$inputfile\" ".
                   "-t $param{threads}  ". 
                   "-b \"$param{blocksize}\" ".
                   "$args | ".
                   "$mplayer -cache 1024 -framedrop - &> /dev/null";
        } else {
            $cmd = "lors_launch lors_download \"$inputfile\" ".
                   "-b \"$param{blocksize}\" " . 
                   "-t $param{threads} $args -o \"$outputfile\"";
        }
    }
    print "CMD: $cmd\n";
    open(CYG, ">>log.txt");
    print CYG "$cmd\n";
    close (CYG);

    #`$cmd `;
    open(CMD, "$cmd |")|| report_error_viz "Executable 'lors_download' not found in PATH";;
    while( <CMD> ) { print; }
}

