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

#include "lodn.h"


int main(int argc, char *argv[])
{
	/* Declarations */
	lodn_status_t lodn_status;
	int port;


	/* Checks the command line */
	if(argc != 3)
	{
		fprintf(stderr, "Usage: %s host port\n", argv[0]);
		return EXIT_FAILURE;
	}

	/* Converts and checks port */
	if(sscanf(argv[2], "%d", &port)!=1 || port < 0 || port > 65536)
	{
		fprintf(stderr, "port is invalid\n");
		return EXIT_FAILURE;
	}

	/* Makes a lodn_getstatus call */
	if(lodn_getstatus("localhost", 5000, &lodn_status) != 0)
	{
		printf("Failure: %s\n", lodn_strerror(lodn_errno));
		return EXIT_FAILURE;
	}

	/* Print results */
	printf("LoDN Depot Version is Major: %d Minor: %d Subminor: %d\n",
			lodn_status->major, lodn_status->minor, lodn_status->subminor);


	/* Returns successfully */
	return EXIT_SUCCESS;
}
