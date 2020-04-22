//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#if defined( INCLUDE_SCALEFORM )

#if !defined( __MOTION_CALIBRATION_SCALEFORM_H__ )
#define __MOTION_CALIBRATION_SCALEFORM_H__
#ifdef _WIN32
#pragma once
#endif


enum InputDisplayAngle_t
{
	INPUT_DISPLAY_TOP_LEFT = 0,
	INPUT_DISPLAY_BOTTOM_RIGHT,

	INPUT_DISPLAY_COUNT,
};


#include "scaleformui/scaleformui.h"

class CMotionCalibrationScaleform : public ScaleformFlashInterface
{
private:
	static CMotionCalibrationScaleform *m_pInstance;

public:
	static void LoadDialog( void );
	static void UnloadDialog( void );

	static bool IsActive() { return m_pInstance != NULL; }
	static bool IsVisible() { return ( m_pInstance != NULL && m_pInstance->m_bVisible ); }

	void OnCancel( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnAccept( SCALEFORM_CALLBACK_ARGS_DECL );
	void TimerCallback( SCALEFORM_CALLBACK_ARGS_DECL );

protected:
	CMotionCalibrationScaleform( );
	virtual ~CMotionCalibrationScaleform( );

	/************************************************************
	 *  Flash Interface methods
	 */

	virtual void FlashReady( void );
	virtual void FlashLoaded( void );

	virtual bool PreUnloadFlash( void );
	virtual void PostUnloadFlash( void );

	void Show( void );
	void Hide( void );

	bool TryAdvance( bool bAccept, bool bCancel );
	void SceneAdvance( void );
	void SceneDraw( int nSceneLevel );
	void SceneThink( void );	

protected:
	enum SceneType_t
	{
		SCENE_START = 0,
		SCENE_CONNECT_EYE,
		SCENE_CONNECT_CONTROLLER,
		SCENE_CALIBRATE_CONTROLLER,
		SCENE_CALIBRATE_CONTROLLER_RESULT,
		SCENE_TARGET_TOP_LEFT,
		SCENE_TARGET_BOTTOM_RIGHT,
		SCENE_TARGET_BOTTOM_LEFT,
		SCENE_TARGET_TOP_RIGHT,
		
		SCENE_ADJUST_SENSITIVITY,
		SCENE_END,
	};

	int						m_iSplitScreenSlot;		// the splitscreen slot that launched the dialog
	bool					m_bVisible;				// Visibility flag
	bool					m_bLoading;				// Loading flag

	ISFTextObject *			m_pInfoText;
	ISFTextObject *			m_pNavText;
	SFVALUE					m_pTargetTopLeft;
	SFVALUE					m_pTargetBottomLeft;
	SFVALUE					m_pTargetTopRight;
	SFVALUE					m_pTargetBottomRight;
	SFVALUE					m_pSensitivity;

	int						m_nSceneLevel;
	bool					m_bCursorVisible;

	//QAngle		m_rgDisplayAngles[INPUT_DISPLAY_COUNT];
};

#endif // __MOTION_CALIBRATION_SCALEFORM_H__

#endif // INCLUDE_SCALEFORM
