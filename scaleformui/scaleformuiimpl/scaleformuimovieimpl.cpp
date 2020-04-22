//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"

// NOTE: This must be the last file included!!!
#include "tier0/memdbgon.h"

using namespace SF::GFx;

// Some utility functions to map SFUI enums to SF enums

Movie::ScaleModeType ScaleformUIImpl::ScaleModeType_SFUI_to_SDK( IScaleformUI::_ScaleModeType scaleModeType )
{
	switch ( scaleModeType )
	{
	case IScaleformUI::SM_NoScale:	return Movie::SM_NoScale;
	case IScaleformUI::SM_ShowAll:	return Movie::SM_ShowAll;
	case IScaleformUI::SM_ExactFit: return Movie::SM_ExactFit;
	case IScaleformUI::SM_NoBorder: return Movie::SM_NoBorder;
	default: 
		AssertMsg( 0, "Unknown ScaleModeType\n");
		return Movie::SM_NoScale;		
	}
};

IScaleformUI::_ScaleModeType ScaleformUIImpl::ScaleModeType_SDK_to_SFUI( Movie::ScaleModeType scaleModeType )
{
	switch ( scaleModeType )
	{
	case Movie::SM_NoScale:		return IScaleformUI::SM_NoScale;
	case Movie::SM_ShowAll:		return IScaleformUI::SM_ShowAll;
	case Movie::SM_ExactFit:	return IScaleformUI::SM_ExactFit;
	case Movie::SM_NoBorder:	return IScaleformUI::SM_NoBorder;
	default: 
		AssertMsg( 0, "Unknown ScaleModeType\n");
		return IScaleformUI::SM_NoScale;
	}
};

Value::ValueType ScaleformUIImpl::ValueType_SFUI_to_SDK( IScaleformUI::_ValueType valueType )
{
	switch ( valueType )
	{
	case IScaleformUI::VT_Undefined :			return Value::VT_Undefined ;				
	case IScaleformUI::VT_Null :				return Value::VT_Null ;
	case IScaleformUI::VT_Boolean :				return Value::VT_Boolean ;
	case IScaleformUI::VT_Int :					return Value::VT_Int ;
	case IScaleformUI::VT_UInt :				return Value::VT_UInt ;
	case IScaleformUI::VT_Number :				return Value::VT_Number ;
	case IScaleformUI::VT_String :				return Value::VT_String ;
	case IScaleformUI::VT_StringW :				return Value::VT_StringW ;
	case IScaleformUI::VT_Object :				return Value::VT_Object ;
	case IScaleformUI::VT_Array :				return Value::VT_Array ;
	case IScaleformUI::VT_DisplayObject :		return Value::VT_DisplayObject ;
	case IScaleformUI::VT_Closure :				return Value::VT_Closure ;
	case IScaleformUI::VT_ConvertBoolean :		return Value::VT_ConvertBoolean ;
	case IScaleformUI::VT_ConvertInt :			return Value::VT_ConvertInt ;
	case IScaleformUI::VT_ConvertUInt :			return Value::VT_ConvertUInt ;
	case IScaleformUI::VT_ConvertNumber :		return Value::VT_ConvertNumber ;
	case IScaleformUI::VT_ConvertString :		return Value::VT_ConvertString ;
	case IScaleformUI::VT_ConvertStringW :		return Value::VT_ConvertStringW ;

	default: 
		AssertMsg( 0, "Unknown ValueType\n");
		return Value::VT_Undefined;
	}
}

IScaleformUI::_ValueType ScaleformUIImpl::ValueType_SDK_to_SFUI( Value::ValueType valueType )
{
	switch ( valueType )
	{
	case Value::VT_Undefined :			return IScaleformUI::VT_Undefined ;				
	case Value::VT_Null :				return IScaleformUI::VT_Null ;
	case Value::VT_Boolean :			return IScaleformUI::VT_Boolean ;
	case Value::VT_Int :				return IScaleformUI::VT_Int ;
	case Value::VT_UInt :				return IScaleformUI::VT_UInt ;
	case Value::VT_Number :				return IScaleformUI::VT_Number ;
	case Value::VT_String :				return IScaleformUI::VT_String ;
	case Value::VT_StringW :			return IScaleformUI::VT_StringW ;
	case Value::VT_Object :				return IScaleformUI::VT_Object ;
	case Value::VT_Array :				return IScaleformUI::VT_Array ;
	case Value::VT_DisplayObject :		return IScaleformUI::VT_DisplayObject ;
	case Value::VT_Closure :			return IScaleformUI::VT_Closure ;
	case Value::VT_ConvertBoolean :		return IScaleformUI::VT_ConvertBoolean ;
	case Value::VT_ConvertInt :			return IScaleformUI::VT_ConvertInt ;
	case Value::VT_ConvertUInt :		return IScaleformUI::VT_ConvertUInt ;
	case Value::VT_ConvertNumber :		return IScaleformUI::VT_ConvertNumber ;
	case Value::VT_ConvertString :		return IScaleformUI::VT_ConvertString ;
	case Value::VT_ConvertStringW :		return IScaleformUI::VT_ConvertStringW ;

	default: 
		AssertMsg( 0, "Unknown ValueType\n");
		return IScaleformUI::VT_Undefined;
	}
}

Movie::AlignType ScaleformUIImpl::AlignType_SFUI_to_SDK( IScaleformUI::_AlignType alignType )
{
	switch ( alignType )
	{
	case IScaleformUI::Align_Center:		return Movie::Align_Center;
	case IScaleformUI::Align_TopCenter:		return Movie::Align_TopCenter;
	case IScaleformUI::Align_BottomCenter:	return Movie::Align_BottomCenter;
	case IScaleformUI::Align_CenterLeft:	return Movie::Align_CenterLeft;
	case IScaleformUI::Align_CenterRight:	return Movie::Align_CenterRight;
	case IScaleformUI::Align_TopLeft:		return Movie::Align_TopLeft;
	case IScaleformUI::Align_TopRight:		return Movie::Align_TopRight;
	case IScaleformUI::Align_BottomLeft:	return Movie::Align_BottomLeft;
	case IScaleformUI::Align_BottomRight:	return Movie::Align_BottomRight;
	default: 
		AssertMsg( 0, "Unknown AlignType\n");
		return Movie::Align_Center;
	}
}

IScaleformUI::_AlignType ScaleformUIImpl::AlignType_SDK_to_SFUI( Movie::AlignType alignType )
{
	switch ( alignType )
	{
	case Movie::Align_Center:		return IScaleformUI::Align_Center;		
	case Movie::Align_TopCenter:		return IScaleformUI::Align_TopCenter;		
	case Movie::Align_BottomCenter:	return IScaleformUI::Align_BottomCenter;	
	case Movie::Align_CenterLeft:	return IScaleformUI::Align_CenterLeft;	
	case Movie::Align_CenterRight:	return IScaleformUI::Align_CenterRight;	
	case Movie::Align_TopLeft:		return IScaleformUI::Align_TopLeft;		
	case Movie::Align_TopRight:		return IScaleformUI::Align_TopRight;		
	case Movie::Align_BottomLeft:	return IScaleformUI::Align_BottomLeft;	
	case Movie::Align_BottomRight:	return IScaleformUI::Align_BottomRight;	
	default: 
		AssertMsg( 0, "Unknown AlignType\n");
		return IScaleformUI::Align_Center;
	}
}

Movie::HitTestType ScaleformUIImpl::HitTestType_SFUI_to_SDK( IScaleformUI::_HitTestType hitTestType )
{
	switch ( hitTestType )
	{
	case IScaleformUI::HitTest_Bounds:				return Movie::HitTest_Bounds;
	case IScaleformUI::HitTest_Shapes:				return Movie::HitTest_Shapes;
	case IScaleformUI::HitTest_ButtonEvents:		return Movie::HitTest_ButtonEvents;
	case IScaleformUI::HitTest_ShapesNoInvisible:	return Movie::HitTest_ShapesNoInvisible;
	default: 
		AssertMsg( 0, "Unknown HitTestType\n");
		return Movie::HitTest_Bounds;
	}
}

IScaleformUI::_HitTestType ScaleformUIImpl::HitTestType_SDK_to_SFUI( Movie::HitTestType hitTestType )
{
	switch ( hitTestType )
	{
	case Movie::HitTest_Bounds:				return IScaleformUI::HitTest_Bounds;		
	case Movie::HitTest_Shapes:				return IScaleformUI::HitTest_Shapes;		
	case Movie::HitTest_ButtonEvents:		return IScaleformUI::HitTest_ButtonEvents;	
	case Movie::HitTest_ShapesNoInvisible:	return IScaleformUI::HitTest_ShapesNoInvisible;	
	default: 
		AssertMsg( 0, "Unknown HitTestType\n");
		return IScaleformUI::HitTest_Bounds;
	}
}


class ScaleformMovieDefHolder: public ASUserData
{
public:
	// this is a weak pointer
	MovieDef* m_pMovieDef;

	inline ScaleformMovieDefHolder( MovieDef *pdef ) :
		ASUserData(), m_pMovieDef( pdef )
	{
	}

	virtual void OnDestroy( Movie* pmovie, void* pobject )
	{
		Release();
	}
};

void ScaleformUIImpl::InitMovieImpl( void )
{
}

void ScaleformUIImpl::ShutdownMovieImpl( void )
{
#if defined( _DEBUG )
	if ( m_MovieViews.Count() )
	{
		LogPrintf( "Scaleform: Some movie views were not released\n" );

		for ( int i = 0; i < m_MovieViews.Count(); i++ )
		{
			ASUserData* pud = ( ASUserData* )m_MovieViews[i]->GetUserData();
			ScaleformMovieDefHolder* pDef = ( ScaleformMovieDefHolder* ) pud;

			if ( pDef )
			{
				LogPrintf( "  %s\n", pDef->m_pMovieDef->GetFileURL() );
			}
			else
			{
				LogPrintf( "  unknown slot movie\n" );
			}

			if ( pud )
			{
				pud->OnDestroy( m_MovieViews[i],NULL );
			}

			m_MovieViews[i]->SetUserData( NULL );
		}

	}

	if ( m_MovieDefNameCache.Count() )
	{
		LogPrintf( "Scaleform: Some movie defs were not released\n" );

		for ( int i = 0; i < m_MovieDefNameCache.Count(); i++ )
		{
			LogPrintf( "  %s\n", m_MovieDefNameCache[i] );
		}

	}
#endif

}

SFMOVIEDEF ScaleformUIImpl::CreateMovieDef( const char* pfilename, unsigned int loadConstants, size_t memoryArena )
{
	// first see if we've already got the guy loaded

	MovieDef* presult = NULL;

	for ( int i = 0; i < m_MovieDefNameCache.Count(); i++ )
	{
		if ( !V_stricmp( m_MovieDefNameCache[i], pfilename ) )
		{
			presult = m_MovieDefCache[i];
			presult->AddRef();
			break;
		}

	}

	if ( presult == NULL )
	{
		presult = m_pLoader->CreateMovie( pfilename, loadConstants, memoryArena );
		m_MovieDefCache.AddToTail( presult );
		char* newString = new char[V_strlen( pfilename )+1];
		V_strcpy( newString, pfilename );
		m_MovieDefNameCache.AddToTail( newString );
	}

	return ToSFMOVIEDEF( presult );
}

void ScaleformUIImpl::ReleaseMovieDef( SFMOVIEDEF movieDef )
{
	MovieDef* pdef = FromSFMOVIEDEF( movieDef );
	int refcount = pdef->GetRefCount();
	pdef->Release();

	// we actually removed this
	if ( refcount == 1 )
	{
		for ( int i = 0; i < m_MovieDefCache.Count(); i++ )
		{
			if ( m_MovieDefCache[i] == pdef )
			{
				delete[] m_MovieDefNameCache[i];
				m_MovieDefCache.FastRemove( i );
				m_MovieDefNameCache.FastRemove( i );
				break;
			}
		}
	}
}

SFMOVIE ScaleformUIImpl::MovieDef_CreateInstance( SFMOVIEDEF movieDef, bool initFirstFrame, size_t memoryArena )
{
	MovieDef* pDef = FromSFMOVIEDEF( movieDef );
	Movie* pView = pDef->CreateInstance( initFirstFrame, memoryArena, 0, m_pThreadCommandQueue );

	if ( pView )
	{
#if defined( _DEBUG )
		// only need this so we can print the name of the movie if we don't release it
		ScaleformMovieDefHolder* def = new ScaleformMovieDefHolder( pDef );
		pView->SetUserData( def );
#endif // _DEBUG
		m_MovieViews.AddToTail( pView );
	}

	return ToSFMOVIE( pView );
}

void ScaleformUIImpl::ReleaseMovieView( SFMOVIE movieView )
{
	if ( movieView )
	{
		m_MovieViews.FindAndFastRemove( FromSFMOVIE( movieView ) );

#if defined( _DEBUG )
		ScaleformMovieDefHolder* def = ( ScaleformMovieDefHolder* )FromSFMOVIE( movieView )->GetUserData();

		if ( def )
		{
			def->OnDestroy( FromSFMOVIE( movieView ), NULL );
		}

		FromSFMOVIE( movieView )->SetUserData( NULL );
#endif
		// This needs to execute on the render thread via a command queue. It only works here
		// because typically when we free movies either QMS is off, or the qms is not very active
		// because we are loading/unloading maps
		Movie *pMovie = FromSFMOVIE( movieView );

		pMovie->ShutdownRendering( false );

		while ( !pMovie->IsShutdownRenderingComplete() )
		{
			if ( m_pRenderer2D->GetContextNotify() )
			{
				MovieDisplayHandle hMovieDisplay = pMovie->GetDisplayHandle();

				hMovieDisplay.NextCapture( m_pRenderer2D->GetContextNotify() );

				m_pRenderer2D->GetContextNotify()->EndFrameContextNotify();
			}
		}

		pMovie->Release();
	}
}

void ScaleformUIImpl::MovieView_Advance( SFMOVIE movieView, float time, unsigned int frameCatchUpCount )
{
	FromSFMOVIE( movieView )->Advance( time, frameCatchUpCount );
}

void ScaleformUIImpl::MovieView_SetBackgroundAlpha( SFMOVIE movieView, float alpha )
{
	FromSFMOVIE( movieView )->SetBackgroundAlpha( alpha );
}

void ScaleformUIImpl::MovieView_SetViewport( SFMOVIE movieView, int bufw, int bufh, int left, int top, int w, int h, unsigned int flags )
{
	FromSFMOVIE( movieView )->SetViewport( bufw, bufh, left, top, w, h, flags );
}

void ScaleformUIImpl::MovieView_Display( SFMOVIE movieView )
{
	MovieDisplayHandle hMovieDisplay = FromSFMOVIE( movieView )->GetDisplayHandle();

	if ( hMovieDisplay.NextCapture( m_pRenderer2D->GetContextNotify() ) )
	{
		m_pRenderer2D->Display( hMovieDisplay );
	}
}

/***
 * view alignment and scaling
 */

void ScaleformUIImpl::MovieView_SetViewScaleMode( SFMOVIE movieView, IScaleformUI::_ScaleModeType type )
{
	FromSFMOVIE( movieView )->SetViewScaleMode( ScaleModeType_SFUI_to_SDK( type ) );
}

IScaleformUI::_ScaleModeType ScaleformUIImpl::MovieView_GetViewScaleMode( SFMOVIE movieView )
{
	return ScaleModeType_SDK_to_SFUI( FromSFMOVIE( movieView )->GetViewScaleMode() );
}

void ScaleformUIImpl::MovieView_SetViewAlignment( SFMOVIE movieView, IScaleformUI::_AlignType type )
{
	FromSFMOVIE( movieView )->SetViewAlignment( AlignType_SFUI_to_SDK( type ) );
}

IScaleformUI::_AlignType ScaleformUIImpl::MovieView_GetViewAlignment( SFMOVIE movieView )
{
	return AlignType_SDK_to_SFUI( FromSFMOVIE( movieView )->GetViewAlignment() );
}

/***************************
 * values
 */

SFVALUE ScaleformUIImpl::MovieView_CreateString( SFMOVIE movieView, const char *str )
{
	Value* pResult = FromSFVALUE( CreateGFxValue() );

	FromSFMOVIE( movieView )->CreateString( pResult, str );

	return ToSFVALUE( pResult );
}

SFVALUE ScaleformUIImpl::MovieView_CreateStringW( SFMOVIE movieView, const wchar_t *str )
{
	Value* pResult = FromSFVALUE( CreateGFxValue() );

	FromSFMOVIE( movieView )->CreateStringW( pResult, str );

	return ToSFVALUE( pResult );
}

SFVALUE ScaleformUIImpl::MovieView_CreateObject( SFMOVIE movieView, const char* className, SFVALUEARRAY args, int numArgs )
{
	Assert(numArgs == args.Count());
	if ( numArgs != args.Count() )
	{
		Warning("MovieView_CreateObject(%s) called with numArgs different than SFVALUEARRAY size\n", className);
	}

	Value* pResult = FromSFVALUE( CreateGFxValue() );

	FromSFMOVIE( movieView )->CreateObject( pResult, className, FromSFVALUE( args.GetValues() ), args.Count() );

	return ToSFVALUE( pResult );
}

SFVALUE ScaleformUIImpl::MovieView_CreateArray( SFMOVIE movieView, int size )
{
	Value* pResult = FromSFVALUE( CreateGFxValue() );

	FromSFMOVIE( movieView )->CreateArray( pResult );

	if ( size != -1 )
		pResult->SetArraySize( size );

	return ToSFVALUE( pResult );
}

SFVALUE ScaleformUIImpl::MovieView_GetVariable( SFMOVIE movieView, const char* variablePath )
{
	Value var;
	Value* pResult = NULL;
	bool worked = FromSFMOVIE( movieView )->GetVariable( &var, variablePath );

	if ( worked )
	{
		pResult = FromSFVALUE( CreateGFxValue( ToSFVALUE( &var ) ) );
	}

	return ToSFVALUE( pResult );
}


/***********
 * hit testing
 */

bool ScaleformUIImpl::MovieView_HitTest( SFMOVIE movieView, float x, float y, IScaleformUI::_HitTestType testCond, unsigned int controllerIdx )
{
	return FromSFMOVIE( movieView )->HitTest( x, y, HitTestType_SFUI_to_SDK(testCond), controllerIdx );
}

