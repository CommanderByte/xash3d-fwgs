/*
crclib.c - generate crc stuff
Copyright (C) 2007 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include <string.h>
#include <stdlib.h>
#include "crclib.h"
#include "crtlib.h"
#include "utilities/compat/checksum_adapter.h"

void GAME_EXPORT CRC32_ProcessByte( uint32_t *pulCRC, byte ch )
{
	const uint32_t *crc32table = Xash_Crc32Table();
	uint32_t	ulCrc = *pulCRC;

	*pulCRC = crc32table[((byte)ulCrc ^ ch)] ^ (ulCrc >> 8);
}

void GAME_EXPORT CRC32_ProcessBuffer( uint32_t *pulCRC, const void *pBuffer, int nBuffer )
{
	const uint32_t *crc32table = Xash_Crc32Table();
	uint32_t	ulCrc = *pulCRC, tmp;
	byte	*pb = (byte *)pBuffer;

	while( nBuffer >= sizeof( uint64_t ))
	{
		memcpy( &tmp, pb, sizeof( tmp ));
		ulCrc ^= LittleLong( tmp );
		ulCrc  = crc32table[(byte)ulCrc] ^ (ulCrc >> 8);
		ulCrc  = crc32table[(byte)ulCrc] ^ (ulCrc >> 8);
		ulCrc  = crc32table[(byte)ulCrc] ^ (ulCrc >> 8);
		ulCrc  = crc32table[(byte)ulCrc] ^ (ulCrc >> 8);
		memcpy( &tmp, pb + sizeof( tmp ), sizeof( tmp ));
		ulCrc ^= LittleLong( tmp );
		ulCrc  = crc32table[(byte)ulCrc] ^ (ulCrc >> 8);
		ulCrc  = crc32table[(byte)ulCrc] ^ (ulCrc >> 8);
		ulCrc  = crc32table[(byte)ulCrc] ^ (ulCrc >> 8);
		ulCrc  = crc32table[(byte)ulCrc] ^ (ulCrc >> 8);
		nBuffer -= sizeof( uint64_t );
		pb += sizeof( uint64_t );
	}

	if( nBuffer & sizeof( uint32_t ))
	{
		memcpy( &tmp, pb, sizeof( tmp ));
		ulCrc ^= LittleLong( tmp );
		ulCrc  = crc32table[(byte)ulCrc] ^ (ulCrc >> 8);
		ulCrc  = crc32table[(byte)ulCrc] ^ (ulCrc >> 8);
		ulCrc  = crc32table[(byte)ulCrc] ^ (ulCrc >> 8);
		ulCrc  = crc32table[(byte)ulCrc] ^ (ulCrc >> 8);
		nBuffer -= sizeof( uint32_t );
		pb += sizeof( uint32_t );
	}

	while( nBuffer-- )
	{
		ulCrc  = crc32table[((byte)ulCrc ^ *pb++)] ^ (ulCrc >> 8);
	}

	*pulCRC = ulCrc;
}

/*
====================
CRC32_BlockSequence

For proxy protecting
====================
*/
byte CRC32_BlockSequence( byte *base, int length, int sequence )
{
	const uint32_t *crc32table = Xash_Crc32Table();
	uint32_t	CRC;
	char	buffer[64];
	int	off;
	uint32_t	le[2];

	if( sequence < 0 ) sequence = abs( sequence );

	if( length > 60 ) length = 60;
	memcpy( buffer, base, length );

	off = sequence % 0x3FC;
	le[0] = LittleLong( crc32table[off / 4] );
	le[1] = LittleLong( crc32table[off / 4 + 1] );
	memcpy( buffer + length, (char *)le + ( off & 3 ), 4 );

	length += 4;

	CRC32_Init( &CRC );
	CRC32_ProcessBuffer( &CRC, buffer, length );
	CRC = CRC32_Final( CRC );

	return (byte)CRC;
}
