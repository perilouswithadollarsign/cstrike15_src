//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GAMEGRAPHIC_H
#define GAMEGRAPHIC_H

#ifdef _WIN32
#pragma once
#endif

#include "uigeometry.h"
#include "tier1/utlvector.h"
#include "inputsystem/ButtonCode.h"
#include "tier1/utlstring.h"
//#include "gameuiscript.h"
#include "filesystem.h"

struct InputEvent_t;
class CGraphicGroup; 
class CGameUIScript;
class CGameUIDefinition;
class CAnimData;
class CDmxElement;
enum EScriptExecution;


struct StageRenderInfo_t
{
	Vector parentPos;
	Vector2D parentScale;
	int parentRot;
	matrix3x4_t relToScreen;
	matrix3x4_t relToScreenHoldAspectRatio;
};

//-----------------------------------------------------------------------------
// A class that contains a rect or text or hit area
//-----------------------------------------------------------------------------
class CGameGraphic
{
public:

	CGameGraphic();
	virtual ~CGameGraphic();

	// Update geometry and execute scripting.
	virtual void UpdateGeometry(){}
	// Populate lists for rendering
	virtual void UpdateRenderTransforms( const StageRenderInfo_t &stageRenderInfo );
	virtual void UpdateRenderData( color32 parentColor, CUtlVector< RenderGeometryList_t > &renderGeometryLists, int firstListIndex );
	virtual void DrawExtents( CUtlVector< RenderGeometryList_t > &renderGeometryLists, int firstListIndex );

	virtual bool HasState( const char *pStateName );
	virtual void SetState( const char *pStateName, bool bStartPlaying = true );
	bool IsDonePlaying();
	const char *GetState();
	virtual void StartPlaying();
	virtual void StopPlaying();
	virtual void AdvanceState();
	
	void SetAnimationTimePassed( DmeTime_t time );
	DmeTime_t GetAnimationTimePassed();

	virtual KeyValues *HandleScriptCommand( KeyValues *args );

	virtual bool HitTest( int x, int y ) { return false; }

	CGraphicGroup *GetGroup() const { return m_pGroup; }
	void SetGroup( CGraphicGroup *pGroup ) { m_pGroup = pGroup; }


	const char *GetName() const { return m_pName; }
	bool IsGraphicNamed( const char *pName );
	virtual CGameGraphic *FindGraphicByName( const char *pName ) const;

	virtual void SetVisible( bool bVisible ){ m_Geometry.m_bVisible = bVisible; }
	bool GetVisible( ) const { return m_Geometry.m_bVisible; }

	bool CanAcceptInput() const { return m_bCanAcceptInput; } 
	virtual bool IsHitArea() const { return false; }
	virtual bool IsGroup() const { return false; }
	virtual bool IsDynamic() const { return false; }

	void SetResultantColor( bool bTop, color32 parentColor );

	virtual bool MaintainAspectRatio() const { return m_Geometry.m_bMaintainAspectRatio; }

	int32 GetScriptHandle() { return m_iScriptHandle; }
	static CGameGraphic * FromScriptHandle( int32 iScriptHandle );

	virtual const char *GetMaterialAlias(){ return NULL; }

	virtual void SetCenter( float x, float y ) { m_Geometry.m_Center = Vector2D( x, y ); }
	virtual void SetScale( float xScale, float yScale ) { m_Geometry.m_Scale = Vector2D( xScale, yScale); }
	virtual void SetColor( color32 c ){ m_Geometry.m_Color = c; }

protected:
	int GetStateIndex( const char *pStateName );

	CUtlString m_pName;
	CGraphicGroup *m_pGroup;
	bool m_bCanAcceptInput;

	CGeometry m_Geometry;
	CUtlVector< CAnimData * > m_Anims;
	int m_CurrentState;	
	DmeTime_t m_flAnimTime;

	int32 m_iScriptHandle;

};


#endif // GAMEGRAPHIC_H
