//===== Copyright © 2005-2006, Valve Corporation, All rights reserved. ======//
//
// Purpose: build a sheet data file and a large image out of multiple images
//
//===========================================================================//

#include "materialobjects/dmeamalgtexture.h"
#include "bitmap/floatbitmap.h"
#include "tier2/fileutils.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "materialobjects/dmesheetsequence.h"
#include "resourcefile/schema/sheet.g.h"
#include "resourcefile/resourcestream.h"
#include "materialobjects/dmeimage.h"
#include "bitmap/psheet.h"

#include "tier0/dbg.h"

//-----------------------------------------------------------------------------
// Helper functions
//-----------------------------------------------------------------------------
static int GetChannelIndexFromChar( char c )
{
	// r->0 b->1 g->2 a->3 else -1

	static char s_ChannelIDs[] = "rgba";

	char const *pChanChar = strchr( s_ChannelIDs, c );
	if ( ! pChanChar )
	{
		Warning( " bad channel name '%c'\n", c );
		return -1;
	}
	else
	{
		return pChanChar - s_ChannelIDs;
	}
}

//-----------------------------------------------------------------------------
// Clear the contents of the channel
//-----------------------------------------------------------------------------
static void ZeroChannel( FloatBitMap_t *newBitmap, FloatBitMap_t *pBitmap, int nDestChannel )
{
	for ( int y = 0; y < newBitmap->NumRows(); y++ )
	{
		for ( int x = 0; x < newBitmap->NumCols(); x++ )
		{
			pBitmap->Pixel( x, y, 0, nDestChannel ) = 0;
		}
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
static void CopyChannel( FloatBitMap_t *newBitmap, FloatBitMap_t *pBitmap, int nSrcChannel, int nDestChannel )
{
	for ( int y = 0; y < newBitmap->NumRows(); y++ )
	{
		for ( int x = 0; x < newBitmap->NumCols(); x++ )
		{
			pBitmap->Pixel( x, y, 0, nDestChannel ) = newBitmap->Pixel( x, y, 0, nSrcChannel );
		}
	}
}


//-----------------------------------------------------------------------------
// Get a full path to fname that is under mod/content/materialsrc
// This is where all tgas live.
//-----------------------------------------------------------------------------
void GetFullPathUsingMaterialsrcContent( const char * fname, char *pFullTGAFileNameDest, int fullPathBufferSize )
{
	char localTexturePath[MAX_PATH];
	Q_snprintf( localTexturePath, sizeof(localTexturePath), "materialsrc\\%s", fname );
	const char *result = g_pFullFileSystem->RelativePathToFullPath( localTexturePath, "CONTENT", pFullTGAFileNameDest, fullPathBufferSize );
	if ( result == NULL )
	{
		Warning( "CDataModel: Unable to generate full path for file %s\n", fname );
		pFullTGAFileNameDest = NULL;
	}
}

//-----------------------------------------------------------------------------
// Get a full path to fname that is under the current directory.
// mksheet is meant to be run from a dir that contains all the tga files 
// referred to by the .mks file. This fxn lets it find the local files.
//-----------------------------------------------------------------------------
void GetFullPathUsingCurrentDir( const char * fname, char *pFullTGAFileNameDest, int fullPathBufferSize )
{
	char pDir[MAX_PATH];
	if ( g_pFullFileSystem->GetCurrentDirectory( pDir, sizeof(pDir) ) )
	{
		CUtlString fullPathName = pDir;
		fullPathName += "\\";
		fullPathName += fname;
		Q_strncpy( pFullTGAFileNameDest, fullPathName.Get(), fullPathBufferSize ); 
	}
	else
	{
		Warning( "CDataModel: Unable to generate full path for file %s\n", fname );
		pFullTGAFileNameDest = NULL;
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static FloatBitMap_t *CreateFloatBitmap( const char *pFilename, bool bUseCurrentDir = false )
{
	if ( strchr( pFilename, ',' ) == NULL )
	{
		char fullTGAFileName[ MAX_PATH ];
		if ( Q_IsAbsolutePath( pFilename ) )
		{
			Q_strncpy( fullTGAFileName, pFilename, sizeof( fullTGAFileName ) );
		}
		else
		{
			if ( bUseCurrentDir )
			{
				GetFullPathUsingCurrentDir( pFilename, fullTGAFileName, sizeof(fullTGAFileName) );
			}
			else
			{
				GetFullPathUsingMaterialsrcContent( pFilename, fullTGAFileName, sizeof(fullTGAFileName) );
			}
		}
		if ( fullTGAFileName == NULL )
		{
			Warning( "CDataModel: Unable to generate full path for file %s\n", pFilename );
		}
		return new FloatBitMap_t( fullTGAFileName );
	}

	// Warning this is Untested	not in use currently.

	// parse extended specifications
	CUtlVector<char *> Images;
	V_SplitString( pFilename, ",", Images );
	FloatBitMap_t *pBitmap = NULL;
	// now, process bitmaps, performing copy operations specified by {} syntax
	for( int i = 0; i < Images.Count(); i++ )
	{
		char fnamebuf[MAX_PATH];
		strcpy( fnamebuf, Images[i] );
		char * pBrace = strchr( fnamebuf, '{' );
		if ( pBrace )
		{
			*pBrace = 0;								// null it
			pBrace++;									// point at control specifier
			char *pEndBrace = strchr( pBrace, '}' );
			if ( ! pEndBrace )
			{
				Msg( "bad extended bitmap synax (no close brace) - %s \n", Images[i] );
			}
		}


		FloatBitMap_t newBitmap( fnamebuf );
		if ( !pBitmap )
		{
			// first image sets size
		   pBitmap = new FloatBitMap_t( &newBitmap );
		}

		// now, process operation specifiers of the form "{chan=chan}" or "{chan=0}"
		if ( pBrace && ( pBrace[1] == '=' ) )
		{
			int nDstChan = GetChannelIndexFromChar( pBrace[0] );
			if ( nDstChan != -1 )
			{
				if ( pBrace[2] == '0' )
				{
					// zero the channel
					ZeroChannel( &newBitmap, pBitmap, nDstChan );
				}
				else
				{
					int nSrcChan = GetChannelIndexFromChar( pBrace[2] );
					if ( nSrcChan != -1 )
					{
						// perform the channel copy
						CopyChannel( &newBitmap, pBitmap, nSrcChan, nDstChan );
					}
				}
			}		
		}
	}
	return pBitmap;
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeAmalgamatedTexture, CDmeAmalgamatedTexture );

void CDmeAmalgamatedTexture::OnConstruction()
{
	m_ImageList.Init( this, "images" );
	m_ePackingMode.InitAndSet( this, "packmode", PCKM_FLAT );
	m_Sequences.Init( this, "sequences" );
	m_nWidth.Init( this, "width" );
	m_nHeight.Init( this, "height" );
	m_pPackedImage.Init( this, "packedImage" );

	m_SequenceCount = 0;
}

void CDmeAmalgamatedTexture::OnDestruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeAmalgamatedTexture::Init( const char *pShtFileName, bool bUseCurrentDir )
{
	CDisableUndoScopeGuard sg;
	m_pCurSequence = NULL;

	// Load up the image bitmaps.
	char pFullDir[MAX_PATH];
	Q_strncpy( pFullDir, pShtFileName, sizeof(pFullDir) );
	Q_StripFilename( pFullDir );

	char pFullPath[MAX_PATH];
	for( int i = 0; i < m_ImageList.Count(); i++ )
	{
		// FIXME: Ugh!
		const char *pImageName = m_ImageList[i]->GetName();
		if ( bUseCurrentDir && Q_IsAbsolutePath( pShtFileName ) )
		{
			Q_ComposeFileName( pFullDir, pImageName, pFullPath, sizeof(pFullPath) );
			pImageName = pFullPath;
		}

		m_ImageList[i]->m_pImage = CreateFloatBitmap( pImageName, bUseCurrentDir );
	}

	m_SequenceCount = 0;
}


//-----------------------------------------------------------------------------
// Whether the frames loop or not
//-----------------------------------------------------------------------------
void CDmeAmalgamatedTexture::SetCurrentSequenceClamp( bool bState )
{
	if ( m_pCurSequence )
	{
		m_pCurSequence->m_Clamp = bState;
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDmeAmalgamatedTexture::GetPackingMode()
{
	return m_ePackingMode;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeAmalgamatedTexture::SetPackingMode( int eMode )
{
	// Assign the packing mode read in to member var.
	if ( !m_Sequences.Count() )
	{
		m_ePackingMode = eMode;
	}
	else if ( m_ePackingMode != eMode )
	{
		// Allow special changes:
		// flat -> rgb+a
		if ( m_ePackingMode == PCKM_FLAT && eMode == PCKM_RGB_A )
		{
			m_ePackingMode = eMode; 
		}
		// everything else
		else
		{
			Warning( "*** line error: incompatible packmode change when %d sequences already defined!\n", m_Sequences.Count() );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDmeAmalgamatedTexture::CreateNewSequence( int mode )
{
	m_pCurSequence = CreateElement<CDmeSheetSequence>( "", GetFileId() );
	m_pCurSequence->m_nSequenceNumber = m_SequenceCount;
	m_SequenceCount++;
	SetSequenceType( mode );

	m_Sequences.AddToTail( m_pCurSequence );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CDmeAmalgamatedTexture::GetSequenceType()
{
	return m_pCurSequence->m_eMode;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDmeAmalgamatedTexture::SetSequenceType( int eMode )
{
	m_pCurSequence->m_eMode = eMode;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CDmeAmalgamatedTexture::CurrentSequenceExists()
{
	return m_pCurSequence != NULL;
}

//-----------------------------------------------------------------------------
// Validate that image packing is correct
//-----------------------------------------------------------------------------
void CDmeAmalgamatedTexture::ValidateImagePacking( CDmeSheetImage *pBitmap, char *pImageName )
{
	if ( m_ePackingMode == PCKM_RGB_A )
	{
		for ( uint16 idx = 0; idx < pBitmap->m_mapSequences.Count(); ++idx )
		{
			CDmeSheetSequence *pSeq = pBitmap->FindSequence( idx );
			Assert( pSeq );

			if ( pSeq->m_eMode != SQM_RGBA &&
				pSeq->m_eMode != m_pCurSequence->m_eMode )
			{
				Warning( "*** line error: 'rgb+a' packing cannot pack image '%s' belonging to sequences %d and %d!\n", 
					pImageName,
					pSeq->m_nSequenceNumber, 
					m_pCurSequence->m_nSequenceNumber );
			}
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDmeAmalgamatedTexture::CreateFrame( CUtlVector<char *> &imageNames, float ftime )
{
	CDmeSheetSequenceFrame *pNewFrame = CreateElement<CDmeSheetSequenceFrame>( "", GetFileId() );
	pNewFrame->m_fDisplayTime = ftime;

	for ( int i = 0; i < imageNames.Count(); i++ )
	{
		Assert( imageNames.Count() <= MAX_IMAGES_PER_FRAME );
		AddImage( pNewFrame, imageNames[i] );
	}

	m_pCurSequence->m_Frames.AddToTail( pNewFrame->GetHandle() );	
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CDmeAmalgamatedTexture::AddImage( CDmeSheetSequenceFrame *pNewSequenceFrame, char *pImageName )
{
	// Store the image in the image list, this is a string - bitmap mapping.
	CDmeSheetImage *pBitmap = FindImage( pImageName );
	if ( !pBitmap )
	{
		CDmeSheetImage *pBitmap = CreateElement<CDmeSheetImage>( pImageName, GetFileId() );
		pBitmap->m_pImage = CreateFloatBitmap( pImageName );
		m_ImageList.AddToTail( pBitmap );
	}

	pBitmap = FindImage( pImageName );
	Assert( pBitmap );

	pNewSequenceFrame->m_pSheetImages.AddToTail( pBitmap );

	ValidateImagePacking( pBitmap, pImageName );

	pBitmap->m_mapSequences.AddToTail( m_pCurSequence);
}


//-----------------------------------------------------------------------------
// Calls packimages with different widths to find the best size.
//-----------------------------------------------------------------------------
bool CDmeAmalgamatedTexture::DetermineBestPacking()
{
	int nBestWidth = -1;
	int nBestSize = (1 << 30 );
	int nBestSquareness = ( 1 << 30 ); // how square the texture is
	for( int nTryWidth = 2048; nTryWidth >= 64; nTryWidth >>= 1 )
	{
		bool bSuccess = PackImages( false, nTryWidth );
		if ( !bSuccess )
			break;

//		Msg( "Packing option: %d x %d (%d pixels)\n", m_nWidth.Get(), m_nHeight.Get(), m_nWidth.Get() * m_nHeight.Get() );

		bool bPreferThisPack = false;

		int thisSize = m_nHeight * m_nWidth;
		int thisSquareness = ( m_nWidth.Get() == m_nHeight.Get() ) ? 1 : ( m_nHeight / m_nWidth + m_nWidth / m_nHeight );

		if ( thisSize < nBestSize )
		{
			while ( (nTryWidth >> 1) >= m_nWidth )
				nTryWidth >>= 1;

			bPreferThisPack = true;
		}
		else if ( thisSize == nBestSize && thisSquareness < nBestSquareness )
		{
			bPreferThisPack = true;
		}

		if ( bPreferThisPack )
		{
			nBestWidth = nTryWidth;
			nBestSize = thisSize;
			nBestSquareness = thisSquareness;
		}
	}
	
	if ( nBestWidth < 0 )
	{
		Warning( "Packing error: failed to pack images!\n" );
		return false;
	}

	m_nWidth = nBestWidth;
	m_nHeight = nBestSize / nBestWidth;

//	Msg( "Best option: %d x %d (%d pixels)%s\n", m_nWidth.Get(), m_nHeight.Get(), m_nWidth.Get() * m_nHeight.Get(), ( m_nWidth.Get() == m_nHeight.Get() ) ? " : square texture" : "" );

	return true;

}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CDmeAmalgamatedTexture::PackImages( bool bGenerateImage, int nWidth )
{
	if ( !m_pPackedImage )
	{
		m_pPackedImage = CreateElement< CDmeImage >( GetName(), GetFileId() );
	}

	switch ( m_ePackingMode )
	{
	case PCKM_FLAT:
		return PackImagesFlat( bGenerateImage, nWidth );
	case PCKM_RGB_A:
		return PackImagesRGBA( bGenerateImage, nWidth );
	case PCKM_INVALID:
	default:
		return false;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CDmeAmalgamatedTexture::PackImagesFlat( bool bGenerateImage, int nWidth )
{
	int nMaxWidth = nWidth;
	int nMaxHeight = 2048;

	// !! bug !! packing algorithm is dumb and no error checking is done!
	FloatBitMap_t &output = m_pPackedImage->BeginFloatBitmapModification();
	if ( bGenerateImage )
	{
		output.Init( nMaxWidth, nMaxHeight );
	}

	int cur_line = 0;
	int cur_column = 0;
	int next_line = 0;
	int max_column_written = 0;

	for ( int i = 0; i < m_ImageList.Count(); i++ )
	{
		CDmeSheetImage &sheetImage = *(m_ImageList[i]);
		if ( sheetImage.m_pImage == NULL )
		{
			Warning( "CDataModel: Image %s was not loaded! Unable to pack.\n", sheetImage.GetName() );
			m_pPackedImage->EndFloatBitmapModification();
			return false;
		}

		if ( cur_column + sheetImage.m_pImage->NumCols() > nMaxWidth )
		{
			// no room!
			cur_column = 0;
			cur_line = next_line;
			next_line = cur_line;
		}
		// now, pack
		if ( ( cur_column + sheetImage.m_pImage->NumCols() > nMaxWidth ) ||
			 ( cur_line + sheetImage.m_pImage->NumRows() > nMaxHeight ) )
		{
			m_pPackedImage->EndFloatBitmapModification();
			return false;									// didn't fit! doh
		}
		
		sheetImage.m_XCoord = cur_column;
		sheetImage.m_YCoord = cur_line;
		
		if ( bGenerateImage )										// don't actually pack the pixel if we're not keeping them
		{
			int ic[4];
			int nc = sheetImage.m_pImage->ComputeValidAttributeList( ic );
			for ( int y = 0; y < sheetImage.m_pImage->NumRows(); y++ )
			{
				for ( int x = 0; x < sheetImage.m_pImage->NumCols(); x++ )
				{
					for ( int c = 0; c < nc; c++ )
					{
						output.Pixel( x + cur_column, y + cur_line, 0, ic[c] ) = sheetImage.m_pImage->Pixel( x, y, 0, ic[c] );
					}
				}
			}
		}

		next_line = MAX( next_line, cur_line + sheetImage.m_pImage->NumRows() );
		cur_column += sheetImage.m_pImage->NumCols();
		max_column_written = MAX( max_column_written, cur_column );
	}
	
	// now, truncate height
	int h = 1;
	for( h; h < next_line; h *= 2 ) 
		;
	// truncate width;
	int w = 1;
	for( 1; w < max_column_written; w *= 2 )
		;
	
	if ( bGenerateImage )
	{
		output.Crop( 0, 0, 0, w, h, 1 );
	}

	// Store these for UV calculation later on
	m_nHeight = h;
	m_nWidth = w;

	m_pPackedImage->EndFloatBitmapModification();
	return true;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CDmeAmalgamatedTexture::PackImagesRGBA( bool bGenerateImage, int nWidth )
{
	int nMaxWidth = nWidth;
	int nMaxHeight = 2048;

	// !! bug !! packing algorithm is dumb and no error checking is done!
	FloatBitMap_t &output = m_pPackedImage->BeginFloatBitmapModification();
	if ( bGenerateImage )
	{
		output.Init( nMaxWidth, nMaxHeight );
	}

	int cur_line[2] = {0};
	int cur_column[2] = {0};
	int next_line[2] = {0};
	int max_column_written[2] = {0};

	bool bPackingRGBA = true;

	for ( int i = 0; i < m_ImageList.Count(); i++ )
	{
		CDmeSheetImage &sheetImage = *( m_ImageList[i] );
		if ( sheetImage.m_pImage == NULL )
		{
			Warning( "CDataModel: Image %s was not loaded! Unable to pack.\n", sheetImage.GetName() );
			m_pPackedImage->EndFloatBitmapModification();
			return false;
		}

		int idxfrm;
		CDmeSheetSequence *pSequence = sheetImage.FindSequence( 0 );
		Assert( pSequence );
		int eMode = pSequence->m_eMode;
		switch ( eMode )
		{
		case SQM_RGB: 
			idxfrm = 0; 
			bPackingRGBA = false; 
			break;
		case SQM_ALPHA: 
			idxfrm = 1; 
			bPackingRGBA = false; 
			break;
		case SQM_RGBA:
			if ( !bPackingRGBA )
			{
				Msg( "*** error when packing 'rgb+a', bad sequence %d encountered for image '%s' after all rgba frames packed!\n", 
					pSequence->m_nSequenceNumber,
					m_ImageList[i]->GetName() );
				m_pPackedImage->EndFloatBitmapModification();
				return false;
			}
			idxfrm = 0; 
			break;
		default:
			{
				Msg( "*** error when packing 'rgb+a', bad sequence %d encountered for image '%s'!\n", 
					pSequence->m_nSequenceNumber,
					m_ImageList[i]->GetName() );
				m_pPackedImage->EndFloatBitmapModification();
				return false;
			}
		}

		if ( cur_column[idxfrm] + sheetImage.m_pImage->NumCols() > nMaxWidth )
		{
			// no room!
			cur_column[idxfrm] = 0;
			cur_line[idxfrm] = next_line[idxfrm];
			next_line[idxfrm] = cur_line[idxfrm];
		}

		// now, pack
		if ( ( cur_column[idxfrm] + sheetImage.m_pImage->NumCols() > nMaxWidth ) ||
			( cur_line[idxfrm] + sheetImage.m_pImage->NumRows() > nMaxHeight ) )
		{
			return false;									// didn't fit! doh
		}

		sheetImage.m_XCoord = cur_column[idxfrm];
		sheetImage.m_YCoord = cur_line[idxfrm];

		if ( bGenerateImage )								// don't actually pack the pixel if we're not keeping them
		{
			for ( int y = 0; y < sheetImage.m_pImage->NumRows(); y++ )
			{
				for ( int x = 0; x < sheetImage.m_pImage->NumCols(); x++ )
				{
					for ( int c = 0; c < 4; c++ )
					{
						switch ( eMode )
						{
							case SQM_RGB:		
								if ( c < 3 )	
									goto setpx; 
								break;
							case SQM_ALPHA:	
								if ( c == 3 )	
									goto setpx; 
								break;
							case SQM_RGBA:	
								if ( c < 4 )	
									goto setpx; 
								break;
							setpx:
								output.Pixel( x + cur_column[idxfrm], y + cur_line[idxfrm], 0, c ) = sheetImage.m_pImage->Pixel(x, y, 0, c);
								break;
						}
					}
				}
			}
		}

		next_line[idxfrm] = MAX( next_line[idxfrm], cur_line[idxfrm] + sheetImage.m_pImage->NumRows() );
		cur_column[idxfrm] += sheetImage.m_pImage->NumCols();
		max_column_written[idxfrm] = MAX( max_column_written[idxfrm], cur_column[idxfrm] );

		if ( bPackingRGBA )
		{
			cur_line[1] = cur_line[0];
			cur_column[1] = cur_column[0];
			next_line[1] = next_line[0];
			max_column_written[1] = max_column_written[0];
		}
	}

	// now, truncate height
	int h = 1;
	for ( int idxfrm = 0; idxfrm < 2; ++idxfrm )
	{
		for ( h; h < next_line[idxfrm]; h *= 2 )
			continue;
	}
	// truncate width;
	int w = 1;
	for ( int idxfrm = 0; idxfrm < 2; ++idxfrm )
	{
		for ( w; w < max_column_written[idxfrm]; w *= 2 )
			continue;
	}

	if ( bGenerateImage )
	{
		output.Crop( 0, 0, 0, w, h, 1 );
	}

	// Store these for UV calculation later on
	m_nHeight = h;
	m_nWidth = w;
	m_pPackedImage->EndFloatBitmapModification();
	return true;
}

//-----------------------------------------------------------------------------
// Write out .sht file. 
//-----------------------------------------------------------------------------
bool CDmeAmalgamatedTexture::WriteTGA( const char *pFileName )
{
	if ( !pFileName )
		goto tgaWriteFailed;

	if ( !m_pPackedImage )
		goto tgaWriteFailed;

	if ( !m_pPackedImage->FloatBitmap()->WriteTGAFile( pFileName ) )
		goto tgaWriteFailed;

	Msg( "Ok: successfully saved TGA \"%s\"\n", pFileName );
	return true;

tgaWriteFailed:
	Msg( "Error: failed to save TGA \"%s\"!\n", pFileName );
	return false;
}


//-----------------------------------------------------------------------------
// Write out .sht file. 
//-----------------------------------------------------------------------------
void CDmeAmalgamatedTexture::WriteFile( const char *pFileName, bool bVerbose )
{
	if ( !pFileName )
	{
		Msg( "Error: No output filename set!\n" );
		return;
	}

	COutputFile Outfile( pFileName );
	if ( !Outfile.IsOk() )
	{
		Msg( "Error: failed to write SHT \"%s\"!\n", pFileName );
		return;
	}

	Outfile.PutInt( 1 );								// version #
	Outfile.PutInt( m_Sequences.Count() );

	// Debugging.
	if ( bVerbose )
	{
		Msg( "1\n");
		Msg( "m_Sequences.Count() %d\n", m_Sequences.Count());
	}

	for ( int i = 0; i < m_Sequences.Count(); i++ )
	{
		Outfile.PutInt( m_Sequences[i]->m_nSequenceNumber );
		
		int nSeqFlags = 0;

		if ( m_Sequences[i]->m_Clamp )
		{
			nSeqFlags |= SEQ_FLAG_CLAMP;
		}

		if ( m_Sequences[i]->m_eMode == SQM_RGB )
		{
			nSeqFlags |= SEQ_FLAG_NO_ALPHA;
		}
		else if ( m_Sequences[i]->m_eMode == SQM_ALPHA )
		{
			nSeqFlags |= SEQ_FLAG_NO_COLOR;
		}

		Outfile.PutInt( nSeqFlags );
		
		Outfile.PutInt( m_Sequences[i]->m_Frames.Count() );
		
		// write total sequence length
		float fTotal = 0.0;
		for ( int j = 0; j < m_Sequences[i]->m_Frames.Count(); j++ )
		{
			fTotal += m_Sequences[i]->m_Frames[j]->m_fDisplayTime;
		}
		Outfile.PutFloat( fTotal );

		// Debugging.
		if ( bVerbose )
		{
			Msg( "m_Sequences[%d]->m_nSequenceNumber %d\n", i, m_Sequences[i]->m_nSequenceNumber.Get() );
			Msg( "m_Sequences[%d]->m_Clamp %d\n", i, m_Sequences[i]->m_Clamp?1:0 );
			Msg( "m_Sequences[%d] flags %d\n", i, nSeqFlags );
			Msg( "m_Sequences[%d]->m_Frames.Count() %d\n", i, m_Sequences[i]->m_Frames.Count());
			Msg( "fTotal %f\n", fTotal );
		}

		for( int j = 0; j < m_Sequences[i]->m_Frames.Count(); j++ )
		{
			Outfile.PutFloat( m_Sequences[i]->m_Frames[j]->m_fDisplayTime );
			if ( bVerbose )
			{
				Msg( "m_Sequences[%d]->m_Frames[%d]->m_fDisplayTime %f\n", i, j, m_Sequences[i]->m_Frames[j]->m_fDisplayTime.Get() );
			}
			// output texture coordinates
			Assert( m_Sequences[i]->m_Frames[j]->m_pSheetImages.Count() > 0 );
			for( int t = 0; t < m_Sequences[i]->m_Frames[j]->m_pSheetImages.Count(); t++ )
			{
				//xmin
				Outfile.PutFloat( UCoord( m_Sequences[i]->m_Frames[j]->m_pSheetImages[t]->m_XCoord ) );
				
				//ymin
				Outfile.PutFloat( VCoord( m_Sequences[i]->m_Frames[j]->m_pSheetImages[t]->m_YCoord ) );
				
				//xmax
				Outfile.PutFloat( 
					UCoord( m_Sequences[i]->m_Frames[j]->m_pSheetImages[t]->m_XCoord +
							m_Sequences[i]->m_Frames[j]->m_pSheetImages[t]->m_pImage->NumCols() - 1 ));
				
				//ymax
				Outfile.PutFloat( 
					VCoord( m_Sequences[i]->m_Frames[j]->m_pSheetImages[t]->m_YCoord +
							m_Sequences[i]->m_Frames[j]->m_pSheetImages[t]->m_pImage->NumRows() - 1 ));
				
				// Debugging.
				if ( bVerbose )
				{
					Msg( "xmin %f\n", UCoord( m_Sequences[i]->m_Frames[j]->m_pSheetImages[t]->m_XCoord ) );
					Msg( "ymin %f\n", VCoord( m_Sequences[i]->m_Frames[j]->m_pSheetImages[t]->m_YCoord ) );
					Msg( "xmax %f\n", UCoord( m_Sequences[i]->m_Frames[j]->m_pSheetImages[t]->m_XCoord +
								m_Sequences[i]->m_Frames[j]->m_pSheetImages[t]->m_pImage->NumCols() - 1 ) );
					Msg( "ymax %f\n", VCoord( m_Sequences[i]->m_Frames[j]->m_pSheetImages[t]->m_YCoord +
								m_Sequences[i]->m_Frames[j]->m_pSheetImages[t]->m_pImage->NumRows() - 1 ) );
				}
				
			}

			// Sequenceframes must have 4 entries in the .sht file.
			// We store up to 4 in the dme elements
			// Add in the missing entries as dummy data.
			for( int t = m_Sequences[i]->m_Frames[j]->m_pSheetImages.Count(); t < MAX_IMAGES_PER_FRAME; t++ )
			{
				Outfile.PutFloat(0.0);		
				Outfile.PutFloat(0.0);
				Outfile.PutFloat(0.0);
				Outfile.PutFloat(0.0);

				// Debugging.
				if ( bVerbose )
				{
					Msg( "xmin %f\nymin %f\nxmax %f\nymax %f\n", 0.0, 0.0, 0.0 ,0.0 );
				}
			}
		}
	}

	Msg( "Ok: successfully saved SHT \"%s\"\n", pFileName );
}


//-----------------------------------------------------------------------------
// Write out .sht file. 
//-----------------------------------------------------------------------------
void *CDmeAmalgamatedTexture::WriteFile( CResourceStream *pStream, ResourceId_t nTextureResourceId )
{
	Sheet_t *pSheet = pStream->Allocate< Sheet_t >( 1 );
	pSheet->m_hTexture.WriteReference( nTextureResourceId );

	int nCount = m_Sequences.Count();
	pSheet->m_Sequences = pStream->Allocate< SheetSequence_t >( nCount );

	for ( int i = 0; i < nCount; i++ )
	{
		CDmeSheetSequence *pDmeSeq = m_Sequences[i];
		SheetSequence_t &seq = pSheet->m_Sequences[i];

		seq.m_nId = pDmeSeq->m_nSequenceNumber;
		seq.m_bClamp = pDmeSeq->m_Clamp;

		int nFrameCount = pDmeSeq->m_Frames.Count();
		seq.m_Frames = pStream->Allocate< SheetSequenceFrame_t >( nFrameCount );

		seq.m_flTotalTime = 0.0;
		for( int j = 0; j < nFrameCount; j++ )
		{
			CDmeSheetSequenceFrame *pDmeFrame = pDmeSeq->m_Frames[j];
			SheetSequenceFrame_t &frame = seq.m_Frames[i];

			// Compute total sequence length
			seq.m_flTotalTime += pDmeFrame->m_fDisplayTime;

			frame.m_flDisplayTime = pDmeFrame->m_fDisplayTime;

			int nImageCount = pDmeFrame->m_pSheetImages.Count();
			frame.m_Images = pStream->Allocate< SheetFrameImage_t >( nImageCount );

			// output texture coordinates
			Assert( nImageCount > 0 );
			for( int t = 0; t < nImageCount; t++ )
			{
				CDmeSheetImage *pDmeImage = pDmeFrame->m_pSheetImages[t];
				SheetFrameImage_t &image = frame.m_Images[t];

				image.uv[0].x = UCoord( pDmeImage->m_XCoord );
				image.uv[0].y = VCoord( pDmeImage->m_YCoord );
				image.uv[1].x = UCoord( pDmeImage->m_XCoord + pDmeImage->m_pImage->NumCols() - 1 );
				image.uv[1].y = VCoord( pDmeImage->m_YCoord + pDmeImage->m_pImage->NumRows() - 1 );
			}
		}
	}

	return pSheet;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
CDmeSheetImage *CDmeAmalgamatedTexture::FindImage( const char *pImageName )
{
	int nCount = m_ImageList.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeSheetImage *pImage = m_ImageList[i];
		if ( !Q_stricmp( pImageName, pImage->GetName() ) ) 
			return pImage;
	}
	return NULL;
}

	  