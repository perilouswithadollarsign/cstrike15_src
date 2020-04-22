//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef C_BASEANIMATINGOVERLAY_H
#define C_BASEANIMATINGOVERLAY_H
#pragma once

#include "c_baseanimating.h"
#include "toolframework/itoolframework.h"

// For shared code.
#define CBaseAnimatingOverlay C_BaseAnimatingOverlay


class C_BaseAnimatingOverlay : public C_BaseAnimating
{
	DECLARE_CLASS( C_BaseAnimatingOverlay, C_BaseAnimating );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_INTERPOLATION();

	// Inherited from C_BaseAnimating
public:
	virtual C_BaseAnimatingOverlay *GetBaseAnimatingOverlay() { return this; }

	virtual void	AccumulateLayers( IBoneSetup &boneSetup, BoneVector pos[], BoneQuaternion q[], float currentTime );
	virtual void	DoAnimationEvents( CStudioHdr *pStudioHdr );
	virtual void	GetRenderBounds( Vector& theMins, Vector& theMaxs );
	virtual CStudioHdr *OnNewModel();

	virtual bool	Interpolate( float flCurrentTime );

#if defined( _PS3 )
	virtual void AccumulateLayers_AddPoseCalls( IBoneSetup_PS3 &boneSetup, BoneVector pos[], BoneQuaternion q[], float currentTime );
#endif


public:
	enum
	{
		MAX_OVERLAYS = 15,
	};

	C_BaseAnimatingOverlay();
	CAnimationLayer* GetAnimOverlay( int i, bool bUseOrder = true );
	void			SetNumAnimOverlays( int num );	// This makes sure there is space for this # of layers.
	int				GetNumAnimOverlays() const;
	void			SetOverlayPrevEventCycle( int nSlot, float flValue );

	void			CheckInterpChanges( void );
	void			CheckForLayerPhysicsInvalidate( void );

	virtual bool UpdateDispatchLayer( CAnimationLayer *pLayer, CStudioHdr *pWeaponStudioHdr, int iSequence );
	void AccumulateDispatchedLayers( C_BaseAnimatingOverlay *pWeapon, CStudioHdr *pWeaponStudioHdr, IBoneSetup &boneSetup, BoneVector pos[], BoneQuaternion q[], float currentTime );
	void RegenerateDispatchedLayers( IBoneSetup &boneSetup, BoneVector pos[], BoneQuaternion q[], float currentTime );

	void AccumulateInterleavedDispatchedLayers( C_BaseAnimatingOverlay *pWeapon, IBoneSetup &boneSetup, BoneVector pos[], BoneQuaternion q[], float currentTime, bool bSetupInvisibleWeapon = false );

	virtual void	NotifyOnLayerChangeSequence( const CAnimationLayer* pLayer, const int nNewSequence ) {};
	virtual void	NotifyOnLayerChangeWeight( const CAnimationLayer* pLayer, const float flNewWeight ) {};
	virtual void	NotifyOnLayerChangeCycle( const CAnimationLayer* pLayer, const float flNewCycle ) {};

private:
	void CheckForLayerChanges( CStudioHdr *hdr, float currentTime );

	CUtlVector < CAnimationLayer >	m_AnimOverlay;
	CUtlVector < CInterpolatedVar< CAnimationLayer > >	m_iv_AnimOverlay;

	float m_flOverlayPrevEventCycle[ MAX_OVERLAYS ];

	C_BaseAnimatingOverlay( const C_BaseAnimatingOverlay & ); // not defined, not accessible

	friend void ResizeAnimationLayerCallback( void *pStruct, int offsetToUtlVector, int len );
};


inline void C_BaseAnimatingOverlay::SetOverlayPrevEventCycle( int nSlot, float flValue )
{
	m_flOverlayPrevEventCycle[nSlot] = flValue;
}

EXTERN_RECV_TABLE(DT_BaseAnimatingOverlay);


#endif // C_BASEANIMATINGOVERLAY_H




