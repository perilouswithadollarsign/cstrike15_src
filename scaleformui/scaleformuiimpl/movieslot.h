//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//


#if !defined( MOVIESLOT_H_ )
#define MOVIESLOT_H_

class ScaleformUIImpl;

#include "UtlStringMap.h"

class BaseSlot
{
public:
	SF::GFx::MovieDef* m_pMovieDef;
	SF::GFx::Movie* m_pMovieView;
	SF::GFx::Value m_GlobalValue;
	SF::GFx::Value m_GameAPI;
	CUtlStringMap< SF::GFx::Value * > m_mapGlobalObjects;
	int m_iSlot;
	int m_iRefCount;
	bool m_bControllerUI;
	float m_fChangeControllerTimeout;

protected:
	enum CVAR_TYPE_WANTED
	{
		CVAR_WANT_NUMBER,
		CVAR_WANT_NUMBER_MIN,
		CVAR_WANT_NUMBER_MAX,
		CVAR_WANT_STRING,
		CVAR_WANT_BOOL,
	};


	void GetConvar( SF::GFx::FunctionHandler::Params* params, CVAR_TYPE_WANTED typeWanted );
	bool ConvertValue( SF::GFx::Value *value, KeyValues* kv );
	void PopulateObject( SF::GFx::Value* value, KeyValues* kv );
	const char* PopulateArray( SF::GFx::Value* value, KeyValues* kv );

public:
	BaseSlot();

	void CreateKeyTable();

	void Init( const char* movieName, int slot );

	void UpdateSafeZone( void );

	void UpdateTint( void );

	void AddRef( void );
	bool Release( void );

	void LoadKVFile( SCALEFORM_CALLBACK_ARGS_DECL );
	void SaveKVFile( SCALEFORM_CALLBACK_ARGS_DECL );

	void GetConvarNumber( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetConvarNumberMin( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetConvarNumberMax( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetConvarString( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetConvarBoolean( SCALEFORM_CALLBACK_ARGS_DECL );

	void GetPlayerColorObject( SCALEFORM_CALLBACK_ARGS_DECL );

	void SetConvar( SCALEFORM_CALLBACK_ARGS_DECL );
	void Translate( SCALEFORM_CALLBACK_ARGS_DECL );
	void ReplaceGlyphs( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetPAXAvatarFromName(SCALEFORM_CALLBACK_ARGS_DECL);

	void SendUIEvent( SCALEFORM_CALLBACK_ARGS_DECL );

	void GetClipboardText( SCALEFORM_CALLBACK_ARGS_DECL );
	void SetClipboardText( SCALEFORM_CALLBACK_ARGS_DECL );

	void MakeStringSafe( SCALEFORM_CALLBACK_ARGS_DECL );

	bool HandleCharTyped( const wchar_t* typed, int slost );

	bool HandleKeyEvent( bool keyDown, ButtonCode_t code, ButtonCode_t vkey, const char* binding, int slot );
	void LockInputToSlot( int slot );
	void UnlockInput( void );
	bool SetToControllerUI( bool value, bool isKeyOrButtonPress );
	bool IsSetToControllerUI( void );
	void Advance( float time );
	void LockMostRecentInputDevice( void );

	void ConsoleCommand( SCALEFORM_CALLBACK_ARGS_DECL );
	void ConsoleCommandExecute( SCALEFORM_CALLBACK_ARGS_DECL );

	void ForceCollectGarbage( void );

	virtual void RequestElement( const char* elementName, ScaleformUIFunctionHandlerObject* object, const IScaleformUIFunctionHandlerDefinitionTable* tableObject )
	{
	}

	virtual void RemoveElement( SF::GFx::Value* element )
	{
	}

	virtual void InstallGlobalObject( const char* elementName, ScaleformUIFunctionHandlerObject* object, const IScaleformUIFunctionHandlerDefinitionTable* tableObject, SF::GFx::Value* *pInstalledGlobalObjectResult )
	{
	}

	virtual void RemoveGlobalObject( SF::GFx::Value* element )
	{
	}

	virtual const ScaleformUIFunctionHandlerDefinition* GetSlotAPITable( void )
	{
		return NULL;
	}

	virtual bool ConsumesInputEvents( void )
	{
		return true;
	}

	// SF4 TODO
	// We create slot 2 while loading the map and start calling advance on it. However, the qms doesn't call
	// render very often during this phase so the advance gets too far ahead of the renderer which causes SF 
	// internal buffers to fill up. This is a hack to work around this and might need to be done differently
	volatile uint32 m_advanceCount;

protected:
	virtual void Unload( void );

};

class MovieSlot: public BaseSlot
{
public:
	int m_iNumInputConsumers;
	bool m_bDisableAnalogNavigation;

public:
	MovieSlot();

public:
	void RequestElement( const char* elementName, ScaleformUIFunctionHandlerObject* object, const IScaleformUIFunctionHandlerDefinitionTable* tableObject );
	void RemoveElement( SF::GFx::Value* element );

	void InstallGlobalObject( const char* elementName, ScaleformUIFunctionHandlerObject* object, const IScaleformUIFunctionHandlerDefinitionTable* tableObject, SF::GFx::Value* *pInstalledGlobalObjectResult );
	void RemoveGlobalObject( SF::GFx::Value* element );

	const ScaleformUIFunctionHandlerDefinition* GetSlotAPITable();

	bool ConsumesInputEvents( void )
	{
		return m_iNumInputConsumers > 0;
	}

public:
	void AddInputConsumer( SCALEFORM_CALLBACK_ARGS_DECL );
	void RemoveInputConsumer( SCALEFORM_CALLBACK_ARGS_DECL );

	void DenyInputToGameFromFlash( SCALEFORM_CALLBACK_ARGS_DECL );

	void SetCursorShape( SCALEFORM_CALLBACK_ARGS_DECL );
	void ShowCursor( SCALEFORM_CALLBACK_ARGS_DECL );
	void HideCursor( SCALEFORM_CALLBACK_ARGS_DECL );

	void PlaySoundScaleform( SCALEFORM_CALLBACK_ARGS_DECL );
	void DisableAnalogStickNavigation( SCALEFORM_CALLBACK_ARGS_DECL );
	bool AnalogStickNavigationDisabled( void );
	
#if defined( _PS3 )
	void PS3UseMoveCursor( SCALEFORM_CALLBACK_ARGS_DECL );
	void PS3UseStandardCursor( SCALEFORM_CALLBACK_ARGS_DECL );
	void PS3ForceCursorStart( SCALEFORM_CALLBACK_ARGS_DECL );
	void PS3ForceCursorEnd( SCALEFORM_CALLBACK_ARGS_DECL );
#endif

};

class CursorSlot: public BaseSlot
{
	bool m_bUIHidden;

public:
	CursorSlot() :
		m_bUIHidden( true )
	{
	}

	void SetCursorShape( int shape );

	void Hide( void );
	void Show( void );

	virtual bool IsHidden( void )
	{
		return m_bUIHidden;
	}

	virtual bool IsVisible( void )
	{
		return !m_bUIHidden;
	}

};

#endif /* MOVIESLOT_H_ */
