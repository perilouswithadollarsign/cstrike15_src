//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef DMEAMALGAMATEDTEXTURE_H
#define DMEAMALGAMATEDTEXTURE_H
#ifdef _WIN32
#pragma once
#endif
	
#include "materialobjects/amalgtexturevars.h"
#include "bitmap/floatbitmap.h"
#include "tier2/tier2.h"

#include "datamodel/dmelement.h"
#include "datamodel/dmehandle.h"
#include "datamodel/dmattributevar.h"
#include "resourcefile/resourcedictionary.h"
#include "materialobjects/dmeimage.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeSheetImage;  // known in mksheet as a SequenceFrame
class CDmeSheetSequenceFrame; // known in mksheet as a SequenceEntry
class CDmeSheetSequence;
class CResourceStream;
class CDmeImage;


//-----------------------------------------------------------------------------
// The Amalgamated texture itself.
//-----------------------------------------------------------------------------
class CDmeAmalgamatedTexture : public CDmElement
{
	DEFINE_ELEMENT( CDmeAmalgamatedTexture, CDmElement );

public:
	// Called when attributes change
	virtual	void OnAttributeChanged( CDmAttribute *pAttribute ) {}
	virtual void OnAttributeArrayElementAdded( CDmAttribute *pAttribute, int nFirstElem, int nLastElem ) {}
	virtual void OnAttributeArrayElementRemoved( CDmAttribute *pAttribute, int nFirstElem, int nLastElem ) {}

	void Init( const char *pShtFileName = NULL, bool bUseCurrentDir = false );
	void *WriteFile( CResourceStream *pStream, ResourceId_t nTextureResourceId );

	// old-style
	void WriteFile( const char *pFileName, bool bVerbose = false );

	bool DetermineBestPacking();
	bool PackImages( bool bGenerateImage, int nWidth );

	int GetWidth() { return m_nWidth; }
	int GetHeight() { return m_nHeight; }
	void GetSize( int &width, int &height )
	{
		width = m_nWidth;
		height = m_nHeight;
	}

	void SetCurrentSequenceClamp( bool bState );
	void SetPackingMode( int mode );

	void CreateNewSequence( int mode = PCKM_FLAT );
	void SetSequenceType( int eMode );
	bool CurrentSequenceExists();

	void CreateFrame( CUtlVector<char *> &frameNames, float ftime = 1.0f );	
	void AddImage( CDmeSheetSequenceFrame *pNewSequenceFrame, char *pImageName );

	int GetSequenceCount(){ return m_SequenceCount; }

	CDmeImage *GetPackedImage();

	bool WriteTGA( const char *pFileName );

private:
	CDmeSheetImage *FindImage( const char *pImageName );
	int GetPackingMode();	
	int GetSequenceType();
	void ValidateImagePacking( CDmeSheetImage *pBitmap, char *pImageName );
	bool PackImagesFlat( bool bGenerateImage, int nWidth );
	bool PackImagesRGBA( bool bGenerateImage, int nWidth );

	float UCoord( int u )
	{
		float uc = u + 0.5;
		return uc / (float) m_nWidth;
	}
	float VCoord( int v )
	{
		float vc = v + 0.5;
		return vc / (float) m_nHeight;
	}

	CDmaElementArray< CDmeSheetImage > m_ImageList;	
	CDmaVar< int > m_ePackingMode;
	CDmaElementArray< CDmeSheetSequence > m_Sequences;
	CDmeSheetSequence *m_pCurSequence;
	CDmaVar< int > m_nWidth;
	CDmaVar< int > m_nHeight;
	CDmaElement< CDmeImage > m_pPackedImage;

	int m_SequenceCount;
};


inline CDmeImage *CDmeAmalgamatedTexture::GetPackedImage()
{
	return m_pPackedImage;
}


#endif // DMEAMALGAMATEDTEXTURE_H