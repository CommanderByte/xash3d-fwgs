/*
filesystem.h - engine FS
Copyright (C) 2003-2006 Mathieu Olivier
Copyright (C) 2000-2007 DarkPlaces contributors
Copyright (C) 2007 Uncle Mike
Copyright (C) 2015-2023 Xash3D FWGS contributors

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#ifndef FILESYSTEM_INTERNAL_H
#define FILESYSTEM_INTERNAL_H

/*
 * Compatibility umbrella for legacy filesystem sources.
 *
 * New adapters should include the narrower private headers directly:
 * - filesystem/compat/private/filesystem_private_types.h for layouts.
 * - filesystem/compat/private/filesystem_private_globals.h for globals.
 * - filesystem/compat/private/filesystem_private_memory.h for callbacks.
 * - filesystem/compat/private/filesystem_private_api.h for FS_* declarations.
 */
#include "filesystem/compat/private/filesystem_private_api.h"

#endif // FILESYSTEM_INTERNAL_H
