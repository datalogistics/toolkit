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

#ifndef IBP_MDNS_H
#define IBP_MDNS_H

#define IBP_ENABLE_BONJOUR 1

#if IBP_ENABLE_BONJOUR

#include "mDNSClientAPI.h"// Defines the interface to the client layer above
#include "mDNSPosix.h"    // Defines the specific types needed to run mDNS on this platform

extern mDNSBool CheckThatRichTextHostNameIsUsable(const char *richTextHostName, mDNSBool printExplanation);
extern mDNSBool CheckThatServiceTypeIsUsable(const char *serviceType, mDNSBool printExplanation);
extern mDNSBool CheckThatServiceTextIsUsable(const char *serviceText, mDNSBool printExplanation, mDNSu8 *pStringList, mDNSu16 *pStringListLen);
extern mDNSBool CheckThatPortNumberIsUsable(long portNumber, mDNSBool printExplanation);
extern void RegistrationCallback(mDNS *const m, ServiceRecordSet *const thisRegistration, mStatus status);
extern mStatus RegisterOneService(const char *  richTextHostName, 
                                  const char *  serviceType, 
                                  const char *  serviceDomain, 
                                  const mDNSu8  text[],
                                  mDNSu16       textLen,
                                  long          portNumber);
extern void DeregisterOurServices(void);


typedef struct PosixService PosixService;

struct PosixService {
    ServiceRecordSet coreServ;
    PosixService *next;
    int serviceID;
};

#endif /* IBP_ENABLE_BONJOUR */

#endif
