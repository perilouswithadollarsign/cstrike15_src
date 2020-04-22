//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "clientframe.h"
#include "packed_entity.h"
#include "framesnapshot.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CClientFrame::CClientFrame( CFrameSnapshot *pSnapshot )
{
	last_entity = 0;
	transmit_always = NULL;	// bit array used only by HLTV and replay client
	from_baseline = NULL;
	tick_count = pSnapshot->m_nTickCount;
	m_pSnapshot = NULL;
	SetSnapshot( pSnapshot );
	m_pNext = NULL;
}

CClientFrame::CClientFrame( int tickcount )
{
	last_entity = 0;
	transmit_always = NULL;	// bit array used only by HLTV and replay client
	from_baseline = NULL;
	tick_count = tickcount;
	m_pSnapshot = NULL;
	m_pNext = NULL;
}

CClientFrame::CClientFrame( void )
{
	last_entity = 0;
	transmit_always = NULL;	// bit array used only by HLTV and replay client
	from_baseline = NULL;
	tick_count = 0;
	m_pSnapshot = NULL;
	m_pNext = NULL;
}

void CClientFrame::Init( int tickcount )
{
	tick_count = tickcount;
}

void CClientFrame::Init( CFrameSnapshot *pSnapshot )
{
	tick_count = pSnapshot->m_nTickCount;
	SetSnapshot( pSnapshot );
}

CClientFrame::~CClientFrame()
{
	SetSnapshot( NULL );	// Release our reference to the snapshot.

	if ( transmit_always != NULL )
	{
		delete transmit_always;
		transmit_always = NULL;
	}
}

void CClientFrame::SetSnapshot( CFrameSnapshot *pSnapshot )
{
	if ( m_pSnapshot == pSnapshot )
		return;

	if( pSnapshot )
		pSnapshot->AddReference();

	if ( m_pSnapshot )
		m_pSnapshot->ReleaseReference();

	m_pSnapshot = pSnapshot;
}

void CClientFrame::CopyFrame( CClientFrame &frame )
{
	tick_count = frame.tick_count;	
	last_entity = frame.last_entity;
	
	SetSnapshot( frame.GetSnapshot() ); // adds reference to snapshot

	transmit_entity = frame.transmit_entity;

	if ( frame.transmit_always )
	{
		Assert( transmit_always == NULL );
		transmit_always = new CBitVec<MAX_EDICTS>;
		*transmit_always = *(frame.transmit_always);
	}
}

CClientFrame *CClientFrameManager::GetClientFrame( int nTick, bool bExact )
{
	if ( nTick < 0 )
		return NULL;

	CClientFrame *frame = m_Frames;
	CClientFrame *lastFrame = frame;

	while ( frame != NULL )
	{
		if ( frame->tick_count >= nTick  )
		{
			if ( frame->tick_count == nTick )
				return frame;
			
			if ( bExact )
				return NULL;

			return lastFrame;
		}

		lastFrame = frame;
		frame = frame->m_pNext;	
	}

	if ( bExact )
		return NULL;
	
	return lastFrame;
}

int	CClientFrameManager::CountClientFrames( void )
{
	int count = 0;

	CClientFrame *f = m_Frames;

	while ( f )
	{
		count++;
		f = f->m_pNext;
	}

	return count;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *cl -
//-----------------------------------------------------------------------------
int CClientFrameManager::AddClientFrame( CClientFrame *frame)
{
	Assert( frame->tick_count > 0 );

	if ( !m_Frames )
	{
		// first client frame at all
		m_Frames = frame;	
		return 1;
	}

	CClientFrame *f = m_Frames;

	int count = 1;

	while ( f->m_pNext )
	{
		f = f->m_pNext;	
		++count;
	}

	++count;
	f->m_pNext = frame;

	return count;
}

void CClientFrameManager::RemoveOldestFrame( void )
{
	CClientFrame *frame = m_Frames; // first

	if ( !frame )
		return;	// no frames at all

	m_Frames = frame->m_pNext; // unlink head

	// deleting frame will decrease global reference counter
	FreeFrame( frame );
}

void CClientFrameManager::DeleteClientFrames(int nTick)
{
	CClientFrame *frame = m_Frames; // first
	CClientFrame *prev = NULL;	  // last

	while ( frame )
	{
		// remove frame if tick small nTick
		// remove all frames if nTick == -1

		if ( (nTick < 0) || (frame->tick_count < nTick) )
		{
			// removed frame

			if ( prev )
			{
				prev->m_pNext = frame->m_pNext;
				// deleting frame will decrease global reference counter
				FreeFrame( frame );
				frame = prev->m_pNext;
			}
			else
			{
				m_Frames = frame->m_pNext;	
				FreeFrame( frame );
				frame = m_Frames;
			}
		}
		else
		{
			// go to next frame
			prev = frame;
			frame = frame->m_pNext;
		}
	}
}

bool CClientFrameManager::DeleteUnusedClientFrame( CClientFrame* pFrameToDelete )
{
	// Call this to deallocate an unused frame (this should NOT have been added to m_Frames)

	CClientFrame **ppFrame = &m_Frames;	// address of pointer to first frame
	while ( *ppFrame )
	{
		if ( *ppFrame == pFrameToDelete )
		{
			Assert( !"ERROR: DeleteUnusedClientFrame called incorrectly...\n" );
			return false;
		}
		ppFrame = &( (*ppFrame)->m_pNext );
	}

	FreeFrame( pFrameToDelete );
	return true;
}


//-----------------------------------------------------------------------------
// Class factory for frames
//-----------------------------------------------------------------------------
CClientFrame* CClientFrameManager::AllocateFrame()
{
	return m_ClientFramePool.Alloc();
}

CClientFrame*	CClientFrameManager::AllocateAndInitFrame( int nTick )
{
	CClientFrame* pFrame = m_ClientFramePool.Alloc();
	pFrame->Init( nTick );
	if ( m_Frames && m_Frames->tick_count > nTick )
	{
		while ( m_Frames )
		{
			CClientFrame* pNext = m_Frames->m_pNext;
			FreeFrame( m_Frames );
			m_Frames = pNext;
		}
	}
	return pFrame;
}

void CClientFrameManager::FreeFrame( CClientFrame* pFrame )
{
	if ( pFrame->IsMemPoolAllocated() )
	{
		m_ClientFramePool.Free( pFrame );
	}
	else
	{
		delete pFrame;
	}
}
