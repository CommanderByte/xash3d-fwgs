/*
infostring.cpp - network info strings
Copyright (C) 2008 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "engine/info_string.hpp"

#include <cstdarg>
#include <cstring>

extern "C"
{
#include "common.h"
}

using xash::engine::InfoStringIsValid;
using xash::engine::InfoStringIterator;
using xash::engine::InfoStringLookupBuffers;
using xash::engine::InfoStringPair;
using xash::engine::InfoStringRemoveKey;
using xash::engine::InfoStringRemovePrefixedKeys;
using xash::engine::InfoStringSetError;
using xash::engine::InfoStringSetResult;
using xash::engine::InfoStringSetValueForKey;
using xash::engine::InfoStringSetValueForStarKey;
using xash::engine::InfoStringValueForKey;

static qboolean Info_ReportSetResult( InfoStringSetResult result )
{
	switch( result.error )
	{
	case InfoStringSetError::Backslash:
		Con_Printf( S_ERROR "SetValueForKey: can't use keys or values with a \\\n" );
		break;
	case InfoStringSetError::Quote:
		Con_Printf( S_ERROR "SetValueForKey: can't use keys or values with a \"\n" );
		break;
	case InfoStringSetError::StarKey:
		Con_Printf( S_ERROR "Can't set *keys\n" );
		break;
	default:
		break;
	}

	return result.ok ? true : false;
}

/*
===============
Info_Print

printing current key-value pair
===============
*/
extern "C" void Info_Print( const char *s )
{
	InfoStringIterator iterator( s );
	InfoStringPair pair;

	while( iterator.next( pair ))
	{
		char key[xash::engine::kInfoStringMaxKeyValueSize];
		const size_t keyLength = std::strlen( pair.key );

		std::memcpy( key, pair.key, keyLength + 1 );

		if( keyLength < 20 )
		{
			std::memset( key + keyLength, ' ', 20 - keyLength );
			key[20] = '\0';
		}

		Con_Printf( "%s", key );

		if( !pair.hasValue )
		{
			Con_Printf( "(null)\n" );
			return;
		}

		Con_Printf( "%s\n", pair.value );
	}
}

/*
==============
Info_IsValid

check infostring for potential problems
==============
*/
extern "C" qboolean Info_IsValid( const char *s )
{
	return InfoStringIsValid( s ) ? true : false;
}

#if !XASH_DEDICATED
/*
==============
Info_WriteVars

==============
*/
extern "C" void Info_WriteVars( file_t *f )
{
	InfoStringIterator iterator( CL_Userinfo() );
	InfoStringPair pair;

	while( iterator.next( pair ))
	{
		if( !pair.hasValue )
			return;

		convar_t *pcvar = Cvar_FindVar( pair.key );

		if( !pcvar && pair.key[0] != '*' ) // don't store out star keys
			FS_Printf( f, "setinfo \"%s\" \"%s\"\n", pair.key, pair.value );
	}
}
#endif // XASH_DEDICATED

/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
extern "C" const char *GAME_EXPORT Info_ValueForKey( const char *s, const char *key )
{
	static InfoStringLookupBuffers buffers;
	return InfoStringValueForKey( s, key, buffers );
}

extern "C" qboolean GAME_EXPORT Info_RemoveKey( char *s, const char *key )
{
	return InfoStringRemoveKey( s, key ) ? true : false;
}

extern "C" void Info_RemovePrefixedKeys( char *start, char prefix )
{
	InfoStringRemovePrefixedKeys( start, prefix );
}

extern "C" qboolean Info_SetValueForStarKey( char *s, const char *key, const char *value, int maxsize )
{
	return Info_ReportSetResult( InfoStringSetValueForStarKey( s, key, value, maxsize ));
}

extern "C" qboolean Info_SetValueForKey( char *s, const char *key, const char *value, int maxsize )
{
	return Info_ReportSetResult( InfoStringSetValueForKey( s, key, value, maxsize ));
}

extern "C" qboolean Info_SetValueForKeyf( char *s, const char *key, int maxsize, const char *format, ... )
{
	char value[MAX_VA_STRING];
	va_list args;

	va_start( args, format );
	Q_vsnprintf( value, sizeof( value ), format, args );
	va_end( args );

	return Info_SetValueForKey( s, key, value, maxsize );
}
