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


//-----------------------------------------------------------------------------
// Serialization class for make sheet files
//-----------------------------------------------------------------------------
class CImportTex : public IDmSerializer
{
public:
	virtual const char *GetName() const { return "tex_source1"; }
	virtual const char *GetDescription() const { return "Valve Texture Configuration File"; }
	virtual bool IsBinaryFormat() const { return false; }
	virtual bool StoresVersionInFile() const { return false; }
	virtual int GetCurrentVersion() const { return 0; } // doesn't store a version
 	virtual const char *GetImportedFormat() const { return "tex"; }
 	virtual int GetImportedVersion() const { return 1; }

	bool Serialize( CUtlBuffer &outBuf, CDmElement *pRoot ) { return false; }
	// Read from the UtlBuffer, return true if successful, and return the read-in root in ppRoot.
	bool Unserialize( CUtlBuffer &buf, const char *pEncodingName, int nEncodingVersion, const char *pSourceFormatName, 
		int nSourceFormatVersion, DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot );

private:
	bool ParseOptionKey( const char *pKeyName,  const char *pKeyValue, CDmElement *pTexture );
	const char *GetPossiblyQuotedWord( const char *pInBuf, char *pOutbuf );
	bool GetKeyValueFromBuffer( CUtlBuffer &buffer, char *key, char *val );
	CDmElement *AddProcessor( CDmElement *pElement, const char *pProcessorType, const char *pName );
	CDmElement *FindProcessor( CDmElement *pElement, const char *pProcessorType, const char *pName );

	// Information for the mipmap processor
	bool m_bNoNice;
	bool m_bAlphatestMipmapping;
	float m_flAlphatestMipmapThreshhold;
	float m_flAlphatestMipmapHiFreqThreshhold;
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CImportTex s_ImportTex;

void InstallTEXImporter( IDataModel *pFactory )
{
	pFactory->AddSerializer( &s_ImportTex );
}


//-----------------------------------------------------------------------------
// Processors
//-----------------------------------------------------------------------------
CDmElement *CImportTex::AddProcessor( CDmElement *pElement, const char *pProcessorType, const char *pName )
{
	CDmrElementArray< CDmElement > processors( pElement, "processors", true );
	CDmElement* pProcessor = CreateElement< CDmElement >( pProcessorType, pName, pElement->GetFileId() );
	processors.AddToTail( pProcessor );
	return pProcessor;
}

CDmElement *CImportTex::FindProcessor( CDmElement *pElement, const char *pProcessorType, const char *pName )
{
	CDmrElementArrayConst< CDmElement > processors( pElement, "processors" );
	for ( int i = 0; i < processors.Count(); ++i )
	{
		if ( !Q_stricmp( pProcessorType, processors[i]->GetTypeString() ) && !Q_stricmp( pName, processors[i]->GetName() ) )
			return processors[i];
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Parses key values out of the txt file
//-----------------------------------------------------------------------------
bool CImportTex::ParseOptionKey( const char *pKeyName, const char *pKeyValue, CDmElement *pTexture )
{
	int iValue = atoi( pKeyValue ); // To properly have "clamps 0" and not enable the clamping

	if( !Q_stricmp( pKeyName, "startframe" ) )
	{
		pTexture->SetValue( "startFrame", iValue );
	}
	else if( !Q_stricmp( pKeyName, "endframe" ) )
	{
		pTexture->SetValue( "endFrame", iValue );
	}
	else if ( !Q_stricmp( pKeyName, "cubemap" ) )
	{
		pTexture->SetValue( "textureType", iValue ? 1 : 0 );
	}
	else if( !Q_stricmp( pKeyName, "volumetexture" ) )
	{
		pTexture->SetValue( "volumeTextureDepth", iValue );

		/*
		// FIXME: Volume textures don't currently support DXT compression
		m_vtfProcOptions.flags0 |= VtfProcessingOptions::OPT_NOCOMPRESS;
		*/
	}
	else if( !Q_stricmp( pKeyName, "clamps" ) )
	{
		pTexture->SetValue( "clamps", iValue ? true : false );
	}
	else if( !Q_stricmp( pKeyName, "clampt" ) )
	{
		pTexture->SetValue( "clampt", iValue ? true : false );
	}
	else if( !Q_stricmp( pKeyName, "clampu" ) )
	{
		pTexture->SetValue( "clampu", iValue ? true : false );
	}
	else if ( !Q_stricmp( pKeyName, "nodebug" ) )
	{
		pTexture->SetValue( "noDebugOverride", iValue ? true : false );
	}
	else if( !Q_stricmp( pKeyName, "pointsample" ) )
	{
		if ( iValue != 0 )
		{
			pTexture->SetValue( "filterType", 4 );
		}
	}
	else if( !Q_stricmp( pKeyName, "trilinear" ) )
	{
		if ( iValue != 0 )
		{
			pTexture->SetValue( "filterType", 2 );
		}
	}
	else if( !Q_stricmp( pKeyName, "anisotropic" ) )
	{
		if ( iValue != 0 )
		{
			pTexture->SetValue( "filterType", 1 );
		}
	}
	else if( !Q_stricmp( pKeyName, "nomip" ) )
	{
		pTexture->SetValue( "noMip", iValue ? true : false );
	}
	else if( !Q_stricmp( pKeyName, "nolod" ) )
	{
		pTexture->SetValue( "noLod", iValue ? true : false );
	}
	else if( !Q_stricmp( pKeyName, "nonice" ) )
	{
		m_bNoNice = iValue ? true : false;
	}
	else if( !Q_stricmp( pKeyName, "alphatest" ) )
	{
		m_bAlphatestMipmapping = iValue ? true : false; 
	}
	else if( !Q_stricmp( pKeyName, "alphatest_threshhold" ) )
	{
		m_flAlphatestMipmapThreshhold = (float)atof( pKeyValue );
	}
	else if( !Q_stricmp( pKeyName, "alphatest_hifreq_threshhold" ) )
	{
		m_flAlphatestMipmapHiFreqThreshhold = (float)atof( pKeyValue );
	}
	else if( !Q_stricmp( pKeyName, "dxt5" ) )
	{
		pTexture->SetValue( "hintDxt5Compression", iValue ? true : false );
	}
	else if( !Q_stricmp( pKeyName, "nocompress" ) )
	{
		pTexture->SetValue( "noCompression", iValue ? true : false );
	}
	else if ( !Q_stricmp( pKeyName, "numchannels" ) )
	{
		CDmElement *pProcessor = AddProcessor( pTexture, "DmeTP_ChangeColorChannels", "changeColorChannels" );
		pProcessor->SetValue( "maxChannels", iValue ); 
	}
	else
	{
		Warning("unrecognized option in text file - %s\n", pKeyName );
	}

	return true;
#if 0
	else if ( !Q_stricmp( pKeyName, "skybox" ) )
	{
		// We're going to treat it like a cubemap until the very end (we have to load and process all cubemap
		// faces at once, so we can match their edges with the texture compression and mipmapping).
		m_bIsSkyBox = iValue ? true : false;
		m_bIsCubeMap = iValue ? true : false;
	}
	else if ( !Q_stricmp( pKeyName, "skyboxcropped" ) )
	{
		m_bIsCroppedSkyBox = iValue ? true : false;
	}
	else if( !Q_stricmp( pKeyName, "bumpscale" ) )
	{
		m_flBumpScale = atof( pKeyValue );
	}
	else if( !Q_stricmp( pKeyName, "border" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_BORDER, iValue );
		// Gets applied to s, t and u   We currently assume black border color
	}
	else if( !Q_stricmp( pKeyName, "normal" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_NORMAL, iValue );

		// Normal maps not supported for manual mip painting
		m_bManualMip = false;
	}
	else if( !Q_stricmp( pKeyName, "normalga" ) )
	{
		m_bNormalToDXT5GA = iValue ? true : false;
		SetFlagValue( m_vtfProcOptions.flags0, VtfProcessingOptions::OPT_NORMAL_GA, iValue );
	}
	else if( !Q_stricmp( pKeyName, "invertgreen" ) )
	{
		m_bNormalInvertGreen = iValue ? true : false;
	}
	else if( !Q_stricmp( pKeyName, "ssbump" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_SSBUMP, iValue );
	}
	else if( !Q_stricmp( pKeyName, "allmips" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_ALL_MIPS, iValue );
	}
	else if( !Q_stricmp( pKeyName, "rendertarget" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_RENDERTARGET, iValue );
	}
	else if( !Q_stricmp( pKeyName, "oneovermiplevelinalpha" ) )
	{
		SetFlagValue( m_vtfProcOptions.flags0, VtfProcessingOptions::OPT_SET_ALPHA_ONEOVERMIP, iValue );
	}
	else if( !Q_stricmp( pKeyName, "premultcolorbyoneovermiplevel" ) )
	{
		SetFlagValue( m_vtfProcOptions.flags0, VtfProcessingOptions::OPT_PREMULT_COLOR_ONEOVERMIP, iValue );
	}
	else if ( !Q_stricmp( pKeyName, "normaltodudv" ) )
	{
		m_bNormalToDuDv = iValue ? true : false;
		SetFlagValue( m_vtfProcOptions.flags0, VtfProcessingOptions::OPT_NORMAL_DUDV, iValue );
	}
	else if ( !Q_stricmp( pKeyName, "stripalphachannel" ) )
	{
		m_bStripAlphaChannel = iValue ? true : false;
	}
	else if ( !Q_stricmp( pKeyName, "stripcolorchannel" ) )
	{
		m_bStripColorChannel = iValue ? true : false;
	}
	else if ( !Q_stricmp( pKeyName, "normalalphatodudvluminance" ) )
	{
		m_bAlphaToLuminance = iValue ? true : false;
	}
	else if ( !Q_stricmp( pKeyName, "dudv" ) )
	{
		m_bDuDv = iValue ? true : false;
	}
	else if( !Q_stricmp( pKeyName, "reduce" ) )
	{
		m_nReduceX = atoi(pKeyValue);
		m_nReduceY = m_nReduceX;
	}
	else if( !Q_stricmp( pKeyName, "reducex" ) )
	{
		m_nReduceX = atoi(pKeyValue);
	}
	else if( !Q_stricmp( pKeyName, "reducey" ) )
	{
		m_nReduceY = atoi(pKeyValue);
	}
	else if( !Q_stricmp( pKeyName, "maxwidth" ) )
	{
		m_nMaxDimensionX = atoi(pKeyValue);
	}
	else if( !Q_stricmp( pKeyName, "maxwidth_360" ) )
	{
		m_nMaxDimensionX_360 = atoi(pKeyValue);
	}
	else if( !Q_stricmp( pKeyName, "maxheight" ) )
	{
		m_nMaxDimensionY = atoi(pKeyValue);
	}
	else if( !Q_stricmp( pKeyName, "maxheight_360" ) )
	{
		m_nMaxDimensionY_360 = atoi(pKeyValue);
	}
	else if( !Q_stricmp( pKeyName, "alphatodistance" ) )
	{
		m_bAlphaToDistance = iValue ? true : false;
	}
	else if( !Q_stricmp( pKeyName, "distancespread" ) )
	{
		m_flDistanceSpread = atof(pKeyValue);
	}
	else if( !Q_stricmp( pKeyName, "pfmscale" ) )
	{
		m_pfmscale=atof(pKeyValue);
		VTexMsg( "pfmscale = %.2f\n", m_pfmscale );
	}
	else if ( !Q_stricmp( pKeyName, "pfm" ) )
	{
		if ( iValue )
			g_eMode = BITMAP_FILE_TYPE_PFM;
	}
	else if ( !Q_stricmp( pKeyName, "specvar" ) )
	{
		int iDecayChannel = -1;

		if ( !Q_stricmp( pKeyValue, "red" ) || !Q_stricmp( pKeyValue, "r" ) )
			iDecayChannel = 0;
		if ( !Q_stricmp( pKeyValue, "green" ) || !Q_stricmp( pKeyValue, "g" ) )
			iDecayChannel = 1;
		if ( !Q_stricmp( pKeyValue, "blue" ) || !Q_stricmp( pKeyValue, "b" ) )
			iDecayChannel = 2;
		if ( !Q_stricmp( pKeyValue, "alpha" ) || !Q_stricmp( pKeyValue, "a" ) )
			iDecayChannel = 3;

		if ( iDecayChannel >= 0 && iDecayChannel < 4 )
		{
			m_vtfProcOptions.flags0 |= ( VtfProcessingOptions::OPT_DECAY_R | VtfProcessingOptions::OPT_DECAY_EXP_R ) << iDecayChannel;
			m_vtfProcOptions.numNotDecayMips[iDecayChannel] = 0;
			m_vtfProcOptions.clrDecayGoal[iDecayChannel] = 0;
			m_vtfProcOptions.fDecayExponentBase[iDecayChannel] = 0.75;
			m_bManualMip = false;
			SetFlagValue( m_nFlags, TEXTUREFLAGS_ALL_MIPS, 1 );
		}
	}
	else if ( Q_stricmp( pKeyName, "manualmip" ) == 0 )
	{
		if ( ( m_nVolumeTextureDepth == 1 ) && !( m_nFlags & ( TEXTUREFLAGS_NORMAL | TEXTUREFLAGS_NOMIP ) ) )
		{
			m_bManualMip = true;
		}
	}
	else if ( !Q_stricmp( pKeyName, "mipblend" ) )
	{
		SetFlagValue( m_nFlags, TEXTUREFLAGS_ALL_MIPS, 1 );

		// Possible values
		if ( !Q_stricmp( pKeyValue, "detail" ) ) // Skip 2 mips and fade to gray -> (128, 128, 128, -)
		{
			for( int ch = 0; ch < 3; ++ ch )
			{
				m_vtfProcOptions.flags0 |= VtfProcessingOptions::OPT_DECAY_R << ch;
				// m_vtfProcOptions.flags0 &= ~(VtfProcessingOptions::OPT_DECAY_EXP_R << ch);
				m_vtfProcOptions.numNotDecayMips[ch] = 2;
				m_vtfProcOptions.clrDecayGoal[ch] = 128;
			}
		}
		/*
		else if ( !Q_stricmp( pKeyValue, "additive" ) ) // Skip 2 mips and fade to black -> (0, 0, 0, -)
		{
		for( int ch = 0; ch < 3; ++ ch )
		{
		m_vtfProcOptions.flags0 |= VtfProcessingOptions::OPT_DECAY_R << ch;
		m_vtfProcOptions.flags0 &= ~(VtfProcessingOptions::OPT_DECAY_EXP_R << ch);
		m_vtfProcOptions.numDecayMips[ch] = 2;
		m_vtfProcOptions.clrDecayGoal[ch] = 0;
		}
		}
		else if ( !Q_stricmp( pKeyValue, "alphablended" ) ) // Skip 2 mips and fade out alpha to 0
		{
		for( int ch = 3; ch < 4; ++ ch )
		{
		m_vtfProcOptions.flags0 |= VtfProcessingOptions::OPT_DECAY_R << ch;
		m_vtfProcOptions.flags0 &= ~(VtfProcessingOptions::OPT_DECAY_EXP_R << ch);
		m_vtfProcOptions.numDecayMips[ch] = 2;
		m_vtfProcOptions.clrDecayGoal[ch] = 0;
		}
		}
		*/
		else
		{
			// Parse the given value:
			// skip=3:r=255:g=255:b=255:a=255  - linear decay
			// r=0e.75 - exponential decay targeting 0 with exponent base 0.75

			int nSteps = 0; // default

			for ( char const *szParse = pKeyValue; szParse; szParse = strchr( szParse, ':' ), szParse ? ++ szParse : 0 )
			{
				if ( char const *sz = StringAfterPrefix( szParse, "skip=" ) )
				{
					szParse = sz;
					nSteps = atoi(sz);
				}
				else if ( StringHasPrefix( szParse, "r=" ) ||
					StringHasPrefix( szParse, "g=" ) ||
					StringHasPrefix( szParse, "b=" ) ||
					StringHasPrefix( szParse, "a=" ) )
				{
					int ch = 0;
					switch ( *szParse )
					{
					case 'g': case 'G': ch = 1; break;
					case 'b': case 'B': ch = 2; break;
					case 'a': case 'A': ch = 3; break;
					}

					szParse += 2;
					m_vtfProcOptions.flags0 |= VtfProcessingOptions::OPT_DECAY_R << ch;
					m_vtfProcOptions.flags0 &= ~(VtfProcessingOptions::OPT_DECAY_EXP_R << ch);
					m_vtfProcOptions.numNotDecayMips[ch] = nSteps;
					m_vtfProcOptions.clrDecayGoal[ch] = atoi( szParse );

					while ( isdigit( *szParse ) )
						++ szParse;

					// Exponential decay
					if ( ( *szParse == 'e' || *szParse == 'E' ) && ( szParse[1] == '.' ) )
					{
						m_vtfProcOptions.flags0 |= VtfProcessingOptions::OPT_DECAY_EXP_R << ch;
						m_vtfProcOptions.fDecayExponentBase[ch] = ( float ) atof( szParse + 1 );
					}
				}
				else
				{
					Warning( "invalid mipblend setting \"%s\"\n", pKeyValue );
				}
			}
		}
	}
	else if( !Q_stricmp( pKeyName, "srgb" ) )
	{
		// Do nothing for now...this will be removed shortly
	}
#endif
}


const char *CImportTex::GetPossiblyQuotedWord( const char *pInBuf, char *pOutbuf )
{
	pInBuf += strspn( pInBuf, " \t" );						// skip whitespace

	const char *pWordEnd;
	bool bQuote = false;
	if (pInBuf[0]=='"')
	{
		pInBuf++;
		pWordEnd=strchr(pInBuf,'"');
		bQuote = true;
	}
	else
	{
		pWordEnd=strchr(pInBuf,' ');
		if (! pWordEnd )
			pWordEnd = strchr(pInBuf,'\t' );
		if (! pWordEnd )
			pWordEnd = pInBuf+strlen(pInBuf);
	}
	if ((! pWordEnd ) || (pWordEnd == pInBuf ) )
		return NULL;										// no word found
	memcpy( pOutbuf, pInBuf, pWordEnd-pInBuf );
	pOutbuf[pWordEnd-pInBuf]=0;

	pInBuf = pWordEnd;
	if ( bQuote )
		pInBuf++;
	return pInBuf;
}

// GetKeyValueFromBuffer:
//		fills in "key" and "val" respectively and returns "true" if succeeds.
//		returns false if:
//			a) end-of-buffer is reached (then "val" is empty)
//			b) error occurs (then "val" is the error message)
//
bool CImportTex::GetKeyValueFromBuffer( CUtlBuffer &buffer, char *key, char *val )
{
	char buf[2048];

	while( buffer.GetBytesRemaining() )
	{
		buffer.GetLine( buf, sizeof( buf ) );

		// Scanning algorithm
		char *pComment = strpbrk( buf, "#\n\r" );
		if ( pComment )
			*pComment = 0;

		pComment = strstr( buf, "//" );
		if ( pComment)
			*pComment = 0;

		const char *scan = buf;
		scan=GetPossiblyQuotedWord( scan, key );
		if ( scan )
		{
			scan=GetPossiblyQuotedWord( scan, val );
			if ( scan )
				return true;
			else
			{
				sprintf( val, "parameter %s has no value", key );
				return false;
			}
		}
	}

	val[0] = 0;
	return false;
}


//-----------------------------------------------------------------------------
// Main entry point for the unserialization
//-----------------------------------------------------------------------------
bool CImportTex::Unserialize( CUtlBuffer &buf, const char *pEncodingName, int nEncodingVersion, const char *pSourceFormatName, 
	int nSourceFormatVersion, DmFileId_t fileid, DmConflictResolution_t idConflictResolution, CDmElement **ppRoot )
{
	*ppRoot = NULL;

	m_bNoNice = m_bAlphatestMipmapping = false;
	m_flAlphatestMipmapThreshhold = m_flAlphatestMipmapHiFreqThreshhold = 0.0f;

	CDmElement *pTexture = CreateElement< CDmElement >( "DmePrecompiledTexture", "root", DMFILEID_INVALID );
	pTexture->SetValue( "imageFileName", "__unspecified_texture" );

	char pKey[2048];
	char pVal[2048];
	while( GetKeyValueFromBuffer( buf, pKey, pVal ) )
	{
		ParseOptionKey( pKey, pVal, pTexture );
	}

	if ( pVal[0] )
	{
		Warning( "Error importing txt file! %s\n", pVal );
		return false;
	}

	// Prefer to do mipmapping as one of the final processors
	if ( !pTexture->GetValue< bool >( "noMip" ) || !pTexture->GetValue< bool >( "noLod" ) )
	{
		CDmElement *pProcessor = AddProcessor( pTexture, "DmeTP_ComputeMipmaps", "computeMipmaps" );
		pProcessor->SetValue( "noNiceFiltering", m_bNoNice ); 
		pProcessor->SetValue( "alphaTestDownsampling", m_bAlphatestMipmapping );
		pProcessor->SetValue( "alphaTestDownsampleThreshhold", m_flAlphatestMipmapThreshhold );
		pProcessor->SetValue( "alphaTestDownsampleHiFreqThreshhold" , m_flAlphatestMipmapHiFreqThreshhold );
	}

	*ppRoot = pTexture;
	bool bOk = g_pDataModel->UpdateUnserializedElements( pSourceFormatName, nSourceFormatVersion, fileid, idConflictResolution, ppRoot );
	if ( !bOk )
	{
		*ppRoot = NULL;
	}
	return bOk; 
}



