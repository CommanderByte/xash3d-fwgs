/*
build.c - returns a engine build number
Copyright (C) 2010 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "crtlib.h"

/*
===============
Q_buildnum

returns days since Apr 1 2015
===============
*/
int Q_buildnum( void )
{
	static int b = 0;

	if( b ) return b;

	b = Q_buildnum_iso( g_buildcommit_date );

	return b;
}
