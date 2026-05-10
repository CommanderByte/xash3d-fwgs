#include <stdlib.h>
#include <string.h>

#include "utflib.h"

static int Test_DecodeUTF8( void )
{
	utfstate_t state = { 0 };

	if( Q_DecodeUTF8( &state, 'A' ) != 'A' )
		return 1;

	if( Q_DecodeUTF8( &state, 0xC3u ) != 0 )
		return 2;

	if( Q_DecodeUTF8( &state, 0xA9u ) != 0x00E9u )
		return 3;

	if( state.len != 0 )
		return 4;

	if( Q_DecodeUTF8( &state, 0xF8u ) != 0 )
		return 5;

	return 0;
}

static int Test_DecodeUTF16( void )
{
	utfstate_t state = { 0 };

	if( Q_DecodeUTF16( &state, 'Z' ) != 'Z' )
		return 1;

	if( Q_DecodeUTF16( &state, 0xD83Du ) != 0 )
		return 2;

	if( Q_DecodeUTF16( &state, 0xDE00u ) != 0x1F600u )
		return 3;

	if( state.len != 0 )
		return 4;

	if( Q_DecodeUTF16( &state, 0xD83Du ) != 0 )
		return 5;

	if( Q_DecodeUTF16( &state, 'x' ) != 0 || state.len != 0 )
		return 6;

	return 0;
}

static int Test_EncodeUTF8( void )
{
	char dst[4];

	if( Q_EncodeUTF8( dst, '$' ) != 1 || memcmp( dst, "$", 1 ))
		return 1;

	if( Q_EncodeUTF8( dst, 0x00A9u ) != 2 ||
		(unsigned char)dst[0] != 0xC2u ||
		(unsigned char)dst[1] != 0xA9u )
		return 2;

	if( Q_EncodeUTF8( dst, 0x20ACu ) != 3 ||
		(unsigned char)dst[0] != 0xE2u ||
		(unsigned char)dst[1] != 0x82u ||
		(unsigned char)dst[2] != 0xACu )
		return 3;

	if( Q_EncodeUTF8( dst, 0x1F600u ) != 4 ||
		(unsigned char)dst[0] != 0xF0u ||
		(unsigned char)dst[1] != 0x9Fu ||
		(unsigned char)dst[2] != 0x98u ||
		(unsigned char)dst[3] != 0x80u )
		return 4;

	return 0;
}

static int Test_UTF8Length( void )
{
	if( Q_UTF8Length( NULL ) != 0 )
		return 1;

	if( Q_UTF8Length( "" ) != 0 )
		return 2;

	if( Q_UTF8Length( "abc" ) != 3 )
		return 3;

	return 0;
}

static int Test_UTF16ToUTF8( void )
{
	const uint16_t src[] = { 'A', 0x00A9u, 0xD83Du, 0xDE00u, 0 };
	char dst[16];
	char tiny[3];

	if( Q_UTF16ToUTF8( dst, sizeof( dst ), src, 5 ) != 7 )
		return 1;

	if( (unsigned char)dst[0] != 'A' ||
		(unsigned char)dst[1] != 0xC2u ||
		(unsigned char)dst[2] != 0xA9u ||
		(unsigned char)dst[3] != 0xF0u ||
		(unsigned char)dst[4] != 0x9Fu ||
		(unsigned char)dst[5] != 0x98u ||
		(unsigned char)dst[6] != 0x80u ||
		dst[7] != '\0' )
		return 2;

	if( Q_UTF16ToUTF8( tiny, sizeof( tiny ), src, 5 ) != 1 || tiny[0] != 'A' || tiny[1] != '\0' )
		return 3;

	if( Q_UTF16ToUTF8( NULL, sizeof( dst ), src, 5 ) != 0 )
		return 4;

	return 0;
}

static int Test_CodepageConversions( void )
{
	if( Q_UnicodeToCP1251( 'A' ) != 'A' )
		return 1;

	if( Q_UnicodeToCP1251( 0x0410u ) != 0xC0u )
		return 2;

	if( Q_UnicodeToCP1251( 0x0401u ) != 0xA8u )
		return 3;

	if( Q_UnicodeToCP1251( 0x2603u ) != '?' )
		return 4;

	if( Q_UnicodeToCP1252( 0x00E9u ) != 0x00E9u )
		return 5;

	if( Q_UnicodeToCP1252( 0x20ACu ) != '?' )
		return 6;

	return 0;
}

int main( void )
{
	int ret = Test_DecodeUTF8();
	if( ret > 0 )
		return ret;

	ret = Test_DecodeUTF16();
	if( ret > 0 )
		return ret + 16;

	ret = Test_EncodeUTF8();
	if( ret > 0 )
		return ret + 32;

	ret = Test_UTF8Length();
	if( ret > 0 )
		return ret + 48;

	ret = Test_UTF16ToUTF8();
	if( ret > 0 )
		return ret + 64;

	ret = Test_CodepageConversions();
	if( ret > 0 )
		return ret + 80;

	return EXIT_SUCCESS;
}
