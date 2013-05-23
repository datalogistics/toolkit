/* $Id: dnsutil.h,v 1.6 2000/03/14 00:28:37 hayes Exp $ */


#ifndef DNSUTIL_H
#define DNSUTIL_H


/*
** This package defines some utilities for determining and converting DNS
** machine names and IP addresses.
*/


#ifdef __cplusplus
extern "C" {
#endif


#define MAX_IP_IMAGE 15
/* Maximum text length of an IP address, i.e. strlen("255.255.255.255") */


typedef unsigned long IPAddress;


/*
** Converts #addr# into a printable string and returns the result.  The value
** returned is volatile and will be overwritten by subsequent calls.
*/
const char *
IPAddressImage(IPAddress addr);


/*
** Converts #addr# to a fully-qualified machine name and returns the result, or
** "" on error.  The value returned is volatile and will be overwritten by
** subsequent calls.
*/
const char *
IPAddressMachine(IPAddress addr);


/*
** Converts #machineOrAddress#, which may be either a DNS name or an IP address
** image, into a list of addresses.  Copies the list into the #atMost#-long
** array #addressList#.  Returns the number of addresses copied, or zero on
** error.  #atMost# may be zero, in which case the function simply returns one
** or zero depending on whether or not #machineOrAddress# is a valid machine
** name or IP address image.
*/
int
IPAddressValues(const char *machineOrAddress,
                IPAddress *addressList,
                unsigned int atMost);
#define IPAddressValue(machineOrAddress,address) \
        IPAddressValues(machineOrAddress,address,1)
#define IsValidIP(machineOrAddress) IPAddressValues(machineOrAddress,NULL,0)


/*
** Returns the fully-qualified name of the host machine, or NULL if the name
** cannot be determined.  Always returns the same value, so multiple calls
** cause no problems.
*/
const char *
MyMachineName(void);


#ifdef __cplusplus
}
#endif


#endif
