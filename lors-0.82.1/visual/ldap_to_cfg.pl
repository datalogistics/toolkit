#!/usr/bin/perl

sub get_rgb
{
    # step, range, mult
    my ($r, $g,  $b) = @_;
    $rr = sprintf "%02x", $r;
    $gg = sprintf "%02x", $g;
    $bb = sprintf "%02x", $b;
    $color = $rr . $gg . $bb;
    return $color;
}
sub get_fname
{
    my ($fname) = @_;
    @parts = split('/', $fname);
    $name = $parts[-1];
    return $name
}
# rr gg bb 
# 
# r  g+ 0
# r- g  0
# 0  g  b+
# 0  g- b
# r+ 0  b
# r  0  b-
# number of depots. (N)
# step = 16M / N
# calc color at each step. 
#
# There are now 6 steps of color.
# 1 0 1  purple
# 0 0 1  blue
# 0 1 1  cyan
# 0 1 0  green
# 1 1 0  yellow
# 1 0 0  red
#
# there are 10 good colors in each range.
#   it begins with a black tone and moves to white.
#   more of less steps may be placed over this sub-spectrum as dictated by
#   the value of 'range'.
#  The first half of the range is distributed over the 'dark' region.
#  The second half is over the 'light' region.
#  Determine region by the scale.
sub step_to_color {
    my ($step) = @_;
    # find range for step
    $range = 2796202;
    $mult = int ($step/ $range) + 1;
    # if ( ($step >= 0) && ($step < $range) )
    if ( $mult == 2  ) 
    {
        # 0 0 1
        $scale = ($step - $range*($mult-1))/$range;
        if ( $scale < 0.5 )
        {
            $color = get_rgb (0, 0, $scale*128+127);
        } else 
        {
            $r = ($scale-0.5)/0.5 *180;
            $color = get_rgb ($r, $r, 255);
        }
    } elsif ( $mult == 4 )
    {
        # 0 1 0
        $scale = ($step - $range*($mult-1))/$range;
        if ( $scale < 0.5 )
        {
            $color = get_rgb (0, $scale*128+127, 0);
        } else 
        {
            $r = ($scale-0.5)/0.5 *180;
            $color = get_rgb ($r, 255, $r);
        }
    } elsif ( $mult == 3 )
    {
        # 0 1 1 
        $scale = ($step - $range*($mult-1))/$range;
        if ( $scale < 0.5 )
        {
            $color = get_rgb (0, $scale*128+127, $scale*128+127);
        } else {
            $r = ($scale-0.5)/0.5 *180;
            $color = get_rgb ($r, 255, 255);
        }
    } elsif ( $mult == 6)
    {
        # 1 0 0
        $scale = ($step - $range*($mult-1))/$range;
        if ( $scale < 0.5 )
        {
            $color = get_rgb ($scale*128+127, 0, 0);
        } else 
        {
            $r = ($scale-0.5)/0.5 *180;
            $color = get_rgb (255, $r, $r);
        }
    } elsif ( $mult == 1 )
    {
        # 1 0 1
        $scale = ($step - $range*($mult-1))/$range;
        if ( $scale < 0.5 )
        {
            $color = get_rgb ($scale*128+127, 0, $scale*128+127);
        } else 
        {
            $r = ($scale-0.5)/0.5 *180;
            $color = get_rgb (255, $r, 255);
        }
    } elsif ( $mult == 5 )
    {
        # 1 1 0
        $scale = ($step - $range*($mult-1))/$range;
        if ( $scale < 0.5 )
        {
            $color = get_rgb ($scale*128+127, $scale*128+127, 0);
        } else 
        {
            $r = ($scale-0.5)/0.5 *180;
            $color = get_rgb (255, 255, $r);
        }
    }
    return $color;
}


my (%depot);

$i = 0;
$conf{name} = 0;
$conf{input} = "-";
$conf{image} = "newusa_55_-130_25_-60_.gif";
foreach $arg (@ARGV)
{
    if ( $arg eq "-name" )
    {
        $conf{name} = 1;
    }
    if ( $arg eq "-i" )
    {
        $conf{input} = $ARGV[$i+1];
    }
    if ( $arg eq "-image" )
    {
        $conf{image} = $ARGV[$i+1];
    }
    $i++;
}
if ( $conf{input} && ( -f $conf{image} ) )
{
    open(DATAFILE, "$conf{input}");
    $image = $conf{image};
} else {
    print "Please specify the output from ldapsearch for depot list\n";
    exit;
}

$STEPS = 16777216;

my ($x, $host);
my $hostcount = -1;
my %dn = undef;

while (<DATAFILE>)
{
    if ( m/^dn:/ ) 
    {
        if ( %dn ne undef )
        {
            $host = $dn{host};
            $depot{$host}{lat} = $dn{lat};
            $depot{$host}{lon} = $dn{lon};
            push @{$depot{$host}{port}}, $dn{port};
            $depot{$dn{host}}{domain} = $dn{domain};
        }
        %dn = {};
    }
    if ( m/hostname:/ )
    {
        ($x, $host) = split;
        $dn{host} = $host;
        @domain = split(/\./, $host);
        # print "HOSTNAME: $host\n";
        # print "DOMAIN: $domain[-2]\n";
        if ( length $domain[-1] == 2 )
        {
            # In this case it is a foreign country with tld of two letters.
            # avoid this and find the real thing.
            if ( $domain[-1] eq "ca" || 
                 $domain[-1] eq "it" ||
                 $domain[-1] eq "th" )
            {
                $dn{domain} = $domain[-2];
            } else {
                $dn{domain} = $domain[-3];
            }
        } elsif ( $domain[-2] eq "intel-research" )
        {
            $dn{domain} = $domain[-3];
        } else 
        {
            $dn{domain} = $domain[-2];
        }
    } 
    if ( m/^port:/ ){
        ($x, $port) = split;
        $dn{port} = $port;
    }
    if ( m/lat:/ ){
        ($x, $lat) = split;
        $dn{lat} = $lat;
    }
    if ( m/lon:/ ){
        ($x, $lon) = split;
        $dn{lon} = $lon;
    }
}
# Handle the very last dn group.  Otherwise the final entry would be lost.
if ( %dn ne undef )
{
    $host = $dn{host};
    $depot{$host}{lat} = $dn{lat};
    $depot{$host}{lon} = $dn{lon};
    push @{$depot{$host}{port}}, $dn{port};
    $depot{$dn{host}}{domain} = $dn{domain};
}


# switch hostname hash into a Domain index.  Thereby sorting by domain.
my %dom;
foreach $host (keys %depot)
{
    $d = $depot{$host}{domain};
    push @{$dom{$d}}, $host;
}

# switch hostname hash into a longitude index.  this sorts by longitude, and
# will allow assignment of color based on longitude.
foreach $host ( keys %depot)
{
    if ( $host ne "" )
    {
        $lon = $depot{$host}{lon};
        push @{$longitudes{$lon}}, $host;
        $hostcount++;
    }
}
$substep =  $STEPS/$hostcount;
$color_position = 0;
foreach $lon ( sort { $a <=> $b } keys %longitudes )
{
    foreach $host ( @{$longitudes{$lon}} )
    {
        $color = step_to_color $color_position;
        $color_position += $substep;
        $depot{$host}{color} = "#$color";
    }
}

print "IMAGE $image\n";
print "GRID 100 100\n";
print "PORT 5240\n";
print "NAMES $conf{name}\n";
print "DEPOTSIZE 12\n";
print "ARROWWIDTH 2\n";
print "FONTCOLOR gray70\n";
$fname = get_fname($image);
@coord = split(/_/, $fname);
$lat1 = $coord[1];
$lon1 = $coord[2];
$lat2 = $coord[3];
$lon2 = $coord[4];
print "KEYPOINT1 $lat1 $lon1\n";
print "KEYPOINT2 $lat2 $lon2\n";
#print "KEYPOINT1 54 -148\n";
#print "KEYPOINT2 15 -35\n";
#print "KEYPOINT1 72 -132\n";
#print "KEYPOINT2 15 30\n";
#print "KEYPOINT1 85 -180\n";
#print "KEYPOINT2 -55 180\n";

print <<EOF;
# violet
SCALE 0 9000d3
SCALE 1 7000ff
# blue
SCALE 2 0000ff
SCALE 3 0086de
# green
SCALE 4 00ff00
SCALE 5 78ff11
# yellow
SCALE 6 ffff00
SCALE 7 ffd700
# orange
SCALE 8 ffa300
SCALE 9 ff5d00
# red
SCALE 10 ff4000
SCALE 11 ff0000
EOF

$x = 1;
$y = 1;
foreach $domain ( keys %dom )
{
    $i = 0;
    foreach $host ( @{$dom{$domain}} )
    {
        if ( $host ne "" ) 
        {
            if ( $i == 0 )
            {
                print "GROUP $depot{$host}{lat} $depot{$host}{lon} $x $y $domain\n";
                $x++;
                $y++;
                $i++;
            }
            foreach $port ( @{$depot{$host}{port}} )
            {
                print "DEPOT a $host $port $depot{$host}{color}\n";
            }
        }
    }
    print "ENDGROUP\n\n";
}

print "GROUP -180 -180 -1 -1 Other\n";
print "DEPOT a unknown 1 gray95\n";
print "ENDGROUP\n";



