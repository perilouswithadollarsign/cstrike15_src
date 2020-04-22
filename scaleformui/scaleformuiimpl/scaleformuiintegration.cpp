//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//


#include "stdafx.h"
#include "vgui/ILocalize.h"
#include "vgui/ISystem.h"
#include "sfuimemoryfile.h"
#include <vstdlib/vstrtools.h>
#if defined( _PS3 )
#include "ps3/ps3_win32stubs.h"
#endif
#include "../../game/shared/econ/econ_item_view_helpers.h"

// NOTE: This must be the last file included!!!
#include "tier0/memdbgon.h"

using namespace SF::GFx;

ConVar dev_scaleform_debug( "dev_scaleform_debug", "0", FCVAR_DEVELOPMENTONLY );

/*************************************
 * memory allocation wrapper
 */

void* CScaleformSysAlloc::Alloc( SF::UPInt size, SF::UPInt align )
{
	return MemAlloc_AllocAlignedUnattributed( size, align );
}

void CScaleformSysAlloc::Free( void* ptr, SF::UPInt size, SF::UPInt align )
{
	MemAlloc_FreeAligned( ptr );
}

void* CScaleformSysAlloc::Realloc( void* oldPtr, SF::UPInt oldSize, SF::UPInt newSize, SF::UPInt align )
{
	return MemAlloc_ReallocAligned( oldPtr, newSize, align );
}

/*****************************************************
 * This redirects the scaleform logging calls to CSTrike
 */
DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_SCALEFORM, "Scaleform" );
DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_SCALEFORM_SCRIPT, "ScaleformScript" );
DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_SCALEFORM_PARSE, "ScaleformParse" );
DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_SCALEFORM_AS, "ScaleformAS" );

#define SCALEFORM_LOG_COLOR (::Color(180,180,255, 255))

void ScaleformUILogging::LogMessageVarg( SF::LogMessageId messageId, const char* pfmt, va_list argList )
{
	if ( !dev_scaleform_debug.GetBool() )
		return;

	const char *pPrefix = "SF: ";
	LoggingChannelID_t logChannel = LOG_SCALEFORM;
	switch ( messageId & SF::LogChannel_Mask )
	{
	case SF::LogChannel_Debug:
		pPrefix = "SF (Debug): ";
		break;
	case SF::LogChannel_Render:
		pPrefix = "SF (Render): ";
		break;
	case SF::LogChannel_Script:
		logChannel = LOG_SCALEFORM_SCRIPT;
		pPrefix = "SF (Script): ";
		break;
	case SF::LogChannel_Parse:
		logChannel = LOG_SCALEFORM_PARSE;
		pPrefix = "SF (Parse): ";
		break;
	case SF::LogChannel_Action:
		logChannel = LOG_SCALEFORM_AS;
		pPrefix = "SF (Action): ";
		break;
	}

	LoggingSeverity_t logSeverity;

	switch ( messageId.GetMessageType() )
	{
	case SF::LogMessage_Error:
		logSeverity = LS_WARNING;
		break;
	case SF::LogMessage_Warning:
		logSeverity = LS_WARNING;
		break;
	case SF::LogMessage_Text:
		logSeverity = LS_MESSAGE;
		break;
	default:
		logSeverity = LS_MESSAGE;
		break;
	}

	if ( LoggingSystem_IsChannelEnabled( logChannel, logSeverity ) )
	{
		tchar formattedMessage[MAX_LOGGING_MESSAGE_LENGTH];

		Tier0Internal_vsntprintf( formattedMessage, sizeof( formattedMessage )-1, pfmt, argList );
		formattedMessage[sizeof( formattedMessage ) - 1] = 0;

		// optional categorizing prefix
		if ( pPrefix )
		{
			LoggingSystem_LogDirect( logChannel, logSeverity, SCALEFORM_LOG_COLOR, pPrefix );
		}

		LoggingSystem_LogDirect( logChannel, logSeverity, SCALEFORM_LOG_COLOR, formattedMessage );

		// scaleform messages randomly lack terminal \n, add to prevent undesired joined spew
		int len = _tcslen( formattedMessage );
		if ( len > 0 && formattedMessage[len-1] != '\n' )
		{
			LoggingSystem_LogDirect( logChannel, logSeverity, SCALEFORM_LOG_COLOR, "\n" );
		}
	}
}


/****************************************************************
 * contains the adapter methods for clipboard
 */


void ScaleformClipboard::OnTextStore( const wchar_t* ptext, SF::UPInt len )
{
	if ( ptext && len )
	{
		g_pVGuiSystem->SetClipboardText( ptext, len );
	}
}


/****************************************************************
 * contains the adapter methods for translations
 */

unsigned int ScaleformTranslatorAdapter::GetCaps( void ) const
{
	return Cap_StripTrailingNewLines;
}


void ScaleformTranslatorAdapter::Translate( TranslateInfo* tinfo )
{
	const wchar_t* pkey = tinfo->GetKey();

	if ( pkey && ( *pkey == L'#' ) )
	{
		int len = Q_wcslen( pkey );

		char *asciiString = ( char * ) stackalloc( len + 1 );

		V_UnicodeToUTF8( pkey, asciiString, len + 1 );

		bool isHTML = false;

		const wchar_t* translated = SFINST.Translate( asciiString, &isHTML );

		tinfo->SetResultHtml( translated );
	}
}

/********************
 * used by CreateAPI.  It attaches the movieview to the GFxValue of the api
 */

void ScaleformMovieUserData::OnDestroy( Movie* pmovie, void* pobject )
{
	m_pMovie = NULL;
	Release();
}

/*******************************
 * this defines the actual m_Callback function for the function handler class
 */

void ScaleformFunctionHandlerAdapter::Call( const Params& params )
{
	ScaleformCallbackHolder* pCallback = ( ScaleformCallbackHolder* ) params.pUserData;

	if ( pCallback )
	{
		pCallback->Execute(const_cast<Params*>(&params ));
	}
}

void ScaleformCallbackHolder::OnDestroy( Movie* pmovie, void* pobject )
{
	Release();
}

/********************************
 * this lets scaleform use the valve file location stuff
 */

SF::File* ScaleformFileOpener::OpenFile( const char *purl, int flags, int modes )
{
	MEM_ALLOC_CREDIT();

	return OpenFileEx( purl, NULL, flags, modes );
}

SF::SInt64 ScaleformFileOpener::GetFileModifyTime( const char *purl )
{
	SF::SInt64 result = g_pFullFileSystem->GetFileTime( purl, "GAME" );
	return !result ? -1 : 0;
}

// Implementation that allows us to override the log.
SF::File* ScaleformFileOpener::OpenFileEx( const char *pfilename, Log *plog, int flags, int modes )
{
	MEM_ALLOC_CREDIT();

	if ( ( flags & ~SF::FileConstants::Open_Buffered ) != SF::FileConstants::Open_Read )
	{
		if ( plog )
		{
			plog->LogError( "Error: GFxLoader cannot open '%s' for writing. writing is not supported\n", pfilename );
		}

		return NULL;
	}

	SFUIMemoryFile* pin = new SFUIMemoryFile( pfilename );

	const char* realName = SFINST.CorrectFlashFileName( pfilename );

	extern IScaleformSlotInitController *g_pExternalScaleformSlotInitController;

	// is this an image stored in the stringtables?
	if ( char const *szExternalImg = StringAfterPrefix( realName, "img://stringtables:" ) )
	{
		// skip width and height attributes if they exist "(64x128):"
		const char * szExternalImgPostSizeParm = strstr( szExternalImg, "):" );
		if ( szExternalImgPostSizeParm != NULL )
		{
			szExternalImg = szExternalImgPostSizeParm + 2;
		}

		int length = 0;
		const void * pImageData = NULL;

		if ( g_pExternalScaleformSlotInitController )
		{
			pImageData = g_pExternalScaleformSlotInitController->GetStringUserData( "InfoPanel", szExternalImg, &length );

			if ( pImageData )
			{
				pin->GetBuffer().SetExternalBuffer( (void *)pImageData, length, 0, pin->GetBuffer().READ_ONLY );
				pin->Init();
			}
			else
			{
				pin->Release();
				pin = NULL;
			}
		}
	}
	else if ( g_pFullFileSystem->ReadFile( realName, "GAME", pin->GetBuffer() ) )
	{
		if ( g_pExternalScaleformSlotInitController )
			g_pExternalScaleformSlotInitController->OnFileLoadedByScaleform( realName, pin->GetBuffer().Base(), pin->GetBuffer().TellPut() );

		pin->Init();
	}
	else
	{
		if ( g_pExternalScaleformSlotInitController )
			g_pExternalScaleformSlotInitController->OnFileLoadedByScaleform( realName, NULL, 0 );

		pin->Release();
		pin = NULL;

		if ( plog )
		{
			plog->LogError( "Error: GFxLoader failed to open '%s'\n", realName );
		}
	}

	return pin;

}

/********************************
 * this lets scaleform use our gamer icons and any other dynamic textures
 */

CScaleformImageCreator::CScaleformImageCreator( IScaleformUI *pSFUI, TextureManager* textureManager /* = 0 */)
	: ImageCreator( textureManager ), m_pScaleformUI( pSFUI )
{
}

Image*  CScaleformImageCreator::LoadProtocolImage(const ImageCreateInfo& info, const SF::String& url)
{
	MEM_ALLOC_CREDIT();

	// We use this to handle loadMovie calls from action script that
	// we can use to load player avatar icons, inventory item images, Chrome HTML images, or extern files on disk.
	// The url coming in should be something like this: img://<type>_<data>
	//

	// This is for loading external image sitting on disk
	// Syntax1: img://loadfile:mylocalfile.jpg - loads the file, can be JPG, PNG, PGA, or DDS (must be uncompressed)
	// Syntax2: img://loadfile:(64x64):mylocalfile.jpg - loads the file, returns a transparent texture of given size if doesn't exist
	//
	char const *szExternalImg = StringAfterPrefix( url, "img://loadfile:" );
	// allow loadjpeg until flash is updated to use loadfile
	if ( !szExternalImg )
	{
		szExternalImg = StringAfterPrefix( url, "img://loadjpeg:" );
	}
	if ( szExternalImg )
	{
		// Parse width and height attributes
		uint width = 0, height = 0;
		if ( szExternalImg[0] == '(' )
		{
			width = Q_atoi( szExternalImg + 1 );
			szExternalImg = strchr( szExternalImg, 'x' );
			if ( !szExternalImg )
				return NULL;
			height = Q_atoi( szExternalImg + 1 );
			szExternalImg = strstr( szExternalImg, "):" );
			if ( !szExternalImg )
				return NULL;
			szExternalImg += 2;
		}

		char chLocalPath[ 2*MAX_PATH + 1 ] = {};
		const char *pchFullImgPath = szExternalImg[0] ? g_pFullFileSystem->RelativePathToFullPath( szExternalImg, "GAME", chLocalPath, Q_ARRAYSIZE( chLocalPath ) - 1 ) : NULL;

		if ( pchFullImgPath )
		{
			Image *pImage = ( ( ScaleformUIImpl* )m_pScaleformUI )->CreateImageFromFile( pchFullImgPath, info, width, height );
			return pImage;
		}
	}
	// "img://avatar_[xuid]" where [xuid] is the xuid of the player whose avatar we want to load.
	else if ( char const *szAvatarXuid = StringAfterPrefix( url, "img://avatar_" ) )
	{
		int64 xuid = Q_atoi64( szAvatarXuid );
		ScaleformUIAvatarImage* pAvatarImage = ( ( ScaleformUIImpl* )m_pScaleformUI )->GetAvatarImage( xuid );
		if ( pAvatarImage )
		{
			return pAvatarImage->GetImage();
		}
	}
	// "img://inventory_[itemid]" where [itemid] is the item's id from GetItemIDbyIndex() in the inventory component.
	else if ( char const *szInventoryItemId = StringAfterPrefix( url, "img://inventory_" ) )
	{
		uint64 itemid = Q_atoi64( szInventoryItemId );

		// look up image here using xuid and itemid and return it
		ScaleformUIInventoryImage* pInventoryImage = ( ( ScaleformUIImpl* )m_pScaleformUI )->GetInventoryImage( itemid );
		if ( pInventoryImage )
		{
			return pInventoryImage->GetImage();
		}
	}
	// "img://itemdata_[defindex]_[paintindex]" where [defindex] & [paintindex] are econ item definition and paint indices.
	else if ( StringHasPrefix( url, "img://itemdata_" ) )
	{
		uint16 iDefIndex = 0;
		uint16 iPaintIndex = 0;
		{
			CUtlVector< char* > urlFragments;
			V_SplitString( url, "_", urlFragments );
			iDefIndex = ( uint16 ) atoi( urlFragments[1] );
			iPaintIndex = ( uint16 ) atoi( urlFragments[2] );
			urlFragments.PurgeAndDeleteElements();
		}
		uint64 ullItemId = CombinedItemIdMakeFromDefIndexAndPaint( iDefIndex, iPaintIndex );

		// look up image here using defindex and paintindex and return it
		ScaleformUIInventoryImage* pInventoryImage = ( ( ScaleformUIImpl* )m_pScaleformUI )->GetInventoryImage( ullItemId );
		if ( pInventoryImage )
		{
			return pInventoryImage->GetImage();
		}
	}
	else if ( char const *szBilinearChromeImg = StringAfterPrefix( url, "img://chrome_" ) )			// using bilinear filtering
	{
		int64 imageid = Q_atoi64( szBilinearChromeImg );

		// look up image here using xuid and itemid and return it
		ScaleformUIChromeHTMLImage* pChromeImage = ( ( ScaleformUIImpl* )m_pScaleformUI )->GetChromeHTMLImage( imageid );
		if ( pChromeImage )
		{
			return pChromeImage->GetImage();
		}
	}
	else if ( char const *szPointSampleChromeImg = StringAfterPrefix( url, "imgps://chrome_" ) )		// point sampling filtering
	{
		int64 imageid = Q_atoi64( szPointSampleChromeImg );

		// look up image here using xuid and itemid and return it
		ScaleformUIChromeHTMLImage* pChromeImage = ( ( ScaleformUIImpl* )m_pScaleformUI )->GetChromeHTMLImage( imageid );
		if ( pChromeImage )
		{
			return pChromeImage->GetImage();
		}
	}
	else if ( char const *szExternalImg = StringAfterPrefix( url, "img://stringtables:" ) )
	{
		// Parse width and height attributes
		uint width = 0, height = 0;
		if ( szExternalImg[ 0 ] == '(' )
		{
			width = Q_atoi( szExternalImg + 1 );
			szExternalImg = strchr( szExternalImg, 'x' );
			if ( !szExternalImg )
				return NULL;
			height = Q_atoi( szExternalImg + 1 );
			szExternalImg = strstr( szExternalImg, "):" );
			if ( !szExternalImg )
				return NULL;
			szExternalImg += 2;
		}

		if ( !szExternalImg || !szExternalImg[ 0 ] )
			return NULL;

		// we're going to pass in the whole url because the prefix will signal to the file loader to get the data from stringtables rather than from the filesystem
		Image *pImage = ( ( ScaleformUIImpl* )m_pScaleformUI )->CreateImageFromFile( url, info, width, height );

		if ( pImage )
			return pImage;
	}

	return NULL;
}
