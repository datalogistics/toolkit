#!/usr/local/bin/perl

## USAGE: 
##
## $Id: tNGC.pl,v 1.4 2001/09/25 19:17:01 atchley Exp $
##
## AUTHOR: Jim Donohoe, CAIDA
## <LICENSE>

my $VERSION = 1.0;

use strict;
use lib "/neon/homes/atchley/projects/lbone/server/src/ngc/CAIDA/";
use CAIDA::NetGeoClient;

my $netgeo = new CAIDA::NetGeoClient;

#testRecord( 1 );

#testCountry( "caida.org" );

#testLatLong( "caida.org" );

#test_getCountryArray( [ 1, 2, 3, "caida.org", "ripe.net", "apnic.net" ] );
#test_getLatLongArray( [ 1, 2, 3, "caida.org", "ripe.net", "apnic.net" ] );

if( 1 )
{
   my @hashArray = ();
   my $startNumber = 13820;
   for( my $i = 0; $i < 20; $i++ )
   {
      my $target = "AS" . ($startNumber + $i); 
      push( @hashArray, { TARGET => $target } );
   }

   my $nonblocking = "true";
   test_updateRecordArray( \@hashArray, $nonblocking );
   @hashArray = ();
   for( my $i = 0; $i < 20; $i++ )
   {
      my $target = "AS" . ($startNumber + $i); 
      push( @hashArray, { TARGET => $target } );
   }
   test_updateRecordArray( \@hashArray, $nonblocking );
}

if( 0 )
{
   test_updateLatLongArray( [ { TARGET => 1 },  { TARGET => 2 },
			   { TARGET => 2 }, { TARGET => "caida.org" },
	    { TARGET => "ripe.net" }, { TARGET => "apnic.net" } ] );
}


sub testRecord
{
   my( $number ) = @_;

   if( 0 )
   {
      print "=========================================================\n";
      print "Testing getRecord( $number )\n";
   }

   if( 0 )
   {
      print "Hit <enter> to begin ...\n";
      my $c = undef;
      while( not defined( $c = getc( STDIN ) ) ) {};

      if( $c eq "q" )
      {
	 exit( 0 );
      }
   }

   my $recordRef = $netgeo->getRecord( $number );

   if( ref( $recordRef ) ne "HASH" )
   {
      print "$recordRef\n";
      return;
   }

   if( 0 )
   {
      print "\nRECORD for $number\n";
      print "$recordRef->{WHOIS_RECORD}\n----------------------------\n";
   }

   if( 1 )
   {
      my $key;
      foreach $key ( keys %$recordRef )
      {
	 if( $key ne "WHOIS_RECORD" )
	 {
	    print "$key: $recordRef->{$key}\n";
	 }
      }
      print "\n";
   }
}


sub testCountry
{
   my( $number ) = @_;

   print "=========================================================\n";
   print "Testing getCountry( $number )\n";

   my $country = $netgeo->getCountry( $number );

   print "\nCOUNTRY for $number = $country\n";
   
}

sub testLatLong
{
   my( $number ) = @_;

   print "=========================================================\n";
   print "Testing getLatLong( $number )\n";

   my $result = $netgeo->getLatLong( $number );

   if( not defined( $result ) )
   {

   }
   elsif( ref( $result ) eq "HASH" )
   {
      if( defined( $result->{LAT} ) )
      {
	 print "Lat/Long for $number = ($result->{LAT},$result->{LONG}), " .
	 "Granularity = $result->{LAT_LONG_GRAN}\n";
      }
      else
      {
	 my $key;
	 foreach $key ( keys %$result )
	 {
	    my $value = $result->{ $key };
	    print "$key: $value\n";
	 }
      }
   }
   else
   {
      print "$result\n";
   }
   
}

sub test_getCountryArray
{
   my( $stringArrayRef ) = @_;

   print "\nTesting getCountryArray\n";
   
   my $arrayRef = $netgeo->getCountryArray( $stringArrayRef );

   my $recordRef;
   foreach $recordRef ( @$arrayRef )
   {
      if( $recordRef->{STATUS} eq "OK" )
      {
	 print "Country for $recordRef->{TARGET} = $recordRef->{COUNTRY}\n";
      }
   }
}


sub test_getLatLongArray
{
   my( @myLboneClientInput ) = @_;
   my( @myTestInputArray ) = [ @myLboneClientInput ];
   
   my( $stringArrayRef ) = @myTestInputArray;

#   print "\nTesting getLatLongArray\n";

   my $netgeo = new CAIDA::NetGeoClient;
   
   my $arrayRef = $netgeo->getLatLongArray( $stringArrayRef );

   my $recordRef;
   my @latlongInfos;
   foreach $recordRef ( @$arrayRef )
   {
      if( $recordRef->{STATUS} eq "OK" )
      {	
	push( @latlongInfos, $recordRef->{TARGET} );
	push( @latlongInfos, $recordRef->{LAT} );
	push( @latlongInfos, $recordRef->{LONG} );
	
#	print "Lat/Long for $recordRef->{TARGET} = " .
#	"($recordRef->{LAT},$recordRef->{LONG})\n";
      }
   }
   
   return @latlongInfos;
}

sub test_updateLatLongArray
{
   my( $hashArrayRef ) = @_;

   print "\nTesting updateLatLongArray\n";
   
   $netgeo->updateLatLongArray( $hashArrayRef );

   my $recordRef;
   foreach $recordRef ( @$hashArrayRef )
   {
      if( $recordRef->{STATUS} eq "OK" )
      {
	 print "Lat/Long for $recordRef->{TARGET} = " .
	 "($recordRef->{LAT},$recordRef->{LONG}), " .
	 "Gran = $recordRef->{LAT_LONG_GRAN}\n";
      }
   }
}


sub test_updateRecordArray
{
   my( $hashArrayRef, $nonblocking ) = @_;

   print "\nTesting updateRecordArray\n";

   $netgeo->setNonblocking( $nonblocking );
   my $result = $netgeo->updateRecordArray( $hashArrayRef );
   if( $result ne "OK" )
   {
      print "\n===========$result\n===========\n";
   }
   my $count = countCompletedLookups( $hashArrayRef );

   my $hashArrayLength = scalar( @$hashArrayRef );

   while( $count < $hashArrayLength )
   {
      $result = $netgeo->updateRecordArray( $hashArrayRef );
      if( $result ne "OK" )
      {
	 print "\n===========$result\n===========\n";
      }
      $count = countCompletedLookups( $hashArrayRef );
   }
}


sub countCompletedLookups
{
   my( $hashArrayRef ) = @_;

   my $count = 0;
   my $hashArrayLength = scalar( @$hashArrayRef );
   my $hash;
   foreach $hash ( @$hashArrayRef )
   {
      my $status = $hash->{ STATUS };
      if( defined( $status ) and 
	  ( $status eq "OK" or
	    $status eq "NO_MATCH" or
	    $status eq "NO_COUNTRY" or
	    $status eq "UNKNOWN" ) )
      {
	 $count += 1;
      }
   }
   print "$count out of $hashArrayLength lookups completed.\n";

   return $count;
}


sub lbone_getLatLongInfo
{
#  my ( @myLboneClientInput ) = ("192.172.226.30", "caida.org", "ripe.net", "apnic.net", "utk.edu"); 

  my ( @myLboneClientInput ) = @_;
  my ( @myTestInputArray ) = [ @myLboneClientInput ];
  
  my ( @latlongInfos ) = test_getLatLongArray( @myTestInputArray );
  
  return @latlongInfos;
}


sub showtime {
  print time, "\n";
}


my ( @myTestInputArray ) = ("192.172.226.30", "caida.org", "ripe.net", "apnic.net", "utk.edu");
my ( @latlongInfos ) = lbone_getLatLongInfo ( @myTestInputArray );

my $recordItem;
foreach $recordItem( @latlongInfos ) {
  print $recordItem, "\n";
}
