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


#include "protocol_envelope.h"




int lodn_MessageHeader_deserialize(unsigned char *data, MessageHeader *header)
{
	/* Declarations */


	/* Reads in the header from little endianness */
	header->type = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
	header->size = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);


	/* Returns success */
	return 0;
}

int lodn_MessageHeader_serialize(MessageHeader *header, unsigned char *data)
{
	/* Declarations */


	/* Writes the header out in little endian */
	data[0] = (header->type)     & 0xff;
	data[1] = (header->type>>8)  & 0xff;
	data[2] = (header->type>>16) & 0xff;
	data[3] = (header->type>>24) & 0xff;

	data[4] = (header->size)     & 0xff;
	data[5] = (header->size>>8)  & 0xff;
	data[6] = (header->size>>16) & 0xff;
	data[7] = (header->size>>24) & 0xff;

	/* Returns success */
	return 0;
}


