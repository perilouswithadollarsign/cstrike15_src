//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include "gamegraphic.h"
#include "animdata.h"
#include "gameuisystemmgr.h"
#include "inputgameui.h"
#include "dmxloader/dmxelement.h"
#include "tier1/fmtstr.h"
#include "graphicgroup.h"
#include "gameuiscript.h"


// A list of script handles exposed by the system
static int32 g_iSerialHandle = 0x01000000;
static CUtlMap< int32, CGameGraphic * > g_mapScriptHandles( DefLessFunc( int32 ) );

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CGameGraphic::CGameGraphic() :
	m_iScriptHandle( ++g_iSerialHandle )
{
	g_mapScriptHandles.InsertOrReplace( m_iScriptHandle, this );

	m_pName = "";
	m_pGroup = NULL;
	m_bCanAcceptInput = false;
	m_CurrentState = -1;
	m_flAnimTime = DMETIME_ZERO;
}

CGameGraphic::~CGameGraphic() 
{
	if ( m_pGroup )
	{
		m_pGroup->RemoveFromGroup( this );
	}

	int nCount = m_Anims.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		delete m_Anims[i];
		m_Anims[i] = NULL;
	}
	m_Anims.RemoveAll();

	g_mapScriptHandles.Remove( m_iScriptHandle );
}

CGameGraphic * CGameGraphic::FromScriptHandle( int32 iScriptHandle )
{
	unsigned short usIdx = g_mapScriptHandles.Find( iScriptHandle );
	return ( usIdx == g_mapScriptHandles.InvalidIndex() ) ? NULL : g_mapScriptHandles.Element( usIdx );
}

//-----------------------------------------------------------------------------
//			  
//-----------------------------------------------------------------------------
DmeTime_t CGameGraphic::GetAnimationTimePassed()
{
	if ( m_Geometry.m_bAnimate )
	{
		m_flAnimTime = g_pGameUISystemMgrImpl->GetTime() - m_Geometry.m_AnimStartTime;
	}

	return m_flAnimTime;
}

KeyValues * CGameGraphic::HandleScriptCommand( KeyValues *args )
{
	char const *szCommand = args->GetName();

	if ( !Q_stricmp( "SetCenter", szCommand ) )
	{
		SetCenter( args->GetFloat( "x" ), args->GetFloat( "y" ) );
		return NULL;
	}
	else if ( !Q_stricmp( "SetScale", szCommand ) )
	{
		SetScale( args->GetFloat( "x" ), args->GetFloat( "y" ) );
		return NULL;
	}
	else if ( !Q_stricmp( "SetRotation", szCommand ) )
	{
		m_Geometry.m_Rotation = args->GetFloat( "rotation", 0.0f );
		return NULL;
	}
	else if ( !Q_stricmp( "SetVisible", szCommand ) )
	{
		SetVisible( args->GetBool( "visible", true ) );
		return NULL;
	}
	else if ( !Q_stricmp( "GetVisible", szCommand ) )
	{
		return new KeyValues( "", "visible", GetVisible() ? 1 : 0 );
	}
	else if ( !Q_stricmp( "SetHorizGradient", szCommand ) )
	{
		m_Geometry.m_bHorizontalGradient = args->GetBool( "horizgradient", true );
		return NULL;
	}
	else if ( !Q_stricmp( "SetColor", szCommand ) )
	{
		Color c = args->GetColor( "color", Color( 255, 255, 255, 255 ) );
		color32 color;	
		color.r = c[0];
		color.g = c[1];
		color.b = c[2];
		color.a = c[3];
		SetColor( color );
		return NULL;
	}




	else if ( !Q_stricmp( "SetState", szCommand ) )
	{
		SetState( args->GetString( "state" ), args->GetBool( "play", true ) );
		SetAnimationTimePassed( DmeTime_t( args->GetFloat( "time" ) ) );
		return NULL;
	}
	else if ( !Q_stricmp( "GetState", szCommand ) )
	{
		return new KeyValues( "", "state", GetState() );
	}
	else if ( !Q_stricmp( "IsDonePlaying", szCommand ) )
	{
		return new KeyValues( "", "done", IsDonePlaying() ? 1 : 0 );
	}
	

	DevWarning( "CGameGraphic::HandleScriptCommand for unknown command %s!\n", args->GetName() );
	return NULL;
}

//-----------------------------------------------------------------------------
// If you don't want to use the clock, you can set the time yourself.		  
//----------------------------------------------------------------------------
void CGameGraphic::SetAnimationTimePassed( DmeTime_t time )
{
	m_flAnimTime = time;
}


//-----------------------------------------------------------------------------
// Populate lists for rendering
//-----------------------------------------------------------------------------
void CGameGraphic::UpdateRenderData( color32 parentColor, CUtlVector< RenderGeometryList_t > &renderGeometryLists, int firstListIndex )
{
	if ( !m_Geometry.m_bVisible )
		return;

	m_Geometry.SetResultantColor( parentColor );
	m_Geometry.UpdateRenderData( renderGeometryLists, firstListIndex );
}

//-----------------------------------------------------------------------------
// Have to do this separately because extents are drawn as rects.
//-----------------------------------------------------------------------------
void CGameGraphic::DrawExtents( CUtlVector< RenderGeometryList_t > &renderGeometryLists, int firstListIndex )
{
	Assert( !IsGroup() );
	color32 extentLineColor = { 0, 255, 0, 255 };	
	m_Geometry.DrawExtents( renderGeometryLists, firstListIndex, extentLineColor );
}


//-----------------------------------------------------------------------------
// Populate lists for rendering
//-----------------------------------------------------------------------------
void CGameGraphic::UpdateRenderTransforms( const StageRenderInfo_t &stageRenderInfo )
{
	m_Geometry.UpdateRenderTransforms( stageRenderInfo, GetGroup() );
}


//-----------------------------------------------------------------------------
// Return the index of the anim this state name maps to.
//-----------------------------------------------------------------------------
bool CGameGraphic::HasState( const char *pStateName )
{
	return (GetStateIndex( pStateName ) != -1);
}

//-----------------------------------------------------------------------------
// Set the animation state of this graphic.
// Default is to start playing it. If it is called with startplaying false, it will stop it.
//-----------------------------------------------------------------------------
void CGameGraphic::SetState( const char *pStateName, bool bStartPlaying )
{
	int newState = GetStateIndex( pStateName );
	if ( ( newState != -1 ) && ( m_CurrentState != newState ) )
	{
		m_CurrentState = newState; 
		if ( bStartPlaying )
		{
			StartPlaying();
		}
	}

	if ( !bStartPlaying )
	{
		StopPlaying();
	}
}

//-----------------------------------------------------------------------------
//   Return true if anim is done playing its current state.
//-----------------------------------------------------------------------------
bool CGameGraphic::IsDonePlaying()
{
	if ( m_CurrentState == -1 )
		return true;

	return m_Anims[m_CurrentState]->IsDone( GetAnimationTimePassed() );	
}

//-----------------------------------------------------------------------------
// Return the index of the anim this state name maps to.
//-----------------------------------------------------------------------------
int CGameGraphic::GetStateIndex( const char *pStateName )
{
	for ( int i = 0; i < m_Anims.Count(); i++ )
	{
		if ( Q_stricmp( pStateName, m_Anims[i]->m_pStateName ) == 0 )
		{
			if ( Q_stricmp( m_Anims[i]->m_pAnimAlias, "" ) != 0 )
			{
				return GetStateIndex( m_Anims[i]->m_pAnimAlias );
			}
			else
			{
				return i;
			}
		}
	}
	return -1;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CGameGraphic::GetState()
{
	if ( m_CurrentState == -1 )
		return "";

	if ( Q_stricmp( m_Anims[m_CurrentState]->m_pAnimAlias, "" ) != 0 )
	{
		return m_Anims[m_CurrentState]->m_pAnimAlias;
	}
	else
	{
		return m_Anims[m_CurrentState]->m_pStateName;
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameGraphic::StartPlaying()
{
	m_Geometry.m_bAnimate = true;
	m_Geometry.m_AnimStartTime = g_pGameUISystemMgrImpl->GetTime();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameGraphic::StopPlaying()
{
	m_Geometry.m_bAnimate = false;
}

//-----------------------------------------------------------------------------
// for demo and unit tests
//-----------------------------------------------------------------------------
void CGameGraphic::AdvanceState()	 
{
	StopPlaying();
	int numStates = m_Anims.Count();
	if ( numStates == 0 )
		return;

	int state = m_CurrentState;
	state++;
	if ( state > numStates -1 )
		state = 0;

	m_CurrentState = state;
}

//-----------------------------------------------------------------------------
// Is this the graphic with this name?
//-----------------------------------------------------------------------------
bool CGameGraphic::IsGraphicNamed( const char *pName )
{
	if ( !Q_stricmp( m_pName.Get(), pName ) )
	{
		// Match.
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Does this graphic own a graphic with this name?
//-----------------------------------------------------------------------------
CGameGraphic *CGameGraphic::FindGraphicByName( const char *pName ) const
{
	return NULL;
}

