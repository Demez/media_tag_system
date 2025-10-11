#pragma once


// we don't use a general u64, since this can be aligned by 4 bytes, and doesn't result in handle mismatches
#define HANDLE_GEN_32( name )                                      \
	struct name                                                    \
	{                                                              \
		u32  index;                                                \
		u32  generation;                                           \
                                                                   \
		bool operator!()                                           \
		{                                                          \
			return generation == 0;                                \
		}                                                          \
		operator bool() const                                      \
		{                                                          \
			return generation > 0;                                 \
		}                                                          \
	};                                                             \
                                                                   \
	namespace std                                                  \
	{                                                              \
	template<>                                                     \
	struct hash< name >                                            \
	{                                                              \
		size_t operator()( name const& handle ) const              \
		{                                                          \
			size_t value = ( hash< u32 >()( handle.index ) );      \
			value ^= ( hash< u32 >()( handle.generation ) );       \
			return value;                                          \
		}                                                          \
	};                                                             \
	}                                                              \
                                                                   \
	inline bool operator==( const name& a, const name& b )         \
	{                                                              \
		return a.index == b.index && a.generation == b.generation; \
	}


// validate a handle
template< typename HANDLE >
bool handle_list_valid( u32 capacity, u32* generation, HANDLE handle )
{
	if ( handle.index >= capacity )
		return false;

	return handle.generation != 0 && handle.generation == generation[ handle.index ];
}


// 32-bit handle list with generation support and ref counts
// TODO: should i have another layer of indirection with an index list so we can defragment the memory and reduce how much memory this takes up?
template< typename HANDLE, typename TYPE, u32 STEP_SIZE = 32 >
struct handle_list_32
{
	//u32   count;
	u32   capacity;
	TYPE* data;
	u32*  generation;
	bool* use_list;  // list of entries that are in use

	handle_list_32()
	{
	}

	~handle_list_32()
	{
		::free( data );
		::free( generation );
		::free( use_list );
	}

	bool allocate()
	{
		if ( util_array_extend( generation, capacity, STEP_SIZE ) )
		{
			::free( generation );
			return false;
		}

		if ( util_array_extend( data, capacity, STEP_SIZE ) )
		{
			::free( data );
			return false;
		}

		if ( util_array_extend( use_list, capacity, STEP_SIZE ) )
		{
			::free( use_list );
			return false;
		}

		capacity += STEP_SIZE;

		return true;
	}

	bool handle_valid( HANDLE s_handle )
	{
		return handle_list_valid( capacity, generation, s_handle );
	}

	bool create( HANDLE& s_handle, TYPE** s_type )
	{
		// Find a free handle
		u32 index = 0;
		for ( ; index < capacity; index++ )
		{
			// is this handle in use?
			if ( !use_list[ index ] )
				break;
		}

		if ( index == capacity && !allocate() )
			return false;

		use_list[ index ]   = true;

		s_handle.index      = index;
		s_handle.generation = ++generation[ index ];

		if ( s_type )
			*s_type = &data[ index ];

		return true;
	}

	void free( u32 index )
	{
		memset( &data[ index ], 0, sizeof( TYPE ) );
		use_list[ index ] = false;
	}

	void free( HANDLE& s_handle )
	{
		if ( !handle_valid( s_handle ) )
			return;

		memset( &data[ s_handle.index ], 0, sizeof( TYPE ) );
		use_list[ s_handle.index ] = false;
	}

	// use an existing handle, potentially useful for loading saves
	// though the generation index would be annoying
	// create_with_handle

	TYPE* get( HANDLE s_handle )
	{
		if ( !handle_valid( s_handle ) )
			return nullptr;

		return &data[ s_handle.index ];
	}
};


// 32-bit handle list with generation support
// TODO: should i have another layer of indirection with an index list so we can defragment the memory and reduce how much memory this takes up?
template< typename HANDLE, u32 STEP_SIZE = 32 >
struct handle_list_simple_32
{
	//u32   count;
	u32   capacity;
	u32*  generation;
	bool* use_list;  // list of entries that are in use

	handle_list_simple_32()
	{
	}

	~handle_list_simple_32()
	{
		free( generation );
		free( use_list );
	}

  private:
	bool allocate()
	{
		u32*  new_generation = util_array_extend( generation, capacity, STEP_SIZE );
		bool* new_use        = util_array_extend( use_list, capacity, STEP_SIZE );

		if ( !new_generation || !new_use )
		{
			free( new_generation );
			free( new_use );
			return false;
		}

		generation = new_generation;
		use_list   = new_use;

		return true;
	}

  public:
	bool handle_valid( HANDLE s_handle )
	{
		return handle_list_valid( capacity, generation, s_handle );
	}

	HANDLE create()
	{
		// Find a free handle
		u32 index = 0;
		for ( ; index < capacity; index++ )
		{
			// is this handle in use?
			if ( !use_list[ index ] )
				break;
		}

		if ( index == capacity && !allocate() )
			return {};

		use_list[ index ] = true;

		HANDLE handle;
		handle.index      = index;
		handle.generation = ++generation[ index ];

		return handle;
	}

	void free( u32 index )
	{
		use_list[ index ] = false;
	}
};

