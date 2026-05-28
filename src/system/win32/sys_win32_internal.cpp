#include "main.h"
#include "sys_win32.h"


// --------------------------------------------------------------------------------------------------------
// Internal String conversion functions


wchar_t* sys_to_wchar( const char* spStr, int sSize )
{
	wchar_t* spDst = (wchar_t*)malloc( ( sSize + 1 ) * sizeof( wchar_t ) );
	
	if ( !spDst )
		return nullptr;

	memset( spDst, 0, ( sSize + 1 ) * sizeof( wchar_t ) );

	MultiByteToWideChar( CP_UTF8, 0, spStr, -1, spDst, sSize );

	mem_add_item( e_mem_category_string, spDst, ( sSize + 1 ) * sizeof( wchar_t ), 1 );

	return spDst;
}


wchar_t* sys_to_wchar( const char* spStr )
{
	int size = MultiByteToWideChar( CP_UTF8, 0, spStr, -1, NULL, 0 );

	if ( size )
		return sys_to_wchar( spStr, size );

	return nullptr;
}


// Allows you to bypass the MAX_PATH limit in windows
wchar_t* sys_to_wchar_extended( const char* spStr, int sSize )
{
	wchar_t* spDst = (wchar_t*)malloc( ( sSize + 5 ) * sizeof( wchar_t ) );

	if ( !spDst )
		return nullptr;

	memset( spDst, 0, ( sSize + 1 ) * sizeof( wchar_t ) );
	wcscat( spDst, L"\\\\?\\" );

	MultiByteToWideChar( CP_UTF8, 0, spStr, -1, spDst + 4, sSize );

	mem_add_item( e_mem_category_string, spDst, ( sSize + 5 ) * sizeof( wchar_t ), 1 );

	return spDst;
}


// Allows you to bypass the MAX_PATH limit in windows
wchar_t* sys_to_wchar_extended( const char* spStr )
{
	int size = MultiByteToWideChar( CP_UTF8, 0, spStr, -1, NULL, 0 );

	if ( size )
		return sys_to_wchar_extended( spStr, size );

	return nullptr;
}


char* sys_to_utf8( const wchar_t* spStr, int sSize )
{
	char* spDst = (char*)malloc( ( sSize + 1 ) * sizeof( char ) );

	if ( !spDst )
		return nullptr;

	memset( spDst, 0, ( sSize + 1 ) * sizeof( char ) );

	WideCharToMultiByte( CP_UTF8, 0, spStr, -1, spDst, sSize, NULL, NULL );

	mem_add_item( e_mem_category_string, spDst, ( sSize + 1 ) * sizeof( char ), 1 );

	return spDst;
}


char* sys_to_utf8( const wchar_t* spStr )
{
	int size = WideCharToMultiByte( CP_UTF8, 0, spStr, -1, NULL, 0, NULL, NULL );

	if ( size )
		return sys_to_utf8( spStr, size );

	return nullptr;
}


// --------------------------------------------------------------------------------------------------------
// Shell Helpers


bool sys_get_data_obj_for_files( const std::vector< fs::path >& paths, IDataObject*& file_obj )
{
	//std::vector< PIDLIST_ABSOLUTE > absolute_pidls{};
	std::vector< PCUITEMID_CHILD >  child_pidls{};

	//absolute_pidls.reserve( paths.size() );
	child_pidls.reserve( paths.size() );

	HRESULT hr = E_FAIL;

	for ( const auto& path : paths )
	{
		PIDLIST_ABSOLUTE pidl = nullptr;
		HRESULT          hr   = SHParseDisplayName( path.c_str(), nullptr, &pidl, 0, nullptr );

		if ( SUCCEEDED( hr ) && pidl )
		{
			// store this to free it later
			//absolute_pidls.push_back( pidl );

			// same as absolute since desktop is root here
			child_pidls.push_back( reinterpret_cast< PCUITEMID_CHILD >( pidl ) );
		}
		else
		{
			goto end;
		}
	}

	if ( !child_pidls.empty() )
	{
		// https://stackoverflow.com/questions/34549603/display-file-properties-dialog-for-files-in-different-directories
		IShellFolder* pDesktop = nullptr;
		hr = SHGetDesktopFolder( &pDesktop );
		
		if ( FAILED( hr ) )
			goto end;

		hr = pDesktop->GetUIObjectOf( HWND_DESKTOP, static_cast< UINT >( child_pidls.size() ), (LPCITEMIDLIST*)child_pidls.data(), IID_IDataObject, NULL, (void**)&file_obj );
		
		// alternatively, you can also use SHCreateDataObject() or CIDLData_CreateFromIDArray() to create the IDataObject
		pDesktop->Release();
	}

end:
	for ( auto pidl : child_pidls )
		CoTaskMemFree( (PIDLIST_ABSOLUTE)pidl );

	// absolute_pidls.clear();
	child_pidls.clear();

	return SUCCEEDED( hr );
}


// GetUIObjectOfFile incorporated by reference
// https://web.archive.org/web/20140424230840/http://blogs.msdn.com/b/oldnewthing/archive/2004/09/21/231739.aspx
HRESULT GetUIObjectOfFile( HWND hwnd, LPCWSTR pszPath, REFIID riid, void** ppv )
{
	*ppv = NULL;
	HRESULT      hr;
	LPITEMIDLIST pidl;
	SFGAOF       sfgao;

	if ( SUCCEEDED( hr = SHParseDisplayName( pszPath, NULL, &pidl, 0, &sfgao ) ) )
	{
		IShellFolder* psf;
		LPCITEMIDLIST pidlChild;

		if ( SUCCEEDED( hr = SHBindToParent( pidl, IID_IShellFolder, (void**)&psf, &pidlChild ) ) )
		{
			hr = psf->GetUIObjectOf( hwnd, 1, &pidlChild, riid, NULL, ppv );
			psf->Release();
		}

		CoTaskMemFree( pidl );
	}

	return hr;
}

