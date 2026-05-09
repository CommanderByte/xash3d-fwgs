/*
android.c - android support for filesystem
Copyright (C) 2022 Velaron

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

#if XASH_ANDROID

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include "filesystem_internal.h"
#include "android_assets_backend_adapter.h"
#include "crtlib.h"
#include "xash3d_mathlib.h"
#include "common/com_strings.h"

#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <time.h>

struct android_assets_s
{
	string package_name;
	qboolean engine;
	AAssetManager *asset_manager;
	AAssetDir *dir;
	void *backend;
};

struct jni_methods_s
{
	JNIEnv *env;
	jobject activity;
	jclass activity_class;
	jmethodID getPackageName;
	jmethodID getCallingPackage;
	jmethodID getAssetsList;
	jmethodID getAssets;
} jni;

static void Android_GetAssetManager( android_assets_t *assets )
{
	jobject assetManager;

	assetManager = (*jni.env)->CallObjectMethod( jni.env, jni.activity, jni.getAssets, assets->engine );

	if( assetManager )
		assets->asset_manager = AAssetManager_fromJava( jni.env, assetManager );
	else if( assets->engine )
		Con_Reportf( S_WARN "Couldn't add engine assets!" );
}

static const char *Android_GetPackageName( qboolean engine )
{
	static string pkg;
	jstring resultJNIStr;
	const char *resultCStr;

	resultJNIStr = (*jni.env)->CallObjectMethod( jni.env, jni.activity, engine ? jni.getPackageName : jni.getCallingPackage );

	if( !resultJNIStr )
		return NULL;

	resultCStr = (*jni.env)->GetStringUTFChars( jni.env, resultJNIStr, NULL );
	Q_strncpy( pkg, resultCStr, sizeof( pkg ));
	(*jni.env)->ReleaseStringUTFChars( jni.env, resultJNIStr, resultCStr );
	(*jni.env)->DeleteLocalRef( jni.env, resultJNIStr );

	return pkg;
}

static void Android_ListDirectory( stringlist_t *list, const char *path, qboolean engine )
{
	jstring JStr = (*jni.env)->NewStringUTF( jni.env, path );
	jobjectArray JNIArray = (*jni.env)->CallObjectMethod( jni.env, jni.activity, jni.getAssetsList, engine, JStr );
	int JNIArraySize = (*jni.env)->GetArrayLength( jni.env, JNIArray );

	for( int i = 0; i < JNIArraySize; i++ )
	{
		jstring JNIStr = (*jni.env)->GetObjectArrayElement( jni.env, JNIArray, i );
		const char *CStr = (*jni.env)->GetStringUTFChars( jni.env, JNIStr, NULL );

		stringlistappend( list, (char *)CStr );
		(*jni.env)->ReleaseStringUTFChars( jni.env, JNIStr, CStr );
		(*jni.env)->DeleteLocalRef( jni.env, JNIStr );
	}

	(*jni.env)->DeleteLocalRef( jni.env, JNIArray );
	(*jni.env)->DeleteLocalRef( jni.env, JStr );
}

static void FS_CloseAndroidAssets( android_assets_t *assets )
{
	if( assets->dir )
		AAssetDir_close( assets->dir );

	Mem_Free( assets );
}

static android_assets_t *FS_LoadAndroidAssets( qboolean engine )
{
	android_assets_t *assets = Mem_Calloc( fs_mempool, sizeof( *assets ));

	assets->engine = engine;

	Android_GetAssetManager( assets );
	if( !assets->asset_manager )
	{
		Con_Printf( S_ERROR "%s: Can't get asset manager\n", __func__ );
		FS_CloseAndroidAssets( assets );
		return NULL;
	}

	assets->dir = AAssetManager_openDir( assets->asset_manager, "" );
	if( !assets->dir )
	{
		Con_Printf( S_ERROR "%s: Can't open root asset directory\n", __func__ );
		FS_CloseAndroidAssets( assets );
		return NULL;
	}

	return assets;
}

static int FS_FileTime_AndroidAssets_Legacy( searchpath_t *search, const char *filename )
{
	static time_t time;

	if( !time )
	{
		struct tm file_tm;

		strptime( g_buildcommit_date, "%Y-%m-%d %H:%M:%S", &file_tm );
		time = mktime( &file_tm );
	}

	return time;
}

static void FS_PrintInfo_AndroidAssets_Legacy( searchpath_t *search, char *dst, size_t size )
{
	Q_snprintf( dst, size, "%s", search->assets->package_name );
}

static void FS_Close_AndroidAssets_Legacy( searchpath_t *search )
{
	FS_CloseAndroidAssets( search->assets );
}

static void *FS_AndroidAssetsAlloc( void *context, size_t size, int clear )
{
	(void)context;
	return clear ? Mem_Calloc( fs_mempool, size ) : Mem_Malloc( fs_mempool, size );
}

static void FS_AndroidAssetsFree( void *context, void *memory )
{
	(void)context;
	Mem_Free( memory );
}

static stringlist_t *FS_AndroidAssetsListCreate( void *context )
{
	stringlist_t *list;

	(void)context;
	list = Mem_Calloc( fs_mempool, sizeof( *list ));
	stringlistinit( list );
	return list;
}

static void FS_AndroidAssetsListDirectory( void *context, stringlist_t *list, const char *path )
{
	searchpath_t *search = (searchpath_t *)context;
	Android_ListDirectory( list, path, search->assets->engine );
}

static void FS_AndroidAssetsListDestroy( void *context, stringlist_t *list )
{
	(void)context;
	stringlistfreecontents( list );
	Mem_Free( list );
}

static int FS_AndroidAssetsMatchPattern( void *context, const char *text, const char *pattern, int caseInsensitive )
{
	(void)context;
	return matchpattern( text, pattern, caseInsensitive );
}

static int FS_AndroidAssetsStringCount( void *context, stringlist_t *list )
{
	(void)context;
	return list ? list->numstrings : 0;
}

static const char *FS_AndroidAssetsStringAt( void *context, stringlist_t *list, int index )
{
	(void)context;
	return ( list && index >= 0 && index < list->numstrings ) ? list->strings[index] : NULL;
}

static void FS_AndroidAssetsAppend( void *context, stringlist_t *list, const char *text )
{
	(void)context;
	stringlistappend( list, text );
}

static fs_android_assets_search_runtime_t FS_MakeAndroidAssetsSearchRuntime( searchpath_t *search )
{
	fs_android_assets_search_runtime_t runtime;

	runtime.context = search;
	runtime.alloc = FS_AndroidAssetsAlloc;
	runtime.free = FS_AndroidAssetsFree;
	runtime.listCreate = FS_AndroidAssetsListCreate;
	runtime.listDirectory = FS_AndroidAssetsListDirectory;
	runtime.listDestroy = FS_AndroidAssetsListDestroy;
	runtime.matchPattern = FS_AndroidAssetsMatchPattern;
	runtime.stringCount = FS_AndroidAssetsStringCount;
	runtime.stringAt = FS_AndroidAssetsStringAt;
	runtime.append = FS_AndroidAssetsAppend;

	return runtime;
}

static void FS_Search_AndroidAssets_Legacy( searchpath_t *search, stringlist_t *list, const char *pattern, int caseinsensitive )
{
	fs_android_assets_search_runtime_t runtime = FS_MakeAndroidAssetsSearchRuntime( search );
	FS_AndroidAssetsBackend_SearchAssets( &runtime, list, pattern, caseinsensitive );
}

static void *FS_AndroidAssetsOpenAsset( void *context, const char *path, int mode )
{
	searchpath_t *search = (searchpath_t *)context;
	int asset_mode = AASSET_MODE_UNKNOWN;

	if( mode == 1 )
		asset_mode = AASSET_MODE_RANDOM;
	else if( mode == 2 )
		asset_mode = AASSET_MODE_BUFFER;

	return AAssetManager_open( search->assets->asset_manager, path, asset_mode );
}

static void FS_AndroidAssetsCloseAsset( void *context, void *asset )
{
	(void)context;
	if( asset )
		AAsset_close( (AAsset *)asset );
}

static fs_android_assets_find_runtime_t FS_MakeAndroidAssetsFindRuntime( searchpath_t *search )
{
	fs_android_assets_find_runtime_t runtime;

	runtime.context = search;
	runtime.openAsset = FS_AndroidAssetsOpenAsset;
	runtime.closeAsset = FS_AndroidAssetsCloseAsset;

	return runtime;
}

static int FS_FindFile_AndroidAssets_Legacy( struct searchpath_s *search, const char *path, char *fixedname, size_t len )
{
	fs_android_assets_find_runtime_t runtime = FS_MakeAndroidAssetsFindRuntime( search );
	return FS_AndroidAssetsBackend_FindAsset( &runtime, path, fixedname, len );
}

static void *FS_AndroidAssetsAllocFile( void *context )
{
	(void)context;
	return Mem_Calloc( fs_mempool, sizeof( file_t ));
}

static void FS_AndroidAssetsFreeFile( void *context, file_t *file )
{
	(void)context;
	Mem_Free( file );
}

static int FS_AndroidAssetsOpenFileDescriptor( void *context, void *asset, fs_offset_t *offset, fs_offset_t *length )
{
	(void)context;
	return AAsset_openFileDescriptor( (AAsset *)asset, offset, length );
}

static void FS_AndroidAssetsSetupFile( void *context, file_t *file, void *searchPath, int handle, fs_offset_t offset, fs_offset_t length )
{
	(void)context;
	file->handle = handle;
	file->offset = offset;
	file->real_length = length;
	file->position = 0;
	file->ungetc = EOF;
	file->searchpath = (searchpath_t *)searchPath;
}

static fs_android_assets_open_runtime_t FS_MakeAndroidAssetsOpenRuntime( searchpath_t *search )
{
	fs_android_assets_open_runtime_t runtime;

	runtime.context = search;
	runtime.allocFile = FS_AndroidAssetsAllocFile;
	runtime.freeFile = FS_AndroidAssetsFreeFile;
	runtime.openAsset = FS_AndroidAssetsOpenAsset;
	runtime.openFileDescriptor = FS_AndroidAssetsOpenFileDescriptor;
	runtime.closeAsset = FS_AndroidAssetsCloseAsset;
	runtime.setupFile = FS_AndroidAssetsSetupFile;

	return runtime;
}

static file_t *FS_OpenFile_AndroidAssets_Legacy( searchpath_t *search, const char *filename, const char *mode, int pack_ind )
{
	fs_android_assets_open_runtime_t runtime = FS_MakeAndroidAssetsOpenRuntime( search );

	(void)mode;
	(void)pack_ind;
	return FS_AndroidAssetsBackend_OpenAsset( &runtime, search, filename );
}

static fs_offset_t FS_AndroidAssetsLength( void *context, void *asset )
{
	(void)context;
	return AAsset_getLength( (AAsset *)asset );
}

static int FS_AndroidAssetsRead( void *context, void *asset, void *buffer, size_t size )
{
	(void)context;
	return AAsset_read( (AAsset *)asset, buffer, size );
}

static void FS_AndroidAssetsAllocationFailed( void *context, size_t size )
{
	(void)context;
	Con_Reportf( "%s: can't alloc %zu bytes, no free memory\n", __func__, size );
}

static fs_android_assets_load_runtime_t FS_MakeAndroidAssetsLoadRuntime( searchpath_t *search )
{
	fs_android_assets_load_runtime_t runtime;

	runtime.context = search;
	runtime.openAsset = FS_AndroidAssetsOpenAsset;
	runtime.length = FS_AndroidAssetsLength;
	runtime.read = FS_AndroidAssetsRead;
	runtime.closeAsset = FS_AndroidAssetsCloseAsset;
	runtime.allocationFailed = FS_AndroidAssetsAllocationFailed;

	return runtime;
}

static byte *FS_LoadAndroidAssetsFile_Legacy( searchpath_t *search, const char *path, int pack_ind, fs_offset_t *filesize, void *( *pfnAlloc )( size_t ), void ( *pfnFree )( void * ))
{
	fs_android_assets_load_runtime_t runtime = FS_MakeAndroidAssetsLoadRuntime( search );

	(void)pack_ind;
	return FS_AndroidAssetsBackend_LoadAsset( &runtime, path, filesize, pfnAlloc, pfnFree );
}

static void FS_Close_AndroidAssets_Hook( void *context )
{
	FS_Close_AndroidAssets_Legacy( (searchpath_t *)context );
}

static void FS_PrintInfo_AndroidAssets_Hook( void *context, char *dst, size_t size )
{
	FS_PrintInfo_AndroidAssets_Legacy( (searchpath_t *)context, dst, size );
}

static file_t *FS_OpenFile_AndroidAssets_Hook( void *context, const char *filename, const char *mode, int pack_ind )
{
	return FS_OpenFile_AndroidAssets_Legacy( (searchpath_t *)context, filename, mode, pack_ind );
}

static int FS_FileTime_AndroidAssets_Hook( void *context, const char *filename )
{
	return FS_FileTime_AndroidAssets_Legacy( (searchpath_t *)context, filename );
}

static int FS_FindFile_AndroidAssets_Hook( void *context, const char *path, char *fixedname, size_t len )
{
	return FS_FindFile_AndroidAssets_Legacy( (searchpath_t *)context, path, fixedname, len );
}

static void FS_Search_AndroidAssets_Hook( void *context, stringlist_t *list, const char *pattern, int caseinsensitive )
{
	FS_Search_AndroidAssets_Legacy( (searchpath_t *)context, list, pattern, caseinsensitive );
}

static byte *FS_LoadAndroidAssetsFile_Hook( void *context, const char *path, int pack_ind, fs_offset_t *filesize, void *( *pfnAlloc )( size_t ), void ( *pfnFree )( void * ))
{
	return FS_LoadAndroidAssetsFile_Legacy( (searchpath_t *)context, path, pack_ind, filesize, pfnAlloc, pfnFree );
}

static void FS_Close_AndroidAssets( searchpath_t *search )
{
	if( search->assets && search->assets->backend )
	{
		void *backend = search->assets->backend;
		search->assets->backend = NULL;
		FS_AndroidAssetsBackendBridge_Close( backend );
		FS_DestroyAndroidAssetsBackendBridge( backend );
		return;
	}

	FS_Close_AndroidAssets_Legacy( search );
}

static void FS_PrintInfo_AndroidAssets( searchpath_t *search, char *dst, size_t size )
{
	if( search->assets && search->assets->backend )
	{
		FS_AndroidAssetsBackendBridge_PrintInfo( search->assets->backend, dst, size );
		return;
	}

	FS_PrintInfo_AndroidAssets_Legacy( search, dst, size );
}

static file_t *FS_OpenFile_AndroidAssets( searchpath_t *search, const char *filename, const char *mode, int pack_ind )
{
	if( search->assets && search->assets->backend )
		return FS_AndroidAssetsBackendBridge_OpenFile( search->assets->backend, filename, mode, pack_ind );

	return FS_OpenFile_AndroidAssets_Legacy( search, filename, mode, pack_ind );
}

static int FS_FileTime_AndroidAssets( searchpath_t *search, const char *filename )
{
	if( search->assets && search->assets->backend )
		return FS_AndroidAssetsBackendBridge_FileTime( search->assets->backend, filename );

	return FS_FileTime_AndroidAssets_Legacy( search, filename );
}

static int FS_FindFile_AndroidAssets( struct searchpath_s *search, const char *path, char *fixedname, size_t len )
{
	if( search->assets && search->assets->backend )
		return FS_AndroidAssetsBackendBridge_FindFile( search->assets->backend, path, fixedname, len );

	return FS_FindFile_AndroidAssets_Legacy( search, path, fixedname, len );
}

static void FS_Search_AndroidAssets( searchpath_t *search, stringlist_t *list, const char *pattern, int caseinsensitive )
{
	if( search->assets && search->assets->backend )
	{
		FS_AndroidAssetsBackendBridge_Search( search->assets->backend, list, pattern, caseinsensitive );
		return;
	}

	FS_Search_AndroidAssets_Legacy( search, list, pattern, caseinsensitive );
}

static byte *FS_LoadAndroidAssetsFile( searchpath_t *search, const char *path, int pack_ind, fs_offset_t *filesize, void *( *pfnAlloc )( size_t ), void ( *pfnFree )( void * ))
{
	if( search->assets && search->assets->backend )
		return FS_AndroidAssetsBackendBridge_LoadFile( search->assets->backend, path, pack_ind, filesize, pfnAlloc, pfnFree );

	return FS_LoadAndroidAssetsFile_Legacy( search, path, pack_ind, filesize, pfnAlloc, pfnFree );
}

searchpath_t *FS_AddAndroidAssets_Fullpath( const char *path, int flags )
{
	searchpath_t *search;
	android_assets_t *assets = NULL;
	qboolean engine = true;

	if( !jni.getPackageName || !jni.getCallingPackage || !jni.getAssetsList || !jni.getAssets )
		return NULL;

	if( FBitSet( flags, FS_STATIC_PATH | FS_CUSTOM_PATH ))
		return NULL;

	if( FBitSet( flags, FS_GAMEDIR_PATH ) && Q_stricmp( GI->basedir, GI->gamefolder ))
		engine = false;

	assets = FS_LoadAndroidAssets( engine );

	if( !assets )
	{
		Con_Reportf( S_ERROR "%s: unable to load Android assets \"%s\"\n", __func__, Android_GetPackageName( engine ));
		return NULL;
	}

	Q_strncpy( assets->package_name, Android_GetPackageName( engine ), sizeof( assets->package_name ));

	search = Mem_Calloc( fs_mempool, sizeof( *search ));

	Q_strncpy( search->filename, assets->package_name, sizeof( search->filename ));
	search->assets = assets;
	search->type = SEARCHPATH_ANDROID_ASSETS;
	SetBits( search->flags, FS_NOWRITE_PATH | FS_CUSTOM_PATH );

	search->pfnPrintInfo = FS_PrintInfo_AndroidAssets;
	search->pfnClose = FS_Close_AndroidAssets;
	search->pfnOpenFile = FS_OpenFile_AndroidAssets;
	search->pfnFileTime = FS_FileTime_AndroidAssets;
	search->pfnFindFile = FS_FindFile_AndroidAssets;
	search->pfnSearch = FS_Search_AndroidAssets;
	search->pfnLoadFile = FS_LoadAndroidAssetsFile;

	{
		fs_android_assets_backend_hooks_t hooks;
		hooks.context = search;
		hooks.close = FS_Close_AndroidAssets_Hook;
		hooks.printInfo = FS_PrintInfo_AndroidAssets_Hook;
		hooks.openFile = FS_OpenFile_AndroidAssets_Hook;
		hooks.fileTime = FS_FileTime_AndroidAssets_Hook;
		hooks.findFile = FS_FindFile_AndroidAssets_Hook;
		hooks.search = FS_Search_AndroidAssets_Hook;
		hooks.loadFile = FS_LoadAndroidAssetsFile_Hook;
		search->assets->backend = FS_CreateAndroidAssetsBackendBridge( search, &hooks );
	}

	Con_Reportf( "Adding Android assets: %s\n", assets->package_name );

	return search;
}

void FS_InitAndroid( void )
{
	jmethodID getContext;

	jni.env = (JNIEnv *)Sys_GetNativeObject( "JNIEnv" );
	jni.activity_class = Sys_GetNativeObject( "ActivityClass" );

	if( !jni.env || !jni.activity_class )
	{
		Con_Reportf( S_WARN "%s: unable to get JNI env to load Android assets\n", __func__ );
		return;
	}

	getContext = (*jni.env)->GetStaticMethodID( jni.env, jni.activity_class, "getContext", "()Landroid/content/Context;" );
	jni.activity = (*jni.env)->CallStaticObjectMethod( jni.env, jni.activity_class, getContext );

	jni.getPackageName = (*jni.env)->GetMethodID( jni.env, jni.activity_class, "getPackageName", "()Ljava/lang/String;" );
	jni.getCallingPackage = (*jni.env)->GetMethodID( jni.env, jni.activity_class, "getCallingPackage", "()Ljava/lang/String;" );
	jni.getAssetsList = (*jni.env)->GetMethodID( jni.env, jni.activity_class, "getAssetsList", "(ZLjava/lang/String;)[Ljava/lang/String;" );
	jni.getAssets = (*jni.env)->GetMethodID( jni.env, jni.activity_class, "getAssets", "(Z)Landroid/content/res/AssetManager;" );

	if( !jni.getPackageName || !jni.getCallingPackage || !jni.getAssetsList || !jni.getAssets )
		Con_Reportf( S_WARN "%s: unable to find required JNI interfaces to load Android assets\n", __func__ );
}

#endif // XASH_ANDROID
