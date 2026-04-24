#include "main.h"
#include "sys_win32.h"


// #include <WinUser.h>
#include <Windows.h>
#include <atlbase.h>
// #include <direct.h>
//#include <fileapi.h>
// #include <handleapi.h>
// #include <io.h>
// #include <memory>
#include <shlobj_core.h>
// #include <shlwapi.h>
#include <shellapi.h>
#include <windowsx.h>  // GET_X_LPARAM/GET_Y_LPARAM
#include <wtypes.h>    // HWND


ULONG g_drop_formats[] = {
	// CF_TEXT,   // dragging in a file path/url from discord or your internet browser
	CF_HDROP,  // file drop
};


constexpr ULONG      g_drop_format_count   = ARR_SIZE( g_drop_formats );

f_drag_drop_receive* g_f_drag_drop_receive = nullptr;


// NOTE: this code is carried over and modified from 2 past projects lmao
// i have no idea where the source of it came from anymore lol
struct window_drop_target : public IDropTarget
{
	HWND                    window_hwnd    = 0;
	LONG                    ref            = 0L;

	bool                    drop_supported = false;

	// ------------------------------------------------------------------------------------------------------------------------

	bool STDMETHODCALLTYPE IsValidClipboardType( IDataObject* pDataObj, FORMATETC& fmtetc )
	{
		ULONG lFmt;
		for ( lFmt = 0; lFmt < g_drop_format_count; lFmt++ )
		{
			fmtetc.cfFormat = g_drop_formats[ lFmt ];
			if ( pDataObj->QueryGetData( &fmtetc ) == S_OK )
				return true;
		}

		return false;
	}

	bool STDMETHODCALLTYPE GetFilesFromDataObject( FORMATETC& fmtetc, IDataObject* pDataObj, std::vector< fs::path >& drop_files )
	{
		if ( !g_f_drag_drop_receive )
			return false;

		STGMEDIUM pmedium;
		HRESULT   ret = pDataObj->GetData( &fmtetc, &pmedium );

		if ( ret != S_OK )
			return false;

		DROPFILES* dropfiles = (DROPFILES*)GlobalLock( pmedium.hGlobal );
		HDROP      drop      = (HDROP)dropfiles;

		auto       fileCount = DragQueryFileW( drop, 0xFFFFFFFF, NULL, NULL );

		for ( UINT i = 0; i < fileCount; i++ )
		{
			TCHAR filepath[ MAX_PATH ];
			DragQueryFile( drop, i, filepath, ARR_SIZE( filepath ) );
			// NOTE: maybe check if we can load this file here?
			// some callback function or ImageLoader_SupportsImage()?

			// TODO: DO ASYNC TO NOT LOCK UP FILE EXPLORER !!!!!!!!
			// 	if ( ImageLoader_SupportsImage( filepath ) )
			{
				drop_files.push_back( filepath );
			}
		}

		GlobalUnlock( pmedium.hGlobal );

		ReleaseStgMedium( &pmedium );

		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------------

	HRESULT STDMETHODCALLTYPE DragEnter( IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect ) override
	{
		// needed?
		// FORMATETC formats;
		// pDataObj->EnumFormatEtc( DATADIR_GET, formats );

		FORMATETC fmtetc = { CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
		bool      valid  = IsValidClipboardType( pDataObj, fmtetc );
		if ( !valid )
			return S_FALSE;

		std::vector< fs::path > drop_files;

		if ( !GetFilesFromDataObject( fmtetc, pDataObj, drop_files ) )
			return S_FALSE;

		if ( drop_files.empty() )
		{
			drop_supported = false;
			*pdwEffect     = DROPEFFECT_SCROLL;
			// return S_FALSE;
			return S_OK;
		}
		else
		{
			drop_supported = true;
			*pdwEffect     = DROPEFFECT_LINK;
			return S_OK;
		}
	}

	HRESULT STDMETHODCALLTYPE DragOver( DWORD grfKeyState, POINTL pt, DWORD* pdwEffect ) override
	{
		if ( drop_supported )
		{
			*pdwEffect = DROPEFFECT_LINK;
		}
		else
		{
			*pdwEffect = DROPEFFECT_SCROLL;  // actually shows the "invalid" cursor or whatever it's called
		}

		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE DragLeave() override
	{
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE Drop( IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect ) override
	{
		FORMATETC fmtetc = { CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
		bool      valid  = IsValidClipboardType( pDataObj, fmtetc );
		if ( !valid )
			return S_FALSE;

		std::vector< fs::path > drop_files;

		if ( !GetFilesFromDataObject( fmtetc, pDataObj, drop_files ) )
			return S_FALSE;

		// TODO: make async/non-blocking

		if ( drop_files.empty() )
			return S_FALSE;

		if ( !g_f_drag_drop_receive( drop_files ) )
			return S_FALSE;

		// SetFocus( aHWND );

		return S_OK;
	}

	// from IUnknown

	// this is not being called, huh
	HRESULT STDMETHODCALLTYPE QueryInterface( REFIID riid, void** ppvObject ) override
	{
		printf( "QueryInterface\n" );

		if ( riid == IID_IUnknown || riid == IID_IDropTarget )
		{
			AddRef();
			*ppvObject = this;
			return S_OK;
		}
		else
		{
			*ppvObject = 0;
			return E_NOINTERFACE;
		};
	}

	ULONG STDMETHODCALLTYPE AddRef() override
	{
		return ++ref;
		// printf( "AddRef\n" );
		// return 0;
	}

	ULONG STDMETHODCALLTYPE Release() override
	{
		ULONG uRet = --ref;
		if ( uRet == 0 )
			delete this;

		return uRet;

		// printf( "Release\n" );
		// return 0;
	}
};


std::vector< window_drop_target > g_drop_target;


// ------------------------------------------------------------------------------------------------------------------------


bool drag_drop_register( HWND hwnd )
{
	// window_drop_target& target = g_drop_target.emplace_back();
	window_drop_target* target = new window_drop_target;

	target->window_hwnd = hwnd;

	auto ret                   = RegisterDragDrop( hwnd, target );

	if ( ret != S_OK )
	{
		if ( ret == DRAGDROP_E_INVALIDHWND )
		{
		}

		printf( "RegisterDragDrop failed\n" );
		sys_print_last_error();
		return false;
	}

	return true;
}


void drag_drop_remove( HWND hwnd )
{
	RevokeDragDrop( hwnd );

	for ( size_t i = 0; i < g_drop_target.size(); i++ )
	{
		if ( g_drop_target[ i ].window_hwnd == hwnd )
		{
			g_drop_target.erase( g_drop_target.begin() + i );
			return;
		}
	}

	printf( "Trying to remove Drag and Drop system that was not registered for any window!\n" );
}


// ------------------------------------------------------------------------------------------------------------------------
// Public Functions


struct DropSourceNotify : public IDropSourceNotify
{
	LONG            ref = 0L;

	virtual HRESULT DragEnterTarget( HWND hwndTarget ) override
	{
		return S_OK;
	}

	virtual HRESULT DragLeaveTarget() override
	{
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE QueryInterface( REFIID riid, void** ppvObject ) override
	{
		// IID_IDropSourceNotify

		if ( riid == IID_IDropSourceNotify )
		{
			AddRef();
			*ppvObject = this;
			return S_OK;
		}
		else
		{
			*ppvObject = 0;
			return E_NOINTERFACE;
		};
	}

	ULONG STDMETHODCALLTYPE AddRef() override
	{
		return ++ref;
	}

	ULONG STDMETHODCALLTYPE Release() override
	{
		if ( ref == 0 )
			return 0;

		ULONG uRet = --ref;

		return uRet;
	}
};


struct DropSource : public IDropSource
{
	LONG             ref = 0L;
	DropSourceNotify notify{};

	virtual HRESULT QueryContinueDrag( BOOL fEscapePressed, DWORD grfKeyState ) override
	{
		if ( fEscapePressed )
			return DRAGDROP_S_CANCEL;

		if ( !(grfKeyState & MK_MBUTTON) )
			return DRAGDROP_S_DROP;

		return S_OK;
	}

	virtual HRESULT GiveFeedback( DWORD dwEffect ) override
	{
		return DRAGDROP_S_USEDEFAULTCURSORS;
	}

	// IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface( REFIID riid, void** ppvObject ) override
	{
		// IID_IDropSourceNotify

		if ( ppvObject == nullptr )
		{
			return E_POINTER;
		}

		if ( riid == IID_IUnknown || riid == IID_IDropSource )
		{
			AddRef();
			*ppvObject = this;
			return S_OK;
		}
		// https://gitlab.com/tortoisegit/tortoisegit/-/blob/master/src/Utils/DragDropImpl.cpp
		else if ( riid == IID_IDropSourceNotify )
		{
			return notify.QueryInterface( riid, ppvObject );
		}
		else
		{
			*ppvObject = 0;
			return E_NOINTERFACE;
		}
	}

	ULONG STDMETHODCALLTYPE AddRef() override
	{
		return ++ref;
		// printf( "AddRef\n" );
		// return 0;
	}

	ULONG STDMETHODCALLTYPE Release() override
	{
		if ( ref == 0 )
			return 0;

		ULONG uRet = --ref;

		return uRet;

		// printf( "Release\n" );
		// return 0;
	}
};


// Start drag and drop of multiple files in the system shell, like dragging to another folder to copy, into discord, etc.
void sys_do_drag_drop_files( const std::vector< fs::path >& files )
{
	// for now, only the first file
	if ( files.empty() )
		return;

	const wchar_t*         path_w = files[ 0 ].c_str();
	bool                   ret    = false;

	CComPtr< IDataObject > file_obj;

	if ( !SUCCEEDED( GetUIObjectOfFile( nullptr, path_w, IID_PPV_ARGS( &file_obj ) ) ) )
	{
		printf( "Failed to find file to drag!\n" );
		return;
	}

	DropSource source{};
	DWORD      out_effect = 0;

	app::in_drag_drop     = true;

	HRESULT res           = DoDragDrop( file_obj, &source, DROPEFFECT_COPY, &out_effect );

	app::in_drag_drop     = false;

	if ( res != DRAGDROP_S_DROP && res != DRAGDROP_S_CANCEL )
	{
		// the page says E_UNSPEC, but that doesn't exist?
		if ( res == E_UNEXPECTED )
		{
			printf( "drag and drop weird error\n" );
			sys_print_last_error();
		}
	}
}


// files have been dragged into this program, the drag and drop system will call this function when it recieves it
void sys_set_receive_drag_drop_func( f_drag_drop_receive* callback )
{
	g_f_drag_drop_receive = callback;
}

