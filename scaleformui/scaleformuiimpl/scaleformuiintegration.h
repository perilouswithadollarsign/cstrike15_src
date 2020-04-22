//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//


#if !defined( __SCALEFORMUIINTEGRATION_H__ )
#define __SCALEFORMUIINTEGRATION_H__


/****************************
 * This is the wrapper around the valve memory manager
 */

class CScaleformSysAlloc: public SF::SysAlloc
{
public:
	virtual void* Alloc( SF::UPInt size, SF::UPInt align );
	virtual void Free( void* ptr, SF::UPInt size, SF::UPInt align );
	virtual void* Realloc( void* oldPtr, SF::UPInt oldSize, SF::UPInt newSize, SF::UPInt align );
};


/*****************************************************
 * This redirects the scaleform logging calls to CSTrike
 */

class ScaleformUILogging: public SF::Log
{
public:
	virtual void LogMessageVarg( SF::LogMessageId messageId, const char* pfmt, va_list argList );
};


/******************************************************
 * gives scaleform access tot he clipboard
 */

class ScaleformClipboard: public SF::GFx::TextClipboard
{
public:
    virtual void OnTextStore( const wchar_t* ptext, SF::UPInt len );
};



/************************
 * wraps the scaleform translation functions
 */

class ScaleformTranslatorAdapter: public SF::GFx::Translator
{

public:
	virtual unsigned GetCaps( void ) const;
	virtual void Translate( TranslateInfo* tinfo );

};

/********************
 * used by CreateAPI.  It attaches the movieview to the GFxValue of the api
 */

class ScaleformMovieUserData: public SF::GFx::ASUserData
{
public:
	// this is a weak link
	SF::GFx::Movie* m_pMovie;

	virtual void OnDestroy( SF::GFx::Movie* pmovie, void* pobject );

};

/*****************************
 * serves as a thunk between the scaleform code and the game code
 */

class ScaleformFunctionHandlerAdapter: public SF::GFx::FunctionHandler
{
public:
	virtual void Call( const Params& params );
};



/********************************
 * this lets scaleform use the valve file location stuff
 */

class ScaleformFileOpener : public SF::GFx::FileOpenerBase
{
public:

    // Override to opens a file using user-defined function and/or GFile class.
    // The default implementation uses buffer-wrapped GSysFile, but only
    // if GFC_USE_SYSFILE is defined.
    // The 'purl' should be encoded as UTF-8 to support international file names.
    virtual SF::File* OpenFile(const char* purl,
        int flags = SF::FileConstants::Open_Read|SF::FileConstants::Open_Buffered,
        int mode = SF::FileConstants::Mode_ReadWrite);

    // Returns last modified date/time required for file change detection.
    // Can be implemented to return 0 if no change detection is desired.
    // Default implementation checks file time if GFC_USE_SYSFILE is defined.
    // The 'purl' should be encoded as UTF-8 to support international file names.
    virtual SF::SInt64 GetFileModifyTime(const char* purl);

    // Open file with customizable log, by relying on OpenFile.
    // If not null, log will receive error messages on failure.
    // The 'purl' should be encoded as UTF-8 to support international file names.
    virtual SF::File* OpenFileEx(const char* purl, SF::GFx::Log *plog,
		int flags = SF::FileConstants::Open_Read|SF::FileConstants::Open_Buffered,
        int mode = SF::FileConstants::Mode_ReadWrite);
};


/********************************
 * this lets scaleform use our gamer icons and any other dynamic textures
 */

class CScaleformImageCreator : public SF::GFx::ImageCreator
{
public:

	CScaleformImageCreator( IScaleformUI *pSFUI, SF::GFx::TextureManager* textureManager = 0);

	// Looks up image for "img://" protocol.
	virtual SF::GFx::Image* LoadProtocolImage(const SF::GFx::ImageCreateInfo& info, const SF::String& url);

private:
	IScaleformUI* m_pScaleformUI;
};



#endif
