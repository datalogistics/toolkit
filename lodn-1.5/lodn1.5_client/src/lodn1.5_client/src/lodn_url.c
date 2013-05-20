/*
 *   Copyright (C) 2009 Christopher Brumgard
 *
 *   This file is part of lodn1.5_client.
 *
 *   lodn_client is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   lodn1.5_client is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with lodn1.5_client.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/param.h>

#include "jval.h"
#include "jrb.h"

#include "lodn_client/lodn_errno.h"


#define DEFAULT_PORT  5000

#define PROTOCOL	"lodn://"


static void _free_options(JRB options)
{
	/* Declarations */
	JRB node;

	/* Frees all of the node and their keys and vals*/
	jrb_traverse(node, options)
	{
		free(node->key.s);
		free(node->val.s);
	}

	/* Free the trees */
	jrb_free_tree(options);
}


static int _check_hostname(char *host)
{
	/* Declarations */
	int host_len;
	int i;

	/* Check that the hostname is valid */
	host_len = strlen(host);

	/* Checks that the host doesn't start or end with - */
	if(host[0] == '-' || host[0] == '.' || host[host_len-1] == '-' ||
	   host[host_len-1] == '.')
	{
		lodn_errno = LODN_EINVALIDHOST;
		return -1;
	}

	/* Checks that there isn't repeated .' */
	for(i=0; i<host_len; i++)
	{
		if(host[i] == '.' && host[i+1] == '.')
		{
			lodn_errno = LODN_EINVALIDHOST;
			return -1;
		}
	}

	/* Return success */
	return 0;
}


static int _check_path(char *path)
{
	/* Checks the path starts with '/' */
	if(path[0] != '/')
	{
		lodn_errno = LODN_EINVALIDPATH;
		return -1;
	}

	/* Returns success */
	return 0;
}

static JRB _parse_options(char *options_str)
{
	/* Declarations */
	char *opt_ptr	= NULL;
	char *field_buf = NULL;
	char *value_buf = NULL;
	char *field     = NULL;
	char *value     = NULL;
	JRB options     = NULL;
	JRB  node		= NULL;
	int opt_len;


	/* Gets the length of the options */
	opt_len = strlen(options_str)+1;

	/* Allocates enough space to hold the field and value */
	if(((field_buf = (char*)malloc(opt_len*sizeof(char))) == NULL) ||
		((value_buf = (char*)malloc(opt_len*sizeof(char))) == NULL))
	{
		lodn_errno = LODN_EMEM;
		goto ERROR_HANDLER;
	}

	/* Allocate a jrb tree for holding the options */
	if((options = make_jrb()) == NULL)
	{
		lodn_errno = LODN_EMEM;
		goto ERROR_HANDLER;
	}

	/* Iterates over the option string dividing up the individual options */
	for(opt_ptr = strsep(&options_str, ";"); opt_ptr != NULL;
		opt_ptr = strsep(&options_str, ";"))
	{
		/* Skip empty */
		if(opt_ptr[0] != '\0')
		{
			/* Splits the option into the field and the value */
			if(sscanf(opt_ptr, "%[^= ]=%[^= ]", field_buf, value_buf) != 2)
			{
				lodn_errno = LODN_EINVALIDPARAM;
				goto ERROR_HANDLER;
			}

			/* Allocate a copy of field_buf for storage */
			if((field = strdup(field_buf)) == NULL)
			{
				lodn_errno = LODN_EMEM;
				goto ERROR_HANDLER;
			}

			/* Allocage a copy of value_buf for storage */
			if((value = strdup(value_buf)) == NULL)
			{
				lodn_errno = LODN_EMEM;
				goto ERROR_HANDLER;
			}

			/* No previous entry so insert into jrb tree */
			if((node = jrb_find_str(options, field)) == NULL)
			{
				if((jrb_insert_str(options, field, new_jval_s(value))) == NULL)
				{
					lodn_errno = LODN_EMEM;
					goto ERROR_HANDLER;
				}

			/* Overwrite previous entry */
			}else
			{
				free(node->key.s);
				free(node->val.s);

				node->key.s = field;
				node->val.s = value;
			}

			/* Clear the pointers so they won't get freed by accident */
			field = NULL;
			value = NULL;
		}
	}

	/* Frees the temporary resources */
	free(field_buf);
	free(value_buf);


	/* Return success */
	return options;


	/* Error hander */
	ERROR_HANDLER:

		/* Free all of the resources */
		if(field_buf != NULL)
		{
			free(field_buf);

			if(value_buf != NULL)
			{
				free(value_buf);

				if(options != NULL)
				{
					_free_options(options);
				}

				if(field != NULL)
				{
					free(field);

					if(value != NULL)
					{
						free(value);
					}
				}
			}
		}

		/* Return failure */
		return NULL;
}

int lodn_parse_url(char *url, char **host, int *port, char **path,
					  JRB *options)
{
	/* Declarations */
	char *options_str;
	char port_str[6] = { '\0' };
	size_t url_length;
	int char_consumed = -1;
	int val;


	/* Initialize the errno value */
	lodn_errno = LODN_OK;

	/* Initialize the output variables */
	*host       = NULL;
	*port       = DEFAULT_PORT;
	*path       = NULL;
	options_str = NULL;

	/* Gets the length of the string */
	url_length = strlen(url);

	/* Allocate space for the hostname */
	if((*host = (char*)malloc(((MAXHOSTNAMELEN < url_length) ? MAXHOSTNAMELEN :
											url_length) * sizeof(char)))== NULL)
	{
		lodn_errno = LODN_EMEM;
		goto ERROR_HANDLER;
	}

	/* Allocate space for the path name */
	if((*path = (char*)malloc(url_length * sizeof(char))) == NULL)
	{
		lodn_errno = LODN_EMEM;
		goto ERROR_HANDLER;
	}

	/* Initializes the host and path cstrings */
	(*host)[0] = '\0';
	(*path)[0] = '\0';


	/* URL to look for lodn://hostname[:port]/path[?options] */

	/* Checks for lodn://hostname:port/path?options */
	if((val=sscanf(url, "lodn://%255[a-z0-9.-]:%5[0-9]%[^; ]%n",
			  *host, port_str, *path, &char_consumed)) < 3 || (*path)[0] != '/')
	{

		/* Checks for lodn://hostname/path?options */
		if(sscanf(url, "lodn://%255[a-z0-9.-]%[^; ]%n", *host, *path,
					&char_consumed) < 2)
		{
			lodn_errno = LODN_EINVALIDURL;
			goto ERROR_HANDLER;
		}
	}else
	{
		/* Convert the port to a number */
		if(sscanf(port_str, "%d", port) != 1)
		{
			lodn_errno = LODN_EINVALIDPORT;
			goto ERROR_HANDLER;
		}
	}


	/* Checks the hostname */
	if(_check_hostname(*host) != 0)
	{
		lodn_errno = LODN_EINVALIDHOST;
		goto ERROR_HANDLER;
	}

	/* Check that the port is valid */
	if(*port < 0 || *port > 65536)
	{
		lodn_errno = LODN_EINVALIDPORT;
		goto ERROR_HANDLER;
	}

	/* Checks that path is valid */
	if(_check_path(*path) != 0)
	{
		lodn_errno = LODN_EINVALIDPATH;
		goto ERROR_HANDLER;
	}

	/* Check for options */
	if(url[char_consumed] == ';')
	{
		/* Allocate space for holding the options */
		if((options_str = (char*)malloc((url_length-char_consumed)*sizeof(char)))
				== NULL)
		{
			lodn_errno = LODN_EMEM;
			goto ERROR_HANDLER;
		}

		/* Grab the options */
		if(sscanf(url+char_consumed, ";%[^ ]", options_str) != 1)
		{
			lodn_errno = LODN_EINVALIDPARAM;
			goto ERROR_HANDLER;
		}

		/* Checks that length of options is correct */
		if(char_consumed + strlen(options_str) +1 != url_length)
		{
			lodn_errno = LODN_EINVALIDURL;
			goto ERROR_HANDLER;
		}

		/* Parses the options */
		if((*options = _parse_options(options_str)) == NULL)
		{
			if(lodn_errno == LODN_OK)
			{
				lodn_errno = LODN_EINVALIDPARAM;
			}

			goto ERROR_HANDLER;
		}
	}


	/* Free temporary resources */
	free(options_str);

	/* Return successfully */
	return 0;


	/* Handle any errors */
	ERROR_HANDLER:

		if(*host != NULL)
		{
			free(*host);
			*host = NULL;

			if(*path != NULL)
			{
				free(*path);
				*path = NULL;

				if(options_str != NULL)
				{
					free(options_str);

					if(*options != NULL)
					{
						_free_options(*options);
					}
				}
			}
		}

		/* Return failure */
		return -1;
}


int test_main(int argc, char *argv[])
{
	/* Declarations */
	char *host;
	int   port;
	char *path;
	JRB   options;
	JRB   node;

	if(lodn_parse_url(argv[1], &host, &port, &path, &options) != 0)
	{
		printf("Error parsing result\n");
	}

	printf("Host: %s\n",   host);
	printf("Port: %d\n",   port);
	printf("Path: %s\n",   path);

	if(options != NULL)
	{
		jrb_traverse(node, options)
		{
			printf("Option[%s] = %s\n", node->key.s, node->val.s);
		}
	}

	return EXIT_SUCCESS;
}
