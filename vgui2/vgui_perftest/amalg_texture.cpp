//===== Copyright © 2005-2006, Valve Corporation, All rights reserved. ======//
//
// Purpose: build a sheet data file and a large image out of multiple images
//
//===========================================================================//

#include "amalg_texture.h"
#include "tier0/platform.h"
#include "tier0/progressbar.h"
#include "mathlib/mathlib.h"
#include "filesystem.h"
#include "tier1/strtools.h"
#include "bitmap/floatbitmap.h"
#include "tier2/fileutils.h"
#include "stdlib.h"
#include "tier0/dbg.h"


static int GetChannelIndexFromChar( char c )
{
	// r->0 b->1 g->2 a->3 else -1

	static char s_ChannelIDs[]="rgba";

	char const *pChanChar = strchr( s_ChannelIDs, c );
	if ( ! pChanChar )
	{
		printf( " bad channel name '%c'\n", c );
		return -1;
	}
	else
	{
		return pChanChar - s_ChannelIDs;
	}
}

static void ZeroChannel( FloatBitMap_t *newBitmap, FloatBitMap_t *pBitmap, int nDestChannel )
{
	for ( int y = 0; y < newBitmap->NumRows(); y++ )
	{
		for ( int x = 0; x < newBitmap->NumCols(); x++ )
		{
			pBitmap->Pixel( x, y, nDestChannel ) = 0;
		}
	}
}

static void CopyChannel( FloatBitMap_t *newBitmap, FloatBitMap_t *pBitmap, int nSrcChannel, int nDestChannel )
{
	for ( int y = 0; y < newBitmap->NumRows(); y++ )
	{
		for ( int x = 0; x < newBitmap->NumCols(); x++ )
		{
			pBitmap->Pixel( x, y, nDestChannel ) = newBitmap->Pixel( x, y, nSrcChannel );
		}
	}
}


static FloatBitMap_t *CreateFloatBitmap( const char * fname )
{
	if ( strchr( fname, ',' ) == NULL )
	{
		char pFileFullPath[ MAX_PATH ];
		if ( !GenerateFullPath( fname, NULL, pFileFullPath, sizeof( pFileFullPath ) ) )
		{
			Warning( "CDataModel: Unable to generate full path for file %s\n", fname );
			return false;
		}
		return new FloatBitMap_t( pFileFullPath );
	}

	// parse extended specifications
	CUtlVector<char *> Images;
	V_SplitString( fname, ",", Images );
	FloatBitMap_t *pBitmap = NULL;
	// now, process bitmaps, performing copy operations specified by {} syntax
	for( int i = 0; i < Images.Count(); i++ )
	{
		char fnamebuf[MAX_PATH];
		strcpy( fnamebuf, Images[i] );
		char * pBrace=strchr( fnamebuf, '{' );
		if ( pBrace )
		{
			*pBrace = 0;								// null it
			pBrace++;									// point at control specifier
			char *pEndBrace = strchr( pBrace, '}' );
			if ( ! pEndBrace )
			{
				printf( "bad extended bitmap synax (no close brace) - %s \n", Images[i] );
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



CAmalgamatedTexture::CAmalgamatedTexture()
{
	m_ePackingMode = PCKM_FLAT;
	m_pCurSequence = NULL;
}

void CAmalgamatedTexture::Init( const char *pShtFileName )
{
	m_pShtFile = pShtFileName;
}


void CAmalgamatedTexture::SetCurrentSequenceClamp( bool bState )
{
	if ( m_pCurSequence )
	{
		m_pCurSequence->m_Clamp = bState;
	}
}

int CAmalgamatedTexture::GetPackingMode()
{
	return m_ePackingMode;
}

void CAmalgamatedTexture::SetPackingMode( int eMode )
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
			printf( "*** line error: incompatible packmode change when %d sequences already defined!\n", m_Sequences.Count() );
			//printf( "*** line %d: incompatible packmode change when %d sequences already defined!\n", m_NumActualLinesRead, m_Sequences.Count() );
			exit( -1 );
		}
	}
}



void CAmalgamatedTexture::CreateNewSequence( int sequenceNumber, int mode )
{
	m_pCurSequence = new Sequence;
	m_pCurSequence->m_nSequenceNumber = sequenceNumber;
	SetSequenceType( mode );

	m_Sequences.AddToTail( m_pCurSequence );
}

void CAmalgamatedTexture::ValidateSequenceType( int eMode, char *word )
{
	switch ( m_ePackingMode )
	{
	case PCKM_FLAT:
		switch ( eMode )
		{
		case SQM_RGBA:	
			break;
		default:
			{
				printf( "*** line error: invalid sequence type '%s', packing 'flat' allows only 'sequence-rgba'!\n", word );
				//printf( "*** line %d: invalid sequence type '%s', packing 'flat' allows only 'sequence-rgba'!\n", m_NumActualLinesRead, word );
				exit( -1 );
			}
		}
		break;
	case PCKM_RGB_A:
		switch ( eMode )
		{
		case SQM_RGB:
		case SQM_ALPHA:	
			break;
		default:
			{
				//printf( "*** line %d: invalid sequence type '%s', packing 'rgb+a' allows only 'sequence-rgb' or 'sequence-a'!\n", m_NumActualLinesRead, word );
				exit( -1 );
			}
		}
		break;
	}
}

int CAmalgamatedTexture::GetSequenceType()
{
	return m_pCurSequence->m_eMode;
}

void CAmalgamatedTexture::SetSequenceType( int eMode )
{
	m_pCurSequence->m_eMode = eMode;
}

bool CAmalgamatedTexture::CurrentSequenceExists()
{
	return m_pCurSequence != NULL;
}

// Validate that frame packing is correct
void CAmalgamatedTexture::ValidateFramePacking( SequenceFrame *pBitmap, char *fileName )
{
	if ( m_ePackingMode == PCKM_RGB_A )
	{
		for ( uint16 idx = 0; idx < pBitmap->m_mapSequences.Count(); ++idx )
		{
			Sequence *pSeq = pBitmap->m_mapSequences.Key( idx );
			if ( pSeq->m_eMode != SQM_RGBA &&
				pSeq->m_eMode != m_pCurSequence->m_eMode )
			{
				printf( "*** line error: 'rgb+a' packing cannot pack frame '%s' belonging to sequences %d and %d!\n", 
					fileName,
					pSeq->m_nSequenceNumber, 
					m_pCurSequence->m_nSequenceNumber );

				//printf( "*** line %d: 'rgb+a' packing cannot pack frame '%s' belonging to sequences %d and %d!\n", 
				//	m_NumActualLinesRead,
				//	fileName,
				//	pSeq->m_nSequenceNumber, 
				//	m_pCurSequence->m_nSequenceNumber );
				exit( -1 );
			}
		}
	}
}

void CAmalgamatedTexture::CreateFrame( float ftime, CUtlVector<char *> &frameNames )
{
	SequenceEntry newSequenceEntry;
	newSequenceEntry.m_fDisplayTime = ftime;

	for ( int i = 0; i < frameNames.Count(); i++ )
	{
		LoadFrame( newSequenceEntry, frameNames[i], i );
	}

	m_pCurSequence->m_Frames.AddToTail( newSequenceEntry );	
}

void CAmalgamatedTexture::LoadFrame( SequenceEntry &newSequenceEntry, char *fnamebuf, int frameNumber )
{
	SequenceFrame *pBitmap;
	// Store the frame in the image list, this is a string - bitmap mapping.
	if ( ! ( m_ImageList.Defined( fnamebuf ) ) )
	{
		SequenceFrame *pNew_frm = new SequenceFrame;
		pNew_frm->m_pImage = CreateFloatBitmap( fnamebuf );
		pBitmap = pNew_frm;
		m_ImageList[ fnamebuf ] = pNew_frm;
	}
	else
	{
		pBitmap = m_ImageList[ fnamebuf ];
	}
	
	newSequenceEntry.m_pSeqFrame[frameNumber] = pBitmap;

	// Validate that frame packing is correct
	ValidateFramePacking( pBitmap, fnamebuf );

	pBitmap->m_mapSequences.Insert( m_pCurSequence, 1 );
	
	if ( frameNumber == 0 )
	{
		for( int j = 1; j < MAX_IMAGES_PER_FRAME; j++ )
		{
			newSequenceEntry.m_pSeqFrame[j] = newSequenceEntry.m_pSeqFrame[0];
		}
	}	
}


void CAmalgamatedTexture::DetermineBestPacking()
{
	int nBestWidth = -1;
	int nBestSize = (1 << 30 );
	int nBestSquareness = ( 1 << 30 ); // how square the texture is
	for( int nTryWidth = 2048 ; nTryWidth >= 64; nTryWidth >>= 1 )
	{
		bool bSuccess = PackImages( NULL, nTryWidth );
		if ( bSuccess )
		{
			printf( "Packing option: %dx%d (%d pixels)\n", m_nWidth, m_nHeight, m_nWidth * m_nHeight );

			bool bPreferThisPack = false;

			int thisSize = m_nHeight * m_nWidth;
			int thisSquareness = ( m_nWidth == m_nHeight ) ? 1 : ( m_nHeight / m_nWidth + m_nWidth / m_nHeight );

			if ( thisSize < nBestSize )
			{
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
		else
		{
			break;
		}
	}
	
	if ( nBestWidth < 0 )
	{
		printf( "Packing error: failed to pack images!\n" );
		exit(1);
	}

	m_nWidth = nBestWidth;
	m_nHeight = nBestSize / nBestWidth;

	printf( "Best option: %dx%d (%d pixels)%s\n", m_nWidth, m_nHeight, m_nWidth * m_nHeight, ( m_nWidth == m_nHeight ) ? " : square texture" : "" );

}

bool CAmalgamatedTexture::PackImages( char const *pFilename, int nWidth )
{
	switch ( m_ePackingMode )
	{
	case PCKM_FLAT:
		return PackImages_Flat( pFilename, nWidth );
	case PCKM_RGB_A:
		return PackImages_Rgb_A( pFilename, nWidth );
	case PCKM_INVALID:
	default:
		return false;
	}
}

bool CAmalgamatedTexture::PackImages_Flat( char const *pFilename, int nWidth )
{
	// !! bug !! packing algorithm is dumb and no error checking is done!
	FloatBitMap_t output( nWidth, 2048 );
	int cur_line = 0;
	int cur_column = 0;
	int next_line = 0;
	int max_column_written = 0;

	for ( int i = 0; i < m_ImageList.GetNumStrings(); i++ )
	{
		SequenceFrame &frm = *(m_ImageList[i]);
		if ( cur_column+frm.m_pImage->NumCols() > output.NumCols() )
		{
			// no room!
			cur_column = 0;
			cur_line = next_line;
			next_line = cur_line;
		}
		// now, pack
		if ( ( cur_column + frm.m_pImage->NumCols() > output.NumCols() ) ||
			 ( cur_line + frm.m_pImage->NumRows() > output.NumRows() ) )
		{
			return false;									// didn't fit! doh
		}
		
		frm.m_XCoord = cur_column;
		frm.m_YCoord = cur_line;
		
		if ( pFilename )										// don't actually pack the pixel if we're not keeping them
		{
			for ( int y = 0; y < frm.m_pImage->NumRows(); y++ )
				for ( int x = 0; x < frm.m_pImage->NumCols(); x++ )
					for ( int c = 0; c < 4; c++ )
					{
						output.Pixel( x+cur_column,y+cur_line, c ) = frm.m_pImage->Pixel(x, y, c);
					}
		}

		next_line = max( next_line, cur_line+frm.m_pImage->NumRows() );
		cur_column += frm.m_pImage->NumCols();
		max_column_written = max( max_column_written, cur_column );
	}
	
	// now, truncate height
	int h = 1;
	for( h; h < next_line; h *= 2 ) 
		;
	// truncate width;
	int w = 1;
	for( 1; w < max_column_written; w *= 2 )
		;
	
	if ( pFilename )
	{
		FloatBitMap_t cropped_output( w, h );
		for( int y = 0;y < cropped_output.NumRows(); y++ )
		{
			for( int x = 0; x < cropped_output.NumCols(); x++ )
			{
				for( int c = 0; c < 4; c++ )
				{
					cropped_output.Pixel( x,y,c ) = output.Pixel( x,y,c );
				}
			}
		}

		bool bWritten = cropped_output.WriteTGAFile( pFilename );
		if ( !bWritten )
			printf( "Error: failed to save TGA \"%s\"!\n", pFilename );
		else
			printf( "Ok: successfully saved TGA \"%s\"\n", pFilename );
	}

	// Store these for UV calculation later on
	m_nHeight = h;
	m_nWidth = w;
	return true;
}

bool CAmalgamatedTexture::PackImages_Rgb_A( char const *pFilename, int nWidth )
{
	// !! bug !! packing algorithm is dumb and no error checking is done!
	FloatBitMap_t output( nWidth, 2048 );
	int cur_line[2] = {0};
	int cur_column[2] = {0};
	int next_line[2] = {0};
	int max_column_written[2] = {0};

	bool bPackingRGBA = true;

	for ( int i = 0; i < m_ImageList.GetNumStrings(); i++ )
	{
		SequenceFrame &frm = *( m_ImageList[i] );

		int idxfrm;
		int eMode = frm.m_mapSequences.Key( 0 )->m_eMode;
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
				printf( "*** error when packing 'rgb+a', bad sequence %d encountered for frame '%s' after all rgba frames packed!\n", 
					frm.m_mapSequences.Key( 0 )->m_nSequenceNumber, 
					m_ImageList.String( i ) );
				exit( -1 );
			}
			idxfrm = 0; 
			break;
		default:
			{
				printf( "*** error when packing 'rgb+a', bad sequence %d encountered for frame '%s'!\n", 
					frm.m_mapSequences.Key( 0 )->m_nSequenceNumber, 
					m_ImageList.String( i ) );
				exit( -1 );
			}
		}

		if ( cur_column[idxfrm] + frm.m_pImage->NumCols() > output.NumCols() )
		{
			// no room!
			cur_column[idxfrm] = 0;
			cur_line[idxfrm] = next_line[idxfrm];
			next_line[idxfrm] = cur_line[idxfrm];
		}

		// now, pack
		if ( ( cur_column[idxfrm] + frm.m_pImage->NumCols() > output.NumCols() ) ||
			( cur_line[idxfrm] + frm.m_pImage->NumRows() > output.NumRows() ) )
		{
			return false;									// didn't fit! doh
		}

		frm.m_XCoord = cur_column[idxfrm];
		frm.m_YCoord = cur_line[idxfrm];

		if ( pFilename )										// don't actually pack the pixel if we're not keeping them
		{
			for ( int y = 0; y < frm.m_pImage->NumRows(); y++ )
			{
				for (int x = 0; x < frm.m_pImage->NumCols(); x++ )
				{
					for(int c = 0; c < 4; c ++ )
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
								output.Pixel( x + cur_column[idxfrm], y + cur_line[idxfrm], c ) = frm.m_pImage->Pixel(x, y, c);
						}
					}
				}
			}
		}

		next_line[idxfrm] = max( next_line[idxfrm], cur_line[idxfrm] + frm.m_pImage->NumRows() );
		cur_column[idxfrm] += frm.m_pImage->NumCols();
		max_column_written[idxfrm] = max( max_column_written[idxfrm], cur_column[idxfrm] );

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

	if ( pFilename )
	{
		FloatBitMap_t cropped_output( w, h );
		for( int y = 0; y < cropped_output.NumRows(); y++ )
		{
			for( int x = 0; x < cropped_output.NumCols(); x++ )
			{
				for( int c = 0; c < 4; c++ )
				{
					cropped_output.Pixel( x, y, c ) = output.Pixel( x, y, c );
				}
			}
		}

		bool bWritten = cropped_output.WriteTGAFile( pFilename );
		if ( !bWritten )
		{
			printf( "Error: failed to save TGA \"%s\"!\n", pFilename );
		}
		else
		{
			printf( "Ok: successfully saved TGA \"%s\"\n", pFilename );
		}
	}

	// Store these for UV calculation later on
	m_nHeight = h;
	m_nWidth = w;
	return true;
}

void CAmalgamatedTexture::WriteFile()
{
	if ( m_pShtFile == NULL )
	{
		printf( "Error: No output filename set!\n" );
		return;
	}

	COutputFile Outfile( m_pShtFile );
	if ( !Outfile.IsOk() )
	{
		printf( "Error: failed to write SHT \"%s\"!\n", m_pShtFile );
		return;
	}

	Outfile.PutInt( 1 );								// version #
	Outfile.PutInt( m_Sequences.Count() );
	for ( int i = 0; i < m_Sequences.Count(); i++ )
	{
		Outfile.PutInt( m_Sequences[i]->m_nSequenceNumber );
		Outfile.PutInt( m_Sequences[i]->m_Clamp );
		Outfile.PutInt( m_Sequences[i]->m_Frames.Count() );
		// write total sequence length
		float fTotal = 0.0;
		for ( int j = 0; j < m_Sequences[i]->m_Frames.Count(); j++ )
		{
			fTotal += m_Sequences[i]->m_Frames[j].m_fDisplayTime;
		}
		Outfile.PutFloat( fTotal );
		for( int j = 0; j < m_Sequences[i]->m_Frames.Count(); j++ )
		{
			Outfile.PutFloat( m_Sequences[i]->m_Frames[j].m_fDisplayTime );
			// output texture coordinates
			for( int t = 0; t < MAX_IMAGES_PER_FRAME; t++ )
			{
				//xmin
				Outfile.PutFloat( UCoord( m_Sequences[i]->m_Frames[j].m_pSeqFrame[t]->m_XCoord ) );
				//ymin
				Outfile.PutFloat( VCoord( m_Sequences[i]->m_Frames[j].m_pSeqFrame[t]->m_YCoord ) );
				//xmax
				Outfile.PutFloat( 
					UCoord( m_Sequences[i]->m_Frames[j].m_pSeqFrame[t]->m_XCoord +
							m_Sequences[i]->m_Frames[j].m_pSeqFrame[t]->m_pImage->NumCols() - 1 ));
				//ymax
				Outfile.PutFloat( 
					VCoord( m_Sequences[i]->m_Frames[j].m_pSeqFrame[t]->m_YCoord +
							m_Sequences[i]->m_Frames[j].m_pSeqFrame[t]->m_pImage->NumRows() - 1 ));
// 				printf( "T %d UV1:( %.2f, %.2f ) UV2:( %.2f, %.2f )\n", t,
// 					UCoord( Sequences[i]->m_Frames[j].m_pSeqFrame[t]->m_XCoord ),
// 					VCoord( Sequences[i]->m_Frames[j].m_pSeqFrame[t]->m_YCoord ),
// 					UCoord( Sequences[i]->m_Frames[j].m_pSeqFrame[t]->m_XCoord+Sequences[i]->m_Frames[j].m_pSeqFrame[t]->m_pImage->NumCols()-1 ),
// 					VCoord( Sequences[i]->m_Frames[j].m_pSeqFrame[t]->m_YCoord+Sequences[i]->m_Frames[j].m_pSeqFrame[t]->m_pImage->NumRows()-1 ));
			}
		}
	}

	printf( "Ok: successfully saved SHT \"%s\"\n", m_pShtFile );
}

