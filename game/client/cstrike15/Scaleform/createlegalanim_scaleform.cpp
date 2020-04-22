//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: [jpaquin] Plays the intro movie before the start screen / main menu
//
// This also plays the credits movie.
//
//=============================================================================//
#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )

#include "basepanel.h"
#include "createlegalanim_scaleform.h"
#include "engineinterface.h"
#include "gameui_interface.h"
#include "engine/IEngineSound.h"
#include "cdll_client_int.h"


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

const int MAX_SOUND_NAME_CHARS = 1024;

CCreateLegalAnimScaleform* CCreateLegalAnimScaleform::m_pInstance = NULL;

SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD( AnimationCompleted ),
	SFUI_DECL_METHOD( PlayAudio ),
	SFUI_DECL_METHOD( GetRatingsBoardForLegals ),
SFUI_END_GAME_API_DEF( CCreateLegalAnimScaleform, LegalAnimation )
;


void CCreateLegalAnimScaleform::CreateIntroMovie( void )
{
	if ( !m_pInstance )
	{
		m_pInstance = new CCreateLegalAnimScaleform( "FinishedIntroMovie" );
		SFUI_REQUEST_ELEMENT( SF_FULL_SCREEN_SLOT, g_pScaleformUI, CCreateLegalAnimScaleform, m_pInstance, LegalAnimation );
	}

}

void CCreateLegalAnimScaleform::CreateCreditsMovie( void )
{
	if ( !m_pInstance )
	{
		m_pInstance = new CCreateLegalAnimScaleform( "RestoreTopLevelMenu" );

        // this is the expansion of the SFUI_REQUEST_ELEMENT macro but modified so we can use the LegalAnimation GAME_API table defined above
		g_pScaleformUI->RequestElement( SF_FULL_SCREEN_SLOT, "CreditsAnimation", reinterpret_cast<ScaleformUIFunctionHandlerObject*>( m_pInstance ), SFUI_OBJ_PTR_NAME( CCreateLegalAnimScaleform, LegalAnimation) );
	}

}


void CCreateLegalAnimScaleform::UnloadDialog( void )
{
	if ( m_pInstance )
	{
		m_pInstance->RemoveFlashElement();
	}
}

void CCreateLegalAnimScaleform::DismissAnimation( void )
{
    if ( m_pInstance )
    {
        m_pInstance->InnerDismissAnimation();
    }
}

CCreateLegalAnimScaleform::CCreateLegalAnimScaleform( const char* commandNameOrNULL ) :
    m_bAnimationCompleted( false ),
    m_iCurrentSoundPlaying( 0 )
{
    if ( commandNameOrNULL && *commandNameOrNULL )
    {
        V_strncpy( &m_MenuCommandToRunOnCompletion[0], commandNameOrNULL, ARRAYSIZE( m_MenuCommandToRunOnCompletion ) );
    }
    else
    {
        m_MenuCommandToRunOnCompletion[0] = 0;
    }
}

void CCreateLegalAnimScaleform::FinishAnimation( void )
{
    // releases the game to show the start screen or main menu
    m_bAnimationCompleted = true;
    if ( m_MenuCommandToRunOnCompletion[0] )
    {
        BasePanel()->PostMessage( BasePanel(), new KeyValues( "RunMenuCommand", "command", m_MenuCommandToRunOnCompletion ) );
    }
}

void CCreateLegalAnimScaleform::FlashLoadError( SCALEFORM_CALLBACK_ARGS_DECL )
{
    FinishAnimation();
    m_pInstance = NULL;
    delete this;
}


void CCreateLegalAnimScaleform::AnimationCompleted( SCALEFORM_CALLBACK_ARGS_DECL )
{
    FinishAnimation();
}

void CCreateLegalAnimScaleform::StopAudio( void )
{
    if ( m_iCurrentSoundPlaying )
    {
        enginesound->StopSoundByGuid( m_iCurrentSoundPlaying, true );
        m_iCurrentSoundPlaying = 0;
        // stop sounds
    }
}

void CCreateLegalAnimScaleform::PlayAudio( SCALEFORM_CALLBACK_ARGS_DECL )
{
    if ( g_pScaleformUI->Params_GetNumArgs( obj ) == 0 )
    {
        StopAudio();
    }
    else
    {
        const char *pSoundName = g_pScaleformUI->Params_GetArgAsString( obj, 0 );

        if ( pSoundName && *pSoundName )
        {
            StopAudio();

            bool addMixDry = true;
            bool addStream = true;
            int len = V_strlen( pSoundName );

            char decoratedSoundName[MAX_SOUND_NAME_CHARS];

            if ( len > 2 )
            {
                if ( pSoundName[0] == '*' || pSoundName[1] == '*' )
                {
                    addMixDry = false;
                }

                if ( pSoundName[0] == '#' || pSoundName[1] == '#' )
                {
                    addStream = false;
                }
            }

            int firstChar = 0;
            if ( addMixDry )
                decoratedSoundName[firstChar++] = '*';

            if ( addStream )
                decoratedSoundName[firstChar++] = '#';

            V_strncpy( decoratedSoundName, pSoundName, MAX_SOUND_NAME_CHARS - firstChar );
            decoratedSoundName[ MAX_SOUND_NAME_CHARS - 1 ] = 0;

            m_iCurrentSoundPlaying = enginesound->EmitAmbientSound( decoratedSoundName, 1.0f );
        }
    }
}

void CCreateLegalAnimScaleform::PostUnloadFlash( void )
{
	m_pInstance = NULL;
	delete this;
}

void CCreateLegalAnimScaleform::InnerDismissAnimation( void )
{
	if ( !FlashAPIIsValid() )	// safety check if overlay triggers some actions that need to dismiss this screen before SWF finished loading
		return;

    WITH_SFVALUEARRAY( args, 1 )
    {
#if defined( _CERT )
        g_pScaleformUI->ValueArray_SetElement( args, 0, true );
#else
        g_pScaleformUI->ValueArray_SetElement( args, 0, false );
#endif

        WITH_SLOT_LOCKED
        {
            ScaleformUI()->Value_InvokeWithoutReturn( m_FlashAPI, "dismissAnimation", args, 1 );
        }
    }
}

void CCreateLegalAnimScaleform::GetRatingsBoardForLegals( SCALEFORM_CALLBACK_ARGS_DECL )
{	
	const char* szRatingBoard = NULL;
#ifdef _X360
	if ( xboxsystem->IsArcadeTitleUnlocked() )
		szRatingBoard = "";
	else
		szRatingBoard = GetConsoleLocaleRatingsBoard();
#else
	szRatingBoard = "";
#endif

	if ( szRatingBoard )
	{
		SFVALUE result = CreateFlashString( szRatingBoard );
		m_pScaleformUI->Params_SetResult( obj, result );
		SafeReleaseSFVALUE( result );
	}
}

#endif // INCLUDE_SCALEFORM
