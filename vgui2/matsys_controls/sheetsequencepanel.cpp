//======= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ======
//
// CSheetSequencePanel - Panel for selecting one sequence from a sprite sheet
//
//===============================================================================

#include "matsys_controls/sheetsequencepanel.h"

#include "matsys_controls/matsyscontrols.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "materialsystem/imaterial.h"
#include "tier1/keyvalues.h"
#include "bitmap/psheet.h"
#include "vgui/IScheme.h"
#include "vgui/IVGui.h"
#include "materialsystem/imaterialvar.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------

// MOC_TODO: Power of two FB texture - do I really need to do this?
static CTextureReference s_pPowerOfTwoFrameBufferTexture_SheetSeq;

static ITexture *GetPowerOfTwoFrameBufferTexture( void )
{
	if ( !s_pPowerOfTwoFrameBufferTexture_SheetSeq )
	{
		s_pPowerOfTwoFrameBufferTexture_SheetSeq.Init( vgui::MaterialSystem()->FindTexture( "_rt_PowerOfTwoFB", TEXTURE_GROUP_RENDER_TARGET ) );
	}

	return s_pPowerOfTwoFrameBufferTexture_SheetSeq;
}

//-----------------------------------------------------------------------------

// Statics
bool CSheetSequencePanel::m_sMaterialsInitialized = false;
CMaterialReference CSheetSequencePanel::m_sColorMat;
CMaterialReference CSheetSequencePanel::m_sAlphaMat;


const int SEQUENCE_PANEL_BORDER = 2;
const int SEQUENCE_PANEL_MAX_SIZE = 256;

CSheetSequencePanel::CSheetSequencePanel( vgui::Panel *pParent, const char *pPanelName ):
	BaseClass(pParent,pPanelName),
	m_pSheet(NULL),
	m_Material(NULL)
{
	m_nHighlightedSequence = -1;
	m_bSeparateAlphaColorMaterial = false;
	m_bIsSecondSequenceView = false;

	EnsureMaterialsExist();
}


void CSheetSequencePanel::EnsureMaterialsExist()
{
	if ( !m_sMaterialsInitialized )
	{
		KeyValues *pKeyValues = new KeyValues( "DebugTextureView" );
		pKeyValues->SetString( "$basetexture", "" );
		pKeyValues->SetInt( "$ShowAlpha", 1 );
		m_sAlphaMat.Init( "SheetSequenceAlphaMaterial", pKeyValues );
		m_sAlphaMat->Refresh();

		pKeyValues = new KeyValues( "DebugTextureView" );
		pKeyValues->SetString( "$basetexture", "" );
		pKeyValues->SetInt( "$ShowAlpha", 0 );
		m_sColorMat.Init( "SheetSequenceColorMaterial", pKeyValues );
		m_sColorMat->Refresh();

		m_sMaterialsInitialized = true;
	}
}

CSheetSequencePanel::~CSheetSequencePanel()
{
	delete m_pSheet;
	m_pSheet = NULL;
}

void CSheetSequencePanel::PrepareMaterials()
{
	if ( !m_Material )
	{
		return;
	}

	m_bSeparateAlphaColorMaterial = CSheetExtended::IsMaterialSeparateAlphaColorMaterial( m_Material );

	bool bFound = false;
	IMaterialVar *pVar = m_Material->FindVar( "$basetexture", &bFound );
	if ( !pVar || !bFound || !pVar->IsDefined() )
		return;

	ITexture *pTex = pVar->GetTextureValue();
	if ( !pTex || pTex->IsError() )
		return;

	//////////////////////////////

	IMaterialVar *BaseTextureVar = m_sAlphaMat->FindVar( "$basetexture", &bFound );

	if ( !bFound || !BaseTextureVar )
		return;

	BaseTextureVar->SetTextureValue( pTex );

	//////////////////////////////

	BaseTextureVar = m_sColorMat->FindVar( "$basetexture", &bFound );

	if ( !bFound || !BaseTextureVar )
		return;

	BaseTextureVar->SetTextureValue( pTex );
}

void CSheetSequencePanel::SetSecondSequenceView( bool bIsSecondSequenceView )
{
	m_bIsSecondSequenceView = bIsSecondSequenceView;
}

void CSheetSequencePanel::SetFromMaterialName( const char* pMaterialName )
{
	if ( m_pSheet )
	{
		delete m_pSheet;
	}

	m_Material.Init( pMaterialName, "editor material" );
	m_pSheet = new CSheetExtended(m_Material);

	PrepareMaterials();
	PerformLayout();
}

void CSheetSequencePanel::SetFromMaterial( IMaterial* sourceMaterial )
{
	if ( m_pSheet )
	{
		delete m_pSheet;
	}

	m_Material.Init(sourceMaterial);
	m_pSheet = new CSheetExtended(m_Material);

	PrepareMaterials();
	PerformLayout();
}

void CSheetSequencePanel::PerformLayout()
{
	int newWidth = SequenceGridCount() * SequenceGridSquareSize() + SEQUENCE_PANEL_BORDER*2;
	int newHeight = SequenceGridRows() * SequenceGridSquareSize() + SEQUENCE_PANEL_BORDER*2;

	if ( SequenceGridCount() == 0 )
	{
		newWidth = 32;
		newHeight = 32;
	}

	SetSize( newWidth, newHeight );
	Repaint();
}

void CSheetSequencePanel::OnCursorExited()
{
	m_nHighlightedSequence = -1;
}

void CSheetSequencePanel::OnCursorMoved(int x, int y)
{
	BaseClass::OnCursorMoved(x,y);

	if ( m_pSheet == NULL || m_pSheet->GetSheetSequenceCount() == 0 )
	{
		m_nHighlightedSequence = -1;
		return;
	}

	int nGridCount = SequenceGridCount();
	int nGridSize = SequenceGridSquareSize();

	int nGridX = (x - SEQUENCE_PANEL_BORDER) / nGridSize;
	int nGridY = (y - SEQUENCE_PANEL_BORDER) / nGridSize;

	if ( nGridX >= 0 && nGridY >= 0 &&
		nGridX < nGridCount && nGridY < nGridCount )
	{
		int nSeqIndex = nGridX + nGridY*nGridCount;

		if ( nSeqIndex < m_pSheet->GetSheetSequenceCount() )
		{
			m_nHighlightedSequence = nSeqIndex;
		}
		else
		{
			m_nHighlightedSequence = -1;
		}
	}
	else
	{
		m_nHighlightedSequence = -1;
	}
}

int CSheetSequencePanel::SequenceGridCount()
{
	return m_pSheet ? Ceil2Int(sqrt((float)m_pSheet->GetSheetSequenceCount())) : 0;
}

int CSheetSequencePanel::SequenceGridSquareSize()
{
	int nGridCount = SequenceGridCount();

	if ( nGridCount == 0 )
	{
		return 0;
	}
	else
	{
		return (SEQUENCE_PANEL_MAX_SIZE / nGridCount);
	}
}

int CSheetSequencePanel::SequenceGridRows()
{
	if ( !m_pSheet )
		return 0;

	int nSequences = m_pSheet->GetSheetSequenceCount();
	int nGridCount = SequenceGridCount();

	if ( nSequences == 0 )
	{
		return 0;
	}
	else
	{
		// nSequences / nGridCount, rounded up
		return (nSequences + nGridCount - 1) / nGridCount;
	}
}

void CSheetSequencePanel::OnMouseReleased( vgui::MouseCode mouseCode )
{
	if ( m_nHighlightedSequence != -1 )
	{
		KeyValues *k = new KeyValues("SheetSequenceSelected");
		k->SetPtr("panel", this);
		k->SetInt("nSequenceNumber", m_nHighlightedSequence);
		k->SetBool("bIsSecondSequence", m_bIsSecondSequenceView );
		PostActionSignal( k );
	}

	SetVisible(false);
}

void CSheetSequencePanel::Paint( void )
{
	int x, y, w, h;
	GetSize(w, h);
	GetPos(x,y);

	vgui::surface()->DrawSetColor( Color(0,0,0,255) );
	vgui::surface()->DrawOutlinedRect( 1, 1, w-1, h-1 );

	if ( m_pSheet == NULL || !m_pSheet->ValidSheetData() )
	{
		return;
	}

	CMatRenderContextPtr pRenderContext( vgui::MaterialSystem() );
	vgui::MatSystemSurface()->Begin3DPaint( 2, 2, w-2, h-2 );

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	pRenderContext->Ortho( 2, 2, w-2, h-2, -1.0f, 1.0f );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	// Deal with refraction
	if ( m_Material->NeedsPowerOfTwoFrameBufferTexture() )
	{
		ITexture *pTexture = GetPowerOfTwoFrameBufferTexture();
		if ( pTexture && !pTexture->IsError() )
		{
			pRenderContext->CopyRenderTargetToTexture( pTexture );
			pRenderContext->SetFrameBufferCopyTexture( pTexture );
		}
	}

	Color bgColor = GetBgColor();
	pRenderContext->ClearColor4ub( bgColor.r(), bgColor.g(), bgColor.b(), 255 );
	pRenderContext->ClearBuffers( true, true );

	pRenderContext->FogMode( MATERIAL_FOG_NONE );
	pRenderContext->SetNumBoneWeights( 0 );

	bool bOverrideSpriteCard = false;
	bool bOnlyColor = false;
	bool bOnlyAlpha = false;
	if ( m_bSeparateAlphaColorMaterial )
	{
		if ( !m_bIsSecondSequenceView )
		{
			pRenderContext->Bind( m_sAlphaMat );
			bOnlyAlpha = true;
		}
		else
		{
			pRenderContext->Bind( m_sColorMat );
			bOnlyColor = true;
		}

		bOverrideSpriteCard = true;
	}
	else
	{
		pRenderContext->Bind( m_Material );
	}

	IMesh *pMesh = pRenderContext->GetDynamicMesh();

	float flAge = fmodf( Plat_FloatTime(), m_pSheet->GetSequenceTimeSpan(0) );

	int nGridCount = SequenceGridCount();
	float flGridSquareSize = SequenceGridSquareSize();
	float flOffset = 0.5f*flGridSquareSize+SEQUENCE_PANEL_BORDER;
	int nSequences = m_pSheet->GetSheetSequenceCount();

	for ( int i = 0; i < nSequences; ++i )
	{
		float x = i % nGridCount;
		float y = i / nGridCount;

		if ( bOnlyColor && !m_pSheet->SequenceHasColorData( i )
		  || bOnlyAlpha && !m_pSheet->SequenceHasAlphaData( i ) )
		{
			continue;
		}

		m_pSheet->DrawSheet( pMesh, Vector(flOffset+x*flGridSquareSize,h-(flOffset+y*flGridSquareSize),0), flGridSquareSize*0.5f, i, flAge, 750.0f, true, -1, bOverrideSpriteCard );
	}

	pRenderContext->PopMatrix();
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();

	vgui::MatSystemSurface()->End3DPaint( );

	//////////////////////////////////////////////////////////////////////////

	flOffset = SEQUENCE_PANEL_BORDER;
	for ( int i = 0; i < nSequences; ++i )
	{
		float x = i % nGridCount;
		float y = i / nGridCount;

		Color drawColor = Color(0,0,0,255);

		if ( m_nHighlightedSequence == i )
		{
			drawColor = Color(255,255,255,255);
		}

		vgui::surface()->DrawSetColor(drawColor);
		vgui::surface()->DrawSetTextColor(drawColor);

		vgui::surface()->DrawOutlinedRect( flOffset+x*flGridSquareSize, flOffset+y*flGridSquareSize, flOffset+(x+1)*flGridSquareSize, flOffset+(y+1)*flGridSquareSize );

		wchar_t strBuffer[8];
		V_snwprintf( strBuffer, ARRAYSIZE( strBuffer ), L"%d", i );
		vgui::surface()->DrawSetTextFont( vgui::scheme()->GetIScheme( GetScheme() )->GetFont( "DefaultVerySmall" ) );
		vgui::surface()->DrawSetTextPos(flOffset+x*flGridSquareSize+2, flOffset+y*flGridSquareSize+1);
		vgui::surface()->DrawUnicodeString( strBuffer );

		if ( bOnlyColor && !m_pSheet->SequenceHasColorData( i )
		  || bOnlyAlpha && !m_pSheet->SequenceHasAlphaData( i ) )
		{
			vgui::surface()->DrawSetTextColor( Color(255,0,0,255) );
			vgui::surface()->DrawSetTextPos(flOffset+(x+0.5f)*flGridSquareSize, flOffset+(y+0.5f)*flGridSquareSize+1);
			vgui::surface()->DrawUnicodeString( L"x" );
		}
	}
}
