#include <stdlib.h>
#include <string.h>

#include "crclib.h"

static int Test_HashKey( void )
{
	if( COM_HashKey( "", 64 ) != 5u )
		return 1;

	if( COM_HashKey( "ABC", 64 ) != 43u )
		return 2;

	if( COM_HashKey( "ABC", 4096 ) != COM_HashKey( "abc", 4096 ))
		return 3;

	if( COM_HashKey( "AbC/Path-01", 31 ) != 20u )
		return 4;

	if( COM_HashKey( "sound/weapons/pl_gun3.wav", 4096 ) != 989u )
		return 5;

	if( COM_HashKey( "textures/{BLUE", 4096 ) != 347u )
		return 6;

	if( COM_HashKey( "ABC", 0 ) != 193450027u )
		return 7;

	return 0;
}

static int Test_CRC32( void )
{
	const byte digits[] = "123456789";
	const byte name[] = "xash3d-fwgs";
	uint32_t crc;
	int i;

	CRC32_Init( &crc );
	if( crc != CRC32_INIT_VALUE )
		return 1;

	CRC32_ProcessBuffer( &crc, digits, 9 );
	if( crc != 0x340BC6D9u )
		return 2;

	if( CRC32_Final( crc ) != 0xCBF43926u )
		return 3;

	CRC32_Init( &crc );
	for( i = 0; i < 9; ++i )
		CRC32_ProcessByte( &crc, digits[i] );

	if( CRC32_Final( crc ) != 0xCBF43926u )
		return 4;

	CRC32_Init( &crc );
	CRC32_ProcessBuffer( &crc, name, 11 );
	if( crc != 0xBB60C42Fu || CRC32_Final( crc ) != 0x449F3BD0u )
		return 5;

	CRC32_Init( &crc );
	CRC32_ProcessBuffer( &crc, "", 0 );
	if( CRC32_Final( crc ) != 0u )
		return 6;

	return 0;
}

static int Test_CRC32BlockSequence( void )
{
	byte empty[] = "";
	byte abc[] = "abc";
	byte longData[] =
		"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

	if( CRC32_BlockSequence( empty, 0, 0 ) != 28u )
		return 1;

	if( CRC32_BlockSequence( abc, 3, 17 ) != 183u )
		return 2;

	if( CRC32_BlockSequence( abc, 3, -7 ) != 163u )
		return 3;

	if( CRC32_BlockSequence( longData, 62, 0 ) != 103u )
		return 4;

	if( CRC32_BlockSequence( abc, 3, 1020 ) != CRC32_BlockSequence( abc, 3, 0 ))
		return 5;

	return 0;
}

static int Test_MD5( void )
{
	MD5Context_t ctx;
	byte digest[16];
	const byte abc[] = "abc";

	MD5Init( &ctx );
	MD5Final( digest, &ctx );
	if( strcmp( MD5_Print( digest ), "D41D8CD98F00B204E9800998ECF8427E" ))
		return 1;

	MD5Init( &ctx );
	MD5Update( &ctx, abc, 3 );
	MD5Final( digest, &ctx );
	if( strcmp( MD5_Print( digest ), "900150983CD24FB0D6963F7D28E17F72" ))
		return 2;

	return 0;
}

int main( void )
{
	int result = Test_HashKey();

	if( result )
		return result;

	result = Test_CRC32();
	if( result )
		return result + 16;

	result = Test_CRC32BlockSequence();
	if( result )
		return result + 32;

	result = Test_MD5();
	if( result )
		return result + 48;

	return EXIT_SUCCESS;
}
