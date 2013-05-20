/*
 * lodn_dispatcher_policy.h
 *
 *  Created on: Jun 24, 2009
 *      Author: Chris Brumgard
 */

#ifndef LODN_DISPATCHER_POLICY_H_
#define LODN_DISPATCHER_POLICY_H_



typedef struct _lodn_dispatcher_policy_t      *lodn_dispatcher_policy_t;
typedef struct _lodn_dispatcher_policy_site_t *lodn_dispatcher_policy_site_t;
typedef struct _lodn_dispatcher_policy_site_t *lodn_dispatcher_policy_site_entry_t;



int lodn_importDispatcherPolicy(unsigned char *lodn_url,
						        lodn_dispatcher_policy_t *policy);
int lodn_exportDispatcherPolicy(unsigned char *lodn_url,
						        lodn_dispatcher_policy_t policy);
int lodn_unlinkDispatcherPolicy(unsigned char *lodn_url);
int lodn_newDispatcherPolicy(lodn_dispatcher_policy_t *policy);
int lodn_freeDispatcherPolicy(lodn_dispatcher_policy_t policy);
int lodn_newDispatcherPolicySite(lodn_dispatcher_policy_site_t *site);
int lodn_freeDispatcherPolicySite(lodn_dispatcher_policy_site_t site);
int lodn_newDispatcherPolicySiteEntry(lodn_dispatcher_policy_site_entry_t *entry);
int lodn_freeDispatcherPolicySiteEntry(lodn_dispatcher_policy_site_entry_t entry);
int lodn_addDispatcherPolicySite(lodn_dispatcher_policy_t policy,
								 lodn_dispatcher_policy_site_t site);
int lodn_addDispatcherPolicySiteEntry(lodn_dispatcher_policy_site_t site,
									  lodn_dispatcher_policy_site_entry_t entry);




#endif /* LODN_DISPATCHER_POLICY_H_ */
