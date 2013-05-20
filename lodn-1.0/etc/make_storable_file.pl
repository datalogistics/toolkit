#!/usr/bin/perl -w

use strict;
use Storable qw(store retrieve);

# Empty hash
my %table;

# Checks that a file to create was given
if(@ARGV != 1)
{
   print STDERR "Usage Error: $0 storage-file\n";
   exit 1;
}

# Gets the file to create
my $file = $ARGV[0];

# Creates the storage file
store(\%table, $file) or die "Can't create $file;";
