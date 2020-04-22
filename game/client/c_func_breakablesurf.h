//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#ifndef C_FUNC_BREAKABLESURF_H
#define C_FUNC_BREAKABLESURF_H
#ifdef _WIN32
#pragma once
#endif

#include "iviewrender.h"
#include "proxyentity.h"

#define MAX_NUM_PANELS 16

#define NUM_EDGE_TYPES		4
#define NUM_EDGE_STYLES		3

extern IVDebugOverlay *debugoverlay;

enum WinSide_t
{
	WIN_SIDE_BOTTOM,
	WIN_SIDE_RIGHT,
	WIN_SIDE_TOP,
	WIN_SIDE_LEFT,
};

enum WinEdge_t
{
	EDGE_NOT = -1,		// No edge
	EDGE_NONE,				/* No edge on both sides	/##\  */
	EDGE_FULL,				// Edge on both sides		|##| 
	EDGE_LEFT,				/* Edge is on left only		|##\  */
	EDGE_RIGHT,				// Edge is on right only	/##|
};

#define STYLE_HIGHLIGHT = -1;

//-----------------------------------------------------------------------------
// All the information associated with a particular handle
//-----------------------------------------------------------------------------
struct Panel_t
{
	char			m_nWidth;
	char			m_nHeight;
	char			m_nSide;
	char			m_nEdgeType;
	char			m_nStyle;
};

struct EdgeTexture_t
{
	int					m_nRenderIndex;
	int					m_nStyle;
	CMaterialReference	m_pMaterialEdge;
	CTextureReference	m_pMaterialEdgeTexture;
};

class C_BreakableSurface : public C_BaseEntity, public IBrushRenderer
{
public:
	DECLARE_CLASS( C_BreakableSurface, C_BaseEntity );
	DECLARE_DATADESC();
	DECLARE_CLIENTCLASS();


	int				m_nNumWide;
	int				m_nNumHigh;
	float			m_flPanelWidth;
	float			m_flPanelHeight;
	Vector			m_vNormal;
	Vector			m_vCorner;
	bool			m_bIsBroken;
	int				m_nSurfaceType;

						
	// This is the texture we're going to use to multiply by the cracked base texture
	ITexture*	m_pCurrentDetailTexture;

	// Stores linked list of edges to render
	CUtlLinkedList< Panel_t, unsigned short >		m_RenderList;


	C_BreakableSurface();
	~C_BreakableSurface();

public:
	void		InitMaterial(WinEdge_t nEdgeType, int nEdgeStyle, char const* pMaterialName);
	virtual void		OnDataChanged( DataUpdateType_t updateType );
	virtual void		OnPreDataChanged( DataUpdateType_t updateType );

	RenderableTranslucencyType_t ComputeTranslucencyType( void );
	bool		HavePanel(int nWidth, int nHeight);
	bool		RenderBrushModelSurface( IClientEntity* pBaseEntity, IBrushSurface* pBrushSurface ); 
	int			DrawModel( int flags, const RenderableInstance_t &instance );
	void		DrawSolidBlocks( IBrushSurface* pBrushSurface );

	virtual void	OnRestore();

	virtual bool	ShouldReceiveProjectedTextures( int flags );

private:
	// One bit per pane
	CNetworkArray( bool, m_RawPanelBitVec, MAX_NUM_PANELS * MAX_NUM_PANELS );
	bool m_PrevRawPanelBitVec[ MAX_NUM_PANELS * MAX_NUM_PANELS ];
	
	// 2 bits of flags and 2 bits of edge type
	byte		m_nPanelBits[MAX_NUM_PANELS][MAX_NUM_PANELS];	//UNDONE: allocate this dynamically?
	CMaterialReference	m_pMaterialBox;	
	EdgeTexture_t	m_pSolid;
	EdgeTexture_t	m_pEdge[NUM_EDGE_TYPES][NUM_EDGE_STYLES];

	inline bool InLegalRange(int nWidth, int nHeight);
	inline bool	IsPanelSolid(int nWidth, int nHeight);
	inline bool	IsPanelStale(int nWidth, int nHeight);
	inline void	SetPanelSolid(int nWidth, int nHeight, bool value);
	inline void	SetPanelStale(int nWidth, int nHeight, bool value);

	void		DrawOneEdge( IBrushSurface* pBrushSurface, IMesh* pMesh, 
					CMeshBuilder *pMeshBuilder, const Vector &vStartPos,  
					const Vector &vWStep, const Vector &vHstep, WinSide_t nEdge);
	void		DrawOneHighlight( IBrushSurface* pBrushSurface, IMesh* pMesh, 
					CMeshBuilder *pMeshBuilder, const Vector &vStartPos,  
					const Vector &vWStep, const Vector &vHstep, WinSide_t nEdge);
	void		DrawOneBlock(IBrushSurface* pBrushSurface, IMesh* pMesh, 
					CMeshBuilder *pMeshBuilder, const Vector &vPosition, 
					const Vector &vWidth, const Vector &vHeight);

	void		DrawRenderList( IBrushSurface* pBrushSurface);
	void		DrawRenderListHighlights( IBrushSurface* pBrushSurface );
	int			FindRenderPanel(int nWidth, int nHeight, WinSide_t nSide);
	void		AddToRenderList(int nWidth, int nHeight, WinSide_t nSide, WinEdge_t nEdgeType, int forceStyle);
	int			FindFirstRenderTexture(WinEdge_t nEdgeType, int nStyle);

	inline void SetStyleType( int w, int h, int type )
	{
		Assert( type < NUM_EDGE_STYLES );
		Assert( type >= 0 );
		// Clear old value
		m_nPanelBits[ w ][ h ] &= ( ~0x03 << 2 );
		// Insert new value
		m_nPanelBits[ w ][ h ] |= ( type << 2 );
	}

	inline int GetStyleType( int w, int h )
	{
		int value = m_nPanelBits[ w ][ h ];
		value = ( value >> 2 ) & 0x03;
		Assert( value < NUM_EDGE_STYLES );
		return value;
	}

	// Gets at the cracked version of the material
	void FindCrackedMaterial();

	CMaterialReference	m_pCrackedMaterial;
	CTextureReference	m_pMaterialBoxTexture;


	void		UpdateEdgeType(int nWidth, int nHeight, int forceStyle = -1 );
};

class CBreakableSurfaceProxy : public CEntityMaterialProxy
{
	public:
	CBreakableSurfaceProxy();
	virtual ~CBreakableSurfaceProxy();
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( C_BaseEntity *pC_BaseEntity );
	virtual IMaterial *GetMaterial();

	private:
	// get at the material whose texture we're going to steal
	void AcquireSourceMaterial( C_BaseEntity* pEnt );

	IMaterialVar* m_BaseTextureVar;
};


#endif // C_FUNC_BREAKABLESURF_H