/*
entry.cpp -- executable entry point to run Xash Engine
Copyright (C) 2011 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "port.h"
#include "build.h"
#include "launcher/application.hpp"
#include "launcher/engine_library.hpp"
#include "launcher/platform/win32_argv.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#if XASH_POSIX
#elif XASH_WIN32

extern "C"
{
// Enable NVIDIA High Performance Graphics while using Integrated Graphics.
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;

// Enable AMD High Performance Graphics while using Integrated Graphics.
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#else
#error // port me!
#endif

static xash::launcher::EngineLibrary engineLibrary;

static void Launch_Error( const char *szFmt, ... )
{
	static char	buffer[16384];	// must support > 1k messages
	va_list		args;

	va_start( args, szFmt );
	vsnprintf( buffer, sizeof(buffer), szFmt, args );
	va_end( args );

#if XASH_WIN32
	MessageBoxA( NULL, buffer, "Xash Error", MB_OK );
#else
	fprintf( stderr, "Xash Error: %s\n", buffer );
#endif

	exit( 1 );
}

static void Sys_ChangeGame( const char *progname )
{
	// presence of this function tells the engine to allow change game
	// but it's never called
	return;
}

static int Sys_Start( int argc, char **argv )
{
	char errorBuffer[16384];
	int ret = xash::launcher::RunApplication( argc, argv, engineLibrary, Sys_ChangeGame,
		errorBuffer, sizeof( errorBuffer ));

	if( ret < 0 )
		Launch_Error( "%s", errorBuffer );
	return ret;
}

#if !XASH_WIN32
int main( int argc, char **argv )
{
	return Sys_Start( argc, argv );
}
#else
int __stdcall WinMain( HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR cmdLine, int nShow )
{
	xash::launcher::platform::Win32Argv argv;

	if( !argv.capture())
		Launch_Error( "Unable to parse command line" );

	return Sys_Start( argv.argc(), argv.argv() );
}
#endif
