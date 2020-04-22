//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#if defined( INCLUDE_SCALEFORM )

#if !defined( __OPTIONS_SCALEFORM_H__ )
#define __OPTIONS_SCALEFORM_H__
#ifdef _WIN32
#pragma once
#endif

#include "messagebox_scaleform.h"
#include "GameEventListener.h"

#define SF_OPTIONS_MAX 64
#define SF_OPTIONS_TOOLTIP_MAX 1024
#define SF_OPTIONS_SLOTS_COUNT 20
#define SF_OPTIONS_SLOTS_COUNT_MAX 60

typedef ScaleformFlashInterfaceMixin<CGameEventListener> CControlsFlashBaseClass;

class COptionsScaleform : public CControlsFlashBaseClass,  public IMessageBoxEventCallback, public IMatchEventsSink
{
private:
	static COptionsScaleform *m_pInstanceOptions;

public:
	enum DialogType_e
	{
		// must match action script MainUI.OptionsDialog.Options DIALOG_MODE_X
		DIALOG_TYPE_NONE = -1,
		DIALOG_TYPE_MOUSE,
		DIALOG_TYPE_KEYBOARD = DIALOG_TYPE_MOUSE,
		DIALOG_TYPE_CONTROLLER,
		DIALOG_TYPE_SETTINGS,
		DIALOG_TYPE_MOTION_CONTROLLER,
		DIALOG_TYPE_MOTION_CONTROLLER_MOVE,
		DIALOG_TYPE_MOTION_CONTROLLER_SHARPSHOOTER,
		DIALOG_TYPE_VIDEO,
		DIALOG_TYPE_VIDEO_ADVANCED,
		DIALOG_TYPE_AUDIO,
		DIALOG_TYPE_SCREENSIZE,

		DIALOG_TYPE_COUNT
	};

	struct DialogQueue_t
	{
		DialogQueue_t()
		{
			m_Type = DIALOG_TYPE_NONE;
			m_strMessage.Clear();
		}

		DialogType_e	m_Type;
		CUtlString		m_strMessage;	// Any special message that should be passed to the queued dialog
	};

	enum NoticeType_e
	{
		NOTICE_TYPE_NONE = -1,
		NOTICE_TYPE_RESET_TO_DEFAULT,
		NOTICE_TYPE_DISCARD_CHANGES,
		NOTICE_TYPE_INFO
	};

	enum OptionType_e
	{
		OPTION_TYPE_SLIDER = 0,
		OPTION_TYPE_CHOICE,
		OPTION_TYPE_DROPDOWN,
		OPTION_TYPE_BIND,
		OPTION_TYPE_BIND_KEYBOARD,
		OPTION_TYPE_CATEGORY,

		OPTION_TYPE_TOTAL
	};

	struct Option_t
	{
		Option_t()
		{
			m_Type = OPTION_TYPE_TOTAL;
			m_nPriority = 0;
			m_nWidgetSlotID = 0;

			V_memset( m_wcLabel, 0, sizeof( m_wcLabel ) );
			V_memset( m_wcTooltip, 0, sizeof( m_wcTooltip ) );	
			V_memset( m_szConVar, 0, sizeof( m_szConVar ) );
		}

		virtual ~Option_t() {}

		OptionType_e	m_Type;									// The type of option
		int				m_nWidgetSlotID;						// Widget slot this option occupies
		int				m_nPriority;							// Display order
		bool			m_bSystemValue;							// Ignore split screen # when setting this
		bool			m_bRefreshInventoryIconsWhenIncreased;	// Only works with advanced video options right now!
		wchar_t			m_wcLabel[ SF_OPTIONS_MAX ];		// Display label
		wchar_t			m_wcTooltip[SF_OPTIONS_TOOLTIP_MAX];		// Tooltip label
		
		char			m_szConVar[ SF_OPTIONS_MAX ];	// ConVar (if any) the option is bound to
	};

	struct OptionChoiceData_t
	{
		wchar_t	m_wszLabel[ SF_OPTIONS_MAX ];
		char	m_szValue[ SF_OPTIONS_MAX ];
	};

	enum BindCommands_e
	{
		BIND_CMD_BIND = 0,	// binds the last pressed key to the command
		BIND_CMD_CLEAR,		// unbinds the command

		BIND_CMD_TOTAL
	};

	struct OptionBind_t : public Option_t
	{
		OptionBind_t()
		{
			V_memset( m_szCommand, 0, sizeof( m_szCommand ) );
		}

		char	m_szCommand[ SF_OPTIONS_MAX ];	// Command the option is bound to
	};

	struct OptionChoice_t : public Option_t
	{
		OptionChoice_t()
		{
			m_nChoiceIndex = -1;
		}

		int		m_nChoiceIndex;						// Index of currently selected choice
		CUtlVector<OptionChoiceData_t>	m_Choices;	// Available choices for choice widgets
	};

	struct OptionDropdown_t : public Option_t
	{
		OptionDropdown_t( )
		{
			m_nChoiceIndex = -1;
		}

		int		m_nChoiceIndex;						// Index of currently selected choice
		CUtlVector<OptionChoiceData_t>	m_Choices;	// Available choices for choice widgets
	};

	struct OptionSlider_t : public Option_t
	{
		OptionSlider_t()
		{
			m_fMinValue = 0.0f;
			m_fMaxValue = 0.0f;
			m_fSlideValue = 0.0f;
			m_bLeftMin = true;
		}

		float	m_fMinValue;						// Lower bound
		float	m_fMaxValue;						// Upper bound
		float	m_fSlideValue;						// Current slide value
		bool	m_bLeftMin;							// Configures the slider so the far left = m_fMinValue and the far right = m_fMaxValue
	};

	// Destruction
	static void UnloadDialog( void );

	/********************************************
	* IMessageBoxEventCallback implementation
	*/
	virtual bool OnMessageBoxEvent( MessageBoxFlags_t buttonPressed );

	// IMatchEventSink
	virtual void OnEvent( KeyValues *kvEvent ) OVERRIDE;

	/************************************
	 * callbacks from scaleform
	 */
	void OnCancel( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnUpdateValue( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnHighlightWidget( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnLayoutComplete( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnPopulateGlyphRequest( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnClearBind( SCALEFORM_CALLBACK_ARGS_DECL );
	virtual void OnResetToDefaults( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnRequestScroll( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnResizeVertical( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnResizeHorizontal( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnSetSizeVertical( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnSetSizeHorizontal( SCALEFORM_CALLBACK_ARGS_DECL );	
	void OnSetNextMenu( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnApplyChanges( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnSetupMic( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnSaveProfile( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnMCCalibrate( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnRefreshValues( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetTotalOptionsSlots( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetCurrentScrollOffset( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetSafeZoneXMin( SCALEFORM_CALLBACK_ARGS_DECL );

	static void ShowMenu( bool bShow, DialogType_e type );
	static bool IsActive() { return m_pInstanceOptions != NULL; }
	static bool IsVisible() { return ( m_pInstanceOptions != NULL && m_pInstanceOptions->m_bVisible ); }

	// Notify the current dialog that the Start or Escape key has been pressed
	static void NotifyStartEvent();

	// CGameEventListener callback
	virtual void FireGameEvent( IGameEvent *event ) {}

	static bool IsBindMenuRaised( void );


protected:
	COptionsScaleform( );
	virtual ~COptionsScaleform( );

	// Construction
	static void LoadDialog( DialogType_e type );

	static int GetDeviceFromDialogType( DialogType_e eDialogType );

	/************************************************************
	 *  Flash Interface methods
	 */

	virtual void FlashReady( void );
	virtual void FlashLoaded( void );

	virtual bool PreUnloadFlash( void );
	virtual void PostUnloadFlash( void );

	void Show( void );
	void Hide( void );

	// fills a choice vector with clan tag labels
	void BuildClanTagsLabels( CUtlVector<OptionChoiceData_t> &choices );	

	// Reads in data that drives the dialog layout and functionality
	void ReadOptionsFromFile( const char * szFileName );

	// Reads options from m_vecOptions and attaches relevant label and widget to the dialog
	// beginning at nVecOptionsOffset.
	void LayoutDialog( const int nVecOptionsOffset, const bool bInit = true );

	// Updates the widget at nWidgetIndex with the data from option
	void UpdateWidget( const int nWidgetIndex,  Option_t const * const option );
	 
	// Sets the option to whatever the current ConVar value
	// bForceDefaultValue signals that unique algorithms should be used to select the value
	virtual void SetChoiceWithConVar( OptionChoice_t * pOption, bool bForceDefaultValue = false );

	// Sets the option to whatever the current ConVar value 
	void SetSliderWithConVar( OptionSlider_t * pOption );

	// Returns true if the two passed in actions are identical
	bool ActionsAreTheSame( const char *szAction1, const char *szAction2 );

	// Resets all options and binds to their default values
	virtual void ResetToDefaults( void );

	// Unbind the action associated with the option from its key
	void UnbindOption( OptionBind_t const * const pOptionBind );

	// Notify the current dialog that the Start or Escape key has been pressed
	void OnNotifyStartEvent( void );

	// Returns the choice ID that matches szMatch
	int FindChoiceFromString( OptionChoice_t * pOption, const char * szMatch );

	// Applues change to a system convar (SplitScreenConVarRef slot 0)
	void ApplyChangesToSystemConVar( const char *pConVarName, int value );

	// saves out profile, restarts renderer etc
	void SaveChanges( void );

	// Invoked before changes are saved
	virtual void PreSaveChanges( void ) {}

	// returns true if supplied convar contains "_restart", outputs split ConVar
	bool SplitRestartConvar( const char * szConVarRestartIn, char * szConVarOut, int nOutLength );

	// Initializes widgets that require unique config steps (filling the resolution widget etc.)
	virtual bool InitUniqueWidget( const char * szWidgetID, OptionChoice_t * pOptionChoice  );


	void DisableConditionalWidgets();

	// Disable widgets based on current state (brightness not available in windowed mode, etc)
	virtual void HandleDisableConditionalWidgets( Option_t * pOption, int & nWidgetIDOut, bool & bDisableOut );

	bool UpdateValue( int nWidgetIndex, int nValue );

	// For Dialog specific updates to choice widgets
	virtual bool HandleUpdateChoice( OptionChoice_t * pOptionChoice, int nCurrentChoice );

	// Called after the dialog has successfully refreshed its widgets
	virtual void PerformPostLayout( void );

	// Refreshes all widgets, then redraws dialog
	void RefreshValues( bool bForceDefault );	

	// Save the values to the user's profile
	void WriteUserSettings( int iSplitScreenSlot );

	static bool IsMotionControllerDialog( void );

protected:
	static DialogType_e		m_DialogType;			// The current dialog

	static CUtlString		m_strMessage;			// Special behavior message

	CMessageBoxScaleform*	m_pConfirmDialog;		// Reset to default dialog
	int						m_iSplitScreenSlot;		// the splitscreen slot that launched the dialog
	
	bool					m_bVisible;				// Visibility flag
	bool					m_bLoading;				// Loading flag
	bool					m_bNavButtonsEnabled;	// true when both nav buttons are enabled
	bool					m_bOptionsChanged;		// Flag set when options are changed and need to be saved
	bool					m_bResetRequired;		// Flag set when options are changed that require the renderer to be reset
	NoticeType_e			m_NoticeType;			// The active message box notice

	int						m_nScrollPos;			// Index of topmost visible element
	CUtlVector<Option_t *>	m_vecOptions;			// Vector of the options, sorted by order of display

	Option_t *				m_rgOptionsBySlot[ SF_OPTIONS_SLOTS_COUNT_MAX ];	// Pointer to the option assigned to a specified slot
	ISFTextObject *			m_rgTextBySlot[ SF_OPTIONS_SLOTS_COUNT_MAX ];		// Pointer to the text label for each slot

	static CUtlQueue<DialogQueue_t>	m_DialogQueue;			// Dialogs to be popped and displayed after the current dialog has been destroyed

	SFVALUE					m_pDeadZonePanel;
};

#endif // __OPTIONS_SCALEFORM_H__

#endif // INCLUDE_SCALEFORM
