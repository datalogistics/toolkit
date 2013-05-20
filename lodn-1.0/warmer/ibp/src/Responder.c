/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@

*/

#include "mDNSClientAPI.h"// Defines the interface to the client layer above
#include "mDNSPosix.h"    // Defines the specific types needed to run mDNS on this platform
#include "ibp_mdns.h"

#include <assert.h>
#include <stdio.h>			// For printf()
#include <stdlib.h>			// For exit() etc.
#include <string.h>			// For strlen() etc.
#include <unistd.h>			// For select()
#include <errno.h>			// For errno, EINTR
#include <signal.h>
#include <fcntl.h>

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark ***** Globals
#endif

mDNS mDNSStorage;       // mDNS core uses this to store its globals
mDNS_PlatformSupport PlatformStorage;  // Stores this platform's globals
static PosixService *gServiceList = NULL;
static int gServiceID = 0;
static const char kDefaultServiceType[] = "_ibprotocol._tcp.";
static const char kDefaultServiceDomain[] = "local.";
static const char gProgramName[]="mDNS";

enum {
    kDefaultPortNumber = 6714 
};

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark ***** Signals
#endif


#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark ***** Parameter Checking
#endif

mDNSBool CheckThatRichTextHostNameIsUsable(const char *richTextHostName, mDNSBool printExplanation)
    // Checks that richTextHostName is a reasonable host name 
    // label and, if it isn't and printExplanation is true, prints 
    // an explanation of why not.
{
    mDNSBool    result;
    domainlabel richLabel;
    domainlabel poorLabel;
    
    result = mDNStrue;
    if (result && strlen(richTextHostName) > 63) {
        if (printExplanation) {
            fprintf(stderr, 
                    "%s: Host name is too long (must be 63 characters or less)\n", 
                    gProgramName);
        }
        result = mDNSfalse;
    }
    if (result && richTextHostName[0] == 0) {
        if (printExplanation) {
            fprintf(stderr, "%s: Host name can't be empty\n", gProgramName);
        }
        result = mDNSfalse;
    }
    if (result) {
        MakeDomainLabelFromLiteralString(&richLabel, richTextHostName);
        ConvertUTF8PstringToRFC1034HostLabel(richLabel.c, &poorLabel);
        if (poorLabel.c[0] == 0) {
            if (printExplanation) {
                fprintf(stderr, 
                        "%s: Host name doesn't produce a usable RFC-1034 name\n", 
                        gProgramName);
            }
            result = mDNSfalse;
        }
    }
    return result;
}

mDNSBool CheckThatServiceTypeIsUsable(const char *serviceType, mDNSBool printExplanation)
    // Checks that serviceType is a reasonable service type 
    // label and, if it isn't and printExplanation is true, prints 
    // an explanation of why not.
{
    mDNSBool result;
    
    result = mDNStrue;
    if (result && strlen(serviceType) > 63) {
        if (printExplanation) {
            fprintf(stderr, 
                    "%s: Service type is too long (must be 63 characters or less)\n", 
                    gProgramName);
        }
        result = mDNSfalse;
    }
    if (result && serviceType[0] == 0) {
        if (printExplanation) {
            fprintf(stderr, 
                    "%s: Service type can't be empty\n", 
                    gProgramName);
        }
        result = mDNSfalse;
    }
    return result;
}

mDNSBool CheckThatServiceTextIsUsable(const char *serviceText, mDNSBool printExplanation,
                                             mDNSu8 *pStringList, mDNSu16 *pStringListLen)
    // Checks that serviceText is a reasonable service text record 
    // and, if it isn't and printExplanation is true, prints 
    // an explanation of why not.  Also parse the text into 
    // the packed PString buffer denoted by pStringList and 
    // return the length of that buffer in *pStringListLen.
    // Note that this routine assumes that the buffer is 
    // sizeof(RDataBody) bytes long.
{
    mDNSBool result;
    size_t   serviceTextLen;
    
    // Note that parsing a C string into a PString list always 
    // expands the data by one character, so the following 
    // compare is ">=", not ">".  Here's the logic:
    //
    // #1 For a string with not ^A's, the PString length is one 
    //    greater than the C string length because we add a length 
    //    byte.
    // #2 For every regular (not ^A) character you add to the C 
    //    string, you add a regular character to the PString list.
    //    This does not affect the equivalence stated in #1.
    // #3 For every ^A you add to the C string, you add a length 
    //    byte to the PString list but you also eliminate the ^A, 
    //    which again does not affect the equivalence stated in #1.
    
    result = mDNStrue;
    serviceTextLen = strlen(serviceText);
    if (result && strlen(serviceText) >= sizeof(RDataBody)) {
        if (printExplanation) {
            fprintf(stderr, 
                    "%s: Service text record is too long (must be less than %d characters)\n", 
                    gProgramName,
                    (int) sizeof(RDataBody) );
        }
        result = mDNSfalse;
    }
    
    // Now break the string up into PStrings delimited by ^A.
    // We know the data will fit so we can ignore buffer overrun concerns. 
    // However, we still have to treat runs long than 255 characters as
    // an error.
    
    if (result) {
        int         lastPStringOffset;
        int         i;
        int         thisPStringLen;
        
        // This algorithm is a little tricky.  We start by copying 
        // the string directly into the output buffer, shifted up by 
        // one byte.  We then fill in the first byte with a ^A. 
        // We then walk backwards through the buffer and, for each 
        // ^A that we find, we replace it with the difference between 
        // its offset and the offset of the last ^A that we found
        // (ie lastPStringOffset).
        
        memcpy(&pStringList[1], serviceText, serviceTextLen);
        pStringList[0] = 1;
        lastPStringOffset = serviceTextLen + 1;
        for (i = serviceTextLen; i >= 0; i--) {
            if ( pStringList[i] == 1 ) {
                thisPStringLen = (lastPStringOffset - i - 1);
                assert(thisPStringLen >= 0);
                if (thisPStringLen > 255) {
                    result = mDNSfalse;
                    if (printExplanation) {
                        fprintf(stderr, 
                                "%s: Each component of the service text record must be 255 characters or less\n", 
                                gProgramName);
                    }
                    break;
                } else {
                    pStringList[i]    = thisPStringLen;
                    lastPStringOffset = i;
                }
            }
        }
        
        *pStringListLen = serviceTextLen + 1;
    }

    return result;
}

mDNSBool CheckThatPortNumberIsUsable(long portNumber, mDNSBool printExplanation)
    // Checks that portNumber is a reasonable port number
    // and, if it isn't and printExplanation is true, prints 
    // an explanation of why not.
{
    mDNSBool result;
    
    result = mDNStrue;
    if (result && (portNumber <= 0 || portNumber > 65535)) {
        if (printExplanation) {
            fprintf(stderr, 
                    "%s: Port number specified by -p must be in range 1..65535\n", 
                    gProgramName);
        }
        result = mDNSfalse;
    }
    return result;
}


#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark ***** Registration
#endif

void RegistrationCallback(mDNS *const m, ServiceRecordSet *const thisRegistration, mStatus status)
    // mDNS core calls this routine to tell us about the status of 
    // our registration.  The appropriate action to take depends 
    // entirely on the value of status.
{
    switch (status) {

        case mStatus_NoError:      
            debugf("Callback: %##s Name Registered",   thisRegistration->RR_SRV.resrec.name.c); 
            // Do nothing; our name was successfully registered.  We may 
            // get more call backs in the future.
            break;

        case mStatus_NameConflict: 
            debugf("Callback: %##s Name Conflict",     thisRegistration->RR_SRV.resrec.name.c); 

            // In the event of a conflict, this sample RegistrationCallback 
            // just calls mDNS_RenameAndReregisterService to automatically 
            // pick a new unique name for the service. For a device such as a 
            // printer, this may be appropriate.  For a device with a user 
            // interface, and a screen, and a keyboard, the appropriate response 
            // may be to prompt the user and ask them to choose a new name for 
            // the service.
            //
            // Also, what do we do if mDNS_RenameAndReregisterService returns an 
            // error.  Right now I have no place to send that error to.
            
            status = mDNS_RenameAndReregisterService(m, thisRegistration, mDNSNULL);
            assert(status == mStatus_NoError);
            break;

        case mStatus_MemFree:      
            debugf("Callback: %##s Memory Free",       thisRegistration->RR_SRV.resrec.name.c); 
            
            // When debugging is enabled, make sure that thisRegistration 
            // is not on our gServiceList.
            /* 
            #if !defined(NDEBUG)
                {
                    PosixService *cursor;
                    
                    cursor = gServiceList;
                    while (cursor != NULL) {
                        assert(&cursor->coreServ != thisRegistration);
                        cursor = cursor->next;
                    }
                }
            #endif
            */
            free(thisRegistration);
            break;

        default:                   
            debugf("Callback: %##s Unknown Status %d", thisRegistration->RR_SRV.resrec.name.c, status); 
            break;
    }
}

mStatus RegisterOneService(const char *  richTextHostName, 
                                  const char *  serviceType, 
                                  const char *  serviceDomain, 
                                  const mDNSu8  text[],
                                  mDNSu16       textLen,
                                  long          portNumber)
{
    mStatus             status;
    PosixService *      thisServ;
    mDNSOpaque16        port;
    domainlabel         name;
    domainname          type;
    domainname          domain;
    
    status = mStatus_NoError;
    thisServ = (PosixService *) malloc(sizeof(*thisServ));
    if (thisServ == NULL) {
        status = mStatus_NoMemoryErr;
    }
    if (status == mStatus_NoError) {
        MakeDomainLabelFromLiteralString(&name,  richTextHostName);
        MakeDomainNameFromDNSNameString(&type, serviceType);
        MakeDomainNameFromDNSNameString(&domain, serviceDomain);
        port.b[0] = (portNumber >> 8) & 0x0FF;
        port.b[1] = (portNumber >> 0) & 0x0FF;;
        status = mDNS_RegisterService(&mDNSStorage, &thisServ->coreServ,
                &name, &type, &domain,				// Name, type, domain
                NULL, port, 						// Host and port
                text, textLen,						// TXT data, length
                NULL, 0,							// Subtypes
                mDNSInterface_Any,					// Interace ID
                RegistrationCallback, thisServ);	// Callback and context
    }
    if (status == mStatus_NoError) {
        thisServ->serviceID = gServiceID;
        gServiceID += 1;

        thisServ->next = gServiceList;
        gServiceList = thisServ;

        if (gMDNSPlatformPosixVerboseLevel > 0) {
            fprintf(stderr, 
                    "%s: Registered service %d, name '%s', type '%s', port %ld\n", 
                    gProgramName, 
                    thisServ->serviceID, 
                    richTextHostName,
                    serviceType,
                    portNumber);
        }
    } else {
        if (thisServ != NULL) {
            free(thisServ);
        }
    }
    return status;
}


void DeregisterOurServices(void)
{
    PosixService *thisServ;
    int thisServID;
    
    while (gServiceList != NULL) {
        thisServ = gServiceList;
        gServiceList = thisServ->next;

        thisServID = thisServ->serviceID;
        
        mDNS_DeregisterService(&mDNSStorage, &thisServ->coreServ);

        if (gMDNSPlatformPosixVerboseLevel > 0) {
            fprintf(stderr, 
                    "mDNS: Deregistered service %d\n",
                    thisServ->serviceID);
        }
    }
}

