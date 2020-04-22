//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//


#include "stdafx.h"

#if defined( _OSX )
//#include "appframework/icocoamgr.h"
//extern ICocoaMgr *g_extCocoaMgr;
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace SF::GFx;

ConVar sfcursortimeout( "cursortimeout", "60.0", FCVAR_ARCHIVE, "Seconds before mouse cursor hides itself due to inactivity" );

void ScaleformUIImpl::InitCursorImpl( void )
{
	m_fCursorTimeUntilHide = sfcursortimeout.GetFloat();
	m_iWantCursorShown = 0;
	m_loadedCursorImage = CURSOR_IMAGE_NONE;
	m_isCursorForced = false;
}

void ScaleformUIImpl::ShutdownCursorImpl( void )
{

}

void ScaleformUIImpl::InitCursor( const char* cursorMovie )
{
	// gurjeets - locks commented out, left here for reference, see comment in LockSlotPtr
	//m_SlotMutexes[SF_RESERVED_CURSOR_SLOT].Lock();

	if ( !m_SlotPtrs[SF_RESERVED_CURSOR_SLOT] )
	{
		CursorSlot* slotPtr = new CursorSlot();

		m_SlotPtrs[SF_RESERVED_CURSOR_SLOT] = slotPtr;

		slotPtr->Init( cursorMovie, SF_RESERVED_CURSOR_SLOT );

		if ( ConsumesInputEvents() )
		{
			slotPtr->Hide();
			slotPtr->Show();
			m_fCursorTimeUntilHide = sfcursortimeout.GetFloat();
		}
		else
		{
			slotPtr->Show();
			slotPtr->Hide();
			m_fCursorTimeUntilHide = 0;
		}
	}
	else
	{
		m_SlotPtrs[SF_RESERVED_CURSOR_SLOT]->AddRef();
	}

	// gurjeets - locks commented out, left here for reference, see comment in LockSlotPtr
	//m_SlotMutexes[SF_RESERVED_CURSOR_SLOT].Unlock();

}

void ScaleformUIImpl::SetCursorViewport( int x, int y, int width, int height )
{
	SetSlotViewport( SF_RESERVED_CURSOR_SLOT, x, y, width, height );
}

void ScaleformUIImpl::ReleaseCursor( void )
{
	SlotRelease( SF_RESERVED_CURSOR_SLOT );
}

bool ScaleformUIImpl::IsCursorVisible( void )
{
	bool result = false;
	CursorSlot* pslot = ( CursorSlot* )m_SlotPtrs[ SF_RESERVED_CURSOR_SLOT ];

	if ( pslot )
	{
		result = pslot->IsVisible();
	}

	return result;

}

void ScaleformUIImpl::RenderCursor( void )
{
	RenderSlot( SF_RESERVED_CURSOR_SLOT );
}

void ScaleformUIImpl::AdvanceCursor( void )
{
	AdvanceSlot( SF_RESERVED_CURSOR_SLOT );
}

void ScaleformUIImpl::SetCursorShape( int shapeIndex )
{
	CursorSlot* pslot = ( CursorSlot* ) LockSlotPtr( SF_RESERVED_CURSOR_SLOT );

	if ( pslot )
	{
		pslot->SetCursorShape( shapeIndex );
	}

	UnlockSlotPtr( SF_RESERVED_CURSOR_SLOT );
}

void ScaleformUIImpl::UpdateCursorWaitTime( float newTime )
{
	if ( ( m_fCursorTimeUntilHide > 0 ) != ( newTime > 0 ) )
	{
		if ( ( newTime > 0 ) )
		{
			if ( m_iWantCursorShown )
				InnerShowCursor();
		}
		else
			InnerHideCursor();
	}

	m_fCursorTimeUntilHide = newTime;
}

void ScaleformUIImpl::CursorMoved( void )
{
	UpdateCursorWaitTime( sfcursortimeout.GetFloat() );
}

void ScaleformUIImpl::ControllerMoved( void )
{
	// the motion calibration screen requires the cursor to remain visible even though it is a 'controller'
	if ( !m_isCursorForced )
	{
		UpdateCursorWaitTime( 0 );
	}
}

void ScaleformUIImpl::UpdateCursorLazyHide( float time )
{
	UpdateCursorWaitTime( m_fCursorTimeUntilHide - time );
}


void ScaleformUIImpl::InnerShowCursor( void )
{
#if defined( USE_SDL ) || defined( OSX )
	m_pLauncherMgr->SetMouseVisible( true );
#endif
	m_fCursorTimeUntilHide = sfcursortimeout.GetFloat();

#if defined( _PS3 )
	// use hardware cursor, and skip the show call for the standard scaleform cursor
	g_pInputSystem->EnableHardwareCursor(); 
#else
	CursorSlot* pslot = ( CursorSlot* ) LockSlotPtr( SF_RESERVED_CURSOR_SLOT );

	if ( pslot )
	{
		pslot->Show();
	}

	UnlockSlotPtr( SF_RESERVED_CURSOR_SLOT );
#endif
}

void ScaleformUIImpl::InnerHideCursor( void )
{
#if defined( USE_SDL ) || defined( OSX ) 
	m_pLauncherMgr->SetMouseVisible( false );
#endif
	MouseEvent mevent( Event::MouseMove, 0, -100, -100 );
	DistributeEvent( mevent, 0, true, false );

	CursorSlot* pslot = ( CursorSlot* ) LockSlotPtr( SF_RESERVED_CURSOR_SLOT );

	if ( pslot )
	{
		pslot->Hide();
	}

#if defined( _OSX )
	// [will] - Grab the delta to clear the mouse delta accumulator.
//	int x, y;
//	g_extCocoaMgr->GetMouseDelta( x, y, true );
#endif

	UnlockSlotPtr( SF_RESERVED_CURSOR_SLOT );
}

void ScaleformUIImpl::ShowCursor( void )
{
	m_iWantCursorShown++;
	
	ConVarRef cl_mouseenable( "cl_mouseenable" );
	cl_mouseenable.SetValue( false );

	if ( m_iWantCursorShown && m_fCursorTimeUntilHide > 0 )
	{
		InnerShowCursor();
		SFDevMsg( "ScaleformUIImpl::ShowCursor  want=%d  cl_mouseenable=false InnerShowCursor\n", m_iWantCursorShown);
	}
	else
	{
		SFDevMsg( "ScaleformUIImpl::ShowCursor  want=%d  cl_mouseenable=false\n", m_iWantCursorShown);
	}
}

void ScaleformUIImpl::HideCursor( void )
{
	Assert( m_iWantCursorShown > 0 );

	m_iWantCursorShown--;

	if ( !m_iWantCursorShown )
	{
		ConVarRef cl_mouseenable( "cl_mouseenable" );
		cl_mouseenable.SetValue( true );

		InnerHideCursor();

		SFDevMsg( "ScaleformUIImpl::HideCursor  want=%d  cl_mouseenable=true  InnerHideCursor\n", m_iWantCursorShown);
	}
	else
	{
		ConVarRef cl_mouseenable( "cl_mouseenable" );
		SFDevMsg( "ScaleformUIImpl::HideCursor  want=%d  cl_mouseenable=%s\n", m_iWantCursorShown, cl_mouseenable.GetBool() ? "TRUE" : "FALSE");
	}
}

