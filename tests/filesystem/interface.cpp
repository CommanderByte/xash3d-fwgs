#include <stdlib.h>
#include <string.h>
#include "fs_test_common.h"
#include "VFileSystem009.h"

fs_test_module_t g_hModule;
pfnCreateInterface_t g_pfnCreateInterface;
fs_api_t g_fs;
fs_globals_t *g_nullglobals;

static bool LoadFilesystem()
{
	int temp = -1;

	g_nullglobals = NULL;
	if( !FS_TestLoadFilesystem( &g_hModule, &g_fs, &g_nullglobals ))
		return false;

	if( !g_nullglobals )
		return false;

	// check Valve-style interface existence
	g_pfnCreateInterface = reinterpret_cast<pfnCreateInterface_t>( FS_TEST_GET_PROC_ADDRESS( g_hModule, "CreateInterface" ));
	if( !g_pfnCreateInterface )
		return false;

	if( !g_pfnCreateInterface( FILESYSTEM_INTERFACE_VERSION, &temp ) || temp != 0 )
		return false;

	temp = -1;

	if( !g_pfnCreateInterface( FS_API_CREATEINTERFACE_TAG, &temp ) || temp != 0 )
		return false;

	return true;
}

static bool TestMissingInterface()
{
	int temp = -1;
	void *iface = g_pfnCreateInterface( "MissingInterface001", &temp );

	if( iface || temp != 1 )
		return false;

	return true;
}

static bool TestVFileSystemBehavior()
{
	int temp = -1;
	char version[16];
	float progress = -1.0f;
	bool override = false;
	int bufferSize = -1;
	IFileSystem *vfs = reinterpret_cast<IFileSystem *>( g_pfnCreateInterface( FILESYSTEM_INTERFACE_VERSION, &temp ));

	if( !vfs || temp != 0 )
		return false;

	memset( version, 0, sizeof( version ));
	vfs->GetInterfaceVersion( version, sizeof( version ));
	if( strcmp( version, "Stdio" ))
		return false;

	if( vfs->GetReadBuffer( NULL, &bufferSize, false ) || bufferSize != 0 )
		return false;

	if( vfs->GetWaitForResourcesProgress( 123, &progress, &override ) )
		return false;

	if( progress != 0.0f || !override )
		return false;

	if( !vfs->IsAppReadyForOfflinePlay( 0 ))
		return false;

	if( !vfs->IsFileImmediatelyAvailable( "anything" ))
		return false;

	return true;
}

static bool TestXashFileSystemCopyBehavior()
{
	int temp = -1;
	fs_api_t *copy = reinterpret_cast<fs_api_t *>( g_pfnCreateInterface( FS_API_CREATEINTERFACE_TAG, &temp ));

	if( !copy || temp != 0 )
		return false;

	if( copy == &g_fs )
		return false;

	if( copy->InitStdio != g_fs.InitStdio ||
		copy->LoadFile != g_fs.LoadFile ||
		copy->MountArchive_Fullpath != g_fs.MountArchive_Fullpath )
		return false;

	copy->InitStdio = NULL;
	copy = reinterpret_cast<fs_api_t *>( g_pfnCreateInterface( FS_API_CREATEINTERFACE_TAG, &temp ));

	if( !copy || temp != 0 )
		return false;

	if( copy->InitStdio != g_fs.InitStdio )
		return false;

	return true;
}

static bool TestFsApiFacadePointers()
{
	if( !g_fs.InitStdio ||
		!g_fs.ShutdownStdio ||
		!g_fs.AddGameHierarchy ||
		!g_fs.Search ||
		!g_fs.FindLibrary ||
		!g_fs.Open ||
		!g_fs.Write ||
		!g_fs.Read ||
		!g_fs.Seek ||
		!g_fs.Tell ||
		!g_fs.Eof ||
		!g_fs.Close ||
		!g_fs.FileLength ||
		!g_fs.MountArchive_Fullpath ||
		!g_fs.OpenFileFromArchive )
	{
		return false;
	}

	return true;
}

int main()
{
	if( !LoadFilesystem() )
		return EXIT_FAILURE;

	if( !TestMissingInterface() ||
		!TestVFileSystemBehavior() ||
		!TestXashFileSystemCopyBehavior() ||
		!TestFsApiFacadePointers() )
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
