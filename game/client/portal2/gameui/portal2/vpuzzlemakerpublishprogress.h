//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#ifndef __VPUZZLEMAKERPUBLISHPROGRESS_H__
#define __VPUZZLEMAKERPUBLISHPROGRESS_H__

#if defined( PORTAL2_PUZZLEMAKER )

#include "basemodui.h"
#include "steam/steam_api.h"
#include "puzzlemaker/puzzlemaker.h"

namespace BaseModUI
{
	void PuzzleMakerPublishScreenshotCallback( const char *pszScreenshotName );

class CPuzzleMakerPublishProgress : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( CPuzzleMakerPublishProgress, CBaseModFrame );

public:
	CPuzzleMakerPublishProgress( vgui::Panel *pParent, const char *pPanelName );

	void BeginPublish( ERemoteStoragePublishedFileVisibility eVisibilty );

protected:
	virtual void OnThink();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme );
	virtual void OnKeyCodePressed( vgui::KeyCode code );

private:
	static void ConfirmPublishFailure_Callback();
	static void ConfirmPublishFailureGoBack_Callback();

	void SuccessDialog( const PublishedFileId_t& id );
	void UpdateFooter();
	void CancelButtonPressed();

	void CancelPublish();
	void CloseDialog();

	void PublishingError( const wchar_t *szError, bool bCloseAll = true );
	bool CheckPublishingError( EResult eResult, bool bError );

	//Publishing functions
	bool PublishFile();
	bool PrepFile( const char *pszFileName );
	void GetCloudFilename( const char *lpszIn, char *lpszOut, int nOutSize );
	CCallResult<CPuzzleMakerPublishProgress, RemoteStoragePublishFileResult_t> m_callbackPublishFile;
	void Steam_OnPublishFile( RemoteStoragePublishFileResult_t *pResult, bool bError );
	CCallResult<CPuzzleMakerPublishProgress, RemoteStorageUpdatePublishedFileResult_t> m_callbackUpdateFile;
	void Steam_OnUpdateFile( RemoteStorageUpdatePublishedFileResult_t *pResult, bool bError );

	vgui::ImagePanel *m_pSpinner;
	PuzzleFilesInfo_t m_PublishInfo;
	ERemoteStoragePublishedFileVisibility m_eVisiblity;

	friend void PuzzleMakerPublishScreenshotCallback( const char *pszScreenshotName );
};

};

#endif //PORTAL2_PUZZLEMAKER

#endif //__VPUZZLEMAKERPUBLISHPROGRESS_H__
