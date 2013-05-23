#!/usr/local/bin/perl5

## $Id: ngc.pl,v 1.1 2001/10/19 21:04:27 atchley Exp $
## $Name:  $
##
## Copyright 1999 The Regents of the University of California 
## All Rights Reserved 
## 
## Permission to use, copy, modify and distribute any part of this
## NetGeoClient software package for educational, research and non-profit
## purposes, without fee, and without a written agreement is hereby
## granted, provided that the above copyright notice, this paragraph and
## the following paragraphs appear in all copies.
## 
## Those desiring to incorporate this into commercial products or use
## for commercial purposes should contact the Technology Transfer
## Office, University of California, San Diego, 9500 Gilman Drive, La
## Jolla, CA 92093-0910, Ph: (858) 534-5815, FAX: (858) 534-7345.
## 
## IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY
## PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
## DAMAGES, INCLUDING LOST PROFITS, ARISING OUT OF THE USE OF THIS
## SOFTWARE, EVEN IF THE UNIVERSITY OF CALIFORNIA HAS BEEN ADVISED OF
## THE POSSIBILITY OF SUCH DAMAGE.
## 
## THE SOFTWARE PROVIDED HEREIN IS ON AN "AS IS" BASIS, AND THE
## UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO PROVIDE MAINTENANCE,
## SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS. THE UNIVERSITY
## OF CALIFORNIA MAKES NO REPRESENTATIONS AND EXTENDS NO WARRANTIES
## OF ANY KIND, EITHER IMPLIED OR EXPRESS, INCLUDING, BUT NOT LIMITED
## TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A
## PARTICULAR PURPOSE, OR THAT THE USE OF THE SOFTWARE WILL NOT INFRINGE
## ANY PATENT, TRADEMARK OR OTHER RIGHTS.
## 
## The NetGeoClient software package is developed by the NetGeo development
## team at the University of California, San Diego under the Cooperative
## Association for Internet Data Analysis (CAIDA) Program.  Support for
## this effort is provided by NSF grant ANI-9996248.  Additional sponsors
## include APNIC, ARIN, GETTY, NSI and RIPE NCC.
## 
## Report bugs and suggestions to netgeo-bugs@caida.org.  For more
## information and documentation visit: http://www.caida.org/Tools/NetGeo/.

## AUTHOR: Jim Donohoe, CAIDA

package CAIDA::NetGeoClient;

$VERSION = "1.1";

$cvs_Id = '$Id: ngc.pl,v 1.1 2001/10/19 21:04:27 atchley Exp $';
$cvs_Author = '$Author: atchley $';
$cvs_Name = '$Name:  $';
$cvs_Revision = '$Revision: 1.1 $';

require Exporter;

@ISA = qw( Exporter );
@EXPORT = qw();
@EXPORT_OK = qw( $OK $LOOKUP_IN_PROGRESS $NO_MATCH $NO_COUNTRY $UNKNOWN 
		 $WHOIS_ERROR $HTTP_ERROR $INPUT_ERROR $WHOIS_TIMEOUT
	         $NETGEO_LIMIT_EXCEEDED $CITY_GRAN $STATE_GRAN $COUNTRY_GRAN
		 $DEFAULT_TIMEOUT $DEFAULT_SERVER_URL 
		 $ARRAY_LENGTH_LIMIT
	       );

use strict;
use LWP::UserAgent;

use vars qw( $VERSION );
use vars qw( $debug_NetGeo );

# Status strings returned by NetGeoClient and NetGeo.  For getRecord and
# getLatLong queries the status string will be returned in the STATUS
# field of a hash.  For getCountry queries the status string will be 
# returned in place of the country string.  For all array queries the
# status of each element of the array will be stored in the STATUS field
# of each element.
use vars qw( $OK $LOOKUP_IN_PROGRESS $NO_MATCH $NO_COUNTRY $UNKNOWN
	     $WHOIS_ERROR $HTTP_ERROR $INPUT_ERROR $WHOIS_TIMEOUT 
	     $NETGEO_LIMIT_EXCEEDED $CITY_GRAN $STATE_GRAN $COUNTRY_GRAN
             $DEFAULT_TIMEOUT $DEFAULT_SERVER_URL $ARRAY_LENGTH_LIMIT );

# Status string returned when a lookup is successful and the results are
# available in the returned data structure.
$OK = "OK";

# Status string returned when a lookup is in progress on a separate
# thread.  When an array query is performed and nonblocking is specified
# the targets that are not found in the database will be looked up on
# a separate thread and stored into the database, so they will be ready
# for a subsequent query.
$LOOKUP_IN_PROGRESS = "Lookup In Progress";

# Status string returned when the target is not found in any whois
# database.
$NO_MATCH = "No Match";

# Status string returned when a whois record is found and parsed but the
# country name cannot be parsed or guessed from the record.  This may be
# due to the record containing a non-standard spelling for the country
# or perhaps the record doesn't contain any location information.  This
# status string is only returned in getCountry, getCountryArray, and
# updateCountryArray queries.
$NO_COUNTRY = "No Country";

# Status string returned for a latitude/longitude query when there is
# a failure to match the target in the whois databases or when the
# address can't be parsed, or when the latitude/longitude for a parsed
# address is unknown.
$UNKNOWN = "Unknown";

# Status string returned when a whois server is unavailable.  The status
# string will consist of WHOIS_ERROR followed by the name of the whois
# server that couldn't be reached, e.g. "WHOIS_ERROR: whois.internic.net". 
# If a whois error occurs (usually server temporarily down) an error string 
# is returned in the STATUS field of hash returned by getRecord or any array 
# method, or as the string result of getCountry. 
$WHOIS_ERROR = "WHOIS_ERROR";

# Status string returned when the NetGeo server is unavailable or some
# other error occurs during the http session between the NetGeo client
# and server.  If there is an error connecting to the NetGeo server the 
# error string will consist of HTTP_ERROR followed by the HTTP error code 
# and description, e.g., "HTTP_ERROR: 404 File Not Found".
$HTTP_ERROR = "HTTP_ERROR";

# Status string returned when the input target is not in a valid format.
# An AS number target may be a string or number, with or without a leading
# "AS", e.g., 123, "123", "AS 123", or "AS123".  A domain name target must
# be a string, e.g. "caida.org".  An IP address target must be a string
# in dotted decimal format, e.g., "192.172.226.77".
$INPUT_ERROR = "INPUT_ERROR";

# Status string returned when a whois lookup is stopped by the NetGeo 
# server because the lookup has exceeded a time limit.  The status
# string will consist of WHOIS_TIMEOUT followed by the name of the whois 
# server that was being queried when the search timed out, e.g. 
# "WHOIS_TIMEOUT: whois.internic.net".  Note: the server that exceeds the
# time limit is not always the cause of the delay.  Some targets require
# lookups on different servers (e.g., whois.arin.net then whois.ripe.net) 
# and a slow lookup on the first server may consume nearly the whole time 
# limit, leading to a timeout during the second lookup.
$WHOIS_TIMEOUT = "WHOIS_TIMEOUT";

# Status string returned when the user exceeds the rate limit set by the
# NetGeo server.  All subsequent requests from the user will be rejected
# for a short time period (e.g., 30s or 1 minute) starting from the most
# recent rejected query--polling the NetGeo server will cause the user to
# be locked out for a longer time.
$NETGEO_LIMIT_EXCEEDED = "NETGEO_LIMIT_EXCEEDED";

# When polling the database using the updateXxxArray methods, the
# error codes ($WHOIS_ERROR, $HTTP_ERROR, $NETGEO_LIMIT_EXCEEDED and
# $INPUT_ERROR) must be cleared from the status field before
# attempting a new lookup.

# Maximum number of entries allowed in an array argument.  This limit is 
# enforced in order to limit the length of the query string sent from the
# client to the server and to limit the length of the response from the
# server.  The array length will also be tested and enforced at the server. 
$ARRAY_LENGTH_LIMIT = 100;

# Values which may be returned in the LAT_LONG_GRAN field.  The LAT_LONG_GRAN
# field may be empty if lat/long = (0,0), which indicates some kind of error
# or timeout condition.  A value of $CITY_GRAN indicates the lat/long was
# found from a lookup of the city parsed from the whois address.  A value of
# $STATE_GRAN means the lat/long was found from a lookup of the state or
# province, and a value of $COUNTRY_GRAN means the lat/long was found from a
# lookup of the country.
$CITY_GRAN = "City";
$STATE_GRAN = "State/Prov";
$COUNTRY_GRAN = "Country";

# Maximum length of time, in seconds, which will be allowed during a whois
# lookup by the NetGeo server.  The actual default value is maintained by
# the server.
$DEFAULT_TIMEOUT = 60;

# URL for the normal NetGeo server.
$DEFAULT_SERVER_URL = "http://netgeo.caida.org/perl/netgeo.cgi";

# String returned by ref operator when arg is a scalar. ref returns
# "SCALAR" when arg is a reference to a scalar, empty string when arg
# is a scalar.
my $SCALAR_TYPE = "";
my $DEBUG = 0;

# Lookup targets
# 1. AS Number
# An AS number can be given in several ways: as a number or string of 
# numerals, with or without a leading "AS".  For example, the following
# are all equivalent:
# getRecord( 1234 ), getRecord( "1234" )
# getRecord( "AS1234" ), getRecord( "AS 1234" )
# 
# 2. IP Address
# An IP address must be given as a string in dotted decimal format, e.g.,
# "192.172.226.30".
#
# 3. Domain Name must be given as a string, e.g., "caida.org".


#---------------------------------------------------------------------------
#                     constructor and utility methods
#---------------------------------------------------------------------------

#--------------------------------------------------------------------------
# $applicationName - optional argument, should be the name of the
#     application using NetGeoClient, e.g., make_AS_graph/1.2.3
# $alternateServerUrl - optional argument, used to specify a server other
#     than the default NetGeo server. Most users should use the default.
# USAGE: $netgeo = new CAIDA::NetGeoClient( "make_AS_graph/1.2.3" );

sub new
{
   my( $this, $applicationName, $alternateServerUrl ) = @_;

   my $url = $DEFAULT_SERVER_URL;
   if( defined( $alternateServerUrl ) )
   {
      $url = $alternateServerUrl;
   }

   if( $DEBUG )
   {
      print "Creating new NetGeoClient object\n";
      print "URL = $url\n";
   }

   my $userAgent = new LWP::UserAgent;

   # Build up the agent name that will be sent to the NetGeo server, for
   # example: "make_AS_graph/1.2.3 NetGeoClient/1.0 libwww-perl/5.42".
   # The app name and version are optional, use $0 if the user doesn't supply
   # any value for app name.
   my $agentName = "NetGeoClient/$VERSION " . $userAgent->agent;

   # Trim any leading or trailing spaces from the app name.
   if( defined( $applicationName ) and $applicationName =~ /^\s*(.*\S)/ )
   {  
      $applicationName = $1;
   }

   # Include the application name if one was provided, otherwise include the 
   # name of the invoker of this module.
   if( defined( $applicationName ) and length( $applicationName ) > 0 )
   {
      $agentName = "$applicationName $agentName";
   }
   else
   {
      $agentName = "$0 $agentName";
   }

   # Store the agent name into the user agent.
   $userAgent->agent( $agentName );

   # Initialize the object. Note that the local cache is enabled by default.
   return bless { USER_AGENT => $userAgent, NETGEO_URL => $url,
		  TIMEOUT => $DEFAULT_TIMEOUT, NONBLOCKING => "false",
		  LOCAL_CACHE => {} }, $this;
}


#--------------------------------------------------------------------------
# Specify the maximum time each whois lookup is allowed to consume before
# returning.  For the client, this timeout is advisory only and may be
# overridden by the server.
sub setTimeout
{
   my( $this, $seconds ) = @_;

   # Check the nonblocking flag, the timeout and nonblocking are mutually
   # exclusive.
   if( $this->{ NONBLOCKING } eq "true" )
   {
      _printWarning( "Setting NONBLOCKING to false. NONBLOCKING and " .
		     "TIMEOUT are mutually exclusive.", "setTimeout" );
      $this->{ NONBLOCKING } = "false";  
   }

   if( defined( $seconds ) and $seconds > 0 )
   {
      $this->{ TIMEOUT } = $seconds;
   }
   else
   {
      _printError( "Argument to setTimeout must be a number > 0", 
		   "setTimeout" );
   }
}


#--------------------------------------------------------------------------
# Specify whether or not any whois lookups should be nonblocking.
sub setNonblocking
{
   my( $this, $trueOrFalse ) = @_;

   if( not defined( $trueOrFalse ) or $trueOrFalse =~ /t(rue)?/i or
       $trueOrFalse == 1 )
   {
      $this->{ NONBLOCKING } = "true";

      # Check the timeout value to see if it's been changed from the default, 
      # the timeout and nonblocking are mutually exclusive.
      if( $this->{ TIMEOUT } != $DEFAULT_TIMEOUT )
      {
	 _printWarning( "NONBLOCKING and TIMEOUT are mutually exclusive." .
			"Queries will now be NONBLOCKING.", "setTimeout" );
      }
   }
   elsif( defined( $trueOrFalse ) and ( $trueOrFalse =~ /f(alse)?/i or
					$trueOrFalse == 0 ) )
   {
      $this->{ NONBLOCKING } = "false";
   }
}


#--------------------------------------------------------------------------
sub DESTROY
{
   my( $this ) = @_;
   
   # If there is a local cache in memory, remove the local cache to free up
   # the memory.
   if( defined( $this->{ LOCAL_CACHE } ) )
   {
      $this->{ LOCAL_CACHE } = undef;
   }
}


#---------------------------------------------------------------------------
#                          local cache methods
#---------------------------------------------------------------------------


#--------------------------------------------------------------------------
# $trueOrFalse is an optional arg.  useLocalCache( "true" ) is equivalent
# to useLocalCache(), both turn on the local caching. useLocalCache( "false" )
# turns off local caching and clears the existing cache.
sub useLocalCache
{
   my( $this, $trueOrFalse ) = @_;

   if( not defined( $trueOrFalse ) or $trueOrFalse =~ /t(rue)?/i or
       $trueOrFalse == 1 )
   {
      # Check for an existing local cache, make sure we don't clobber an
      # existing cache.
      if( not defined( $this->{ LOCAL_CACHE } ) )
      {
	 # Initialize the local cache with an empty hash.
	 $this->{ LOCAL_CACHE } = {};
      }
   }
   elsif( defined( $trueOrFalse ) and ( $trueOrFalse =~ /f(alse)?/i or
					$trueOrFalse == 0 ) )
   {
      if( defined( $this->{ LOCAL_CACHE } ) )
      {
	 # Remove the local cache.
	 $this->{ LOCAL_CACHE } = undef;
      }
   }
}


#--------------------------------------------------------------------------
sub clearLocalCache
{
   my( $this ) = @_;

   if( defined( $this->{ LOCAL_CACHE } ) )
   {
      # Re-initialize the local cache with an empty hash.
      $this->{ LOCAL_CACHE } = {};
   }
   else
   {
      _printWarning( "Local cache not in use", "clearLocalCache" );
   }
}


#--------------------------------------------------------------------------
sub clearLocalCacheEntry
{
   my( $this, $target ) = @_;

   if( defined( $this->{ LOCAL_CACHE } ) )
   {
      my $localCache = $this->{ LOCAL_CACHE };

      # Standardize the target string.  This will return null if the
      # input is not in a valid format.  To use the verifyInputFormat
      # code we need to put the target into a hashtable.  
      my $hashRef = { TARGET => $target };
      _verifyInputFormat( "clearLocalCacheEntry", $hashRef );
      
      # Get the standardized string from the hash.
      $target = $hashRef->{ STANDARDIZED_TARGET };

      if( defined( $target ) and defined( $localCache->{ $target } ) )
      {
	 delete( $localCache->{ $target } );
      }
   }
   else
   {
      _printWarning( "Local cache not in use", "clearLocalCacheEntry" );
   }
}


#---------------------------------------------------------------------------
#                             scalar methods
#---------------------------------------------------------------------------

# The input could be an IP address, a host name, or an AS number.  Returns
# a reference to a hash or undef if no match is found in the database or
# whois servers.  Returns undef if no match is found in the database and
# if the nonblocking was specified, so we can't do whois lookups.
sub getRecord
{
   my( $this, $target ) = @_;

   return $this->_execute( "getRecord", $target );
}


#--------------------------------------------------------------------------
# Returns the 2-letter ISO 3166 country code associated with the AS with the
# input number, if the AS information is stored in the database.  Returns 
# $NO_MATCH if the AS number has been looked up but nothing was found in the
# whois lookups.  Returns $NO_COUNTRY if the lookup returned a record but no
# country could be found.  Returns an empty string if nothing was found in
# the database but nonblocking was specified, so couldn't do a whois lookup.
sub getCountry
{
   my( $this, $target ) = @_;

   my $result = $this->_execute( "getCountry", $target );

   if( ref( $result ) eq "HASH" )
   {
      # Convert the result hash into a country code scalar.
      $result = $result->{ COUNTRY };
   }

   return $result;
}


#--------------------------------------------------------------------------
# Returns a hashtable with keys LAT, LONG, LAT_LONG_GRAN, and STATUS.
# Lat/Long will be (0,0) if the target has been looked up but there was no 
# match in the whois lookups, or if no address could be parsed from the whois
# record, or if the lat/long for the address is unknown.  Returns undef if 
# nothing was found in the database but nonblocking was specified, so couldn't
# do a whois lookup.
sub getLatLong
{
   my( $this, $target ) = @_;

   return $this->_execute( "getLatLong", $target );
}


#---------------------------------------------------------------------------
#                          getXxxArray methods
#---------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Convenience method that may be used on the first invocation to build up an
# array of records from an array of identifiers, and lookup the identifiers.
# Returns a reference to an array containing hash refs.
sub getRecordArray
{
   my( $this, $stringArrayRef ) = @_;

   my $hashArrayRef = _makeHashArray( "getRecordArray", $stringArrayRef );
   $this->_updateArray( "getRecordArray", $hashArrayRef );
   return $hashArrayRef;
}


#--------------------------------------------------------------------------
sub getCountryArray
{
   my( $this, $stringArrayRef ) = @_;

   my $hashArrayRef = _makeHashArray( "getCountryArray", $stringArrayRef );
   $this->_updateArray( "getCountryArray", $hashArrayRef );
   return $hashArrayRef;
}


#--------------------------------------------------------------------------
sub getLatLongArray
{
   my( $this, $stringArrayRef ) = @_;

   my $hashArrayRef = _makeHashArray( "getLatLongArray", $stringArrayRef );
   $this->_updateArray( "getLatLongArray", $hashArrayRef );
   return $hashArrayRef;
}

#---------------------------------------------------------------------------
#                         updateXxxArray methods
#---------------------------------------------------------------------------

#-------------------------------------------------------------------------- 
sub updateRecordArray
{
   my( $this, $hashArrayRef ) = @_;

   return $this->_updateArray( "updateRecordArray", $hashArrayRef );
}


#-------------------------------------------------------------------------- 
sub updateCountryArray
{
   my( $this, $hashArrayRef ) = @_;

   return $this->_updateArray( "updateCountryArray", $hashArrayRef );
}


#-------------------------------------------------------------------------- 
sub updateLatLongArray
{
   my( $this, $hashArrayRef ) = @_;

   return $this->_updateArray( "updateLatLongArray", $hashArrayRef );
}

#---------------------------------------------------------------------------
#                          utility methods
#---------------------------------------------------------------------------

# NOTE: These are included here to make the NetGeoClient.pm interface as
# similar as possible to the NetGeoClient.java interface.  No conversion is
# needed between string and float in Perl, so it's probably just as easy for 
# the user to extract lat and long directly from the hash.

sub getLat
{
   my( $latLongHashRef ) = @_;
   my $lat = 0.0;
   if( ref( $latLongHashRef ) eq "HASH" )
   {
      $lat = $latLongHashRef->{ LAT };
   }
   return $lat;
}

sub getLong
{
   my( $latLongHashRef ) = @_;
   my $long = 0.0;
   if( ref( $latLongHashRef ) eq "HASH" )
   {
      $long = $latLongHashRef->{ LONG };
   }
   return $long;
}


#============================================================================
#                           internal methods  
#============================================================================

#--------------------------------------------------------------------------
# Convert an array of scalar targets into an array of hashes, with each
# hash containing a TARGET key and a scalar target value.
sub _makeHashArray
{
   my( $methodName, $stringArrayRef ) = @_;
   my @hashArray = ();

   # Test the caller of this method, to make sure it was called internally.
   my $caller = caller;
   if( not defined( $caller ) or $caller ne "CAIDA::NetGeoClient" )
   {
      # This method must have been invoked directly by user code. Print
      # the error and return an empty hash array.
      _printExternalCallerError( "_makeHashArray" );
      return \@hashArray;
   }

   # Extract the scalar lookup targets from stringArrayRef and put them 
   # into hashes, add the new hashes to the hashArray.
   if( ref( $stringArrayRef ) eq "ARRAY" )
   {
      # Check the scalars in the array to make sure they're defined.  We'll
      # check their formats later, in _verifyInputFormat.
      for( my $i = 0; $i < scalar( @$stringArrayRef ); $i++ )
      {
	 my $string = $stringArrayRef->[ $i ];

	 # Make sure the array element is defined and is a string or int.
	 # ref returns the empty string when arg is a scalar.
	 if( defined( $string ) and ref( $string ) eq $SCALAR_TYPE )
	 {
	    push( @hashArray, { TARGET => $string } );
	 }
	 else
	 {
	    _printError( "Bad array element $i in argument to $methodName. " .
			 "Elements in array must be strings and/or ints", 
			 $methodName );
	 }
      }
   }
   else
   {
      _printError( "Bad argument to $methodName, should be an array of " . 
		   "scalar lookup targets", $methodName );
   }
   return \@hashArray;
}


#--------------------------------------------------------------------------
# Returns a single hash ref.  This method creates a single hashtable and
# stores it into a 1-element array, so that updateArray can be used to
# perform the query.
sub _execute
{
   my( $this, $methodName, $target ) = @_;

   my $hashArrayRef = [ { TARGET => $target } ];
   $this->_updateArray( $methodName, $hashArrayRef );

   return $hashArrayRef->[0];
}


#-------------------------------------------------------------------------- 
# Returns the status string $OK or an error string starting with "HTTP_ERROR"
# or "NETGEO_LIMIT_EXCEEDED".  If status string is $OK then some or all of 
# the hash records in the input array were updated in place during this 
# method's execution.  It status string is "NETGEO_LIMIT_EXCEEDED" there 
# may be some records at the front of the array which were updated.
sub _updateArray
{
   my( $this, $methodName, $inputArrayRef ) = @_;

   # Test the caller of this method, to make sure it was called internally.
   my $caller = caller;
   if( not defined( $caller ) or $caller ne "CAIDA::NetGeoClient" )
   {
      # This method must have been invoked directly by user code. 
      _printExternalCallerError( "_updateArray" );
      return;
   }
   my $returnString = $OK;

   # Test the target strings in the input array.  Any targets not in
   # an acceptable format will have their STATUS field set to INPUT_ERROR.
   # This method will also store the standardized target into the hashtable, 
   # for use as a key in the cache table. 
   $returnString = _verifyInputFormatArray( $methodName, $inputArrayRef );

   # Check for the error string sent back when the array length exceeds
   # $ARRAY_LENGTH_LIMIT.
   if( $returnString eq $INPUT_ERROR )
   {
      return $returnString;
   }

   # Check to see if we're using a local cache.
   my $localCacheRef = $this->{ LOCAL_CACHE };
   if( defined( $localCacheRef ) )
   {
      # We're using a local cache so try to lookup the target(s) in the
      # cache.  Any values found in the cache will be copied into the 
      # $target array.
      _lookupLocalCacheArray( $methodName, $inputArrayRef, $localCacheRef );
   }

   # Build a list of the target strings which need to be looked up, from
   # the hashes in the input array ref.  
   my $targetList = _makeList( $methodName, $inputArrayRef );

   my $resultArrayRef = [];
   if( defined( $targetList ) and length( $targetList ) > 0 )
   {
      # If we reach this point it means we didn't find everything in the 
      # cache so we need to do a lookup on the NetGeo server.  
      # _executeHttpRequest will make an HTTP request to the server and 
      # return the resulting text.
      my $text = $this->_executeHttpRequest( $methodName, $targetList );
      if( $text =~ /^$HTTP_ERROR/o )
      {
	 return $text;
      }
   
      if( $text =~ /$NETGEO_LIMIT_EXCEEDED/o )
      {
	 $returnString = $NETGEO_LIMIT_EXCEEDED;
      }

      # Parse the text from the HTTP response and into hashes. This will 
      # return  a ref to an array with 0 or more hashes.
      $resultArrayRef = _convertToHashArray( $text );
   }

   # Merge the results in the result array into the input array.
   _mergeArrays( $inputArrayRef, $resultArrayRef, $localCacheRef );

   if( defined( $localCacheRef ) )
   {
      # Store the new result(s) into the local cache.
      _storeLocalCacheArray( $resultArrayRef, $localCacheRef );
   }
   # Return the status string to the invoker.
   return $returnString;
}




#-------------------------------------------------------------------------
# Form an HTTP request and send it to the NetGeo server, then parse the
# resulting text into an array of hashes.  Returns a hash array ref.
sub _executeHttpRequest
{
   my( $this, $methodName, $targetList ) = @_;

   # Test the caller of this method, to make sure it was called internally.
   my $caller = caller;
   if( not defined( $caller ) or $caller ne "CAIDA::NetGeoClient" )
   {
      # This method must have been invoked directly by user code. 
      _printExternalCallerError( "_executeHttpRequest" );
      return undef;
   }

   # Convert the method name to the appropriate scalar method name.  For
   # example convert updateRecordArray to getRecord.
   $methodName =~ /^(?:get|update)(Record|Country|LatLong)(Array)?/;

   # Combine "get" with the requested lookup unit to get one of the
   # scalar method names, e.g., getRecord.
   $methodName = "get" . $1;

   # Form the list of name/value pairs to be supplied with the POST statement.
   my $parameterString = "method=$methodName&target=$targetList";

   # Set the timeout or nonblocking flag if it's been specified by the user.
   if( $this->{ NONBLOCKING } eq "true" )
   {
      $parameterString .= "&nonblocking=true";
   }
   elsif( $this->{ TIMEOUT } != $DEFAULT_TIMEOUT )
   {
      $parameterString .= "&timeout=$this->{ TIMEOUT }";
   }

   if( $DEBUG )
   {
      print "_executeHttpRequest: parameter string: $parameterString\n";
   }

   # Build a request object and fill in the fields.
   my $httpRequest = new HTTP::Request( POST => $this->{ NETGEO_URL } );
   $httpRequest->content_type( "application/x-www-form-urlencoded" );
   $httpRequest->content( $parameterString );

   # Send the HTTP request to the server, store the result into a response
   # object.
   my $userAgent = $this->{ USER_AGENT };
   my $httpResponse = $userAgent->request( $httpRequest );

   my $text = "";
   if ( $httpResponse->is_success ) 
   {
      $text = $httpResponse->content;
   } 
   else 
   {
      # The HTTP request was not successful, the server is down or there was 
      # an error in the server or in this client code.  Return the status
      # string with key HTTP_ERROR.  The string will be in the format: 
      # <error code> <message>, according to the HTTP::Request documentation.
      $text = $HTTP_ERROR . $httpResponse->status_line;
   }
   return $text;
}


#-------------------------------------------------------------------------
# Merge the new results into the original input array.
sub _mergeArrays
{
   my( $inputArrayRef, $resultArrayRef, $localCacheRef ) = @_;
   
   # Test the caller of this method, to make sure it was called internally.
   my $caller = caller;
   if( not defined( $caller ) or $caller ne "CAIDA::NetGeoClient" )
   {
      # This method must have been invoked directly by user code. 
      _printExternalCallerError( "_mergeArrays" );
      return;
   }

   # Copy any new results from the result array to the input array.  The
   # number of entries in the two arrays may be different, but the relative
   # order of elements will be the same.
   # Example: Assume that records A, D, and E have already been found in an
   # earlier lookup, so the identifer list would contain B, C, and F.  After
   # the current lookup the result array will contain hashes for B, C, and F.
   # 
   #                     $inputIndex
   #                          |
   #                          V
   #                    +---+---+---+---+---+---+
   # $inputArrayRef ->  | A | B | C | D | E | F |
   #                    +---+---+---+---+---+---+
   # 
   #                    +---+---+---+
   # $resultArrayRef -> | B | C | F |
   #                    +---+---+---+
   # 
   # The strings in the TARGET field of the result array are really the
   # standardized targets that were sent to the server.

   my $inputIndex = 0;
   my $inputArrayLength = scalar( @$inputArrayRef );

   # Loop through the result array, for each hash in the result array find
   # the corresponding hash in the input array and then update the slot in
   # the input array.
   my $resultHashRef;
   foreach $resultHashRef ( @$resultArrayRef )
   {
      # Extract the target string from the current result hash.  The
      # string stored here in the TARGET field is really the standardized
      # target that was sent to the server, not necessarily the same as
      # the original target in the TARGET field of the input array.
      my $resultTarget = $resultHashRef->{ TARGET };
      if( not defined( $resultTarget ) || $resultTarget =~ /^\s*$/o )
      {
	 _printWarning( "Null or empty target string in result array",
			"_mergeArrays" );
	 next;
      }

      # Find the matching target in the input array, starting from the
      # current input index.  Note that we compare the resultTarget
      # (which is standardized) against the value from the STANDARDIZED
      # target field of the input array.
      my $standardizedTarget =
            $inputArrayRef->[$inputIndex]->{ STANDARDIZED_TARGET };
      my $matchedResultTarget = 0;

      while( $inputIndex < $inputArrayLength )
      {
	 if( defined( $standardizedTarget ) &&
	     $resultTarget eq $standardizedTarget )
	 {
	    # We found the standardizedInputTarget matching the result.
	    $matchedResultTarget = 1;
               
	    # Get the original target from the input array, so we can
	    # copy it into the result array.  At this point the original
	    # target might be "caida.org" and the standardized target
	    # and result target would both be "CAIDA.ORG".
            my $originalTarget = $inputArrayRef->[$inputIndex]->{ TARGET };

	    # Replace the hash in the input array with the hash returned
	    # from the lookup.
	    $inputArrayRef->[$inputIndex] = $resultHashRef;

	    # If the standardized target is different from the original
	    # then we need to fix up the fields in the hashtable.
	    if( $originalTarget ne $standardizedTarget )
	    {
	       # Fix up the TARGET field in the result array.  The user 
	       # is expecting to find the original target in the TARGET
	       # field.
	       $inputArrayRef->[$inputIndex]->{ TARGET } = $originalTarget;
	    }

	    if( defined( $localCacheRef ) )
	    {
	       # Store the standardized value into the STANDARDIZED field
	       # in the result hashtable, so it can be stored into the 
	       # local cache correctly.  This is adding a field to the 
	       # $resultHashRef, which is the same hash referenced by 
	       # $inputArrayRef->[$inputIndex].
	       $resultHashRef->{ STANDARDIZED_TARGET } = $standardizedTarget;
	    }
               
	    # Advance to the next slot in the input array, no need to test 
	    # the current slot again.  The standardized target will be
	    # extracted from the new slot prior to the start of the while
	    # loop.
	    $inputIndex += 1;

	    # Break out of this while loop.
	    last;
	 }
	 else
	 {
	    # The current element in the input array didn't match the
	    # result, so advance to the next element in the input array.
	    $inputIndex += 1;
	    $standardizedTarget = 
	       $inputArrayRef->[$inputIndex]->{ STANDARDIZED_TARGET };
	 }
      } # End while

      if( not $matchedResultTarget )
      {
	 # Somehow we failed to find a matching target in the input array.
	 _printError( "Can\'t find \'$resultTarget\' in input array",
		      "_mergeArrays" );
      }
   } # End foreach
}


#---------------------------------------------------------------------------
#                     local cache helper methods
#---------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Method to perform the local cache lookups on all the targets in an array
# which need to be looked up.
sub _lookupLocalCacheArray
{
   my( $methodName, $hashArrayRef, $localCacheRef ) = @_;

   # Test the caller of this method, to make sure it was called internally.
   my $caller = caller;
   if( not defined( $caller ) or $caller ne "CAIDA::NetGeoClient" )
   {
      # This method must have been invoked directly by user code. 
      _printExternalCallerError( "_lookupLocalCacheArray" );
      return;
   }

   # For each defined element in the hash array, check its status then lookup
   # the target in the cache if needed.
   for( my $i = 0; $i < scalar( @$hashArrayRef ); $i++ )
   {
      my $hashRef = $hashArrayRef->[ $i ];

      # Some of the elements of the hash array may be undef if they didn't
      # pass the verification tests.  But if the hash is defined it should
      # contain at least a TARGET field.
      if( defined( $hashRef ) )
      {
	 # Get the standardized form of the target string from the hash.
         my $standardizedTarget = $hashRef->{ STANDARDIZED_TARGET };

	 if( defined( $standardizedTarget ) )
	 {
	    # Lookup the target in the local cache.  This will return undef
            # if there is no match, otherwise returns a hash.
	    my $cachedValue = _lookupLocalCache( $methodName,
						 $standardizedTarget,
					         $localCacheRef );

	    # If we found a match in the cache, store the cached value 
            # into the current slot in the array.
	    if( defined( $cachedValue ) )
	    {
	       $hashArrayRef->[ $i ] = $cachedValue;
	    }
	 }
	 else
	 {
	    _printError( "Bad element $i in argument to " .
			 "_lookupLocalCacheArray, missing  " .
			 "STANDARDIZED_TARGET field", $methodName );
	    next;
	 }  
      }  # End if( defined( $hashRef ) )
   }  # End for( my $i = 0; $i < scalar( @$hashArrayRef ); $i++ )
}


#--------------------------------------------------------------------------
# The value stored with the LOCAL_CACHE key is a ref to a hash with IP addrs, 
# AS numbers, or domain names for keys, and with hashes as values.
#
# Example:  Assume we've already done the lookups getRecord( "caida.org" ),
# getLatLong( "192.172.226.0" ), and getCountry( "AS 1234" ).  Then the 
# cache would look like:
# { "CAIDA.ORG"      => { full hash record },
#   "192.172.226.0"  => { lat/long hash record },
#   "1234"           => { country hash record } }
# The lookup targets (in standard form) are used as keys in the local cache,
# and the values are hash refs.  This method automatically converts the
# cached hash record to the requested format, when possible, otherwise
# returns null to force a new lookup.
sub _lookupLocalCache
{
   my( $methodName, $standarizedTarget, $localCacheRef ) = @_;

   # Test the caller of this method, to make sure it was called internally.
   my $caller = caller;
   if( not defined( $caller ) or $caller ne "CAIDA::NetGeoClient" )
   {
      # This method must have been invoked directly by user code. 
      _printExternalCallerError( "_lookupLocalCache" );
      return;
   }

   my $cachedValue = $localCacheRef->{ $standarizedTarget };
   if( defined( $cachedValue ) )
   {
      # We matched the target.  Check the contents of the cached hash to see
      # if the cached value is the kind of result expected for the current
      # query.
      if( $methodName =~ /^getRecord/ )
      { 
	 # Test the hash to see if it contains a LOOKUP_TYPE key/value.  
	 # Full records always contain a LOOKUP_TYPE key/value pair with 
	 # a defined value, lat/long hashes never do.  
	 if( not defined( $cachedValue->{ LOOKUP_TYPE } ) )
	 {
	    # The current request is getRecord but the cached value is NOT 
	    # a full record, so it doesn't satisfy the current request.  
	    # Set $cachedValue to undef to force a new lookup.
	    $cachedValue = undef;
	 }
	 
	 # Otherwise, $cachedValue is a ref to a full record, so it matches
	 # the type expected by the current request.
      }
      elsif( $methodName =~ /^getLatLong/ )
      {
	 # Test the hash to see if it contains a LOOKUP_TYPE key/value.  
	 # Full records always contain a LOOKUP_TYPE key/value pair with 
	 # a defined value, lat/long hashes never do.  
	 if( defined( $cachedValue->{ LOOKUP_TYPE } ) )
	 {
	    # The cached value is a full record, but the current request 
	    # is only for a lat/long hash.  Extract the needed values 
	    # from the full record and return a lat/long hash.  This 
	    # doesn't change the value stored in the cache, cache keeps 
	    # the full record.
	    my $latLongHash = { TARGET => $cachedValue->{ TARGET },
				LAT => $cachedValue->{ LAT },
				LONG => $cachedValue->{ LONG },
				LAT_LONG_GRAN => 
				$cachedValue->{ LAT_LONG_GRAN },
				STATUS => $cachedValue->{ STATUS } };
	    # Copy the ref into the $cachedValue ref for return to invoker.
	    $cachedValue = $latLongHash;
	 }
	 elsif( not defined( $cachedValue->{ LAT } ) )
	 {
	    # The cached value must be a country hash, so set $cachedValue 
	    # to undef to force a new lookup.
	    $cachedValue = undef;
	 }
	 
	 # Otherwise, $cachedValue is a ref to a lat/long hash, so it 
	 # matches the type expected by the current request.
      }
      elsif( $methodName =~ /^getCountry/ )
      {
	 # Test the hash to see if it contains a COUNTRY key/value, to see
	 # if this is a full record or country hash.
	 if( not defined( $cachedValue->{ COUNTRY } ) )
	 {
	    # The country is undef so the cached value must be a lat/long
	    # hash, set $cachedValue to undef to force a new lookup.
	    $cachedValue = undef;
	 }
      }
   }
   # Return the value found in the cache.  This will be undef if no there was
   # no key matching the target or if the result type was wrong so we need to
   # force a new lookup on the server.
   return $cachedValue;
}


#--------------------------------------------------------------------------
sub _storeLocalCacheArray
{
   my( $resultArrayRef, $localCacheRef ) = @_;

   # Test the caller of this method, to make sure it was called internally.
   my $caller = caller;
   if( not defined( $caller ) or $caller ne "CAIDA::NetGeoClient" )
   {
      # This method must have been invoked directly by user code. 
      _printExternalCallerError( "_storeLocalCacheArray" );
      return;
   }

   my $hashRef;
   foreach $hashRef ( @$resultArrayRef )
   {
      _storeLocalCache( $hashRef, $localCacheRef );
   }
}


#--------------------------------------------------------------------------
sub _storeLocalCache
{
   my( $hashRef, $localCacheRef ) = @_;

   # Test the caller of this method, to make sure it was called internally.
   my $caller = caller;
   if( not defined( $caller ) or $caller ne "CAIDA::NetGeoClient" )
   {
      # This method must have been invoked directly by user code. 
      _printExternalCallerError( "_storeLocalCache" );
      return;
   } 

   if( ref( $hashRef ) eq "HASH" and ref( $localCacheRef ) eq "HASH" )
   {
      my $standardizedTarget = $hashRef->{ STANDARDIZED_TARGET };
      my $status = $hashRef->{ STATUS };
      if( defined( $standardizedTarget ) and 
	  defined( $status ) and ( $status eq $OK or 
	  $status eq $NO_MATCH or $status eq $NO_COUNTRY ) )
      {
	 $localCacheRef->{ $standardizedTarget } = $hashRef;
      }

      if( defined( $standardizedTarget ) )
      {
	 # We're done using the standardized name for now, so remove it
         # from the hashtable.  The hashtable will still contain the
         # original target in the TARGET field.
         delete( $hashRef->{ STANDARDIZED_TARGET } );
      }
   }
}

#---------------------------------------------------------------------------
#                      input verification methods
#---------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Verify the type of the target argument and verify types of array elements
# and fields within hashes, if argument is an array.  Call _verifyInputFormat
# on each target to standardize the target string and make sure it is in a
# valid format.  Returns undef if $target input is undef or not ARRAY or 
# SCALAR, or if $target is a bad format SCALAR.
sub _verifyInputFormatArray
{
   my( $methodName, $inputArrayRef ) = @_;

   # Test the caller of this method, to make sure it was called internally.
   my $caller = caller;
   if( not defined( $caller ) or $caller ne "CAIDA::NetGeoClient" )
   {
      # This method must have been invoked directly by user code. 
      _printExternalCallerError( "_verifyInputFormatArray" );
      return $INPUT_ERROR;
   } 

   my $inputArrayRefType = ref( $inputArrayRef );
   if( $inputArrayRefType eq "ARRAY" )
   {
      if( scalar( @$inputArrayRef ) > $ARRAY_LENGTH_LIMIT )
      {
	 # The input array length exceeds the array length limit, return
	 # undef to short-circuit the lookup.
	 _printError( "Input array for $methodName is too long, array " .
		      "length limit is $ARRAY_LENGTH_LIMIT", $methodName );
	 return $INPUT_ERROR;
      }

      # Verify each of the elements in the target array.
      for( my $i = 0; $i < scalar( @$inputArrayRef ); $i++ )
      {
	 my $hashRef = $inputArrayRef->[ $i ];

	 if( ref( $hashRef ) eq "HASH" )
	 {
	    # Check the status, to see if this element has already been found
	    # in a previous lookup.  We don't waste time verifying the target
	    # string if we don't need to look it up.
	    my $status = $hashRef->{ STATUS };

	    # Status is undef for hashtables created in makeHashArray, from
	    # calls to getXxxArray.  Otherwise, there could be some existing
	    # status value from a previous lookup.  If the status is not one
	    # of the 'defintitive' values, we need to look up the target.
	    if( not defined( $status ) or 
		( $status ne $OK && $status ne $NO_MATCH &&
		  $status ne $NO_COUNTRY ) )
	    {
	       # Test the target string and store the standardized target
	       # string into the hashtable.
	       _verifyInputFormat( $methodName, $hashRef);
	    } 
	 }
	 else
	 {
	    # The array element is not a hash, so the input is bad.
	    _printError( "Input array for $methodName must contain hashes",
			 $methodName );
	    return $INPUT_ERROR;
	 }
      } # End for
   }
   else
   {
      # The input array is undef or something other than an array.
      return $INPUT_ERROR;
   }

   return $OK;
}


#--------------------------------------------------------------------------
# Test the input and make sure it is in an acceptable format.  The input
# can be an AS number (with or without a leading "AS"), an IP address in
# dotted decimal format, or a domain name.  Stores the standardized target
# string into the hash if input target is valid format, otherwise stores
# undef into hash.
sub _verifyInputFormat
{
   my( $methodName, $hashRef ) = @_;

   # Test the caller of this method, to make sure it was called internally.
   my $caller = caller;
   if( not defined( $caller ) or $caller ne "CAIDA::NetGeoClient" )
   {
      # This method must have been invoked directly by user code. 
      _printExternalCallerError( "_verifyInputFormat" );
      return;
   } 
   
   my $target = $hashRef->{ TARGET };
   my $standardizedTarget = undef;

   if( defined( $target ) )
   {
      # Trim any leading or trailing spaces.
      $target =~ s/^\s*(.*\S)\s*$/$1/;
      
      if( $target =~ /^(?:AS|as)?\s?(\d{1,5})$/ )
      {
	 # The target is an AS number.  This must be between 1 and 64K.
	 my $asNumber = $1;
	 if( $asNumber >= 1 and $asNumber < 65536 )
	 {
	    $standardizedTarget = $asNumber;
	 }
	 else
	 {
	    $hashRef->{ $INPUT_ERROR } = $INPUT_ERROR;
	    _printError( "Bad format for input '$target', AS number must " .
			 "be between 1 and 65536", $methodName );
	 }
      }
      elsif( $target =~ /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$/ )
      {
	 # Only digits are allowed in this regex so we know there can be no 
	 # negative numbers. Make sure the numbers are in the proper ranges.
	 if( $1 <= 255 and $2 <= 255 and $3 <= 255 and $4 <= 255 )
	 {
	    $standardizedTarget = $target;
	 }
	 else
	 {
	    $hashRef->{ $INPUT_ERROR } = $INPUT_ERROR;
	    _printError( "Bad format for input '$target', each octet in IP " .
			 "address must be between 0 and 255", $methodName );
	 }
      }
      elsif( $target =~ /^(?:[\w\-]+\.)*[\w\-]+\.([A-Za-z]{2,3})$/ )
      {
	 my $tld = $1;
	 
	 # TLD length is either 2 or 3.  If length is 2 we just accept it,
	 # otherwise we test the TLD against the list.
	 if( length( $tld ) == 2 or 
	     $tld =~ /^(com|net|org|edu|gov|mil|int)/io )
	 {
	    # Input is in a valid format for a domain name.
	    $standardizedTarget = $target;
	 }
	 else
	 {
	    $hashRef->{ $INPUT_ERROR } = $INPUT_ERROR;
	    _printError( "Bad TLD in domain name '$target', 3-letter TLDs " .
			 "must be one of {com,net,org,edu,gov,mil,int}", 
			 $methodName );
	 }
      }
      else
      {
	 $hashRef->{ $INPUT_ERROR } = $INPUT_ERROR;
	 _printError( "Unrecognized format for input '$target'", 
		      $methodName );
      }
   }
   
   # Store the standardized target string into the hash table. It will be 
   # used for comparison to the results returned from the server and as the 
   # key to the cache table.  This string will be null if the input target 
   # was not in a valid format.
   $hashRef->{ STANDARDIZED_TARGET } = $standardizedTarget;
}


#---------------------------------------------------------------------------
#                       other helper methods
#---------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Parse the block of text output from netgeo.cgi, such as:
# <!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">
# <HTML><HEAD><TITLE>NetGeo Results</TITLE>
# </HEAD><BODY>
# TARGET:        caida.org<br>
# COUNTRY:       US<br>
# STATUS:        OK<br>
# </BODY></HTML>
sub _convertToHashArray
{
   my( $text ) = @_;

   # Initialize the array as an array of length zero.
   my @hashArray = ();
   my $hashRef;

   if( not defined( $text ) or $text =~ /^\s*$/ )
   {
      # Nothing to convert, return an empty hash array.
      return \@hashArray;
   }

   my @lineArray = split( /\n/, $text );
   my $line = shift( @lineArray );

   # First, check for an HTTP error message or an empty string.
   if( $line =~ /$HTTP_ERROR/o or $text =~ /^\s*$/ )
   {
      # An error occurred in the HTTP connection.  Make a 1-element array
      # for returning the error message to the original invoker.
      if( $text =~ /^\s*$/ )
      {
	 $text = "Empty content string";
      }
      # Return a ref to a 1-element array containing a hash ref.
      return [ { STATUS => $text } ];
   }
   elsif( $line =~ /$NETGEO_LIMIT_EXCEEDED/o )
   {
      # The user exceeded the query limit, return an array with a single
      # element containing the message.
      return [ { STATUS => $text } ];
   }

   # Find the start of the first block of name/value pairs.
   while( defined( $line ) and $line !~ /^TARGET:/ )
   {
      $line = shift( @lineArray );
   }

   # Read all the lines in the text, storing name/value pairs into hashes
   # and storing completed hashes into the hash array.
   while( defined( $line ) )
   {
      if( $line =~ /^TARGET:\s+(.*\S)\s*<br>/ )
      {
	 # Start a new hash with the current identifier.
	 $hashRef = { TARGET => $1 };
      }
      elsif( $line =~ /^STATUS:\s+([\w\s]+\S)\s*<br>/ )
      {
	 $hashRef->{ STATUS } = $1;

	 # We found the last line of a block, push the current hash onto
	 # the array.
	 push( @hashArray, $hashRef );
      }
      elsif( $line =~ /^(\w+):\s+(.*\S)\s*<br>/ )
      {
	 # Add the name/value pair to the current hash.
	 $hashRef->{ $1 } = $2;
      }
      $line = shift( @lineArray );
   }
   
   # Return a reference to the hash array.  This array may contain any number
   # of hashes, including 0 if the original query list was empty.
   return \@hashArray;
}


#--------------------------------------------------------------------------
# Input should be an array of hash refs, each hash containing at least the 
# TARGET key.
sub _makeList
{
   my( $methodName, $arrayRef ) = @_;

   # Test the caller of this method, to make sure it was called internally.
   my $caller = caller;
   if( not defined( $caller ) or $caller ne "CAIDA::NetGeoClient" )
   {
      # This method must have been invoked directly by user code. 
      _printExternalCallerError( "_makeList" );
      return undef;
   } 

   # Check for unusual case where user supplied only a single scalar instead
   # of an array to a getXxxArray or updateXxxArray method.
   if( ref( $arrayRef ) ne "ARRAY" )
   {
      # Enclose the input scalar in a single-element array, so it will be in
      # the correct format for processing below.
      my $inputScalar = $arrayRef;
      $arrayRef = [ $inputScalar ];

      printWarning( "Input to $methodName should be an array, not a " .
		    "scalar", $methodName );
   }

   my $targetList = "";
   for( my $index = 0; $index < scalar( @$arrayRef ); $index++ )
   {
      my $standardizedTarget;
      my $arrayElement = $arrayRef->[$index];

      # For each element in the array, check the type and compare against
      # the expected type.  
      if( ref( $arrayElement ) eq "HASH" )
      {
	 # The element type is HASH, as required.  Extract the status, if
	 # any.  If this is the first time updateXxxArray has been called
	 # the status might be undefined.  Otherwise the status should be
	 # one of the constant strings $OK, $NO_MATCH, $LOOKUP_IN_PROGRESS,
	 # $NO_COUNTRY, or possibly some other value put in by the user.
	 # The user might put in a user-defined value such as "done" after 
	 # reading the result.  If status is $OK the result was found in an
	 # earlier lookup.  If status is $NO_MATCH or $NO_COUNTRY then an 
	 # earlier lookup failed, no need to do another lookup.
	 my $status = $arrayElement->{ STATUS };
	 if( not defined( $status ) or $status eq $LOOKUP_IN_PROGRESS
	     or $status eq $WHOIS_TIMEOUT or
	     $status eq $NETGEO_LIMIT_EXCEEDED )
	 {
	    # Status is not defined, meaning the current target hasn't
	    # been looked up yet, or status is $LOOKUP_IN_PROGRESS meaning
	    # there was an earlier nonblocking lookup of the target.  If
	    # status is $LOOKUP_IN_PROGRESS the target wasn't found in the
	    # database in the earlier lookup, so a whois lookup is 
	    # scheduled or in progress or recently completed.  In either
	    # case (status not defined or lookup in progress) we will 
	    # query the database.  
	    $standardizedTarget = $arrayElement->{ STANDARDIZED_TARGET };

	    # Test the target to make sure it is valid.  If the original
	    # target in the current slot had a bad format the _verifyTarget
	    # method made it undef, so here the target string will either
	    # be a valid target or undef.
	    if( defined( $standardizedTarget ) )
	    {
	       # Append the target to the target list.
	       if( length( $targetList ) == 0 )
	       {
		  $targetList .= "$standardizedTarget";
	       }
	       else
	       {
		  $targetList .= ",$standardizedTarget";
	       }
	    }
	    else
	    {
	       _printError( "Array element $index in input to " .
			    "$methodName missing STANDARDIZED_TARGET key", 
			    $methodName );
	    }
	 }
      }
      else
      {
	 # The type was not HASH and so the array contains something wrong.
	 _printError( "Bad type for array element $index in input to " .
		      "$methodName, should be HASH", $methodName );
      }
   }

   return $targetList;
}


#============================================================================
#                            debug methods
#============================================================================

#--------------------------------------------------------------------------
# Used for debug print statements
sub _getArgString
{
   my( $target ) = @_;
   my $argString;

   if( ref( $target ) eq "ARRAY" )
   {
      my $size = scalar( @$target );
      $argString = "[$size elements]";  
   }
   else
   {
      $argString = "$target";
   }

   return $argString;
}


#--------------------------------------------------------------------------
sub _printError
{
   my( $message, $methodName ) = @_;

   print STDERR "ERROR in CAIDA::NetGeoClient::$methodName, $message.\n";
}

  
#--------------------------------------------------------------------------
sub _printWarning
{
   my( $message, $methodName ) = @_;

   print STDERR "WARNING in CAIDA::NetGeoClient::$methodName, $message.\n";
}


#--------------------------------------------------------------------------
sub _printExternalCallerError
{
   my( $methodName ) = @_;

   _printError( "$methodName invoked incorrectly. This is an internal " .
		"method and should not be invoked directly", $methodName );
}

    
#--------------------------------------------------------------------------
# Method called from debug blocks in updateRecordArray, updateCountryArray,
# etc.  Prints the number of elements of each status in the input array.
sub _printArrayStatus
{
   my( $hashArrayRef ) = @_;

   my $inProgressCount = 0;
   my $notDefinedCount = 0;
   for( my $index = 0; $index < scalar( @$hashArrayRef ); $index++ )
   {
      if( $hashArrayRef->[$index]->{ STATUS } eq $LOOKUP_IN_PROGRESS )
      {
	 $inProgressCount += 1;
      }
      elsif( not defined( $hashArrayRef->[$index]->{ STATUS } ) )
      {
	 $notDefinedCount += 1;
      }
   }

   # Other status values are lumped into one category because we don't know
   # if the user has changed some values from OK to some other value, to 
   # indicate they've already been found and processed.  It's expected that
   # users will change status OK to "Done" or something similar, to avoid
   # repeated printing or plotting of the values that have already been found.
   my $otherCount = scalar( @$hashArrayRef ) - $inProgressCount - 
                       $notDefinedCount;

   print "$inProgressCount lookups pending or in progress, $otherCount " .
      "completed\n";
}


#1;


## USAGE: 
##
## $Id: ngc.pl,v 1.1 2001/10/19 21:04:27 atchley Exp $
##
## AUTHOR: Jim Donohoe, CAIDA
## <LICENSE>

#my $VERSION = 1.0;

use strict;
#use CAIDA::NetGeoClient;

my $netgeo = new CAIDA::NetGeoClient;

#testRecord( 1 );

#testCountry( "caida.org" );

#testLatLong( "caida.org" );

#test_getCountryArray( [ 1, 2, 3, "caida.org", "ripe.net", "apnic.net" ] );
#test_getLatLongArray( [ 1, 2, 3, "caida.org", "ripe.net", "apnic.net" ] );

#if( 1 )
#{
#   my @hashArray = ();
#   my $startNumber = 13820;
#   for( my $i = 0; $i < 20; $i++ )
#   {
#      my $target = "AS" . ($startNumber + $i); 
#      push( @hashArray, { TARGET => $target } );
#   }

#   my $nonblocking = "true";
#   test_updateRecordArray( \@hashArray, $nonblocking );
#   @hashArray = ();
#   for( my $i = 0; $i < 20; $i++ )
#   {
#      my $target = "AS" . ($startNumber + $i); 
#      push( @hashArray, { TARGET => $target } );
#   }
#   test_updateRecordArray( \@hashArray, $nonblocking );
#}

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
   my( $stringArrayRef ) = @_;

   my $netgeo = new CAIDA::NetGeoClient;
   
   my $arrayRef = $netgeo->getLatLongArray( $stringArrayRef );
    
   my $recordRef;
   my $number = 0;  
   foreach $recordRef ( @$arrayRef ) {
      if( $recordRef->{STATUS} eq "OK" ) {
        $number ++;
      }
   }
   
   print "$number\n";
   
   foreach $recordRef ( @$arrayRef )
   {
      if( $recordRef->{STATUS} eq "OK" )
      {
	print "$recordRef->{TARGET}\n$recordRef->{LAT}\n$recordRef->{LONG}\n";
      }
   }
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
  my ( @myTestInputArray ) = [ @_ ];
  
  test_getLatLongArray( @myTestInputArray );
}


sub showtime {
  print time, "\n";
}

my $line = <STDIN>;
my @myTestInputArray = split (/\s+/, $line);
#my ( @myTestInputArray ) = ("192.172.226.30", "caida.org", "ripe.net", "apnic.net", "utk.edu");
lbone_getLatLongInfo ( @myTestInputArray );

