//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "milesbase.h"
#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static int s_MilesRefCount = 0;


void IncrementRefMiles()
{
	if(s_MilesRefCount == 0)
	{
#ifdef WIN32
		AIL_set_redist_directory( "." );
#elif defined( OSX )
		AIL_set_redist_directory( "osx32" );
#elif defined( LINUX )
#ifdef PLATFORM_64BITS
		AIL_set_redist_directory( "bin/linux64" );
#else
		AIL_set_redist_directory( "bin/linux32" );
#endif
#else
		Assert( !"Using default MSS_REDIST_DIR_NAME - this will most likely fail." );
		AIL_set_redist_directory( MSS_REDIST_DIR_NAME );
#endif

		AIL_startup();
	}
	
	++s_MilesRefCount;
}

void DecrementRefMiles()
{
	Assert(s_MilesRefCount > 0);
	--s_MilesRefCount;
	if(s_MilesRefCount == 0)
	{
		CProvider::FreeAllProviders();
		AIL_shutdown();
	}
}


// ------------------------------------------------------------------------ //
// CProvider functions.
// ------------------------------------------------------------------------ //

CProvider *CProvider::s_pHead = NULL;


CProvider::CProvider( HPROVIDER hProvider )
{
	m_hProvider = hProvider;

	// Add to the global list of CProviders.
	m_pNext = s_pHead;
	s_pHead = this;
}


CProvider::~CProvider()
{
	RIB_free_provider_library( m_hProvider );

	// Remove from the global list.
	CProvider **ppPrev = &s_pHead;
	for ( CProvider *pCur=s_pHead; pCur; pCur=pCur->m_pNext )
	{
		if ( pCur == this )
		{
			*ppPrev = m_pNext;
			return;
		}

		ppPrev = &pCur->m_pNext;
	}
}


CProvider* CProvider::FindProvider( HPROVIDER hProvider )
{
	for ( CProvider *pCur=s_pHead; pCur; pCur=pCur->m_pNext )
	{
		if ( pCur->GetProviderHandle() == hProvider )
		{
			return pCur;
		}
	}
	return NULL;
}


void CProvider::FreeAllProviders()
{
	CProvider *pNext;
	for ( CProvider *pCur=s_pHead; pCur; pCur=pNext )
	{
		pNext = pCur->m_pNext;
		delete pCur;
	}
}


HPROVIDER CProvider::GetProviderHandle()
{
	return m_hProvider;
}


// ------------------------------------------------------------------------ //
// ASISTRUCT functions.
// ------------------------------------------------------------------------ //
ASISTRUCT::ASISTRUCT()
{
	Clear();
	IncrementRefMiles();
}

ASISTRUCT::~ASISTRUCT()
{
	Shutdown();
	DecrementRefMiles();
}


void ASISTRUCT::Clear()
{
	m_pProvider = NULL;
	ASI_stream_open = NULL;
	ASI_stream_process = NULL;
	ASI_stream_close = NULL;
	ASI_stream_property = NULL;
	ASI_stream_seek = NULL;
	OUTPUT_BITS = NULL;
	OUTPUT_CHANNELS = NULL;
	INPUT_BITS = NULL;
	INPUT_CHANNELS = NULL;
	POSITION = NULL;
	m_stream = NULL;
}


bool ASISTRUCT::Init( void *pCallbackObject, const char *pInputFileType, const char *pOutputFileType, AILASIFETCHCB cb )
{
	// Get the provider.
	HPROVIDER hProvider = RIB_find_files_provider( "ASI codec", 
		"Output file types", pOutputFileType, "Input file types", pInputFileType );

	if ( !hProvider )
		return false;

	m_pProvider = CProvider::FindProvider( hProvider );
	if ( !m_pProvider )
	{
		m_pProvider = new CProvider( hProvider );
	}

	if ( !m_pProvider )
		return false;

	RIB_INTERFACE_ENTRY ASISTR[] =
	{
		FN( ASI_stream_property ),
		FN( ASI_stream_open ),
		FN( ASI_stream_close ),
		FN( ASI_stream_process ),

		PR( "Output sample rate",       OUTPUT_RATE ),
		PR( "Output sample width",      OUTPUT_BITS ),
		PR( "Output channels",          OUTPUT_CHANNELS ),

		PR( "Input sample rate",        INPUT_RATE ),
		PR( "Input sample width",       INPUT_BITS ),
		PR( "Input channels",           INPUT_CHANNELS ),

		PR( "Minimum input block size", INPUT_BLOCK_SIZE ),
		PR( "Position",                 POSITION ),
	};

	RIBRESULT result = RIB_request( m_pProvider->GetProviderHandle(), "ASI stream", ASISTR );
	if(result != RIB_NOERR)
		return false;

	
	// This function doesn't exist for the voice DLLs, but it's not fatal in that case.
	RIB_INTERFACE_ENTRY seekFn[] = 
	{
		FN( ASI_stream_seek ),
	};
	result = RIB_request( m_pProvider->GetProviderHandle(), "ASI stream", seekFn );
	if(result != RIB_NOERR)
		ASI_stream_seek = NULL;




	m_stream = ASI_stream_open( (MSS_ALLOC_TYPE *)MSS_CALLBACK_ALIGNED_NAME( MSS_alloc_info ), 
								(MSS_FREE_TYPE *)MSS_CALLBACK_ALIGNED_NAME( MSS_free_info ),
								(UINTa)pCallbackObject, cb, 0);
	if(!m_stream)
		return false;

	return true;
}

void ASISTRUCT::Shutdown()
{
	if ( m_pProvider )
	{
		if (m_stream && ASI_stream_close)
		{
			ASI_stream_close(m_stream);
			m_stream = NULL;
		}

		//m_pProvider->Release();
		m_pProvider = NULL;
	}

	Clear();
}


int ASISTRUCT::Process( void *pBuffer, unsigned int bufferSize )
{
	return ASI_stream_process( m_stream, pBuffer, bufferSize );
}


bool ASISTRUCT::IsActive() const
{
	return m_stream != NULL ? true : false;
}


unsigned int ASISTRUCT::GetProperty( HPROPERTY hProperty )
{
	uint32 nValue = 0;
	if ( ASI_stream_property( m_stream, hProperty, &nValue, NULL, NULL ) )
	{
		return nValue;
	}
	return 0;
}

void ASISTRUCT::Seek( int position )
{
	if ( !ASI_stream_seek )
		Error( "ASI_stream_seek called, but it doesn't exist." );

	ASI_stream_seek( m_stream, (S32)position );
}


