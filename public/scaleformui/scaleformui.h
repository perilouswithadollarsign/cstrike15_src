//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#if !defined( __SCALEFORMUI_H__ )
#define __SCALEFORMUI_H__

// this is needed so that pointers to members match with those in CStrike15
#pragma pointers_to_members( full_generality, virtual_inheritance )

#include "appframework/iappsystem.h"
#include "inputsystem/InputEnums.h"
#include "inputsystem/ButtonCode.h"
#include "refcount.h"
#include "shaderapi/IShaderDevice.h"

#ifdef CLIENT_DLL
	#include "cs_workshop_manager.h"
#endif

#if ( defined( _OSX ) || defined ( _LINUX ) ) && ( defined( _DEBUG ) || defined( USE_MEM_DEBUG ) )
#include <typeinfo>
#endif

#ifndef NO_STEAM
#include "steam/steam_api.h"
extern CSteamAPIContext *steamapicontext;
#endif

#define USE_DEFAULT_INVENTORY_ICON_BACKGROUNDS

enum
{
SF_RESERVED_CURSOR_SLOT,
SF_RESERVED_BEGINFRAME_SLOT	= 0xFFFFFFFE,				// These two reserved slots are used with SF4 to put a single BeginFrame/EndFrame call pair into the render context
SF_RESERVED_ENDFRAME_SLOT	= 0xFFFFFFFF
};

// SF4 TODO
// Using a set of void * for now because we need to keep both SF4 and SF3 working
//namespace Scaleform
//{
//	namespace GFx
//	{
//		class Value;
//		class MovieDef;
//		class Movie;
//	}
//}
//
//typedef SFVALUE SFVALUE;
// typedef Scaleform::GFx::MovieDef* SFMOVIEDEF;
//typedef Scaleform::GFx::Movie* SFMOVIE;
/* SFPARAMS
*
* SF4 TODO
*	This should actually be declared as
*		typedef Scaleform::GFx::FunctionHandler::Params* SFPARAMS
*	Unfortunately this is not possible without including SF SDK header files, because Params
*	is internal to FunctionHandler which is a class and not a namespace.
*
*	SFPARAMS is passed by SF to callbacks in game code, which extract info from it by passing it 
*	on to ScaleformUI helper functions.
*
*	Declaring it as a void* is nasty but seems the only way out, without dragging in SF SDK into
*	other projects such as Client
*/
struct SFPARAMS_opaque_tag;
typedef SFPARAMS_opaque_tag *SFPARAMS;

struct SFMOVIE_opaque_tag;
typedef SFMOVIE_opaque_tag * SFMOVIE;
struct SFMOVIEDEF_opaque_tag;
typedef SFMOVIEDEF_opaque_tag* SFMOVIEDEF;
struct SFVALUE_opaque_tag;
typedef SFVALUE_opaque_tag* SFVALUE;

/*
	This class is a wrapper for a tuple of SFVALUEs (actually Scaleform::GFx::Value objects) that is use for passing parameters to
	flash functions. It exists primarily to do bounds checking for indices and verify count parity when calling Invoke.
*/
class SFVALUEARRAY
{
public:
	SFVALUEARRAY() :
		m_count(0),
		m_pValues(NULL)
	  {}

	SFVALUEARRAY( int count, SFVALUE pValues ) :
		m_count(count),
		m_pValues(pValues)
	{}
	~SFVALUEARRAY() {}

	SFVALUE GetValues() const { return m_pValues; }
	int Count() const { return m_count; }
	
	SFVALUE operator[] (int index );	// only usable from within the materialsystem.dll

	void SetValues( int count, SFVALUE pValues )
	{
		m_count = count;
		m_pValues = pValues;
	}

private:
	SFVALUE	m_pValues;
	int			m_count;
};

enum
{
	SF_FIRST_UNRESERVED_SLOT = 1, SF_FULL_SCREEN_SLOT = SF_FIRST_UNRESERVED_SLOT, SF_FIRST_SS_SLOT, SF_SLOT_IDS_COUNT
};

#define SF_SS_SLOT( x ) ( SF_FIRST_SS_SLOT + ( x ) )

#define SF_ENGINE_UI_SLOT_MASK 3
#define SF_GAME_UI_SLOT_MASK ( ~3 )

// Custom virtual keys the scaleform implementation uses for the right thumbstick.
// We're using the values below for these VK's because according to WinUser.h ( where the VKs are defined )
// the range 0x88 - 0x8F is unassigned.
#define VK_XSTICK2_UP		0x88
#define VK_XSTICK2_RIGHT	0x89
#define VK_XSTICK2_DOWN		0x8A
#define VK_XSTICK2_LEFT		0x8B
#define VK_MWHEEL_UP		0x8C
#define VK_MWHEEL_DOWN		0x8D

/* REI: This is a pretty unsafe function, and doesn't seem to be used.  Commenting out for now.
template<class C, class T>
inline C MakeSFHandle( T* ptr )
{
	return ( C ) ptr;
}
*/

class IScaleformUI;
class IUIMarshalHelper;

/******************************************************
 * callback handling machinery
 */

class ScaleformUIFunctionHandlerObject
{
};

#define SCALEFORM_CALLBACK_ARGS_DECL IUIMarshalHelper* pui, SFPARAMS obj
typedef void ( ScaleformUIFunctionHandlerObject::*ScaleformUIFunctionHandler )( SCALEFORM_CALLBACK_ARGS_DECL );

struct ScaleformUIFunctionHandlerDefinition
{
	const char *m_pName;
	union{
		// Doing this lame union to force the compiler to properly align the
		// member function pointer to at least 16-bytes for warning control.
		ScaleformUIFunctionHandler m_pHandler;
		char						__dont_use_me_pad[16];
	};
};

class IScaleformUIFunctionHandlerDefinitionTable
{
public:
	virtual const ScaleformUIFunctionHandlerDefinition* GetTable( void ) const = 0;
};

/***************************************************************************
 * The following should be used as follows:
 * define some methods in a class that you want to serve as function callbacks, and use
 * the method signature shown here:

 class GFxTutorial
 {
 public:
 // GameAPI stuff
 void ToggleFullScreen( SCALEFORM_CALLBACK_ARGS_DECL );
 void SetNewBackground( SCALEFORM_CALLBACK_ARGS_DECL );
 void NewLight( SCALEFORM_CALLBACK_ARGS_DECL );
 void NewScene( SCALEFORM_CALLBACK_ARGS_DECL );
 void SetLuminance( SCALEFORM_CALLBACK_ARGS_DECL );
 void SFGrabFocus( SCALEFORM_CALLBACK_ARGS_DECL );
 void SetMeshPath( SCALEFORM_CALLBACK_ARGS_DECL );
 };

 * in the same file, add an API_DEF block to expose the methods:
 * make sure you put a comma after each SFUI_DECL_METHOD function!

 SFUI_BEGIN_GAME_API_DEF
 SFUI_DECL_METHOD( ToggleFullScreen ),
 SFUI_DECL_METHOD( SetNewBackground ),
 SFUI_DECL_METHOD( NewLight ),
 SFUI_DECL_METHOD( NewScene ),
 SFUI_DECL_METHOD( SetLuminance ),
 SFUI_DECL_METHOD( SFGrabFocus ),
 SFUI_DECL_METHOD( SetMeshPath ),
 SFUI_END_GAME_API_DEF( GFxTutorial );

 * install the methods into flash like this:

 SFUI_INSTALL_GAME_API( pScaleformUI, sfMovieView, "GameAPI" );

 * after this, flash can call _root.GameAPI.ToggleFullScreen() etc.
 *
 * To make the API_DEF available outside of the file it's defined in,
 * add the following after you end the API_DEF block:
 *

 SFUI_MAKE_GAME_API_PUBLIC( ExternalName );

 * and install it like this:

 SFUI_INSTALL_EXTERNAL_GAME_API( ExternalName, pScaleformUI, sfMovieView, "GameAPI" );

 *
 */

#define SFUI_OBJ_NAME( classname, uiname ) ScaleformFunctionHandler_##classname##_##uiname
#define SFUI_OBJ_PTR_NAME( classname, uiname ) pScaleformFunctionHandler_##classname##_##uiname

#define SFUI_BEGIN_GAME_API_DEF \
template<class T>\
	class FunctionCallbackDefTable : public IScaleformUIFunctionHandlerDefinitionTable\
	{\
		static const ScaleformUIFunctionHandlerDefinition m_FunctionTable[];\
		public: const ScaleformUIFunctionHandlerDefinition* GetTable( void ) const {return m_FunctionTable;}\
	};\
template<class T>\
	const ScaleformUIFunctionHandlerDefinition FunctionCallbackDefTable<T>::m_FunctionTable[] = {\
			SFUI_DECL_METHOD( OnLoadFinished ),\
			SFUI_DECL_METHOD( OnReady ),\
			SFUI_DECL_METHOD( OnLoadProgress ),\
			SFUI_DECL_METHOD( OnLoadError ),\
			SFUI_DECL_METHOD( OnUnload ),\

// Verify that the method has the right arguments before converting it
template <typename T>
inline ScaleformUIFunctionHandler ToScaleformUIFunctionHandler(void (T::*method)(IUIMarshalHelper*, SFPARAMS))
{
	return reinterpret_cast< ScaleformUIFunctionHandler >( method );
}

#define SFUI_DECL_METHOD( method ) {#method, ToScaleformUIFunctionHandler( &T::method )}

#define SFUI_DECL_METHOD_AS( method, asname ) {asname, ToScaleformUIFunctionHandler( &T::method )}

#define SFUI_END_GAME_API_DEF( classname, uiname )\
		{NULL, NULL}\
	};\
	static FunctionCallbackDefTable<classname> SFUI_OBJ_NAME( classname, uiname );\
	IScaleformUIFunctionHandlerDefinitionTable* SFUI_OBJ_PTR_NAME( classname, uiname ) = &SFUI_OBJ_NAME( classname, uiname )

#define SFUI_REQUEST_ELEMENT( slot, pScaleformUI, classname, pinstance, uiname )\
		extern IScaleformUIFunctionHandlerDefinitionTable* SFUI_OBJ_PTR_NAME( classname, uiname );\
		pScaleformUI->RequestElement( slot, #uiname, reinterpret_cast<ScaleformUIFunctionHandlerObject*>( pinstance ), SFUI_OBJ_PTR_NAME( classname, uiname ) );

/***************************************************************
 * This is a helper class that helps us effeciently set SFText 
 * elements in flash
 */

class ISFTextObject
{

public:
	virtual void SetText( int value ) = 0;
	virtual void SetText( float value ) = 0;
	virtual void SetText( const char* pszText ) = 0;
	virtual void SetTextHTML( const char* pszText ) = 0;
	virtual void SetText( const wchar_t* pwszText ) = 0;
	virtual void SetTextHTML( const wchar_t* pwszText ) = 0;
	virtual bool IsValid( void ) = 0;
	virtual void Release( void ) = 0;
	virtual void SetVisible( bool visible ) = 0;
};

/*******************************************************************
 * This is used to pass state information back and forth to flash
 * MovieClip objects
 */

class ScaleformDisplayInfo
{
protected:
	enum SET_FLAGS
	{
		X_SET 			= 0x01,
		Y_SET 			= 0x02,
		ROTATION_SET 	= 0x04,
		ALPHA_SET 		= 0x08,
		VISIBILITY_SET 	= 0x10,
		XSCALE_SET 		= 0x20,
		YSCALE_SET		= 0x40,
	};

	double m_fX;
	double m_fY;
	double m_fRotation;
	double m_fAlpha;
	double m_fXScale;
	double m_fYScale;
	int m_iSetFlags;
	bool m_bVisibility;


public:
	ScaleformDisplayInfo() : m_iSetFlags( 0 ) {}

	void Clear( void ) {m_iSetFlags = 0;}

	inline bool IsXSet( void ) const			{return ( m_iSetFlags & X_SET 			 ) != 0;}
	inline bool IsYSet( void ) const			{return ( m_iSetFlags & Y_SET 			 ) != 0;}
	inline bool IsRotationSet( void ) const		{return ( m_iSetFlags & ROTATION_SET 	 ) != 0;}
	inline bool IsAlphaSet( void ) const		{return ( m_iSetFlags & ALPHA_SET 		 ) != 0;}
	inline bool IsVisibilitySet( void ) const	{return ( m_iSetFlags & VISIBILITY_SET 	 ) != 0;}
	inline bool IsXScaleSet( void ) const		{return ( m_iSetFlags & XSCALE_SET 		 ) != 0;}
	inline bool IsYScaleSet( void ) const		{return ( m_iSetFlags & YSCALE_SET		 ) != 0;}
	 
	inline void SetX( double value ) 			{m_iSetFlags |= X_SET 			; m_fX = value;}
	inline void SetY( double value ) 			{m_iSetFlags |= Y_SET 			; m_fY = value;}
	inline void SetRotation( double value ) 	{m_iSetFlags |= ROTATION_SET 	; m_fRotation = value;}
	inline void SetAlpha( double value ) 		{m_iSetFlags |= ALPHA_SET 		; m_fAlpha = value;}
	inline void SetVisibility( bool value ) 	{m_iSetFlags |= VISIBILITY_SET 	; m_bVisibility = value;}
	inline void SetXScale( double value ) 		{m_iSetFlags |= XSCALE_SET 		; m_fXScale = value;}
	inline void SetYScale( double value ) 		{m_iSetFlags |= YSCALE_SET		; m_fYScale = value;}
	
	inline double GetX( void ) const			{return m_fX;}
	inline double GetY( void ) const			{return m_fY;}
	inline double GetRotation( void ) const		{return m_fRotation;}
	inline double GetAlpha( void ) const		{return m_fAlpha;}
	inline bool   GetVisibility( void ) const	{return m_bVisibility;}
	inline double GetXScale( void ) const		{return m_fXScale;}
	inline double GetYScale( void ) const		{return m_fYScale;}

};


/***************************************************************
 * This is the interface used to initialize slot
 */
class IScaleformSlotInitController
{
public:
	// A new slot has been created and InitSlot almost finished, perform final configuration
	virtual void ConfigureNewSlotPostInit( int slot ) = 0;

	// Notification to external systems that a file was loaded by Scaleform libraries
	virtual bool OnFileLoadedByScaleform( char const *pszFilename, void *pvBuffer, int numBytesLoaded ) = 0;

	virtual const void * GetStringUserData( const char * pchStringTableName, const char * pchKeyName, int * pLength ) = 0;

	virtual void PassSignaturesArray( void *pvArray ) = 0;
};


/***************************************************************
 * This is the interface used to initialize avatar image data
 */
class IScaleformAvatarImageProvider
{
public:
	struct ImageInfo_t
	{
		void const *m_pvImageData;
		uint32 m_cbImageData;
	};

public:
	// Scaleform low-level image needs rgba bits of the inventory image (if it's ready)
	virtual bool GetImageInfo( uint64 xuid, ImageInfo_t *pImageInfo ) = 0;
};


/***************************************************************
 * This is the interface used to initialize inventory image data
 */
class IScaleformInventoryImageProvider
{
public:
	struct ImageInfo_t
	{
		void *m_pvEconItemView;
		const char *m_pDefaultIconName;
		const CUtlBuffer* m_bufImageDataRGBA;
		int m_nWidth;
		int m_nHeight;
	};

public:
	// Scaleform low-level image needs rgba bits of the inventory image (if it's ready)
	virtual bool GetInventoryImageInfo( uint64 uiItemId, ImageInfo_t *pImageInfo ) = 0;
};



/***************************************************************
 * This is the main interface which is used to interact with scaleform
 */

class IUIMarshalHelper
{
public:
	// SF4 TODO - REMOVE
	enum _ValueType {
		VT_Undefined,
		VT_Null,
		VT_Boolean,
		VT_Int,
		VT_UInt,
		VT_Number,
		VT_String,
		VT_StringW,
		VT_Object,
		VT_Array,
		VT_DisplayObject,
		VT_Closure,
		VT_ConvertBoolean,
		VT_ConvertInt,
		VT_ConvertUInt,
		VT_ConvertNumber,
		VT_ConvertString,
		VT_ConvertStringW
	};


	/***********************************************
	 * callback parameter handling
	 */

	virtual SFVALUEARRAY Params_GetArgs( SFPARAMS params ) = 0;
	virtual unsigned int Params_GetNumArgs( SFPARAMS params ) = 0;
	virtual bool Params_ArgIs( SFPARAMS params, unsigned int index, _ValueType v ) = 0;
	virtual SFVALUE Params_GetArg( SFPARAMS params, int index = 0 ) = 0;
	virtual _ValueType Params_GetArgType( SFPARAMS params, int index = 0 ) = 0;
	virtual double Params_GetArgAsNumber( SFPARAMS params, int index = 0 ) = 0;
	virtual bool Params_GetArgAsBool( SFPARAMS params, int index = 0 ) = 0;
	virtual const char* Params_GetArgAsString( SFPARAMS params, int index = 0 ) = 0;
	virtual const wchar_t* Params_GetArgAsStringW( SFPARAMS params, int index = 0 ) = 0;

	virtual void Params_DebugSpew( SFPARAMS params ) = 0;

	virtual void Params_SetResult( SFPARAMS params, SFVALUE value ) = 0;
	virtual void Params_SetResult( SFPARAMS params, int value ) = 0;
	virtual void Params_SetResult( SFPARAMS params, float value ) = 0;
	virtual void Params_SetResult( SFPARAMS params, bool value ) = 0;
	virtual void Params_SetResult( SFPARAMS params, const char* value, bool bMakeNewValue = true) = 0;
	virtual void Params_SetResult( SFPARAMS params, const wchar_t* value, bool bMakeNewValue = true ) = 0;

	virtual SFVALUE Params_CreateNewObject( SFPARAMS params ) = 0;
	virtual SFVALUE Params_CreateNewString( SFPARAMS params, const char* value ) = 0;
	virtual SFVALUE Params_CreateNewString( SFPARAMS params, const wchar_t* value ) = 0;
	virtual SFVALUE Params_CreateNewArray( SFPARAMS params, int size = -1 ) = 0;
};

#define SCALEFORMUI_INTERFACE_VERSION "ScaleformUI002"
class IScaleformUI: public IAppSystem, public IUIMarshalHelper
{

	/*******************************
	 * high level functions.  These are the bread and butter
	 * of the interface.  They deal mostly with controlling / updating slots
	 */

public:

	virtual void DumpMeshCacheStats() = 0;
        virtual void SetSingleThreadedMode( bool bSingleThreded ) = 0;

	virtual void RunFrame( float time ) = 0;
	virtual void AdvanceSlot( int slot ) = 0;
	virtual bool HandleInputEvent( const InputEvent_t &event ) = 0;

	virtual bool HandleIMEEvent( size_t hwnd, unsigned int uMsg, unsigned int  wParam, long lParam ) = 0;
	virtual bool PreProcessKeyboardEvent( size_t hwnd, unsigned int uMsg, unsigned int  wParam, long lParam ) = 0;
	virtual void SetIMEEnabled( bool bEnabled ) = 0;
	virtual void SetIMEFocus( int slot ) = 0;
	virtual void ShutdownIME() = 0;

	virtual float GetJoyValue( int slot, int stickIndex, int axis ) = 0;

	virtual void SetSlotViewport( int slot, int x, int y, int width, int height ) = 0;
	virtual void RenderSlot( int slot ) = 0;
	virtual void ForkRenderSlot( int slot ) = 0;
	virtual void JoinRenderSlot( int slot ) = 0;

	virtual void InitSlot( int slotID, const char* rootMovie, IScaleformSlotInitController *pController ) = 0;
	virtual void SlotRelease( int slotID ) = 0;
	virtual void SlotAddRef( int slot ) = 0;

	virtual void LockSlot( int slot ) = 0;
	virtual void UnlockSlot( int slot ) = 0;

	virtual void RequestElement( int slot, const char* elementName, ScaleformUIFunctionHandlerObject* object, const IScaleformUIFunctionHandlerDefinitionTable* tableObject ) = 0;
	virtual void RemoveElement( int slot, SFVALUE element ) = 0;

	virtual void InstallGlobalObject( int slot, const char* elementName, ScaleformUIFunctionHandlerObject* object, const IScaleformUIFunctionHandlerDefinitionTable* tableObject, SFVALUE *pInstalledGlobalObjectResult ) = 0;
	virtual void RemoveGlobalObject( int slot, SFVALUE element ) = 0;

	virtual bool SlotConsumesInputEvents( int slot ) = 0;
	virtual bool ConsumesInputEvents( void ) = 0;

	virtual bool SlotDeniesInputToGame( int slot ) = 0;
	virtual void DenyInputToGame( bool value ) = 0;

	// Called from the movieslot to inform Scaleform singleton that we do/don't want our slot to deny input to game
	virtual void DenyInputToGameFromFlash( int slot, bool value ) = 0;

	virtual void LockInputToSlot( int slot ) = 0;
	virtual void UnlockInput( void ) = 0;

	virtual bool AvatarImageAddRef( uint64 playerID ) = 0;
	virtual void AvatarImageRelease( uint64 playerID ) = 0;
	virtual void AvatarImageReload( uint64 playerID, IScaleformAvatarImageProvider *pProvider = NULL ) = 0;
	virtual void AddDeviceDependentObject( IShaderDeviceDependentObject * pObject ) = 0;
	virtual void RemoveDeviceDependentObject( IShaderDeviceDependentObject * pObject ) = 0;

	virtual bool InventoryImageAddRef( uint64 iItemId, IScaleformInventoryImageProvider *pGlobalInventoryImageProvider ) = 0;
	virtual void InventoryImageUpdate( uint64 iItemId, IScaleformInventoryImageProvider *pGlobalInventoryImageProvider ) = 0;
	virtual void InventoryImageRelease( uint64 iItemId ) = 0;

#ifdef USE_DEFAULT_INVENTORY_ICON_BACKGROUNDS
	virtual void InitInventoryDefaultIcons( CUtlVector< const char * > *vecIconDefaultNames ) = 0;
#endif

	virtual bool ChromeHTMLImageAddRef( uint64 imageID ) = 0;
	virtual void ChromeHTMLImageUpdate( uint64 imageID, const byte* rgba, int width, int height, ::ImageFormat format ) = 0;
	virtual void ChromeHTMLImageRelease( uint64 imageID ) = 0;

	virtual void ForceUpdateImages() = 0;

	virtual ButtonCode_t GetCurrentKey() = 0;

	virtual void SendUIEvent( const char* action, const char* eventData, int slot = 0 ) = 0;

	/*********
	 * Code to deal with the scaleform cursor
	 */

public:
	virtual void InitCursor( const char* cursorMovie ) = 0;
	virtual void ReleaseCursor( void ) = 0;

	virtual bool IsCursorVisible( void ) = 0;
	virtual void RenderCursor( void ) = 0;
	virtual void AdvanceCursor( void ) = 0;

	virtual void SetCursorViewport( int x, int y, int width, int height ) = 0;

	virtual void ShowCursor( void ) = 0;
	virtual void HideCursor( void ) = 0;

#if defined( _PS3 )
	virtual void PS3UseMoveCursor( void ) = 0;
	virtual void PS3UseStandardCursor( void ) = 0;
	virtual void PS3ForceCursorStart( void ) = 0;
	virtual void PS3ForceCursorEnd( void ) = 0;


#endif

	virtual void SetCursorShape( int shapeIndex ) = 0;

	virtual void ForceCollectGarbage( int slot ) = 0;

	virtual bool IsSetToControllerUI( int slot ) = 0;

	virtual void LockMostRecentInputDevice( int slot ) = 0;

	virtual void ClearCache( void ) = 0;

	/*********
	 * MovieDef and MovieView related stuff
	 */
public:
	
	/*****************
	 * There are many more methods in GFxMovieView.  If you need something that's not here,
	 * check the doucmention at www.scaleform.com or look in the GFxMovieView.h include file
	 * and add the functionality to this interface and then to ScaleformUIImpl
	 */

	virtual SFMOVIEDEF CreateMovieDef( const char* pfilename, unsigned int loadConstants = 0, size_t memoryArena = 0 ) = 0;
	virtual void ReleaseMovieDef( SFMOVIEDEF movieDef ) = 0;

	virtual SFMOVIE MovieDef_CreateInstance( SFMOVIEDEF movieDef, bool initFirstFrame = true, size_t memoryArena = 0 ) = 0;
	virtual void ReleaseMovieView( SFMOVIE movieView ) = 0;

	virtual void MovieView_Advance( SFMOVIE movieView, float time, unsigned int frameCatchUpCount = 2 ) = 0;
	virtual void MovieView_SetBackgroundAlpha( SFMOVIE movieView, float alpha ) = 0;
	virtual void MovieView_SetViewport( SFMOVIE movieView, int bufw, int bufh, int left, int top, int w, int h, unsigned int flags = 0 ) = 0;
	virtual void MovieView_Display( SFMOVIE movieView ) = 0;

	/**************************
	 * movie alignment
	 */

	// these come from gfxplayer.h
	// SF4 TODO - Remove
	enum _ScaleModeType
	{
		SM_NoScale, SM_ShowAll, SM_ExactFit, SM_NoBorder
	};
	enum _AlignType
	{
		Align_Center, Align_TopCenter, Align_BottomCenter, Align_CenterLeft, Align_CenterRight, Align_TopLeft, Align_TopRight, Align_BottomLeft, Align_BottomRight
	};

	virtual void MovieView_SetViewScaleMode( SFMOVIE movieView, _ScaleModeType type ) = 0;
	virtual _ScaleModeType MovieView_GetViewScaleMode( SFMOVIE movieView ) = 0;

	virtual void MovieView_SetViewAlignment( SFMOVIE movieView, _AlignType type ) = 0;
	virtual _AlignType MovieView_GetViewAlignment( SFMOVIE movieView ) = 0;

	/******************************
	 * create / access values
	 */

	virtual SFVALUE MovieView_CreateObject( SFMOVIE movieView, const char* className = NULL, SFVALUEARRAY args = SFVALUEARRAY(0, NULL), int numArgs = 0 ) = 0;
	virtual SFVALUE MovieView_GetVariable( SFMOVIE movieView, const char* variablePath ) = 0;
	virtual SFVALUE MovieView_CreateString( SFMOVIE movieView, const char *str ) = 0;
	virtual SFVALUE MovieView_CreateStringW( SFMOVIE movieView, const wchar_t *str ) = 0;
	virtual SFVALUE MovieView_CreateArray( SFMOVIE movieView, int size = -1 ) = 0;

	/*************************************
	 * movie input events
	 */
	
	/********************************************
	 * Methods to handle translation of strings and
	 * replacing ${glyph} type constructs in strings
	 */

	// if key does not start with '#' this function returns NULL
	virtual const wchar_t* Translate( const char *key, bool* pIsHTML ) = 0;

	// escape the '$' in strings to avoid glyph replacment '\\${not a glyph}'
	virtual const wchar_t* ReplaceGlyphKeywordsWithHTML( const wchar_t* pin, int fontSize = 0, bool bForceControllerGlyph = false ) = 0;
	virtual const wchar_t* ReplaceGlyphKeywordsWithHTML( const char* text, int fontSize = 0, bool bForceControllerGlyph = false ) = 0;

	// these convert html codes to &lt; style codes and escape the $ and @
	virtual void MakeStringSafe( const wchar_t* stringin, OUT_Z_BYTECAP(outlength) wchar_t* stringout, int outlength ) = 0;


	/********************************************
	 * keyboard codes
	 */

	enum KeyCode
	{
		VoidSymbol = 0,

		// A through Z and numbers 0 through 9.
		A = 65,
		B,
		C,
		D,
		E,
		F,
		G,
		H,
		I,
		J,
		K,
		L,
		M,
		N,
		O,
		P,
		Q,
		R,
		S,
		T,
		U,
		V,
		W,
		X,
		Y,
		Z,
		Num0 = 48,
		Num1,
		Num2,
		Num3,
		Num4,
		Num5,
		Num6,
		Num7,
		Num8,
		Num9,

		// Numeric keypad.
		KP_0 = 96,
		KP_1,
		KP_2,
		KP_3,
		KP_4,
		KP_5,
		KP_6,
		KP_7,
		KP_8,
		KP_9,
		KP_Multiply,
		KP_Add,
		KP_Enter,
		KP_Subtract,
		KP_Decimal,
		KP_Divide,

		// Function keys.
		F1 = 112,
		F2,
		F3,
		F4,
		F5,
		F6,
		F7,
		F8,
		F9,
		F10,
		F11,
		F12,
		F13,
		F14,
		F15,

		// Other keys.
		Backspace = 8,
		Tab,
		Clear = 12,
		Return,
		Shift = 16,
		Control,
		Alt,
		Pause,
		CapsLock = 20, // Toggle
		Escape = 27,
		Space = 32,
		PageUp,
		PageDown,
		End = 35,
		Home,
		Left,
		Up,
		Right,
		Down,
		Insert = 45,
		Delete,
		Help,

		XStick2Up = 0x88,		// These 4 are custom key codes for the right thumbstick.
		XStick2Left = 0x89,
		XStick2Right = 0x8A,
		XStick2Down = 0x8B,


		MWHEEL_UP,
		MWHEEL_DOWN,

		NumLock = 144, // Toggle
		ScrollLock = 145, // Toggle

		Semicolon = 186,
		Equal = 187,
		Comma = 188, // Platform specific?
		Minus = 189,
		Period = 190, // Platform specific?
		Slash = 191,
		Bar = 192,
		BracketLeft = 219,
		Backslash = 220,
		BracketRight = 221,
		Quote = 222,

		OEM_AX = 0xE1, //  'AX' key on Japanese AX kbd
		OEM_102 = 0xE2, //  "<>" or "\|" on RT 102-key kbd.
		ICO_HELP = 0xE3, //  Help key on ICO
		ICO_00 = 0xE4, //  00 key on ICO

		// Total number of keys.
		KeyCount
	};

	// these are for strings that use $@x where x is a code for the controller button function
	// the function x refers to ( Select ) gets mapped to a controller button ( PS3_Square ), and
	// the PS3_Square gets mapped to an image to show in the UI.


	// these are the buttons that we have glyphs for

	class ControllerButton
	{
	public:
		enum Enum
		{
			// ** Important **
			// if you add or remove functions from this enum, be sure to reflect the changes in the
			// following arrays in ScaleformUITranslationsImpl.cpp:
			// g_buttonFunctionKeywords
			// g_controllerButtonToButtonCodeLookup
			// g_controllerButtonImageNames

			// generic enums

			Confirm,
			Cancel,
			West,
			North,

			LShoulder,
			RShoulder,
			LTrigger,
			RTrigger,

			DPadUp,
			DPadDown,
			DPadLeft,
			DPadRight,
			DPad,

			LStickUp,
			LStickDown,
			LStickLeft,
			LStickRight,
			LStickButton,
			LStick,

			RStickUp,
			RStickDown,
			RStickLeft,
			RStickRight,
			RStickButton,
			RStick,

			Start,
			AltStart,

			NumButtons,

			Undefined = NumButtons,


			// xbox enums ( these have the same ordinal values as the generics above )

			XBoxA = Confirm,
			XBoxB,
			XBoxX,
			XBoxY,

			XBoxLB,
			XBoxRB,
			XBoxLT,
			XBoxRT,

			XBoxStart = Start,
			XBoxBack,


			// ps3 enums ( these have the same ordinal values as the generics above )

			PS3X = Confirm,
			PS3Circle,
			PS3Square,
			PS3Triangle,

			PS3L1,
			PS3R1,
			PS3L2,
			PS3R2,

			PS3L3 = LStickButton,
			PS3R3 = RStickButton,

			PS3Start = Start,
			PS3Select,

		};
	};

	virtual void RefreshKeyBindings( void ) = 0;
	virtual void ShowActionNameWhenActionIsNotBound( bool value ) = 0;
	virtual void UpdateBindingForButton( ButtonCode_t bt, const char* pbinding ) = 0;


	/********************************************
	 * Hit Testing
	 */

	// this comes from GFxPlayer.h
	// SF4 TODO - Remove
	enum _HitTestType
	{
		HitTest_Bounds = 0, HitTest_Shapes = 1, HitTest_ButtonEvents = 2, HitTest_ShapesNoInvisible = 3
	};

	virtual bool MovieView_HitTest( SFMOVIE movieView, float x, float y, _HitTestType testCond = HitTest_Shapes, unsigned int controllerIdx = 0 ) = 0;

	/**********************************************
	 * Scaleform::GFx::Value stuff
	 */

protected:
	
public:
	// This comes from GFxPlayer.h

	virtual SFVALUE CreateValue( SFVALUE value ) = 0;
	virtual SFVALUE CreateValue( int value ) = 0;
	virtual SFVALUE CreateValue( float value ) = 0;
	virtual SFVALUE CreateValue( bool value ) = 0;
	virtual SFVALUE CreateValue( const char* value ) = 0;
	virtual SFVALUE CreateValue( const wchar_t* value ) = 0;

	virtual SFVALUE CreateNewObject( int slot ) = 0;
	virtual SFVALUE CreateNewString( int slot, const char* value ) = 0;
	virtual SFVALUE CreateNewString( int slot, const wchar_t* value ) = 0;
	virtual SFVALUE CreateNewArray( int slot, int size = -1 ) = 0;

	virtual void Value_SetValue( SFVALUE obj, SFVALUE value ) = 0;
	virtual void Value_SetValue( SFVALUE obj, int value ) = 0;
	virtual void Value_SetValue( SFVALUE obj, float value ) = 0;
	virtual void Value_SetValue( SFVALUE obj, bool value ) = 0;
	virtual void Value_SetValue( SFVALUE obj, const char* value ) = 0;
	virtual void Value_SetValue( SFVALUE obj, const wchar_t* value ) = 0;

	virtual void Value_SetColor( SFVALUE obj, int color ) = 0;
	virtual void Value_SetColor( SFVALUE obj, float r, float g, float b, float a ) = 0;
	virtual void Value_SetTint( SFVALUE obj, int color ) = 0;
	virtual void Value_SetTint( SFVALUE obj, float r, float g, float b, float a ) = 0;
	virtual void Value_SetColorTransform( SFVALUE obj, int colorMultiply, int colorAdd ) = 0;
	virtual void Value_SetColorTransform( SFVALUE obj, float r, float g, float b, float a, int colorAdd ) = 0;

	virtual void Value_SetArraySize( SFVALUE obj, int size ) = 0;
	virtual int  Value_GetArraySize( SFVALUE obj ) = 0;
	virtual void Value_ClearArrayElements( SFVALUE obj ) = 0;
	virtual void Value_RemoveArrayElement( SFVALUE obj, int index ) = 0;
	virtual void Value_RemoveArrayElements( SFVALUE obj, int index, int count ) = 0;
	virtual SFVALUE Value_GetArrayElement( SFVALUE obj, int index ) = 0;

	virtual void Value_SetArrayElement( SFVALUE obj, int index, SFVALUE value ) = 0;
	virtual void Value_SetArrayElement( SFVALUE obj, int index, int value ) = 0;
	virtual void Value_SetArrayElement( SFVALUE obj, int index, float value ) = 0;
	virtual void Value_SetArrayElement( SFVALUE obj, int index, bool value ) = 0;
	virtual void Value_SetArrayElement( SFVALUE obj, int index, const char* value ) = 0;
	virtual void Value_SetArrayElement( SFVALUE obj, int index, const wchar_t* value ) = 0;

	virtual void Value_SetText( SFVALUE obj, const char* value ) = 0;
	virtual void Value_SetText( SFVALUE obj, const wchar_t* value ) = 0;
	virtual void Value_SetTextHTML( SFVALUE obj, const char* value ) = 0;
	virtual void Value_SetTextHTML( SFVALUE obj, const wchar_t* value ) = 0;
	virtual int  Value_SetFormattedText( SFVALUE obj, const char* pFormat, ... ) = 0;

	virtual void ReleaseValue( SFVALUE value ) = 0;

	virtual void CreateValueArray( SFVALUEARRAY& valueArray, int length ) = 0;
	virtual SFVALUEARRAY CreateValueArray( int length ) = 0;
	virtual void ReleaseValueArray( SFVALUEARRAY& valueArray ) = 0;
	virtual void ReleaseValueArray( SFVALUEARRAY& valueArray, int count ) = 0;	// DEPRECATED
	virtual SFVALUE ValueArray_GetElement( SFVALUEARRAY, int index ) = 0;
	virtual _ValueType ValueArray_GetType( SFVALUEARRAY array, int index ) = 0;
	virtual double ValueArray_GetNumber( SFVALUEARRAY array, int index ) = 0;
	virtual bool ValueArray_GetBool( SFVALUEARRAY array, int index ) = 0;
	virtual const char* ValueArray_GetString( SFVALUEARRAY array, int index ) = 0;
	virtual const wchar_t* ValueArray_GetStringW( SFVALUEARRAY array, int index ) = 0;

	virtual void ValueArray_SetElement( SFVALUEARRAY, int index, SFVALUE value ) = 0;
	virtual void ValueArray_SetElement( SFVALUEARRAY, int index, int value ) = 0;
	virtual void ValueArray_SetElement( SFVALUEARRAY, int index, float value ) = 0;
	virtual void ValueArray_SetElement( SFVALUEARRAY, int index, bool value ) = 0;
	virtual void ValueArray_SetElement( SFVALUEARRAY, int index, const char* value ) = 0;
	virtual void ValueArray_SetElement( SFVALUEARRAY, int index, const wchar_t* value ) = 0;

	virtual void ValueArray_SetElementText( SFVALUEARRAY, int index, const char* value ) = 0;
	virtual void ValueArray_SetElementText( SFVALUEARRAY, int index, const wchar_t* value ) = 0;
	virtual void ValueArray_SetElementTextHTML( SFVALUEARRAY, int index, const char* value ) = 0;
	virtual void ValueArray_SetElementTextHTML( SFVALUEARRAY, int index, const wchar_t* value ) = 0;

	virtual bool Value_HasMember( SFVALUE value, const char* name ) = 0;
	virtual SFVALUE Value_GetMember( SFVALUE value, const char* name ) = 0;

	virtual bool Value_SetMember( SFVALUE obj, const char *name, SFVALUE value ) = 0;
	virtual bool Value_SetMember( SFVALUE obj, const char *name, int value ) = 0;
	virtual bool Value_SetMember( SFVALUE obj, const char *name, float value ) = 0;
	virtual bool Value_SetMember( SFVALUE obj, const char *name, bool value ) = 0;
	virtual bool Value_SetMember( SFVALUE obj, const char *name, const char* value ) = 0;
	virtual bool Value_SetMember( SFVALUE obj, const char *name, const wchar_t* value ) = 0;

	virtual ISFTextObject* TextObject_MakeTextObject( SFVALUE value ) = 0;
	virtual ISFTextObject* TextObject_MakeTextObjectFromMember( SFVALUE value, const char* pName ) = 0;

	virtual bool Value_InvokeWithoutReturn( SFVALUE obj, const char* methodName, const SFVALUEARRAY& args ) = 0;
	virtual SFVALUE Value_Invoke( SFVALUE obj, const char* methodName, const SFVALUEARRAY& args ) = 0;
	virtual bool Value_InvokeWithoutReturn( SFVALUE obj, const char* methodName, SFVALUE args, int numArgs ) = 0;
	virtual SFVALUE Value_Invoke( SFVALUE obj, const char* methodName, SFVALUE args, int numArgs ) = 0;
	virtual bool Value_InvokeWithoutReturn( SFVALUE obj, const char* methodName, const SFVALUEARRAY& args, int numArgs ) = 0;	// DEPRECATED
	virtual SFVALUE Value_Invoke( SFVALUE obj, const char* methodName, const SFVALUEARRAY& args, int numArgs ) = 0;				// DEPRECATED

	virtual void Value_SetVisible( SFVALUE obj, bool visible ) = 0;
	virtual void Value_GetDisplayInfo( SFVALUE obj, ScaleformDisplayInfo* dinfo ) = 0;
	virtual void Value_SetDisplayInfo( SFVALUE obj, const ScaleformDisplayInfo* dinfo ) = 0;

	virtual _ValueType Value_GetType( SFVALUE obj ) = 0;
	virtual double Value_GetNumber( SFVALUE obj ) = 0;
	virtual bool Value_GetBool( SFVALUE obj ) = 0;
	virtual const char* Value_GetString( SFVALUE obj ) = 0;
	virtual const wchar_t* Value_GetStringW( SFVALUE obj ) = 0;

	virtual SFVALUE Value_GetText( SFVALUE obj ) = 0;
	virtual SFVALUE Value_GetTextHTML( SFVALUE obj ) = 0;
};

inline void ScaleformInitFullScreenAndCursor( IScaleformUI* pui, const char* fullScreenName, const char* cursorName, IScaleformSlotInitController *pInitController )
{
	pui->InitSlot( SF_FULL_SCREEN_SLOT, fullScreenName, pInitController );
	if ( !IsX360() )
		pui->InitCursor( cursorName );
}

inline void ScaleformReleaseFullScreenAndCursor( IScaleformUI* pui )
{
	pui->SlotRelease( SF_FULL_SCREEN_SLOT );
	pui->ReleaseCursor();
}

inline void SFDevMsg( const char *pMsgFormat, ... )
{
#if !defined( DBGFLAG_STRINGS_STRIP )
	ConVarRef dev_scaleform_debug("dev_scaleform_debug");
	if ( dev_scaleform_debug.GetBool() )
	{
		char str[4096] = "SF: ";
		char startIdx = strlen(str);
		va_list marker;
		va_start( marker, pMsgFormat );
		V_vsnprintf( str + startIdx, sizeof( str ) - startIdx, pMsgFormat, marker );
		va_end( marker );

		DevMsg( "%s", str );
	}
#endif
}

template<class T>
class ScaleformFlashInterfaceMixin : public T
{
public:

	// the fancy for statement below is so we can use this macro like
	// so:
	//
	// WITH_SLOT_LOCKED
	// {
	// 		//code requiring lock
	// }

	class ForLoopOnce
	{
	public:
		ForLoopOnce() : m_bDone(false) {}

		bool Looping() { return !m_bDone; }
		void SetDone() { m_bDone = true; }

	private:
		bool	m_bDone;
	};

	class AutoScaleformSlotLocker : public ForLoopOnce
	{
	public:
		explicit AutoScaleformSlotLocker( ScaleformFlashInterfaceMixin<T>& parent ) : m_parent(parent)
		{
			m_parent.LockScaleformSlot();
		}

		~AutoScaleformSlotLocker()
		{
			m_parent.UnlockScaleformSlot();
		}

	private:
		AutoScaleformSlotLocker();
		ScaleformFlashInterfaceMixin<T>& m_parent;
	};

#define WITH_SLOT_LOCKED for ( AutoScaleformSlotLocker __locker__(*this); __locker__.Looping(); __locker__.SetDone() )

	class AutoScaleformValueArray : public SFVALUEARRAY, public ForLoopOnce
	{
	public:
		AutoScaleformValueArray( ScaleformFlashInterfaceMixin<T>& parent, int count ) : 
			m_parent(parent)
		{
			m_parent.m_pScaleformUI->CreateValueArray(*this, count);
		}

		~AutoScaleformValueArray()
		{
			m_parent.m_pScaleformUI->ReleaseValueArray(*this);
		}

	private:
		AutoScaleformValueArray();
		ScaleformFlashInterfaceMixin<T>& m_parent;
	};

#define WITH_SFVALUEARRAY( name, count ) for ( AutoScaleformValueArray name(*this, count); name.Looping(); name.SetDone() )

	class AutoScaleformValueArraySlotLocker : public SFVALUEARRAY, public ForLoopOnce
	{
	public:
		explicit AutoScaleformValueArraySlotLocker( class ScaleformFlashInterfaceMixin& parent, int count ) : 
			m_parent(parent)
		{
			m_parent.LockScaleformSlot();
			m_parent.m_pScaleformUI->CreateValueArray(*this, count);
		}

		~AutoScaleformValueArraySlotLocker()
		{
			m_parent.m_pScaleformUI->ReleaseValueArray(*this);
			m_parent.UnlockScaleformSlot();
		}

	private:
		AutoScaleformValueArraySlotLocker();
		ScaleformFlashInterfaceMixin& m_parent;
	};

#define WITH_SFVALUEARRAY_SLOT_LOCKED( name, count ) for ( AutoScaleformValueArraySlotLocker name(*this, count); name.Looping(); name.SetDone() )

#define SF_SPLITSCREEN_PLAYER_GUARD() ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_iFlashSlot - SF_FIRST_SS_SLOT ); GAMEUI_ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_iFlashSlot - SF_FIRST_SS_SLOT )

#define SF_FORCE_SPLITSCREEN_PLAYER_GUARD(x) ACTIVE_SPLITSCREEN_PLAYER_GUARD( (x) ); GAMEUI_ACTIVE_SPLITSCREEN_PLAYER_GUARD( (x) )

	// enum to distinguish the desired joystick.  Passed into the GetJoyX and GetJoyY methods )
	enum
	{
		LEFT_STICK,
		RIGHT_STICK
	};

	//scaleform callbacks
	// GameAPI stuff

	IScaleformUI* m_pScaleformUI;
	SFVALUE m_FlashAPI;
	int m_iFlashSlot;
	bool m_bFlashAPIIsValid;

	ScaleformFlashInterfaceMixin()
	{
		InitScaleformMixinAfterConstruction();
	}

	void InitScaleformMixinAfterConstruction( void )
	{
		m_bFlashAPIIsValid = false;
		m_iFlashSlot = -1;
		m_pScaleformUI = NULL;
		m_FlashAPI = NULL;
	}

	virtual ~ScaleformFlashInterfaceMixin()
	{
		RemoveFlashElement();
	}

	/***********************
	 * required callback handlers
	 */


	void OnLoadFinished( IUIMarshalHelper* puiHelper, SFPARAMS params )
	{
		// $$$REI TODO: This callback needs access to Scaleform directly; figure out how to avoid this callback in Panorama
		IScaleformUI* pui = static_cast< IScaleformUI* >( puiHelper );

		m_pScaleformUI = pui;
		m_FlashAPI = m_pScaleformUI->CreateValue( pui->Params_GetArg( params, 0 ) );
		m_iFlashSlot = ( int ) m_pScaleformUI->Params_GetArgAsNumber( params, 1 );
		m_pScaleformUI->SlotAddRef( m_iFlashSlot );

		m_bFlashAPIIsValid = true;

		#if (defined(_DEBUG) || defined(USE_MEM_DEBUG))
				char szName[128];
				V_snprintf( szName, ARRAYSIZE(szName),  "ScaleformUI::OnLoadFinished_%s", typeid( *this ).name() );
				MEM_ALLOC_CREDIT_( szName );
		#endif

		FlashLoaded();

		#if defined(_DEBUG) 
			SFDevMsg("ScaleformUI::OnLoadFinished_%s slot=%d\n", typeid( *this ).name(), m_iFlashSlot);
		#else
			SFDevMsg("ScaleformUI::OnLoadFinished slot=%d\n", m_iFlashSlot);
		#endif
	}

	void OnReady( IUIMarshalHelper* pui, SFPARAMS params )
	{
		#if (defined(_DEBUG) || defined(USE_MEM_DEBUG))
				char szName[128];
				V_snprintf( szName, ARRAYSIZE(szName),  "ScaleformUI::OnReady_%s", typeid( *this ).name() );
				MEM_ALLOC_CREDIT_( szName );
		#endif

		FlashReady();

		SFDevMsg("ScaleformUI::OnReady\n");
	}

	void OnLoadProgress( IUIMarshalHelper* pui, SFPARAMS params )
	{
		int loadedBytes;
		int totalBytes;

		loadedBytes = ( int ) pui->Params_GetArgAsNumber( params, 1 );
		totalBytes = ( int ) pui->Params_GetArgAsNumber( params, 2 );

		FlashLoadProgress( loadedBytes, totalBytes );
	}

	void OnLoadError( IUIMarshalHelper* puiHelper, SFPARAMS params )
	{
		// $$$REI TODO: This callback needs access to Scaleform directly; figure out how to avoid this callback in Panorama
		IScaleformUI* pui = static_cast< IScaleformUI* >( puiHelper );

		FlashLoadError( pui, params );
	}

	void OnUnload( IUIMarshalHelper* puiHelper, SFPARAMS params )
	{
#if (defined(_DEBUG) || defined(USE_MEM_DEBUG))
		char szName[128];
		V_snprintf( szName, ARRAYSIZE(szName),  "ScaleformUI::OnUnload_%s", typeid( *this ).name() );		
#endif

		if ( PreUnloadFlash() )
		{
			SFDevMsg("ScaleformUI::OnUnload slot=%d\n", m_iFlashSlot);
			m_pScaleformUI->Params_SetResult( params, true );
			m_bFlashAPIIsValid = false;
			m_pScaleformUI->ReleaseValue( m_FlashAPI );
			m_FlashAPI = NULL;
			m_pScaleformUI->SlotRelease( m_iFlashSlot );
			m_pScaleformUI = NULL;
			PostUnloadFlash();
		}
		else
		{
			SFDevMsg("ScaleformUI::OnUnload slot=%d FAILED\n", m_iFlashSlot);
			m_pScaleformUI->Params_SetResult( params, false );
		}
	}

	/**************************
	 * useful functions
	 */

	void RemoveFlashElement( void )
	{
#if (defined(_DEBUG) || defined(USE_MEM_DEBUG))
		char szName[128];
		V_snprintf( szName, ARRAYSIZE(szName),  "ScaleformUI::RemoveFlashElement_%s", typeid( *this ).name() );		
#endif
		if ( FlashAPIIsValid() )
		{
			// the RemoveElement call takes care of locking for us
			m_pScaleformUI->RemoveElement( m_iFlashSlot, m_FlashAPI );
		}
	}

	bool FlashAPIIsValid( void )
	{
		return m_bFlashAPIIsValid;
	}

	// override these functions for you own use

	virtual void FlashLoaded( void )
	{
	}

	virtual void FlashReady( void )
	{
	}

	virtual bool PreUnloadFlash( void )
	{
		// return false if you don't want to unload now
		return true;
	}

	virtual void PostUnloadFlash( void )
	{
	}

	virtual void FlashLoadProgress( int loadedBytes, int totalBytes )
	{
	}

	virtual void FlashLoadError( IScaleformUI* pui, SFPARAMS params )
	{
		// Failed to load a movie clip (dependency swf file corrupt)
		Error( "Error loading swf file!" );
	}

	float GetJoyX( int stick )
	{
		if ( FlashAPIIsValid() )
		{
			return m_pScaleformUI->GetJoyValue( m_iFlashSlot - SF_FIRST_SS_SLOT, stick, 0 );
		}
		else
			return 0.0f;
	}

	float GetJoyY( int stick )
	{
		if ( FlashAPIIsValid() )
		{
			return m_pScaleformUI->GetJoyValue( m_iFlashSlot - SF_FIRST_SS_SLOT, stick, 1 );
		}
		else
			return 0.0f;
	}

	SFVALUE CreateFlashObject()
	{
		SFVALUE result = NULL;

		if ( FlashAPIIsValid() )
		{
			WITH_SLOT_LOCKED
			{
				result = m_pScaleformUI->CreateNewObject( m_iFlashSlot );
			}
		}

		return result;
	}

	SFVALUE CreateFlashArray( int length = -1 )
	{
		SFVALUE result = NULL;

		if ( FlashAPIIsValid() )
		{
			WITH_SLOT_LOCKED
			{
				result = m_pScaleformUI->CreateNewArray( m_iFlashSlot, length );
			}
		}

		return result;
	}

	SFVALUE CreateFlashString( const char *value )
	{
		SFVALUE result = NULL;

		if ( FlashAPIIsValid() )
		{
			WITH_SLOT_LOCKED
			{
				result = m_pScaleformUI->CreateNewString( m_iFlashSlot, value );
			}
		}

		return result;
	}

	SFVALUE CreateFlashString( const wchar_t* value )
	{
		SFVALUE result = NULL;

		if ( FlashAPIIsValid() )
		{
			WITH_SLOT_LOCKED
			{
				result = m_pScaleformUI->CreateNewString( m_iFlashSlot, value );
			}
		}

		return result;

	}

	void LockInputToSlot( int slot = -1 )
	{
		AssertMsg( m_pScaleformUI, "Called LockInputToSlot with NULL m_pScaleformUI pointer (probably from PostUnloadFlash" );

		if ( m_pScaleformUI )
		{
			if ( slot == -1 )
				slot = m_iFlashSlot - SF_FIRST_SS_SLOT;

			m_pScaleformUI->LockInputToSlot( slot );
		}
	}

	void UnlockInput()
	{
		AssertMsg( m_pScaleformUI, "Called UnlockInput with NULL m_pScaleformUI pointer (probably from PostUnloadFlash" );

		if ( m_pScaleformUI )
		{
			m_pScaleformUI->UnlockInput();
		}
	}


	void LockScaleformSlot( void )
	{
		AssertMsg( m_pScaleformUI, "Called LockScaleformSlot with NULL m_pScaleformUI pointer (probably from PostUnloadFlash" );

		if ( m_pScaleformUI )
		{
			m_pScaleformUI->LockSlot( m_iFlashSlot );
		}
	}

	void UnlockScaleformSlot( void )
	{
		AssertMsg( m_pScaleformUI, "Called UnlockScaleformSlot with NULL m_pScaleformUI pointer (probably from PostUnloadFlash" );

		if ( m_pScaleformUI )
		{
			m_pScaleformUI->UnlockSlot( m_iFlashSlot );
		}
	}

	void SafeReleaseSFVALUE( SFVALUE& value )
	{
		AssertMsg( m_pScaleformUI, "Called SafeReleaseSFVALUE with NULL m_pScaleformUI pointer (probably from PostUnloadFlash" );

		if ( m_pScaleformUI && value )
		{
			m_pScaleformUI->ReleaseValue( value );
			value = NULL;
		}
	}

	void SafeReleaseSFTextObject( ISFTextObject*& value )
	{
		if ( value )
		{
			WITH_SLOT_LOCKED
			{
				value->Release();
			}
			value = NULL;
		}
	}

	void SendUIEvent( const char* action, const char* eventData )
	{
		AssertMsg( m_pScaleformUI, "Called SendUIEvent with NULL m_pScaleformUI pointer (probably from PostUnloadFlash" );

		if ( m_pScaleformUI )
			m_pScaleformUI->SendUIEvent( action, eventData, m_iFlashSlot );
	}
};


class ScaleformEmptyClass{};

typedef ScaleformFlashInterfaceMixin<ScaleformEmptyClass> ScaleformFlashInterface;

#endif
