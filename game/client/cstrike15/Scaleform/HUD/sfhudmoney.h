//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#ifndef SFHUDMONEY_H_
#define SFHUDMONEY_H_

#include "hud.h"
#include "hud_element_helper.h"
#include "scaleformui/scaleformui.h"
#include "sfhudflashinterface.h"
#include "c_cs_hostage.h"


class SFHudMoney : public SFHudFlashInterface
{
public:
	explicit SFHudMoney( const char *value );
	virtual ~SFHudMoney();

	// These overload the CHudElement class
	virtual void ProcessInput( void );
	virtual void LevelInit( void );
	virtual void LevelShutdown( void );
	virtual void SetActive( bool bActive );
	virtual void Init( void );
	virtual bool ShouldDraw( void );

	// these overload the ScaleformFlashInterfaceMixin class	
	virtual void FlashReady( void );
	virtual bool PreUnloadFlash( void );	
	
	void Show( bool show );

	bool MsgFunc_AdjustMoney( const CCSUsrMsg_AdjustMoney &msg );

	void UpdateMoneyChange( int nDelta = 0 );

	void DoneAnimatingAdd( SCALEFORM_CALLBACK_ARGS_DECL );
	void DoneAnimatingSub( SCALEFORM_CALLBACK_ARGS_DECL );
	
	CUserMessageBinder m_UMCMsgAdjustMoney;
	
private:
	void UpdateCurrentMoneyText();

	int m_nMoneyChange;

	int m_nLastMoney;
	int	m_lastEntityIndex;

	int m_nShiftState;
	bool m_bShowBuyZoneIcon;

	ISFTextObject * m_hCash;
	ISFTextObject * m_hAddCash;
	ISFTextObject * m_hRemoveCash;
	ISFTextObject * m_hBuyZoneIcon;

	CUtlVector<int> m_cashAdjustmentQueue;

	bool m_bAnimatingAdd;
	bool m_bAnimatingSub;

};

#endif /* SFHUDMONEY_H_ */
