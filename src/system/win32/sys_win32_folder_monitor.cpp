#include "main.h"
#include "sys_win32.h"


#include <thread>
#include <Windows.h>


static bool     g_dir_watcher_running = true;
static bool     g_scan_path_changed   = false;
static bool     g_dir_changed         = false;


void            RefreshDirectory( LPCWSTR lpDir )
{
	// This is where you might place code to refresh your
	// directory listing, but not the subtree because it
	// would not be necessary.

	// _tprintf( TEXT( "Directory (%s) changed.\n" ), lpDir );
}


void RefreshTree( LPCWSTR lpDrive )
{
	// This is where you might place code to refresh your
	// directory listing, including the subtree.

	// _tprintf( TEXT( "Directory tree (%s) changed.\n" ), lpDrive );
}


void WatchDirectory( fs::path watch_path )
{
	DWORD  dwWaitStatus;
	HANDLE dwChangeHandles[ 2 ];

	// Watch the directory for file creation and deletion.
	dwChangeHandles[ 0 ] = FindFirstChangeNotification(
	  watch_path.c_str(),                                              // directory to watch
	  directory::recursive,                                            // do not watch subtree
	  FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE );  // watch file name and date modified changes

	if ( dwChangeHandles[ 0 ] == INVALID_HANDLE_VALUE )
	{
		printf( "ERROR: FindFirstChangeNotification function failed\n" );
		return;
	}

	fs::path parentPath  = watch_path.parent_path();

	// Watch the subtree for directory creation and deletion.
	dwChangeHandles[ 1 ] = FindFirstChangeNotification(
	  parentPath.c_str(),             // directory to watch
	  TRUE,                           // watch the subtree
	  FILE_NOTIFY_CHANGE_DIR_NAME );  // watch dir name changes

	if ( dwChangeHandles[ 1 ] == INVALID_HANDLE_VALUE )
	{
		printf( "ERROR: FindFirstChangeNotification function failed\n" );
		return;
	}

	// Make a final validation check on our handles.
	if ( ( dwChangeHandles[ 0 ] == NULL ) || ( dwChangeHandles[ 1 ] == NULL ) )
	{
		printf( "ERROR: Unexpected NULL from FindFirstChangeNotification\n" );
		return;
	}

	// Change notification is set. Now wait on both notification
	// handles and refresh accordingly.
	while ( !g_scan_path_changed && app::running )
	{
		if ( watch_path != directory::path )
			break;

		// Wait for notification.
		dwWaitStatus = WaitForMultipleObjects( 2, dwChangeHandles, FALSE, 500 );

		switch ( dwWaitStatus )
		{
			case WAIT_OBJECT_0:

				// A file was created, renamed, or deleted in the directory.
				// Refresh this directory and restart the notification.
				printf( "FolderMonitor: File was created, renamed, or deleted\n" );

				// RefreshDirectory( lpDir );
				g_dir_changed = true;
				if ( FindNextChangeNotification( dwChangeHandles[ 0 ] ) == FALSE )
				{
					printf( "ERROR: FindNextChangeNotification function failed\n" );
					return;
				}
				break;

			case WAIT_OBJECT_0 + 1:

				// A directory was created, renamed, or deleted.
				// Refresh the tree and restart the notification.
				printf( "FolderMonitor: Directory was created, renamed, or deleted\n" );

				// RefreshTree( lpDrive );
				// g_dir_changed = true;
				if ( FindNextChangeNotification( dwChangeHandles[ 1 ] ) == FALSE )
				{
					printf( "ERROR: FindNextChangeNotification function failed\n" );
					return;
				}
				break;

			case WAIT_TIMEOUT:
				break;

			default:
				printf( "ERROR: Unhandled dwWaitStatus\n" );
				break;
		}
	}
}


static void folder_monitor_worker()
{
	//SDL_Delay( 1000 );

	//fs::path current_path = directory::path;

	while ( g_dir_watcher_running )
	{
		// if ( g_scan_path_changed )
		// 	g_scan_path_changed = false;

		//if ( current_path != directory::path )
		//{
		//	g_scan_path_changed = true;
		//	current_path = directory::path;
		//}
		//
		//if ( !current_path.empty() )
			WatchDirectory( directory::path );

		//g_scan_path_changed = false;

		SDL_Delay( 100 );
	}
}


std::thread g_folder_mon_thread( folder_monitor_worker );


bool sys_folder_mon_init()
{
	return true;
}


void sys_folder_mon_shutdown()
{
	g_dir_watcher_running = false;
	g_folder_mon_thread.join();
}


bool sys_folder_mon_update_path()
{
	g_scan_path_changed = true;
	return true;
}


bool sys_folder_mon_changed()
{
	if ( g_dir_changed )
	{
		g_dir_changed = false;
		return true;
	}

	return false;
}

