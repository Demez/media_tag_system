#include "main.h"
#include "sys_win32.h"


CLIPFORMAT g_drop_formats[] = {
	// CF_TEXT,   // dragging in a file path/url from discord or your internet browser
	CF_HDROP,  // file drop
};


constexpr ULONG        g_drop_format_count   = ARR_SIZE( g_drop_formats );

f_drag_drop_receive*   g_f_drag_drop_receive = nullptr;

static bool            TRIED_TO_DROP_ON_SELF = false;
static bool            DROP_CANCELLED_LAST = false;


bool STDMETHODCALLTYPE GetFilesFromDataObject( FORMATETC& fmtetc, IDataObject* pDataObj, std::vector< fs::path >& drop_files )
{
	if ( !g_f_drag_drop_receive )
		return false;

	//if ( app::in_drag_drop )
	//	return true;

	STGMEDIUM pmedium;
	HRESULT   ret = pDataObj->GetData( &fmtetc, &pmedium );

	if ( ret != S_OK )
		return false;

	DROPFILES* dropfiles = (DROPFILES*)GlobalLock( pmedium.hGlobal );
	HDROP      drop      = (HDROP)dropfiles;

	auto        fileCount     = DragQueryFileW( drop, 0xFFFFFFFF, NULL, NULL );

	std::string filepath_utf8;
	std::string ext;

	for ( UINT i = 0; i < fileCount; i++ )
	{
		wchar_t filepath[ MAX_PATH ];
		DragQueryFileW( drop, i, filepath, ARR_SIZE( filepath ) );
		// NOTE: maybe check if we can load this file here?
		// some callback function or ImageLoader_SupportsImage()?

		// TODO: DO ASYNC TO NOT LOCK UP FILE EXPLORER !!!!!!!!
		// 	if ( ImageLoader_SupportsImage( filepath ) )
		if ( fs_is_file( filepath ) )
		{
			filepath_utf8 = sys_path_to_string( filepath );
			ext.clear();
			fs_get_extension< char >( filepath_utf8, ext );

			e_media_type type;
			if ( !media_check_extension_fast( ext, type ) )
				continue;
		}
		
		drop_files.push_back( filepath );

		// for now, we only do the first file, no handling for anything else still
		break;
	}

	GlobalUnlock( pmedium.hGlobal );

	ReleaseStgMedium( &pmedium );

	return true;
}


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

	// ------------------------------------------------------------------------------------------------------------------------

	void get_drop_effect( DWORD* pdwEffect )
	{
		if ( drop_supported )
			*pdwEffect = DROPEFFECT_COPY;
		else
			*pdwEffect = DROPEFFECT_NONE;
	}

	HRESULT STDMETHODCALLTYPE DragEnter( IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect ) override
	{
		//if ( app::in_drag_drop )
		//	S_FALSE;

		// needed?
		// FORMATETC formats;
		// pDataObj->EnumFormatEtc( DATADIR_GET, formats );

		TRIED_TO_DROP_ON_SELF = false;

		if ( app::in_drag_drop )
			TRIED_TO_DROP_ON_SELF = true;

		send_frame_draw_event();

		FORMATETC fmtetc = { CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
		bool      valid  = IsValidClipboardType( pDataObj, fmtetc );
		if ( !valid )
			return S_FALSE;

		std::vector< fs::path > drop_files;

		if ( !GetFilesFromDataObject( fmtetc, pDataObj, drop_files ) )
			return S_FALSE;

		drop_supported = drop_files.size() > 0;
		get_drop_effect( pdwEffect );

		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE DragOver( DWORD grfKeyState, POINTL pt, DWORD* pdwEffect ) override
	{
		send_frame_draw_event();

		if ( app::in_drag_drop )
			TRIED_TO_DROP_ON_SELF = true;

		get_drop_effect( pdwEffect );

		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE DragLeave() override
	{
		send_frame_draw_event();

		//if ( app::in_drag_drop )
		//	S_FALSE;

		if ( app::in_drag_drop )
		{
			// TRIED_TO_DROP_ON_SELF = false;
			return S_OK;
		}

		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE Drop( IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect ) override
	{
		if ( app::in_drag_drop )
		{
			TRIED_TO_DROP_ON_SELF = true;
			return S_OK;
		}

		send_frame_draw_event();

		FORMATETC fmtetc = { CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
		bool      valid  = IsValidClipboardType( pDataObj, fmtetc );
		if ( !valid )
			return S_OK;

		std::vector< fs::path > drop_files;

		if ( !GetFilesFromDataObject( fmtetc, pDataObj, drop_files ) )
			return S_OK;

		// TODO: make async/non-blocking
		// use a new SDL user event instead, at least until we get fullscreen media loading on a job

		if ( drop_files.empty() )
			return S_OK;

		if ( !g_f_drag_drop_receive( drop_files ) )
			return S_OK;

		// this seems to behave weirdly lol
		// SetFocus( window_hwnd );

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


std::vector< window_drop_target* > g_drop_target;


// ------------------------------------------------------------------------------------------------------------------------
// Recieving Drag/Drops


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
		target->Release();
		return false;
	}

	g_drop_target.push_back( target );
	return true;
}


void drag_drop_remove( HWND hwnd )
{
	RevokeDragDrop( hwnd );

	for ( size_t i = 0; i < g_drop_target.size(); i++ )
	{
		if ( g_drop_target[ i ]->window_hwnd == hwnd )
		{
			g_drop_target[ i ]->Release();
			g_drop_target.erase( g_drop_target.begin() + i );
			return;
		}
	}

	printf( "Trying to remove Drag and Drop system that was not registered for any window!\n" );
}


// ------------------------------------------------------------------------------------------------------------------------
// Sending Drag/Drops


struct DropSourceNotify : public IDropSourceNotify
{
	LONG            ref = 1L;
	HWND            hwnd{};

	virtual HRESULT DragEnterTarget( HWND hwndTarget ) override
	{
		hwnd = hwndTarget;

		// does nothing???
		//if ( hwndTarget == g_main_hwnd )
		//	return S_FALSE;

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

		if ( uRet == 0 )
			delete this;

		return uRet;
	}
};


struct DropSource : public IDropSource
{
	LONG             ref = 1L;
	DropSourceNotify* notify{};
	DWORD            key = 0;

	DropSource()
	{
		notify = new DropSourceNotify;
	}

	~DropSource()
	{
		notify->Release();
	}

	virtual HRESULT QueryContinueDrag( BOOL fEscapePressed, DWORD grfKeyState ) override
	{
		if ( fEscapePressed )
			return DRAGDROP_S_CANCEL;

		if ( !( grfKeyState & key ) )
		{
			//if ( notify.hwnd == g_main_hwnd )
			//	return DRAGDROP_S_CANCEL;

			return DRAGDROP_S_DROP;
		}

		return S_OK;
	}

	virtual HRESULT GiveFeedback( DWORD dwEffect ) override
	{
		// TODO: use the deny cursor or whatever it's called
	//	if ( notify.hwnd == g_main_hwnd )
	//	{
	//		return S_OK;
	//	}

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
			return notify->QueryInterface( riid, ppvObject );
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

		if ( uRet == 0 )
			delete this;

		return uRet;

		// printf( "Release\n" );
		// return 0;
	}
};


// TODO: look at this
// https://learn.microsoft.com/en-us/windows/win32/shell/datascenarios?redirectedfrom=MSDN#dragging-and-dropping-shell-objects-asynchronously
// Async drag and drop, but not really needed for this kinda program i feel


// Start drag and drop of multiple files in the system shell, like dragging to another folder to copy, into discord, etc.
void sys_do_drag_drop_files( const std::vector< fs::path >& files, u32 sdl_mouse_btn )
{
	// for now, only the first file
	if ( files.empty() )
		return;

	IDataObject* file_obj = nullptr;
	if ( !sys_get_data_obj_for_files( files, file_obj ) )
		return;

	auto  source     = new DropSource;
	DWORD out_effect = 0;

	switch ( sdl_mouse_btn )
	{
		default:
		case SDL_BUTTON_LEFT:
			source->key = MK_LBUTTON;
			break;

		case SDL_BUTTON_RIGHT:
			source->key = MK_RBUTTON;
			break;
	}

	if ( DROP_CANCELLED_LAST )
		printf( "TEMP\n" );

	app::in_drag_drop = true;

	// printf( "DRAG DROP BEGIN\n" );

	printf( "drag and drop started!\n" );

	// SHDoDragDrop adds an IMAGE PREVIEW TO THE DRAG DROP AUTOMATICALLY
	// though, you may need to implement this yourself with IDragSourceHelper for other image types
	HRESULT res       = SHDoDragDrop( NULL, file_obj, source, DROPEFFECT_COPY, &out_effect );

	source->Release();

	app::in_drag_drop = false;  // Reset flag

	// EVIL FUCKING HACK:
	// if you do a drag and drop, but release the drag button while hovering over this app,
	// the window stops reciveing a lot of messages, and all hit tests
	// the titlebar and frame don't react at all, can't drag or resize the window
	// only user fix is to just click the window once, or unfocus and refocus the window
	// and i have tried to find a fix for it, but this is the best i can do unless a real, proper fix is done
	
//	SetForegroundWindow( GetNextWindow( g_main_hwnd, GW_HWNDNEXT ) );
//	SetForegroundWindow( g_main_hwnd );

	// better fix that doesn't look visually weird, and still fixes the issue
	INPUT input       = { 0 };
	input.type        = INPUT_MOUSE;
	input.mi.dwFlags  = MOUSEEVENTF_LEFTDOWN;
	input.mi.dx       = 0;
	input.mi.dy       = 0;
	SendInput( 1, &input, sizeof( INPUT ) );

	input.type        = INPUT_MOUSE;
	input.mi.dwFlags  = MOUSEEVENTF_LEFTUP;
	input.mi.dx       = 0;
	input.mi.dy       = 0;
	SendInput( 1, &input, sizeof( INPUT ) );

	DROP_CANCELLED_LAST = false;

	if ( res == S_OK )
	{
		printf( "drag and drop returned S_OK!\n" );
	}
	else if ( res == DRAGDROP_S_DROP )
	{
		printf( "drag and drop returned DRAGDROP_S_DROP!\n" );
	}
	else if ( res == DRAGDROP_S_CANCEL )
	{
		printf( "drag and drop returned DRAGDROP_S_CANCEL!\n" );
		DROP_CANCELLED_LAST = true;

		send_frame_draw_event();
	}
	// the page says E_UNSPEC, but that doesn't exist?
	else // if ( res == E_UNEXPECTED )
	{
		printf( "drag and drop returned E_UNEXPECTED!\n" );
		sys_print_last_error();
	}
}


// files have been dragged into this program, the drag and drop system will call this function when it recieves it
void sys_set_receive_drag_drop_func( f_drag_drop_receive* callback )
{
	g_f_drag_drop_receive = callback;
}

