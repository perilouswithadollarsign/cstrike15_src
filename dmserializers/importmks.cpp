//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "importkeyvaluebase.h"
#include "dmserializers.h"
#include "datamodel/idatamodel.h"
#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"
#include "tier1/KeyValues.h"
#include "tier1/UtlBuffer.h"
#include "datamodel/dmattribute.h"
#include "filesystem.h"
#include "tier2/tier2.h"
#include "materialobjects/amalgtexturevars.h"


//-----------------------------------------------------------------------------
// Serialization class for make sheet files
//-----------------------------------------------------------------------------
class CImportMKS : public IDmSerializer
{
public:
	virtual const char *GetName() const { return "mks"; }
	virtual const char *GetDescription() const { return "Valve Make Sheet File"; }
	virtual bool IsBinaryFormat() const { return false; }
	virtual bool StoresVersionInFile() const { return false; }
	virtual int GetCurrentVersion() const { return 0; } // doesn't store a version
 	virtual const char *GetImportedFormat() const { return "mks"; }
 	virtual int GetImportedVersion() const { return 1; }

	bool Serialize( CUtlBuffer &outBuf, CDmElement *pRoot );
	// Read from the UtlBuffer, return true if successful, and return the read-in root in ppRoot.
	bool Unserialize( CUtlBuffer &buf, const char *pEncodingName, int nEncodingVersion, const char *pSourceFormatName, 
		int nSourceFormatVersion, DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot );

private:

	CDmElement *CreateDmeAmalgamatedTexture( const char *pName );
	CDmElement *CreateDmeSequence( const char *pName );
	CDmElement *CreateDmeSequenceFrame( const char *pName );
	CDmElement *CreateDmeSheetImage( const char *pImageName );
	

	void SetCurrentSequenceClamp( bool bState );

	int ParsePackingMode( char *word );
	bool SetPackingMode( int eMode );

	int ParseSequenceType( char *word );
	bool ValidateSequenceType( int eMode, char *word );
	bool CreateNewSequence( int mode );

	void ParseFrameImages( CUtlVector<char *> &words, CUtlVector<char *> &outImageNames );
	bool CreateNewFrame( CUtlVector<char *> &imageNames, float ftime );

	bool ValidateImagePacking( CDmElement *pBitmap, char *pImageName );
	CDmElement *FindSequence( CDmrElementArray< CDmElement > &mapsequences, int index );
	CDmElement *FindImage( const char *pFrameName );
	void AddImage( CDmElement *newSequenceEntry, char *pImageName );

	CDmElement *m_pRoot;
	int m_NumActualLinesRead;
	CDmElement *m_pCurrentSequence;
	DmFileId_t m_Fileid;

	int m_SequenceCount;
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CImportMKS s_ImportMKS;

void InstallMKSImporter( IDataModel *pFactory )
{
	pFactory->AddSerializer( &s_ImportMKS );
}


//-----------------------------------------------------------------------------
// Writes out a new vmt file
//-----------------------------------------------------------------------------
bool CImportMKS::Serialize( CUtlBuffer &buf, CDmElement *pRoot )
{
	//PrintStringAttribute( pRoot, buf, "shader", false, true );
	buf.Printf( "{\n" );
	buf.PushTab();

	//if ( !SerializeShaderParameter( buf, pRoot ) )
	//	return false;

	buf.PopTab();
	buf.Printf( "}\n" );
	return true;
}

//--------------------------------------------------
// Helper functions
//--------------------------------------------------
static void ApplyMacros( char * in_buf )
{
	CUtlVector<char *> Words;
	V_SplitString( in_buf, " ", Words);
	if ( ( Words.Count() == 4 ) && (! stricmp( Words[0],"ga_frame") ) )
	{
		// ga_frame frm1 frm2 n -> frame frm1{r=a},frm1{g=a},frm1{b=a},frm2{a=a} n
		sprintf( in_buf, "frame %s{r=0},%s{g=a},%s{b=0},%s{a=a} %s",
				Words[1], Words[1], Words[1], Words[2], Words[3] );
	}
	Words.PurgeAndDeleteElements();
}
		 
static char *MoveToStart( char *pLineBuffer )
{
	// Kill newline	'\n'
	char *pChop = strchr( pLineBuffer, '\n' );
	if ( pChop )
		*pChop = 0;

	// Kill '//' remove comment lines.
	char *comment = Q_strstr( pLineBuffer, "//" );
	if ( comment )
		*comment = 0;

	// Move to start of non-whitespace
	char *in_str = pLineBuffer;
	while( ( in_str[0]==' ' ) || ( in_str[0]=='\t') )
		in_str++;

	return in_str;
}

//--------------------------------------------------
// Parse out the packing mode
//--------------------------------------------------
int CImportMKS::ParsePackingMode( char *word )
{
	// Read in the packing mode requested.
	int eRequestedMode = PCKM_INVALID;

	if ( !stricmp( word, "flat" ) || !stricmp( word, "rgba" ) )
	{
		eRequestedMode = PCKM_FLAT;
	}
	else if ( !stricmp( word, "rgb+a" ) )
	{
		eRequestedMode = PCKM_RGB_A;
	}
	
	if ( eRequestedMode == PCKM_INVALID )
	{
		Warning( "*** line %d: invalid packmode specified, allowed values are 'rgba' or 'rgb+a'!\n", m_NumActualLinesRead );
	}	

	return eRequestedMode;
}  


//--------------------------------------------------
// Parse out the sequence type
//--------------------------------------------------
int CImportMKS::ParseSequenceType( char *word )
{
	int eMode = SQM_ALPHA_INVALID;

	char const *szSeqType = StringAfterPrefix( word, "sequence" );
	if ( !stricmp( szSeqType, "" ) || !stricmp( szSeqType, "-rgba" ) )
	{
		eMode = SQM_RGBA;
	}
	else if ( !stricmp( szSeqType, "-rgb" ) )
	{
		eMode = SQM_RGB;
	}
	else if ( !stricmp( szSeqType, "-a" ) )
	{
		eMode = SQM_ALPHA;
	}
	else
	{
		Warning( "*** line %d: invalid sequence type '%s', allowed 'sequence-rgba' or 'sequence-rgb' or 'sequence-a'!\n", m_NumActualLinesRead, word );
	}

	return eMode;
}





//--------------------------------------------------
// Functions to set attribute values
//--------------------------------------------------
void CImportMKS::SetCurrentSequenceClamp( bool bState )
{
	Warning( "Attempting to set clamp when there is no current sequence!\n" );
	if ( m_pCurrentSequence )
	{
		CDmAttribute *pClamp= m_pCurrentSequence->GetAttribute( "clamp" );
		Assert( pClamp );
		pClamp->SetValue< bool >( bState );
	}
}

bool CImportMKS::SetPackingMode( int eMode )
{
	CDmAttribute *pCurrentPackingModeAttr = m_pRoot->GetAttribute( "packmode" );
	Assert( pCurrentPackingModeAttr );
	int currentPackingMode = pCurrentPackingModeAttr->GetValue< int >();

	CDmrElementArray< CDmElement > sequences( m_pRoot, "sequences", true );

	// Assign the packing mode read in to member var.
	if ( !sequences.Count() )
	{
		pCurrentPackingModeAttr->SetValue< int >( eMode );
	}
	else if ( currentPackingMode != eMode )
	{
		// Allow special changes:
		// flat -> rgb+a
		if ( currentPackingMode == PCKM_FLAT && eMode == PCKM_RGB_A )
		{
			Msg( "Warning changing packing mode when %d sequences already defined. This may not be serialized correctly.\n", sequences.Count() );
			pCurrentPackingModeAttr->SetValue< int >( eMode );
		}
		// everything else
		else
		{
			Msg( "*** line error: incompatible packmode change when %d sequences already defined!\n", sequences.Count() );
			return false;
		}
	}

	return true;
}

//--------------------------------------------------
// Validation
//--------------------------------------------------
bool CImportMKS::ValidateSequenceType( int eMode, char *word )
{
	CDmAttribute *pCurrentPackingModeAttr = m_pRoot->GetAttribute( "packmode" );
	Assert( pCurrentPackingModeAttr );
	int currentPackingMode = pCurrentPackingModeAttr->GetValue< int >();

	switch ( currentPackingMode )
	{
	case PCKM_FLAT:
		switch ( eMode )
		{
		case SQM_RGBA:	
			break;
		default:
			Msg( "*** line error: invalid sequence type '%s', packing 'flat' allows only 'sequence-rgba'!\n", word );
			return false;	
		}
		break;
	case PCKM_RGB_A:
		switch ( eMode )
		{
		case SQM_RGB:
		case SQM_ALPHA:	
			break;
		default:
			return false;
			
		}
		break;
	default:
		Warning( "Invalid packing mode!" );
		return false;
	}

	return true;
}

//--------------------------------------------------
// Validate that image packing is correct
//--------------------------------------------------
bool CImportMKS::ValidateImagePacking( CDmElement *pBitmap, char *pImageName )
{
	CDmAttribute *pCurrentPackingModeAttr = m_pRoot->GetAttribute( "packmode" );
	Assert( pCurrentPackingModeAttr );
	int currentPackingMode = pCurrentPackingModeAttr->GetValue< int >();

	if ( currentPackingMode == PCKM_RGB_A )
	{
		CDmrElementArray< CDmElement > mapsequences( pBitmap, "mapsequences", true );
		for ( uint16 idx = 0; idx < mapsequences.Count(); ++idx )
		{
			CDmElement *pSeq = FindSequence( mapsequences, idx );

			Assert( pSeq );

			CDmAttribute *pSequenceNumberAttr = pSeq->GetAttribute( "sequencenumber" );
			Assert( pSequenceNumberAttr );
			int sequenceNumber = pSequenceNumberAttr->GetValue<int>();

			CDmAttribute *pModeAttr = pSeq->GetAttribute( "mode" );
			Assert( pModeAttr );
			int mode = pModeAttr->GetValue<int>();

			CDmAttribute *pCurrentSequenceNumberAttr = m_pCurrentSequence->GetAttribute( "sequencenumber" );
			Assert( pCurrentSequenceNumberAttr );
			int currentSequenceNumber = pCurrentSequenceNumberAttr->GetValue<int>();

			CDmAttribute *pCurrentModeAttr = m_pCurrentSequence->GetAttribute( "mode" );
			Assert( pCurrentModeAttr );
			int currentMode = pCurrentModeAttr->GetValue<int>();

			if ( ( mode != SQM_RGBA ) &&  ( mode != currentMode ) )
			{
				Msg( "*** line error: 'rgb+a' packing cannot pack image '%s' belonging to sequences %d and %d!\n", 
					pImageName,
					sequenceNumber, 
					currentSequenceNumber );

				return false;
			}
		}
	}

	return true;
}


//--------------------------------------------------
// Functions to create dme elements
//--------------------------------------------------
CDmElement *CImportMKS::CreateDmeAmalgamatedTexture( const char *pName )
{
	DmElementHandle_t hElement = g_pDataModel->CreateElement( "DmeAmalgamatedTexture", "CDmeAmalgamatedTexture", m_Fileid );
	if ( hElement == DMELEMENT_HANDLE_INVALID )
	{
		Warning( "Element uses unknown element type %s\n", "CDmeAmalgamatedTexture" );
		return NULL;
	}

	CDmElement *pElement = g_pDataModel->GetElement( hElement );
	if ( !pElement )
		return NULL;

	// Base members
	if ( !pElement->AddAttribute( "images", AT_ELEMENT_ARRAY ) )
		return NULL;
	if ( !pElement->AddAttribute( "packmode", AT_INT ) )
		return NULL;
	if ( !pElement->AddAttribute( "width", AT_INT ) )
		return NULL;
	if ( !pElement->AddAttribute( "height", AT_INT ) )
		return NULL;

	return pElement;
}

CDmElement *CImportMKS::CreateDmeSequence( const char *pName )
{
	DmElementHandle_t hElement = g_pDataModel->CreateElement( "DmeSheetSequence", pName, m_Fileid );
	if ( hElement == DMELEMENT_HANDLE_INVALID )
	{
		Warning( "Element uses unknown element type %s\n", "CDmeSheetSequence" );
		return false;
	}

	CDmElement *pElement = g_pDataModel->GetElement( hElement );
	if ( !pElement )
		return NULL;

	if ( !pElement->AddAttribute( "sequencenumber", AT_INT ) )
		return false;
	if ( !pElement->AddAttribute( "clamp", AT_BOOL ) )
		return false;
	if ( !pElement->AddAttribute( "mode", AT_INT ) )
		return false;
	if ( !pElement->AddAttribute( "frames", AT_ELEMENT_ARRAY ) )
		return false;

	CDmAttribute *pClapAttr = pElement->GetAttribute( "clamp" );
	Assert( pClapAttr );
	pClapAttr->SetValue< bool >( true );

	CDmAttribute *pModeAttr = pElement->GetAttribute( "mode" );
	Assert( pModeAttr );
	pModeAttr->SetValue< int >( SQM_RGBA );

	return pElement;
}

CDmElement *CImportMKS::CreateDmeSequenceFrame( const char *pName )
{
	DmElementHandle_t hElement = g_pDataModel->CreateElement( "DmeSheetSequenceFrame", pName, m_Fileid );
	if ( hElement == DMELEMENT_HANDLE_INVALID )
	{
		Warning( "Element uses unknown element type %s\n", "CDmeSheetSequenceFrame" );
		return false;
	}

	CDmElement *pElement = g_pDataModel->GetElement( hElement );
	if ( !pElement )
		return NULL;

	if ( !pElement->AddAttribute( "sheetimages", AT_ELEMENT_ARRAY ) )
		return false;
	if ( !pElement->AddAttribute( "displaytime", AT_FLOAT ) )
		return false;

	return pElement;
}

CDmElement *CImportMKS::CreateDmeSheetImage( const char *pImageName )
{
	DmElementHandle_t hElement = g_pDataModel->CreateElement( "DmeSheetImage", pImageName, m_Fileid );
	if ( hElement == DMELEMENT_HANDLE_INVALID )
	{
		Warning("Element uses unknown element type %s\n", "CDmeSheetImage" );
		return false;
	}

	CDmElement *pElement = g_pDataModel->GetElement( hElement );
	if ( !pElement )
		return NULL;

	if ( !pElement->AddAttribute( "xcoord", AT_INT ) )
		return NULL;
	if ( !pElement->AddAttribute( "ycoord", AT_INT ) )
		return NULL;

	return pElement;
}

//---------------------------------------------------------------
// Functions to put this all together
//---------------------------------------------------------------

bool CImportMKS::CreateNewSequence( int mode )
{
	m_pCurrentSequence = CreateDmeSequence( "CDmeSheetSequence" );
	if ( !m_pCurrentSequence )
		return false;

	CDmAttribute *pSeqNoAttr = m_pCurrentSequence->GetAttribute( "sequencenumber" );
	Assert( pSeqNoAttr );
	pSeqNoAttr->SetValue< int >( m_SequenceCount );
	m_SequenceCount++;

	CDmAttribute *pSeqType = m_pCurrentSequence->GetAttribute( "mode" );
	Assert( pSeqType );
	pSeqType->SetValue< int >( mode );

	CDmrElementArray< CDmElement > sequences( m_pRoot, "sequences", true );
	sequences.AddToTail( m_pCurrentSequence );

	return true;
}


bool CImportMKS::CreateNewFrame( CUtlVector<char *> &imageNames, float ftime )
{
	CDmElement *pNewFrame = CreateDmeSequenceFrame( "CDmeSheetSequenceFrame" );
	if ( !pNewFrame )
		return false;

	CDmAttribute *pDisplayTimeAttr = pNewFrame->GetAttribute( "displaytime" );
	Assert( pDisplayTimeAttr );
	pDisplayTimeAttr->SetValue< float >( ftime );

	for ( int i = 0; i < imageNames.Count(); ++i )
	{	
		Assert( imageNames.Count() <= MAX_IMAGES_PER_FRAME );
		AddImage( pNewFrame, imageNames[i] );	
	}

	CDmrElementArray< CDmElement > currentFrames( m_pCurrentSequence->GetAttribute( "frames" ) );
	currentFrames.AddToTail( pNewFrame );

	return true;
}

CDmElement *CImportMKS::FindImage( const char *pFrameName )
{
	CDmrElementArray< CDmElement > images = m_pRoot->GetAttribute( "images" );
	int nCount = images.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( pFrameName, images[i]->GetName() ) ) 
			return images[i];
	}
	return NULL;
}

CDmElement *CImportMKS::FindSequence( CDmrElementArray< CDmElement > &mapsequences, int index )
{
	if ( index < mapsequences.Count() )
	{
		return mapsequences[index];
	}
	return NULL;
}

void CImportMKS::AddImage( CDmElement *pSequenceEntry, char *pImageName )
{
	// Store the image in the image list, this is a string - bitmap mapping.
	CDmElement *pBitmap = FindImage( pImageName );
	if ( !pBitmap )
	{
		CDmElement *pBitmap = CreateDmeSheetImage( pImageName );
		if ( !pBitmap )
			return;

		CDmrElementArray< CDmElement > images = m_pRoot->GetAttribute( "images" );
		images.AddToTail( pBitmap );
	}

	pBitmap = FindImage( pImageName );
	Assert( pBitmap );


	CDmrElementArray< CDmElement > sheetImages = pSequenceEntry->GetAttribute( "sheetimages" );
	sheetImages.AddToTail( pBitmap );

	if ( !ValidateImagePacking( pBitmap, pImageName ) )
	{
		Warning( "Image packing validation failed!" );
	}

	CDmrElementArray< CDmElement > mapSequences( pBitmap, "mapsequences", true );
	mapSequences.AddToTail( m_pCurrentSequence );
	
}

void CImportMKS::ParseFrameImages( CUtlVector<char *> &words, CUtlVector<char *> &outImageNames )
{
	for ( int i = 0; i < words.Count() - 2; i++ )
	{
		char *fnamebuf = words[i+1];
		outImageNames.AddToTail( fnamebuf );	
	}
}



//-----------------------------------------------------------------------------
// Main entry point for the unserialization
//-----------------------------------------------------------------------------
bool CImportMKS::Unserialize( CUtlBuffer &buf, const char *pEncodingName, int nEncodingVersion, const char *pSourceFormatName, 
				 int nSourceFormatVersion, DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot )
{
	*ppRoot = NULL;
	m_Fileid = fileid;

	// Create the main element
	m_pRoot = CreateDmeAmalgamatedTexture( "CDmeAmalgamatedTexture" );
	if ( !m_pRoot )
		return false;

	*ppRoot = m_pRoot;

	// Initial value for this param
	m_SequenceCount = 0;
	bool bSuccess = SetPackingMode( PCKM_FLAT );
	if ( !bSuccess )
		return false;

	char linebuffer[4096];
	m_NumActualLinesRead = 0;
	
	while ( buf.IsValid() )
	{
		buf.GetLine( linebuffer, sizeof(linebuffer) );
		++m_NumActualLinesRead;

		char *in_str = MoveToStart( linebuffer );
		if ( in_str[0] == NULL )
			continue;

		strlwr( in_str ); // send string to lowercase.
		ApplyMacros( in_str );
		CUtlVector<char *> words;
		V_SplitString( in_str, " ", words);
		if ( ( words.Count() == 1) && ( !stricmp( words[0], "loop" ) ) )
		{
			SetCurrentSequenceClamp( false );
		}
		else if ( ( words.Count() == 2 ) && ( !stricmp( words[0], "packmode" ) ) )
		{
			// Read in the packing mode requested.
			int eRequestedMode = ParsePackingMode( words[1] );
			if ( ( eRequestedMode == PCKM_INVALID ) || !SetPackingMode( eRequestedMode ) )
			{
				Warning( "Unable to set packing mode." );
				return NULL;
			}
		}
		else if ( ( words.Count() == 2) && StringHasPrefix( words[0], "sequence" ) )
		{
			int seq_no = atoi( words[1] );
			if ( seq_no != m_SequenceCount )
			{
				Warning( "Sequence number mismatch.\n" );
			}
			
			// Figure out the sequence type
			int mode = ParseSequenceType( words[0] );
			if ( ( mode == SQM_ALPHA_INVALID ) || !ValidateSequenceType( mode, words[0] ) )
			{
				Warning( "Invalid sequence type.\n" );
				return NULL;
			}
		
			bool bSuccess = CreateNewSequence( mode );
			if ( !bSuccess )
			{
				Warning( "Unable to create new sequence.\n" );
				return NULL;
			}
		}
		else if  ( ( words.Count() >= 3) && (! stricmp( words[0], "frame" ) ) )
		{
			if ( m_pCurrentSequence )
			{
				float ftime = atof( words[ words.Count() - 1 ] );
//				Warning( "ftime is %f\n", ftime );
				
				CUtlVector<char *> imageNames;
				ParseFrameImages( words, imageNames );
				bool bSuccess = CreateNewFrame( imageNames, ftime );
				if ( !bSuccess )
				{
					Warning( "Unable to create new frame.\n" );
					return NULL;
				}
			}
			else
			{
				Warning( "Trying to add a frame when there is no current sequence.\n" );
			}
		}
		else
		{
			Warning( "*** line %d: Bad command \"%s\"!\n", m_NumActualLinesRead, in_str );
			return NULL;
		}
		words.PurgeAndDeleteElements();
	}	

	// Import compiler settings
	char pTexFile[MAX_PATH];
	const char *pFileName = g_pDataModel->GetFileName( fileid );
	Q_strncpy( pTexFile, pFileName, sizeof(pTexFile) );
	Q_SetExtension( pTexFile, "txt", sizeof(pTexFile) );
	if ( g_pFullFileSystem->FileExists( pTexFile ) )
	{
		CDmElement *pTextureCompileSettings = NULL;
		if ( !g_pDataModel->RestoreFromFile( pTexFile, NULL, "tex_source1", &pTextureCompileSettings, CR_COPY_NEW ) )
		{
			Warning( "Error reading texture compile settings file \"%s\"!\n", pTexFile );
			return NULL;
		}

		pTextureCompileSettings->SetFileId( m_pRoot->GetFileId(), TD_DEEP, true );
		m_pRoot->SetValue( "textureCompileSettings", pTextureCompileSettings );
	}

	return g_pDataModel->UpdateUnserializedElements( pSourceFormatName, nSourceFormatVersion, fileid, idConflictResolution, ppRoot );
}



