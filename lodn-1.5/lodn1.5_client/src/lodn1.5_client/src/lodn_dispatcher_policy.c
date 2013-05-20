
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>


#include "jrb.h"
#include "jval.h"
#include "lodn_client/lodn_errno.h"
#include "lodn_url.h"
#include "lodn_client/lodn_dispatcher_policy.h"



struct _lodn_dispatcher_policy_site_entry_t
{

};

struct _lodn_dispatcher_policy_site_t
{


};

struct _lodn_dispatcher_policy_t
{

};




int lodn_importDispatcherPolicy(unsigned char *lodn_url,
						        lodn_dispatcher_policy_t *policy)
{
	/* Declarations */
	char *host 		= NULL;
	int   port;
	char *path 		= NULL;
	JRB   options 	= NULL;
	JRB   node      = NULL;


	/* Parses the URL */
	if(lodn_parse_url(lodn_url, &host, &port, &path, &options) != 0)
	{
		// lodn_errno should already be set
		return -1;
	}


	/* Set up message handling and use shared component */


	/* Frees resources */
	free(host);
	free(path);

	jrb_traverse(node, options)
	{
		jrb_free_tree(node->val.s);
	}

	jrb_free_tree(options);


	/* Return successfully */
	return 0;

	ERROR_HANDLER:

		/* Frees resources */
		if(host != NULL)
		{
			free(host);
			free(path);

			jrb_traverse(node, options)
			{
				jrb_free_tree(node->val.s);
			}


			jrb_free_tree(options);
		}

		/* Return error */
		return -1;
}



int lodn_exportDispatcherPolicy(unsigned char *lodn_url,
						        lodn_dispatcher_policy_t policy)
{
	/* Declarations */
	char *host 		= NULL;
	int   port;
	char *path 		= NULL;
	JRB   options 	= NULL;
	JRB   node      = NULL;


	/* Parses the URL */
	if(lodn_parse_url(lodn_url, &host, &port, &path, &options) != 0)
	{
		// lodn_errno should already be set
		return -1;
	}


	/* Set up message handling and use shared component */


	/* Frees resources */
	free(host);
	free(path);

	jrb_traverse(node, options)
	{
		jrb_free_tree(node->val.s);
	}

	jrb_free_tree(options);


	/* Return successfully */
	return 0;

	ERROR_HANDLER:

		/* Frees resources */
		if(host != NULL)
		{
			free(host);
			free(path);

			jrb_traverse(node, options)
			{
				jrb_free_tree(node->val.s);
			}


			jrb_free_tree(options);
		}

		/* Return error */
		return -1;

	/* Return successfully */
	return 0;
}

int lodn_unlinkDispatcherPolicy(unsigned char *lodn_url)
{
	/* Declarations */
	char *host 		= NULL;
	int   port;
	char *path 		= NULL;
	JRB   options 	= NULL;
	JRB   node      = NULL;


	/* Parses the URL */
	if(lodn_parse_url(lodn_url, &host, &port, &path, &options) != 0)
	{
		// lodn_errno should already be set
		return -1;
	}


	/* Set up message handling and use shared component */


	/* Frees resources */
	free(host);
	free(path);

	jrb_traverse(node, options)
	{
		jrb_free_tree(node->val.s);
	}

	jrb_free_tree(options);


	/* Return successfully */
	return 0;

	ERROR_HANDLER:

		/* Frees resources */
		if(host != NULL)
		{
			free(host);
			free(path);

			jrb_traverse(node, options)
			{
				jrb_free_tree(node->val.s);
			}


			jrb_free_tree(options);
		}

		/* Return error */
		return -1;

	/* Return successfully */
	return 0;
}



int lodn_newDispatcherPolicy(lodn_dispatcher_policy_t *policy)
{
	/* Declarations */
	struct _lodn_dispatcher_policy_t *_policy = NULL;


	/* Checks the validity of the argument */
	if(policy == NULL)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Allocate policy */
	if((_policy = malloc(sizeof(struct _lodn_dispatcher_policy_t))) == NULL)
	{
		lodn_errno = LODN_EMEM;
		return -1;
	}

	/* Assigns the address policy struct */
	*policy = _policy;

	/* Return successfully */
	return 0;
}



int lodn_freeDispatcherPolicy(const lodn_dispatcher_policy_t policy)
{
	/* Declarations */
	struct _lodn_dispatcher_policy_t *_policy = NULL;

	/* Checks the validity of the argument */
	if(policy == NULL)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Gets a pointer from the opague type */
	_policy = policy;

	/* Frees the memory */
	free(_policy);

	/* Return successfully */
	return 0;
}



int lodn_newDispatcherPolicySite(lodn_dispatcher_policy_site_t *site);

int lodn_freeDispatcherPolicySite(const lodn_dispatcher_policy_site_t site);

int lodn_newDispatcherPolicySiteEntry(lodn_dispatcher_policy_site_entry_t *entry);

int lodn_freeDispatcherPolicySiteEntry(const lodn_dispatcher_policy_site_entry_t entry);

int lodn_addDispatcherPolicySite(lodn_dispatcher_policy_t policy,
								 lodn_dispatcher_policy_site_t site);

int lodn_addDispatcherPolicySiteEntry(lodn_dispatcher_policy_site_t site,
									  lodn_dispatcher_policy_site_entry_t entry);








