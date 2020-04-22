//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// 4-23-98  
// JOHN:  implementation of interface between client-side DLL and game engine.
//  The cdll shouldn't have to know anything about networking or file formats.
//  This file is Win32-dependant
//
//===========================================================================//

#include "client_pch.h"
#include "getintersectingsurfaces_struct.h"
#include "gl_model_private.h"
#include "surfinfo.h"
#include "vstdlib/random.h"
#include "cdll_int.h"
#include "cmodel_engine.h"
#include "tmessage.h"
#include "console.h"
#include "snd_audio_source.h"
#include <vgui_controls/Controls.h>
#include <vgui/IInput.h>
#include "iengine.h"
#include "keys.h"
#include "con_nprint.h"
#include "tier0/vprof.h"
#include "sound.h"
#include "gl_rmain.h"
#include "client_class.h"
#include "gl_rsurf.h"
#include "server.h"
#include "r_local.h"
#include "lightcache.h"
#include "gl_matsysiface.h"
#include "inputsystem/iinputstacksystem.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/materialsystem_config.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "istudiorender.h"
#include "l_studio.h"
#include "voice.h"
#include "enginestats.h"
#include "testscriptmgr.h"
#include "r_areaportal.h"
#include "host.h"
#include "vox.h"
#include "iprediction.h"
#include "icliententitylist.h"
#include "eiface.h"
#include "engine/IClientLeafSystem.h"
#include "dt_recv_eng.h"
#include <vgui/IVGui.h>
#include "sys_dll.h"
#include "vphysics_interface.h"
#include "materialsystem/imesh.h"
#include "IOcclusionSystem.h"
#include "filesystem_engine.h"
#include "tier0/icommandline.h"
#include "client_textmessage.h"
#include "host_saverestore.h"
#include "cl_main.h"
#include "demo.h"
#include "vgui_baseui_interface.h"
#include "LocalNetworkBackdoor.h"
#include "lightcache.h"
#include "vgui/ISystem.h"
#include "Steam.h"
#include "ivideomode.h"
#include "icolorcorrectiontools.h"
#include "toolframework/itoolframework.h"
#include "engine/view_sharedv1.h"
#include "view.h"
#include "game/client/iclientrendertargets.h"
#include "tier2/tier2.h"
#include "inputsystem/iinputsystem.h"
#include "iachievementmgr.h"
#include "profile.h"
#include "singleplayersharedmemory.h"
#include "cl_demo.h"
#include "MapReslistGenerator.h"
#include "iclientalphaproperty.h"
#include "cl_steamauth.h"
#include "enginebugreporter.h"
#include "world.h"
#include "staticpropmgr.h"
#include "sv_uploadgamestats.h"
#include "host_cmd.h"
#include "ixboxsystem.h"
#include "matchmaking/imatchframework.h"
#include "appframework/ilaunchermgr.h"
#include "paint.h"
#include "igame.h"
#include "sys_mainwind.h"
#include "dbginput.h"
#include "cl_broadcast.h"

#if defined( _PS3 )
#include "engine_helper_ps3.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
IMaterial* BrushModel_GetLightingAndMaterial( const Vector &start, 
	const Vector &end, Vector &diffuseLightColor, Vector &baseColor );
const char *Key_NameForBinding( const char *pBinding, int userId );
void CL_GetBackgroundLevelName( char *pszBackgroundName, int bufSize, bool bMapName );
CreateInterfaceFn g_ClientFactory = NULL;
extern	CGlobalVars g_ServerGlobalVariables;

extern ConVar host_timescale;



//-----------------------------------------------------------------------------
// globals
//-----------------------------------------------------------------------------
CSysModule		*g_ClientDLLModule = NULL; // also used by materialproxyfactory.cpp
bool g_bClientGameDLLGreaterThanV13;

void AddIntersectingLeafSurfaces( mleaf_t *pLeaf, GetIntersectingSurfaces_Struct *pStruct )
{
	SurfaceHandle_t *pHandle = &host_state.worldbrush->marksurfaces[pLeaf->firstmarksurface];
	for ( int iSurf=0; iSurf < pLeaf->nummarksurfaces; iSurf++ )
	{
		SurfaceHandle_t surfID = pHandle[iSurf];
		ASSERT_SURF_VALID( surfID );
		
		if ( MSurf_Flags(surfID) & SURFDRAW_SKY )
			continue;

		// Make sure we haven't already processed this one.
		bool foundSurf = false;
		for(int iTest=0; iTest < pStruct->m_nSetInfos; iTest++)
		{
			if(pStruct->m_pInfos[iTest].m_pEngineData == (void *)surfID)
			{
				foundSurf = true;
				break;
			}
		}
		if ( foundSurf )
			continue;

		// Make sure there's output room.
		if(pStruct->m_nSetInfos >= pStruct->m_nMaxInfos)
			return;
		SurfInfo *pOut = &pStruct->m_pInfos[pStruct->m_nSetInfos];
		pOut->m_nVerts = 0;
		pOut->m_pEngineData = (void *)surfID;

		// Build vertex list and bounding box.			
		Vector vMin( 1000000.0f,  1000000.0f,  1000000.0f);
		Vector vMax(-1000000.0f, -1000000.0f, -1000000.0f);
		for(int iVert=0; iVert < MSurf_VertCount( surfID ); iVert++)
		{
			int vertIndex = pStruct->m_pModel->brush.pShared->vertindices[MSurf_FirstVertIndex( surfID ) + iVert];

			pOut->m_Verts[pOut->m_nVerts] = pStruct->m_pModel->brush.pShared->vertexes[vertIndex].position;
			vMin = vMin.Min(pOut->m_Verts[pOut->m_nVerts]);
			vMax = vMax.Max(pOut->m_Verts[pOut->m_nVerts]);

			++pOut->m_nVerts;
			if(pOut->m_nVerts >= MAX_SURFINFO_VERTS)
				break;
		}

		// See if the sphere intersects the box.
		int iDim=0;
		for(; iDim < 3; iDim++)
		{
			if(((*pStruct->m_pCenter)[iDim]+pStruct->m_Radius) < vMin[iDim] || 
				((*pStruct->m_pCenter)[iDim]-pStruct->m_Radius) > vMax[iDim])
			{
				break;
			}
		}
		
		if(iDim == 3)
		{
			// (Couldn't reject the sphere in the loop above).
			pOut->m_Plane = MSurf_GetForwardFacingPlane( surfID );
			++pStruct->m_nSetInfos;
		}
	}
}

void GetIntersectingSurfaces_R(
	GetIntersectingSurfaces_Struct *pStruct,
	mnode_t *pNode
	)
{
	if(pStruct->m_nSetInfos >= pStruct->m_nMaxInfos)
		return;

	// Ok, this is a leaf. Check its surfaces.
	if(pNode->contents >= 0)
	{
		mleaf_t *pLeaf = (mleaf_t*)pNode;

		if(pStruct->m_bOnlyVisible && pStruct->m_pCenterPVS)
		{
			if(pLeaf->cluster < 0)
				return;

			if(!(pStruct->m_pCenterPVS[pLeaf->cluster>>3] & (1 << (pLeaf->cluster&7))))
				return;
		}

		// First, add tris from displacements.
		for ( int i = 0; i < pLeaf->dispCount; i++ )
		{
			IDispInfo *pDispInfo = MLeaf_Disaplcement( pLeaf, i );
			pDispInfo->GetIntersectingSurfaces( pStruct );
		}

		// Next, add brush tris.
		AddIntersectingLeafSurfaces( pLeaf, pStruct );
		return;
	}
	
	// Recurse.
	float dot;
	cplane_t *plane = pNode->plane;
	if ( plane->type < 3 )
	{
		dot = (*pStruct->m_pCenter)[plane->type] - plane->dist;
	}
	else
	{
		dot = pStruct->m_pCenter->Dot(plane->normal) - plane->dist;
	}

	// Recurse into child nodes.
	if(dot > -pStruct->m_Radius)
		GetIntersectingSurfaces_R(pStruct, pNode->children[SIDE_FRONT]);
	
	if(dot < pStruct->m_Radius)
		GetIntersectingSurfaces_R(pStruct, pNode->children[SIDE_BACK]);
}


//-----------------------------------------------------------------------------
// slow routine to draw a physics model
// NOTE: very slow code!!! just for debugging!
//-----------------------------------------------------------------------------
void DebugDrawPhysCollide( const CPhysCollide *pCollide, IMaterial *pMaterial, const matrix3x4_t& transform, const color32 &color, bool drawAxes )
{
	if ( !pMaterial )
	{
		pMaterial = materials->FindMaterial("debug/debugwireframevertexcolor", TEXTURE_GROUP_OTHER);
	}

	CMatRenderContextPtr pRenderContext( materials );

	Vector *outVerts;
	int vertCount = physcollision->CreateDebugMesh( pCollide, &outVerts );
	if ( vertCount )
	{
		IMesh *pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, pMaterial );

		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, vertCount/3 );

		for ( int j = 0; j < vertCount; j++ )
		{
			Vector out;
			VectorTransform( outVerts[j].Base(), transform, out.Base() );
			meshBuilder.Position3fv( out.Base() );
			meshBuilder.Color4ub( color.r, color.g, color.b, color.a );
			meshBuilder.TexCoord2f( 0, 0, 0 );
			meshBuilder.AdvanceVertex();
		}
		meshBuilder.End();
		pMesh->Draw();
	}
	physcollision->DestroyDebugMesh( vertCount, outVerts );

	// draw the axes
	if ( drawAxes )
	{
		Vector xaxis(10,0,0), yaxis(0,10,0), zaxis(0,0,10);
		Vector center;
		Vector out;

		MatrixGetColumn( transform, 3, center );
		IMesh *pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, pMaterial );
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_LINES, 3 );

		// X
		meshBuilder.Position3fv( center.Base() );
		meshBuilder.Color4ub( 255, 0, 0, 255 );
		meshBuilder.TexCoord2f( 0, 0, 0 );
		meshBuilder.AdvanceVertex();
		VectorTransform( xaxis.Base(), transform, out.Base() );
		meshBuilder.Position3fv( out.Base() );
		meshBuilder.Color4ub( 255, 0, 0, 255 );
		meshBuilder.TexCoord2f( 0, 0, 0 );
		meshBuilder.AdvanceVertex();

		// Y
		meshBuilder.Position3fv( center.Base() );
		meshBuilder.Color4ub( 0, 255, 0, 255 );
		meshBuilder.TexCoord2f( 0, 0, 0 );
		meshBuilder.AdvanceVertex();
		VectorTransform( yaxis.Base(), transform, out.Base() );
		meshBuilder.Position3fv( out.Base() );
		meshBuilder.Color4ub( 0, 255, 0, 255 );
		meshBuilder.TexCoord2f( 0, 0, 0 );
		meshBuilder.AdvanceVertex();

		// Z
		meshBuilder.Position3fv( center.Base() );
		meshBuilder.Color4ub( 0, 0, 255, 255 );
		meshBuilder.TexCoord2f( 0, 0, 0 );
		meshBuilder.AdvanceVertex();
		VectorTransform( zaxis.Base(), transform, out.Base() );
		meshBuilder.Position3fv( out.Base() );
		meshBuilder.Color4ub( 0, 0, 255, 255 );
		meshBuilder.TexCoord2f( 0, 0, 0 );
		meshBuilder.AdvanceVertex();
		meshBuilder.End();
		pMesh->Draw();
	}
}

//-----------------------------------------------------------------------------
//
// implementation of IVEngineHud
//
//-----------------------------------------------------------------------------

// UNDONE: Move this to hud export code, subsume previous functions
class CEngineClient : public IVEngineClient
{
public:
	CEngineClient();
	~CEngineClient();

	int		GetIntersectingSurfaces(
		const model_t *model,
		const Vector &vCenter, 
		const float radius,
		const bool bOnlyVisible,
		SurfInfo *pInfos, 
		const int nMaxInfos);

	Vector	GetLightForPoint(const Vector &pos, bool bClamp);
	Vector	GetLightForPointFast(const Vector &pos, bool bClamp);
	const char *ParseFile( const char *data, char *token, int maxlen );
	virtual bool CopyLocalFile( const char *source, const char *destination );
	void GetScreenSize( int& w, int &h );
	void ServerCmd( const char *szCmdString, bool bReliable );
	void ClientCmd( const char *szCmdString );
	void ClientCmd_Unrestricted( const char *szCmdString, bool fromConsoleOrKeybind = false );
	void ClientCmd_Unrestricted( const char *szCmdString, bool fromConsoleOrKeybind, int nUserSlot, bool bCheckValidSlot = true );
	void SetRestrictServerCommands( bool bRestrict );
	void SetRestrictClientCommands( bool bRestrict );
	bool GetPlayerInfo( int ent_num, player_info_t *pinfo );
	client_textmessage_t *TextMessageGet( const char *pName );
	bool Con_IsVisible( void );
	int GetLocalPlayer( void );
	float GetLastTimeStamp( void );
	virtual int GetLastAcknowledgedCommand( void );
	virtual int GetServerTick( void );
	const model_t *LoadModel( const char *pName, bool bProp );
	void UnloadModel( const model_t *model, bool bProp );
	CSentence *GetSentence( CAudioSource *pAudioSource );
	float GetSentenceLength( CAudioSource *pAudioSource );
	bool IsStreaming( CAudioSource *pAudioSource ) const;

	// FIXME, move entirely to client .dll
	void GetViewAngles( QAngle& va );
	void SetViewAngles( QAngle& va );
	int GetMaxClients( void );
	void Key_Event( ButtonCode_t key, int down );
	const char *Key_LookupBinding( const char *pBinding );
	const char *Key_BindingForKey( ButtonCode_t code );
	void Key_SetBinding( ButtonCode_t code, const char *pBinding );
	void StartKeyTrapMode( void );
	bool CheckDoneKeyTrapping( ButtonCode_t &key );
	bool IsInGame( void );
	bool IsConnected( void );
	bool IsDrawingLoadingImage( void );
	void HideLoadingPlaque( void );
	void Con_NPrintf( int pos, const char *fmt, ... );
	void Con_NXPrintf( const struct con_nprint_s *info, const char *fmt, ... );
	IMaterial *TraceLineMaterialAndLighting( const Vector &start, const Vector &end, 
		                                     Vector &diffuseLightColor, Vector &baseColor );
	int		IsBoxVisible( const Vector& mins, const Vector& maxs );
	int		IsBoxInViewCluster( const Vector& mins, const Vector& maxs );

	void Sound_ExtraUpdate( void );
#if defined(_PS3)
	void Sound_ServerUpdateSoundsPS3( void );
#endif

	bool CullBox ( const Vector& mins, const Vector& maxs );
	const char *GetGameDirectory( void );
	const VMatrix& WorldToScreenMatrix();
	const VMatrix& WorldToViewMatrix();

	// Loads a game lump off disk
	int		GameLumpVersion( int lumpId ) const;
	int		GameLumpSize( int lumpId ) const;
	bool	LoadGameLump( int lumpId, void* pBuffer, int size );

	// Returns the number of leaves in the level
	int		LevelLeafCount() const;
	virtual ISpatialQuery* GetBSPTreeQuery();

	// Convert texlight to gamma...
	virtual void LinearToGamma( float* linear, float* gamma );

	// Get the lightstyle value
	virtual float LightStyleValue( int style );
	virtual void DrawPortals();

	// Computes light due to dynamic lighting at a point
	// If the normal isn't specified, then it'll return the maximum lighting
	virtual void ComputeDynamicLighting( Vector const& pt, Vector const* pNormal, Vector& color );

	// Computes light due to dynamic lighting at a point
	// If the normal isn't specified, then it'll return the maximum lighting
	// If pBoxColors is specified (it's an array of 6), then it'll copy the light contribution at each box side.
	virtual void ComputeLighting( const Vector& pt, const Vector* pNormal, bool bClamp, Vector& color, Vector *pBoxColors );

	// Returns the color of the ambient light
	virtual void GetAmbientLightColor( Vector& color );

	// Returns the dx support level
	virtual int	GetDXSupportLevel();
	
	virtual bool SupportsHDR();
	virtual void Mat_Stub( IMaterialSystem *pMatSys );

	// menu display
	virtual void GetChapterName( char *pchBuff, int iMaxLength );
	virtual char const *GetLevelName( void );
	virtual char const *GetLevelNameShort( void );
	virtual char const *GetMapGroupName( void );	
	virtual bool IsLevelMainMenuBackground( void );
	virtual void GetMainMenuBackgroundName( char *dest, int destlen );

	// Occlusion system control
	virtual void SetOcclusionParameters( const OcclusionParams_t &params );

	//-----------------------------------------------------------------------------
	// Purpose: Takes a trackerID and returns which player slot that user is in
	//			returns 0 if no player found with that ID
	//-----------------------------------------------------------------------------
	virtual int	GetPlayerForUserID(int userID);
#if !defined( NO_VOICE )
	virtual struct IVoiceTweak_s *GetVoiceTweakAPI( void );
	virtual void SetVoiceCasterID( uint32 casterID );
#endif
	virtual void EngineStats_BeginFrame( void );
	virtual void EngineStats_EndFrame( void );
	virtual void FireEvents();
	virtual void ClearEvents();
	virtual void CheckPoint( const char *pName );
	virtual int GetLeavesArea( unsigned short *pLeaves, int nLeaves );
	virtual bool DoesBoxTouchAreaFrustum( const Vector &mins, const Vector &maxs, int iArea );
	virtual int GetFrustumList( Frustum_t **pList, int listMax );
	virtual bool ShouldUseAreaFrustum( int area );

	// Sets the hearing origin
	virtual void SetAudioState( const AudioState_t &audioState );

	//-----------------------------------------------------------------------------
	//
	// Sentence API
	//
	//-----------------------------------------------------------------------------

	virtual int SentenceGroupPick( int groupIndex, char *name, int nameLen );
	virtual int SentenceGroupPickSequential( int groupIndex, char *name, int nameLen, int sentenceIndex, int reset );
	virtual int SentenceIndexFromName( const char *pSentenceName );
	virtual const char *SentenceNameFromIndex( int sentenceIndex );
	virtual int SentenceGroupIndexFromName( const char *pGroupName );
	virtual const char *SentenceGroupNameFromIndex( int groupIndex );
	virtual float SentenceLength( int sentenceIndex );
	virtual void DebugDrawPhysCollide( const CPhysCollide *pCollide, IMaterial *pMaterial, const matrix3x4_t& transform, const color32 &color );

	// Activates/deactivates an occluder...
	virtual void ActivateOccluder( int nOccluderIndex, bool bActive );
	virtual bool IsOccluded( int occlusionViewId, const Vector &vecAbsMins, const Vector &vecAbsMaxs );
	virtual int	GetOcclusionViewId() const;
	virtual void *SaveAllocMemory( size_t num, size_t size );
	virtual void SaveFreeMemory( void *pSaveMem );
	virtual INetChannelInfo *GetNetChannelInfo( void );
	virtual bool IsPlayingDemo( void );
	virtual bool IsRecordingDemo( void );
	virtual bool IsPlayingTimeDemo( void );
	virtual int  GetDemoRecordingTick( void );
	virtual int  GetDemoPlaybackTick( void );
	virtual int  GetDemoPlaybackStartTick( void );
	virtual float GetDemoPlaybackTimeScale( void );
	virtual int  GetDemoPlaybackTotalTicks( void );
	virtual CDemoPlaybackParameters_t const * GetDemoPlaybackParameters() OVERRIDE;
	virtual bool IsDemoSkipping( void ) OVERRIDE;
	virtual int GetConnectionDataProtocol() const OVERRIDE;
	virtual bool EngineGotvSyncPacket( const CEngineGotvSyncPacket *pPkt ) OVERRIDE;
	virtual void SetDemoImportantEventData( const KeyValues *pData ) OVERRIDE;
	virtual bool IsPaused( void );
	virtual float GetTimescale( void ) const;
	virtual bool IsTakingScreenshot( void );
	virtual void WriteScreenshot( const char *pFilename );
	virtual bool IsHLTV( void );
	virtual bool IsReplay( void );
	virtual void GetUILanguage( char *dest, int destlen );

	// Can skybox be seen from a particular point?
	virtual SkyboxVisibility_t IsSkyboxVisibleFromPoint( const Vector &vecPoint );

	virtual const char* GetMapEntitiesString();
	virtual bool		IsInEditMode( void );
	virtual bool		IsInCommentaryMode( void );
	virtual float		GetScreenAspectRatio( int viewportWidth, int viewportHeight );

	virtual unsigned int		GetEngineBuildNumber() { return GetHostVersion(); }
	virtual const char *		GetProductVersionString() { return Sys_GetVersionString(); }
	virtual void				GrabPreColorCorrectedFrame( int x, int y, int width, int height );
	virtual bool				IsHammerRunning( ) const;

	// Stuffs the cmd into the buffer & executes it immediately (vs ClientCmd() which executes it next frame)
	virtual void				ExecuteClientCmd( const char *szCmdString );

	virtual bool MapHasHDRLighting( void) ;
	virtual bool MapHasLightMapAlphaData(void);
	virtual int GetAppID();

	virtual void				SetOverlayBindProxy( int iOverlayID, void *pBindProxy );

	virtual bool				CopyFrameBufferToMaterial( const char *pMaterialName );

	// Matchmaking
	virtual void				ReadConfiguration( const int iController, const bool readDefault );

	virtual void SetAchievementMgr( IAchievementMgr *pAchievementMgr );
	virtual IAchievementMgr *GetAchievementMgr();

	virtual bool				MapLoadFailed( void );
	virtual void				SetMapLoadFailed( bool bState );

	virtual bool				IsLowViolence();
	virtual const char			*GetMostRecentSaveGame( bool bEnsureExists );
	virtual void				SetMostRecentSaveGame( const char *lpszFilename );

	virtual void				StartXboxExitingProcess();

	virtual bool				IsSaveInProgress();
	virtual bool				IsAutoSaveDangerousInProgress();
	virtual bool				IsAutoSaveInProgress();

	virtual const char *		GetSaveDirName(); // get a pointer to the path where saves should go (with a trailing slash already added)

	
	virtual uint				OnStorageDeviceAttached( int iController );
	virtual void				OnStorageDeviceDetached( int iController );

	virtual void				ResetDemoInterpolation( void );

	virtual bool		REMOVED_SteamRefreshLogin( const char *password, bool isSecure ) { return false; }
	virtual bool		REMOVED_SteamProcessCall( bool & finished ) { return false; }

// For non-split screen games this will always be zero
	virtual int				GetActiveSplitScreenPlayerSlot();
	virtual int				SetActiveSplitScreenPlayerSlot( int slot );
	virtual bool			SetLocalPlayerIsResolvable( char const *pchContext, int nLine, bool bResolvable );
	virtual bool			IsLocalPlayerResolvable();

	virtual int				GetSplitScreenPlayer( int nSlot );
	virtual bool			IsSplitScreenActive();
	virtual bool			IsValidSplitScreenSlot( int nSlot );
	virtual int				FirstValidSplitScreenSlot(); // -1 == invalid
	virtual int				NextValidSplitScreenSlot( int nPreviousSlot ); // -1 == invalid

	virtual ISPSharedMemory *	GetSinglePlayerSharedMemorySpace( const char *handle, int ent_num = MAX_EDICTS );

	// Computes an ambient cube that includes ALL dynamic lights
	virtual void ComputeLightingCube( const Vector& pt, bool bClamp, Vector *pBoxColors );

	//All callbacks have to be registered before demo recording begins. TODO: Macro'ize a way to do it at startup
	virtual void RegisterDemoCustomDataCallback( string_t szCallbackSaveID, pfnDemoCustomDataCallback pCallback );
	virtual void RecordDemoCustomData( pfnDemoCustomDataCallback pCallback, const void *pData, size_t iDataLength );

	virtual void SetLeafFlag( int nLeafIndex, int nFlagBits );

	// you must call this once done modifying flags. Not super fast.
	virtual void RecalculateBSPLeafFlags( void );

	virtual bool DSPGetCurrentDASRoomNew(void);
	virtual bool DSPGetCurrentDASRoomChanged(void);
	virtual bool DSPGetCurrentDASRoomSkyAbove(void);
	virtual float DSPGetCurrentDASRoomSkyPercent(void);
	virtual void SetMixGroupOfCurrentMixer( const char *szgroupname, const char *szparam, float val, int setMixerType );
	virtual int GetMixLayerIndex( const char *szmixlayername );
	virtual void SetMixLayerLevel(int index, float level );
	virtual int GetMixGroupIndex( const char *pMixGroupName );
	virtual void SetMixLayerTriggerFactor( int nMixLayerIndex, int nMixGroupIndex, float flFactor );
	virtual void SetMixLayerTriggerFactor( const char *pMixLayerIndex, const char *pMixGroupIndex, float flFactor );


	virtual bool IsCreatingReslist();
	virtual bool IsCreatingXboxReslist();

	virtual void SetTimescale( float flTimescale );

	virtual void SetGamestatsData( CGamestatsData *pGamestatsData );
	virtual CGamestatsData *GetGamestatsData();

	virtual	const char *Key_LookupBindingEx( const char *pBinding, int iUserId = -1, int iStartCount = 0, BindingLookupOption_t nFlags = BINDINGLOOKUP_ALL );
	virtual int	Key_CodeForBinding( const char *pBinding, int iUserId = -1, int iStartCount = 0, BindingLookupOption_t nFlags = BINDINGLOOKUP_ALL );

	virtual void UpdateDAndELights( void );

	// Methods to get bug count for internal dev work stat tracking.
	// Will get the bug count and clear it every map transition
	virtual int			GetBugSubmissionCount() const;
	virtual void		ClearBugSubmissionCount();

	virtual bool	DoesLevelContainWater() const;
	virtual float	GetServerSimulationFrameTime() const;
	virtual void SolidMoved( IClientEntity *pSolidEnt, ICollideable *pSolidCollide, const Vector* pPrevAbsOrigin, bool accurateBboxTriggerChecks );
	virtual void TriggerMoved( IClientEntity *pTriggerEnt, bool accurateBboxTriggerChecks );
	virtual void ComputeLeavesConnected( const Vector &vecOrigin, int nCount, const int *pLeaves, bool *pIsConnected );

	virtual void	SetBlurFade( float scale );
	virtual bool	IsTransitioningToLoad();
	virtual void	SearchPathsChangedAfterInstall();
	virtual void ConfigureSystemLevel( int nCPULevel, int nGPULevel );
	virtual void SetConnectionPassword( char const *pchCurrentPW );
	virtual CSteamAPIContext* GetSteamAPIContext();
	virtual void SubmitStatRecord( char const *szMapName, uint uiBlobVersion, uint uiBlobSize, const void *pvBlob );

	virtual void ServerCmdKeyValues( KeyValues *pKeyValues );
	virtual void SendMessageToServer( INetMessage *pMessage, bool bForceReliable, bool bVoice ) OVERRIDE;

	// global sound pitch scaling
	virtual void SetPitchScale( float flPitchScale );
	virtual float GetPitchScale( void );

	// Load/unload the SFM - used by Replay
	virtual bool LoadFilmmaker();
	virtual void UnloadFilmmaker();

	//paint stuff
	virtual bool SpherePaintSurface( const model_t *pModel, const Vector& vPosition, BYTE color, float flSphereRadius, float flPaintCoatPercent );
	virtual bool HasPaintmap();
	virtual void EnablePaintmapRender();
	virtual void SphereTracePaintSurface( const model_t *pModel, const Vector& vPosition, const Vector& vContactNormal, float flSphereRadius, CUtlVector<BYTE>& surfColors );
	virtual void RemoveAllPaint();
	virtual void PaintAllSurfaces( BYTE color );
	virtual void RemovePaint( const model_t *pModel );

	virtual bool IsActiveApp();

	// is this client running inside the same process as an active server?
	virtual bool IsClientLocalToActiveServer();
	
#if defined( USE_SDL ) || defined ( OSX )
	virtual void GetMouseDelta( int &x, int &y, bool bIgnoreNextMouseDelta );
#endif	

	// Callback for LevelInit to tick the progress bar during time consuming operations
	virtual void TickProgressBar();
	// Returns the requested input context
	virtual InputContextHandle_t GetInputContext( EngineInputContextId_t id );

	virtual void GetStartupImage( char *pOutBuffer, int nOutBufferSize );

	virtual bool IsUsingLocalNetworkBackdoor();

	virtual bool SaveGame( const char *pSaveFilename, bool bIsXSave, char *pOutName, int nOutNameSize, char *pOutComment, int nOutCommentSize );

	// Request 'generic' memory stats (returns a list of N named values; caller should assume this list will change over time)
	virtual int GetGenericMemoryStats( GenericMemoryStat_t **ppMemoryStats );

	// On exit from a map, this becomes true once all map-related assets are flushed from memory:
	virtual bool GameHasShutdownAndFlushedMemory();

	virtual void FinishContainerWrites( int iController );

	virtual void FinishAsyncSave();

	const char *GetModDirectory( void );

	virtual void AudioLanguageChanged();

	virtual void StartLoadingScreenForCommand( const char* command );
	
	virtual void StartLoadingScreenForKeyValues( KeyValues* keyValues );

	virtual bool SOSSetOpvarFloat( const char *pOpVarName, float flValue );
	virtual bool SOSGetOpvarFloat( const char *pOpVarName, float &flValue );

#if defined(_PS3)
	virtual void* GetHostStateWorldBrush( void );
	virtual bool PS3_IsUserRestrictedFromChat( void );
	virtual bool PS3_IsUserRestrictedFromOnline( void );
	virtual bool PS3_PendingInvitesFound( void );
	virtual void PS3_ShowInviteOverlay( void );

	virtual bool  bOverrideCSMConvars( void ); 
	virtual bool  bDrawWorldIntoCSM( void );
	virtual bool  bDrawStaticPropsIntoCSM( void ); 
	virtual float GetCSMMaxDist( void );
#endif

	virtual bool IsSubscribedMap( const char *pchMapName, bool bOnlyOnDisk );
	virtual bool IsFeaturedMap( const char *pchMapName, bool bOnlyOnDisk );

	virtual int GetClientVersion() const;

	virtual float GetSafeZoneXMin( void ) const;

	virtual bool IsVoiceRecording() const OVERRIDE;
	virtual void ForceVoiceRecordOn() const OVERRIDE;
	virtual const char* AliasToCommandString( const char* szAliasName ) OVERRIDE;
};


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CEngineClient s_VEngineClient;
IVEngineClient *engineClient = &s_VEngineClient;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CEngineClient, IVEngineClient, VENGINE_CLIENT_INTERFACE_VERSION, s_VEngineClient );


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CEngineClient::CEngineClient()
{
}

CEngineClient::~CEngineClient()
{
}

int	CEngineClient::GetIntersectingSurfaces(
	const model_t *model,
	const Vector &vCenter, 
	const float radius,
	const bool bOnlyVisible,
	SurfInfo *pInfos, 
	const int nMaxInfos)
{
	if ( !model )
		return 0;

	byte pvs[MAX_MAP_LEAFS/8];
	GetIntersectingSurfaces_Struct theStruct;
	theStruct.m_pModel = ( model_t * )model;
	theStruct.m_pCenter = &vCenter;
	theStruct.m_pCenterPVS = CM_Vis( pvs, sizeof(pvs), CM_LeafCluster( CM_PointLeafnum( vCenter ) ), DVIS_PVS );
	theStruct.m_Radius = radius;
	theStruct.m_bOnlyVisible = bOnlyVisible;
	theStruct.m_pInfos = pInfos;
	theStruct.m_nMaxInfos = nMaxInfos;
	theStruct.m_nSetInfos = 0;		

	// Go down the BSP.
	GetIntersectingSurfaces_R(
		&theStruct,
		&model->brush.pShared->nodes[ model->brush.firstnode ] );

	return theStruct.m_nSetInfos;
}

Vector	CEngineClient::GetLightForPoint(const Vector &pos, bool bClamp)
{
	Vector vRet;
	ComputeLighting( pos, NULL, bClamp, vRet, NULL );
	return vRet;
}

Vector CEngineClient::GetLightForPointFast(const Vector &pos, bool bClamp)
{
	Vector vRet;
	int leafIndex = CM_PointLeafnum(pos);
	vRet.Init();
	Vector cube[6];
	Mod_LeafAmbientColorAtPos( cube, pos, leafIndex );
	for ( int i = 0; i < 6; i++ )
	{
		vRet.x = fpmax(vRet.x, cube[i].x );
		vRet.y = fpmax(vRet.y, cube[i].y );
		vRet.z = fpmax(vRet.z, cube[i].z );
	}
	if ( bClamp )
	{
		if ( vRet.x > 1.0f )
			vRet.x = 1.0f;
		if ( vRet.y > 1.0f )
			vRet.y = 1.0f;
		if ( vRet.z > 1.0f )
			vRet.z = 1.0f;
	}
	return vRet;
}

#if defined( OSX ) || defined( USE_SDL )

void CEngineClient::GetMouseDelta( int &x, int &y, bool bIgnoreNextMouseDelta )
{
	g_pLauncherMgr->GetMouseDelta( x, y, bIgnoreNextMouseDelta );
}
#endif

const char *CEngineClient::ParseFile( const char *data, char *token, int maxlen )
{
	return ::COM_ParseFile( data, token, maxlen );
}

bool CEngineClient::CopyLocalFile( const char *source, const char *destination )
{
	return ::COM_CopyFile( source, destination );
}

void CEngineClient::GetScreenSize( int& w, int &h )
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->GetWindowSize( w, h );
}

void CEngineClient::ServerCmd( const char *szCmdString, bool bReliable )
{
	// info handling
	char buf[255];
	Q_snprintf( buf, sizeof( buf ), "cmd %s", szCmdString );

	CCommand args;
	args.Tokenize( buf );
	Cmd_ForwardToServer( args, bReliable );
}

// NOTE: This code runs client commands *from the client*; it doesn't execute commands from the server.
//       You will want to use g_pVEngineServer->ClientCommand() for that, but that is subject to the
//       much more rigorous restriction of FCVAR_SERVER_CAN_EXECUTE since it is a remote command
//       execution path.
void CEngineClient::ClientCmd( const char *szCmdString )
{
	cmd_source_t commandSrc = kCommandSrcClientCmd;

	// If we aren't restricting client commands, then this is just something "from code"
	if ( !GetBaseLocalClient().m_bRestrictClientCommands )
		commandSrc = kCommandSrcCode;
	
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), szCmdString, commandSrc );
}

extern ConVar in_forceuser;

void CEngineClient::ClientCmd_Unrestricted( const char *szCmdString, bool fromConsoleOrKeybind )
{
	int nSplitScreenPlayerSlot = 0;
	if ( splitscreen->IsValidSplitScreenSlot( in_forceuser.GetInt() ) )
	{
		nSplitScreenPlayerSlot = in_forceuser.GetInt();
	}

	ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSplitScreenPlayerSlot );

	Cbuf_AddText( Cbuf_GetCurrentPlayer(), szCmdString, fromConsoleOrKeybind ? kCommandSrcUserInput : kCommandSrcCode );
}

void CEngineClient::ClientCmd_Unrestricted( const char *szCmdString, bool fromConsoleOrKeybind, int nUserSlot, bool bCheckValidSlot )
{
	if ( nUserSlot < 0 || nUserSlot >= host_state.max_splitscreen_players )
	{
		return;
	}

	if ( bCheckValidSlot )
	{
		if ( !splitscreen->IsValidSplitScreenSlot( nUserSlot ) )
		{
			return;
		}
	}

	ACTIVE_SPLITSCREEN_PLAYER_GUARD( nUserSlot );

	Cbuf_AddText( Cbuf_GetCurrentPlayer(), szCmdString, fromConsoleOrKeybind ? kCommandSrcUserInput : kCommandSrcCode );
}

void CEngineClient::SetRestrictServerCommands( bool bRestrict )
{
	GetBaseLocalClient().m_bRestrictServerCommands = bRestrict;
}

void CEngineClient::SetRestrictClientCommands( bool bRestrict )
{
	GetBaseLocalClient().m_bRestrictClientCommands = bRestrict;
}

void CEngineClient::ExecuteClientCmd( const char *szCmdString )
{
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), szCmdString );
	Cbuf_Execute();
}

bool CEngineClient::GetPlayerInfo( int ent_num, player_info_t *pinfo )
{
	ent_num--; // player list if offset by 1 from ents

	if ( ent_num >= GetBaseLocalClient().m_nMaxClients || ent_num < 0 )
	{
		Q_memset( pinfo, 0, sizeof( player_info_t ) );
		return false;
	}

	Assert( GetBaseLocalClient().m_pUserInfoTable );
	if ( !GetBaseLocalClient().m_pUserInfoTable )
	{
		Q_memset( pinfo, 0, sizeof( player_info_t ) );
		return false;
	}

	Assert( ent_num < GetBaseLocalClient().m_pUserInfoTable->GetNumStrings() );
	if ( ent_num >= GetBaseLocalClient().m_pUserInfoTable->GetNumStrings() )
	{
		Q_memset( pinfo, 0, sizeof( player_info_t ) );
		return false;
	}

	player_info_t *pi = (player_info_t*) GetBaseLocalClient().m_pUserInfoTable->GetStringUserData( ent_num, NULL );

	if ( !pi )
	{
		Q_memset( pinfo, 0, sizeof( player_info_t ) );
		return false;
	}

	// Check the version of player info coming in
	uint64 iVersion = BigQWord( pi->version );
	if ( iVersion != CDLL_PLAYER_INFO_S_VERSION_CURRENT )
	{
		// This is "old version 1"
		player_info_t_version_1 * pi_old_version_1 = reinterpret_cast< player_info_t_version_1 * >( pi );
		pinfo->version = BigQWord( CDLL_PLAYER_INFO_S_VERSION_CURRENT );
		Q_memcpy( &pinfo->xuid, &pi_old_version_1->xuid, sizeof( pi_old_version_1->xuid ) );
		Q_memcpy( pinfo->name, pi_old_version_1->name, sizeof( pi_old_version_1->name ) );
		Q_memcpy( &pinfo->userID, &pi_old_version_1->userID, sizeof( pi_old_version_1->userID ) );
		Q_memcpy( pinfo->guid, pi_old_version_1->guid, sizeof( pi_old_version_1->guid ) );
		Q_memcpy( &pinfo->friendsID, &pi_old_version_1->friendsID, sizeof( pi_old_version_1->friendsID ) );
		Q_memcpy( pinfo->friendsName, pi_old_version_1->friendsName, sizeof( pi_old_version_1->friendsName ) );
		Q_memcpy( &pinfo->fakeplayer, &pi_old_version_1->fakeplayer, sizeof( pi_old_version_1->fakeplayer ) );
		Q_memcpy( &pinfo->ishltv, &pi_old_version_1->ishltv, sizeof( pi_old_version_1->ishltv ) );
		Q_memcpy( pinfo->customFiles, pi_old_version_1->customFiles, sizeof( pi_old_version_1->customFiles ) );
		Q_memcpy( &pinfo->filesDownloaded, &pi_old_version_1->filesDownloaded, sizeof( pi_old_version_1->filesDownloaded ) );
	}
	else
	{
		Q_memcpy( pinfo, pi, sizeof( player_info_t ) );
	}

	// Fixup from network order (big endian)
	CByteswap byteswap;
	byteswap.SetTargetBigEndian( true );
	byteswap.SwapFieldsToTargetEndian( pinfo );

	return true;
}

client_textmessage_t *CEngineClient::TextMessageGet( const char *pName )
{
	return ::TextMessageGet( pName );
}

bool CEngineClient::Con_IsVisible( void )
{
	return ::Con_IsVisible();
}

int CEngineClient::GetLocalPlayer( void )
{
	return GetLocalClient().m_nPlayerSlot + 1;
}

float CEngineClient::GetLastTimeStamp( void )
{
	return GetBaseLocalClient().m_flLastServerTickTime;
}

int CEngineClient::GetLastAcknowledgedCommand( void )
{
	return GetBaseLocalClient().command_ack;
}

int CEngineClient::GetServerTick( void )
{
	return GetBaseLocalClient().GetServerTickCount();
}

bool CEngineClient::MapHasHDRLighting( void)
{
	return modelloader->LastLoadedMapHasHDRLighting();
}

bool CEngineClient::MapHasLightMapAlphaData( void)
{
	return modelloader->LastLoadedMapHasLightmapAlphaData();
}

const model_t *CEngineClient::LoadModel( const char *pName, bool bProp )
{
	return modelloader->GetModelForName( pName, bProp ? IModelLoader::FMODELLOADER_DETAILPROP : IModelLoader::FMODELLOADER_CLIENTDLL );
}

CSentence *CEngineClient::GetSentence( CAudioSource *pAudioSource )
{
	if (pAudioSource)
	{
		return pAudioSource->GetSentence();
	}
	return NULL;
}

float CEngineClient::GetSentenceLength( CAudioSource *pAudioSource )
{
	if (pAudioSource && pAudioSource->SampleRate() > 0 )
	{
		float length = (float)pAudioSource->SampleCount() / (float)pAudioSource->SampleRate();
		return length;
	}
	return 0.0f;
}

bool CEngineClient::IsStreaming( CAudioSource *pAudioSource ) const
{
	if ( pAudioSource )
	{
		return pAudioSource->IsStreaming();
	}
	return false;
}

// FIXME, move entirely to client .dll
void CEngineClient::GetViewAngles( QAngle& va )
{
	VectorCopy( GetLocalClient().viewangles, va );
}

void CEngineClient::SetViewAngles( QAngle& va )
{
	if ( !va.IsValid() )
	{
		Warning( "CEngineClient::SetViewAngles:  rejecting invalid value [%f %f %f]\n", VectorExpand( va ) );
		// Just zero it out
		GetLocalClient().viewangles = vec3_angle;
		return;
	}

	GetLocalClient().viewangles.x = AngleNormalize( va.x );
	GetLocalClient().viewangles.y = AngleNormalize( va.y );
	GetLocalClient().viewangles.z = AngleNormalize( va.z );
}

int CEngineClient::GetMaxClients( void )
{
	return GetBaseLocalClient().m_nMaxClients;
}

void CEngineClient::SetMapLoadFailed( bool bState )
{
	g_ServerGlobalVariables.bMapLoadFailed = bState;
}

bool CEngineClient::MapLoadFailed( void )
{
	return g_ServerGlobalVariables.bMapLoadFailed;
}

void CEngineClient::ReadConfiguration( const int iController, const bool readDefault )
{
	Host_ReadConfiguration( iController, readDefault );
}

const char *CEngineClient::Key_LookupBinding( const char *pBinding )
{
	return ::Key_NameForBinding( pBinding );
}

const char *CEngineClient::Key_BindingForKey( ButtonCode_t code )
{
	return ::Key_BindingForKey( code );
}

void CEngineClient::Key_SetBinding( ButtonCode_t code, const char *pBinding )
{
	::Key_SetBinding( code, pBinding );
}

void CEngineClient::StartKeyTrapMode( void )
{
	Key_StartTrapMode();
}

bool CEngineClient::CheckDoneKeyTrapping( ButtonCode_t &code )
{
	return Key_CheckDoneTrapping( code );
}

bool CEngineClient::IsInGame( void )
{
	return GetBaseLocalClient().IsActive();
}

bool CEngineClient::IsConnected( void )
{
	return GetBaseLocalClient().IsConnected();
}

bool CEngineClient::GameHasShutdownAndFlushedMemory( void )
{
	return HostState_GameHasShutDownAndFlushedMemory();
}

bool CEngineClient::IsDrawingLoadingImage( void )
{
	return scr_drawloading;
}

void CEngineClient::HideLoadingPlaque( void )
{
	// Now the client DLL should let us actually shutdown the loading plaque
	if ( !scr_drawloading )
	{
		DevWarning( "Attempted to HideLoadingPlaque when not loading...\n" );
	}
	if ( g_ClientDLL && !g_ClientDLL->ShouldHideLoadingPlaque() )
	{
		DevWarning( "Attempted to HideLoadingPlaque when client prevents hiding loading plaque...\n" );
	}

	// Shutdown the plaque
	SCR_EndLoadingPlaque();
}

void CEngineClient::Con_NPrintf( int pos, const char *fmt, ... )
{
	va_list		argptr;
	char		text[4096];
	va_start (argptr, fmt);
	Q_vsnprintf(text, sizeof( text ), fmt, argptr);
	va_end (argptr);

	::Con_NPrintf( pos, "%s", text );
}

void CEngineClient::Con_NXPrintf( const struct con_nprint_s *info, const char *fmt, ... )
{
	va_list		argptr;
	char		text[4096];
	va_start (argptr, fmt);
	Q_vsnprintf(text, sizeof( text ), fmt, argptr);
	va_end (argptr);

	::Con_NXPrintf( info, "%s", text );
}

IMaterial *CEngineClient::TraceLineMaterialAndLighting( const Vector &start, const Vector &end, 
		                                 Vector &diffuseLightColor, Vector &baseColor )
{
	return BrushModel_GetLightingAndMaterial( start, end, diffuseLightColor, baseColor );
}

int	CEngineClient::IsBoxVisible( const Vector& mins, const Vector& maxs ) 
{
	return CM_BoxVisible( mins, maxs, Map_VisCurrent(), CM_ClusterPVSSize() );
}

int	CEngineClient::IsBoxInViewCluster( const Vector& mins, const Vector& maxs )
{
	// See comments in Map_VisCurrentCluster for why we might get a negative number.
	int curCluster = Map_VisCurrentCluster();
	if ( curCluster < 0 )
		return false;

	byte pvs[MAX_MAP_LEAFS/8];
	const byte *ppvs = CM_Vis( pvs, sizeof(pvs), curCluster, DVIS_PVS );
	return CM_BoxVisible(mins, maxs, ppvs, sizeof(pvs) );
}

void CEngineClient::Sound_ExtraUpdate( void )
{
	// On xbox, sound is mixed on another thread, this is not necessary ever
	if ( IsGameConsole() )
		return;

	S_ExtraUpdate();
}

#if defined(_PS3)
extern void Host_UpdateSounds( void );

void CEngineClient::Sound_ServerUpdateSoundsPS3( void )
{
	if (sv.IsActive())
	{
		Host_UpdateSounds();
	}
}
#endif

bool CEngineClient::CullBox ( const Vector& mins, const Vector& maxs )
{
	return g_Frustum.CullBox( mins, maxs );
}

const char *CEngineClient::GetGameDirectory( void )
{
	return com_gamedir;
}

const char *CEngineClient::GetModDirectory( void )
{
	return COM_GetModDirectory();
}

const VMatrix& CEngineClient::WorldToScreenMatrix()
{
	// FIXME: this is only valid if we're currently rendering.  If not, it should use the player, or it really should pass one in.
	return g_EngineRenderer->WorldToScreenMatrix();
}

const VMatrix& CEngineClient::WorldToViewMatrix()
{
	// FIXME: this is only valid if we're currently rendering.  If not, it should use the player, or it really should pass one in.
	return g_EngineRenderer->ViewMatrix();
}

// Loads a game lump off disk
int	CEngineClient::GameLumpVersion( int lumpId ) const
{
	return Mod_GameLumpVersion( lumpId ); 
}

int	CEngineClient::GameLumpSize( int lumpId ) const 
{ 
	return Mod_GameLumpSize( lumpId ); 
}

bool CEngineClient::LoadGameLump( int lumpId, void* pBuffer, int size ) 
{ 
	return Mod_LoadGameLump( lumpId, pBuffer, size ); 
}

// Returns the number of leaves in the level
int	CEngineClient::LevelLeafCount() const
{
	return host_state.worldbrush->numleafs;
}

static void SetNodeFlagBits( mnode_t *node )
{
	if ( node->contents < 0 )								// has children
	{
		SetNodeFlagBits( node->children[0] );
		SetNodeFlagBits( node->children[1] );
		node->flags = 
			( node->flags & ( LEAF_FLAGS_SKY | LEAF_FLAGS_SKY2D | LEAF_FLAGS_RADIAL ) ) |
			node->children[0]->flags | node->children[1]->flags;
	}
}



void CEngineClient::SetLeafFlag( int nLeafIndex, int nFlagBits )
{
	assert( nLeafIndex < host_state.worldbrush->numleafs );
	host_state.worldbrush->leafs[nLeafIndex].flags |= nFlagBits;
}

void CEngineClient::RecalculateBSPLeafFlags( void )
{
	SetNodeFlagBits( host_state.worldbrush->nodes );
}


ISpatialQuery* CEngineClient::GetBSPTreeQuery()
{
	return g_pToolBSPTree;
}

// Convert texlight to gamma...
void CEngineClient::LinearToGamma( float* linear, float* gamma )
{
	gamma[0] = LinearToTexture( linear[0] ) / 255.0f;
	gamma[1] = LinearToTexture( linear[1] ) / 255.0f;
	gamma[2] = LinearToTexture( linear[2] ) / 255.0f;
}

// Get the lightstyle value
float CEngineClient::LightStyleValue( int style )
{
	return ::LightStyleValue( style );
}


void CEngineClient::DrawPortals()
{
	R_DrawPortals();
}

// Computes light due to dynamic lighting at a point
// If the normal isn't specified, then it'll return the maximum lighting
void CEngineClient::ComputeDynamicLighting( Vector const& pt, Vector const* pNormal, Vector& color )
{
	::ComputeDynamicLighting( pt, pNormal, color );
}

// Computes light due to dynamic lighting at a point
// If the normal isn't specified, then it'll return the maximum lighting
void CEngineClient::ComputeLighting( const Vector& pt, const Vector* pNormal, bool bClamp, Vector& color, Vector *pBoxColors )
{
	::ComputeLighting( pt, pNormal, bClamp, false, color, pBoxColors );
}


// Computes an ambient cube that includes ALL dynamic lights
void CEngineClient::ComputeLightingCube( const Vector& pt, bool bClamp, Vector *pBoxColors )
{
	Vector dummy;
	::ComputeLighting( pt, NULL, bClamp, true, dummy, pBoxColors );
}

// Returns the color of the ambient light
void CEngineClient::GetAmbientLightColor( Vector& color )
{
	dworldlight_t* pWorldLight = FindAmbientLight();
	if (!pWorldLight)
		color.Init( 0, 0, 0 );
	else
		VectorCopy( pWorldLight->intensity, color );
}

// Returns the dx support level
int	CEngineClient::GetDXSupportLevel()
{
	return g_pMaterialSystemHardwareConfig->GetDXSupportLevel();
}

bool CEngineClient::SupportsHDR()
{
	// deprecated.
//	Assert( 0 );
	return false;
}

void CEngineClient::Mat_Stub( IMaterialSystem *pMatSys )
{
	materials = pMatSys;
	
	// Pass the call to the model renderer.
	if ( g_pStudioRender )
		g_pStudioRender->Mat_Stub( pMatSys );
}

void CEngineClient::GetChapterName( char *pchBuff, int iMaxLength )
{
	serverGameDLL->GetSaveComment( pchBuff, iMaxLength, 0.0f, 0.0f, true );
}

char const *CEngineClient::GetLevelName( void )
{
	if ( sv.IsDedicated() )
	{
		return "Dedicated Server";
	}
	else if ( !GetBaseLocalClient().IsConnected() )
	{
		return "";
	}

	return GetBaseLocalClient().m_szLevelName;
}

char const *CEngineClient::GetLevelNameShort( void )
{
	if ( sv.IsDedicated() )
	{
		return "dedicated";
	}
	else if ( !GetBaseLocalClient().IsConnected() )
	{
		return "";
	}

	return GetBaseLocalClient().m_szLevelNameShort;
}

char const *CEngineClient::GetMapGroupName( void )
{	
	if ( !GetBaseLocalClient().IsConnected() )
	{
		return "";
	}

	return GetBaseLocalClient().m_szMapGroupName;
}

bool CEngineClient::IsLevelMainMenuBackground( void )
{
	return sv.IsLevelMainMenuBackground();
}

void CEngineClient::GetMainMenuBackgroundName( char *dest, int destlen )
{
	CL_GetBackgroundLevelName( dest, destlen, false );
}

void CEngineClient::GetStartupImage( char *dest, int destlen )
{
	CL_GetStartupImage( dest, destlen );
}

bool CEngineClient::IsUsingLocalNetworkBackdoor()
{
	return ( g_pLocalNetworkBackdoor != NULL );
}

bool CEngineClient::SaveGame( const char *pSaveFilename, bool bIsXSave, char *pOutName, int nOutNameSize, char *pOutComment, int nOutCommentSize )
{
	return saverestore->SaveGame( pSaveFilename, bIsXSave, pOutName, nOutNameSize, pOutComment, nOutCommentSize );
}

// Occlusion system control
void CEngineClient::SetOcclusionParameters( const OcclusionParams_t &params )
{
	OcclusionSystem()->SetOcclusionParameters( params.m_flMaxOccludeeArea, params.m_flMinOccluderArea );
}

//-----------------------------------------------------------------------------
// Purpose: Takes a trackerID and returns which player slot that user is in
//			returns 0 if no player found with that ID
//-----------------------------------------------------------------------------
int	CEngineClient::GetPlayerForUserID(int userID)
{
	if ( !GetBaseLocalClient().m_pUserInfoTable )
		return 0;

	for ( int i = 0; i < GetBaseLocalClient().m_nMaxClients; i++ )
	{
		// [mhansen] We send the user info in big endian... so here we do what L4D did
		int iEntIndex = i + 1;
		player_info_t ent_info;

		if ( GetPlayerInfo( iEntIndex, &ent_info ) && ent_info.userID == userID )
			return iEntIndex;
	}

	return 0;
}

#if !defined( NO_VOICE )
struct IVoiceTweak_s *CEngineClient::GetVoiceTweakAPI( void )
{
	return &g_VoiceTweakAPI;
}
void CEngineClient::SetVoiceCasterID( uint32 casterID )
{
	Voice_SetCaster( casterID );
}
#endif

void CEngineClient::EngineStats_BeginFrame( void )
{
	g_EngineStats.BeginFrame();
}

void CEngineClient::EngineStats_EndFrame( void )
{
	g_EngineStats.EndFrame();
}

void CEngineClient::FireEvents()
{
	// Run any events queued up for this frame
	CL_FireEvents();
}

void CEngineClient::ClearEvents()
{
	// clear any queued up events
	GetBaseLocalClient().events.RemoveAll();
}

void CEngineClient::CheckPoint( const char *pName )
{
	GetTestScriptMgr()->CheckPoint( pName );
}

int CEngineClient::GetLeavesArea( unsigned short *pLeaves, int nLeaves )
{
	if ( nLeaves == 0 )
		return -1;

	int iArea = host_state.worldbrush->leafs[pLeaves[0]].area;
	for ( int i=1; i < nLeaves; i++ )
	{
		int iTestArea = host_state.worldbrush->leafs[pLeaves[i]].area;
		if ( iTestArea != iArea )
			return -1;
	}

	return iArea;
}

bool CEngineClient::DoesBoxTouchAreaFrustum( const Vector &mins, const Vector &maxs, int iArea )
{
	const Frustum_t *pFrustum = GetAreaFrustum( iArea );
	return !pFrustum->CullBox( mins, maxs );
}

int CEngineClient::GetFrustumList( Frustum_t **pList, int listMax )
{
	pList[0] = &g_Frustum;
	int count = GetAllAreaFrustums( pList + 1, listMax-1 );
	return count+1;
}

bool CEngineClient::ShouldUseAreaFrustum( int area )
{
	return R_ShouldUseAreaFrustum( area );
}

//-----------------------------------------------------------------------------
// Sets the hearing origin
//-----------------------------------------------------------------------------
void CEngineClient::SetAudioState( const AudioState_t &audioState )
{
	Host_SetAudioState( audioState );
}


//-----------------------------------------------------------------------------
//
// Sentence API
//
//-----------------------------------------------------------------------------

int CEngineClient::SentenceGroupPick( int groupIndex, char *name, int nameLen )
{
	return VOX_GroupPick( groupIndex, name, nameLen );
}


int CEngineClient::SentenceGroupPickSequential( int groupIndex, char *name, int nameLen, int sentenceIndex, int reset )
{
	return VOX_GroupPickSequential( groupIndex, name, nameLen, sentenceIndex, reset );
}

int CEngineClient::SentenceIndexFromName( const char *pSentenceName )
{
	int sentenceIndex = -1;
	
	VOX_LookupString( pSentenceName, &sentenceIndex );
	
	return sentenceIndex;
}

const char *CEngineClient::SentenceNameFromIndex( int sentenceIndex )
{
	return VOX_SentenceNameFromIndex( sentenceIndex );
}


int CEngineClient::SentenceGroupIndexFromName( const char *pGroupName )
{
	return VOX_GroupIndexFromName( pGroupName );
}

const char *CEngineClient::SentenceGroupNameFromIndex( int groupIndex )
{
	return VOX_GroupNameFromIndex( groupIndex );
}


float CEngineClient::SentenceLength( int sentenceIndex )
{
	return VOX_SentenceLength( sentenceIndex );
}

void CEngineClient::DebugDrawPhysCollide( const CPhysCollide *pCollide, IMaterial *pMaterial, const matrix3x4_t& transform, const color32 &color )
{
	::DebugDrawPhysCollide( pCollide, pMaterial, transform, color, false );
}

// Activates/deactivates an occluder...
void CEngineClient::ActivateOccluder( int nOccluderIndex, bool bActive )
{
	OcclusionSystem()->ActivateOccluder( nOccluderIndex, bActive );
}

bool CEngineClient::IsOccluded( int occlusionViewId, const Vector &vecAbsMins, const Vector &vecAbsMaxs )
{
	return OcclusionSystem()->IsOccluded( occlusionViewId, vecAbsMins, vecAbsMaxs );
}

int	CEngineClient::GetOcclusionViewId() const
{
	return OcclusionSystem()->GetViewId();
}

void *CEngineClient::SaveAllocMemory( size_t num, size_t size )
{
	return ::SaveAllocMemory( num, size );
}

void CEngineClient::SaveFreeMemory( void *pSaveMem )
{
	::SaveFreeMemory( pSaveMem );
}

INetChannelInfo *CEngineClient::GetNetChannelInfo( void )
{
	return (INetChannelInfo*) GetBaseLocalClient().m_NetChannel;
}

bool CEngineClient::IsPlayingDemo( void )
{
	return demoplayer->IsPlayingBack();
}

bool CEngineClient::IsRecordingDemo( void )
{
	return demorecorder->IsRecording();
}

bool CEngineClient::IsPlayingTimeDemo( void )
{
	return demoplayer->IsPlayingTimeDemo();
}

CDemoPlaybackParameters_t const * CEngineClient::GetDemoPlaybackParameters()
{
	return demoplayer->GetDemoPlaybackParameters();
}

bool CEngineClient::IsDemoSkipping()
{
	return demoplayer->IsSkipping();
}

int CEngineClient::GetConnectionDataProtocol() const
{
	return GetBaseLocalClient().m_nServerInfoMsgProtocol;
}

bool CEngineClient::EngineGotvSyncPacket( const CEngineGotvSyncPacket *pPkt )
{
	return s_ClientBroadcastPlayer.OnEngineGotvSyncPacket( pPkt );
}


void CEngineClient::SetDemoImportantEventData( const KeyValues *pData )
{
	demoplayer->SetImportantEventData( pData );
}

bool CEngineClient::IsPaused( void )
{
	return  GetBaseLocalClient().IsPaused();
}

float  CEngineClient::GetTimescale( void ) const
{
	extern float CL_GetHltvReplayTimeScale();
	return sv.GetTimescale() * host_timescale.GetFloat() * CL_GetHltvReplayTimeScale();
}

bool CEngineClient::IsTakingScreenshot( void )
{
	return cl_takesnapshot;
}

extern bool cl_takesnapshot;
extern bool cl_takejpeg;
extern char cl_snapshot_fullpathname[MAX_OSPATH];
void CEngineClient::WriteScreenshot( const char *pFilename )
{
	cl_takesnapshot = true;
	cl_takejpeg = true;
	V_strncpy( cl_snapshot_fullpathname, pFilename, MAX_OSPATH );
}
int CEngineClient::GetDemoRecordingTick( void )	
{
	return demorecorder->GetRecordingTick();
}

int CEngineClient::GetDemoPlaybackTick( void )
{
	return demoplayer->GetPlaybackTick();
}

int CEngineClient::GetDemoPlaybackStartTick( void )
{
	return demoplayer->GetPlaybackStartTick();
}

float CEngineClient::GetDemoPlaybackTimeScale( void )
{
	return demoplayer->GetPlaybackTimeScale();
}

int CEngineClient::GetDemoPlaybackTotalTicks( void )
{
	return demoplayer->GetDemoStream()->GetTotalTicks();
}

bool CEngineClient::IsHLTV( void )
{
	return GetBaseLocalClient().ishltv || GetBaseLocalClient().GetHltvReplayDelay() > 0;
}

bool CEngineClient::IsReplay( void )
{
#if defined( REPLAY_ENABLED )
	return GetBaseLocalClient().isreplay;
#else
	return false;
#endif
}

void CEngineClient::GetUILanguage( char *dest, int destlen )
{
	const char *pStr = cl_language.GetString();
	if ( pStr )
	{
		V_strncpy( dest, pStr, destlen );
	}
	else
	{
		dest[0] = 0;
	}
}

//-----------------------------------------------------------------------------
// Can skybox be seen from a particular point?
//-----------------------------------------------------------------------------
SkyboxVisibility_t CEngineClient::IsSkyboxVisibleFromPoint( const Vector &vecPoint )
{
	// In the mat_fullbright 1 case, it's always visible 
	// (we may have no lighting in the level, and vrad is where LEAF_FLAGS_SKY is computed)
	if ( g_pMaterialSystemConfig->nFullbright == 1 )
		return SKYBOX_3DSKYBOX_VISIBLE;

	int nLeaf = CM_PointLeafnum( vecPoint );
	int nFlags = GetCollisionBSPData()->map_leafs[nLeaf].flags;
	if ( nFlags & LEAF_FLAGS_SKY )
		return SKYBOX_3DSKYBOX_VISIBLE;
	return ( nFlags & LEAF_FLAGS_SKY2D ) ? SKYBOX_2DSKYBOX_VISIBLE : SKYBOX_NOT_VISIBLE;
}

const char* CEngineClient::GetMapEntitiesString()
{
	return CM_EntityString();
}

bool CEngineClient::IsInEditMode( void )
{
	return g_bInEditMode;
}

bool CEngineClient::IsInCommentaryMode( void )
{
	return g_bInCommentaryMode;
}

float CEngineClient::GetScreenAspectRatio( int viewportWidth, int viewportHeight )
{
	return GetScreenAspect( viewportWidth, viewportHeight );
}

int CEngineClient::GetAppID()
{
	return GetSteamAppID();
}

void CEngineClient::SetOverlayBindProxy( int iOverlayID, void *pBindProxy )
{
	OverlayMgr()->SetOverlayBindProxy( iOverlayID, pBindProxy );
}

//-----------------------------------------------------------------------------
// Returns true if copy occured
//-----------------------------------------------------------------------------
bool CEngineClient::CopyFrameBufferToMaterial( const char *pMaterialName )
{
	if ( !IsX360() )
	{
		// not for PC
		Assert( 0 );
		return false;
	}

	IMaterial *pMaterial = materials->FindMaterial( pMaterialName, TEXTURE_GROUP_OTHER );
	if ( pMaterial->IsErrorMaterial() )
	{
		// unknown material
		return false;
	}

	bool bFound;
	IMaterialVar *pMaterialVar = pMaterial->FindVar( "$baseTexture", &bFound, false );
	if ( !bFound || pMaterialVar->GetType() != MATERIAL_VAR_TYPE_TEXTURE )
	{
		// lack of expected $basetexture
		return false;
	}

	ITexture *pTexture = pMaterialVar->GetTextureValue();
	if ( !pTexture || !pTexture->IsRenderTarget() )
	{
		// base texture is not a render target
		return false;
	}

	CMatRenderContextPtr pRenderContext( materials );

	int width, height;
	pRenderContext->GetRenderTargetDimensions( width, height );
	if ( width != pTexture->GetActualWidth() || height != pTexture->GetActualHeight() )
	{
		// better be matched, not supporting a disparate blit in this context
		// disparate blit may very well use same RT we are trying to copy into
		return false;
	}

	pRenderContext->CopyRenderTargetToTexture( pTexture );
	return true;
}


//-----------------------------------------------------------------------------
// Used by the color correction UI
//-----------------------------------------------------------------------------
void CEngineClient::GrabPreColorCorrectedFrame( int x, int y, int width, int height )
{
	colorcorrectiontools->GrabPreColorCorrectedFrame( x, y, width, height );
}

//-----------------------------------------------------------------------------
// Is hammer running?
//-----------------------------------------------------------------------------
bool CEngineClient::IsHammerRunning( ) const
{
	return IsPC() ? InEditMode() : false;
}

extern IAchievementMgr *g_pAchievementMgr;

//-----------------------------------------------------------------------------
// Sets achievement mgr
//-----------------------------------------------------------------------------
void CEngineClient::SetAchievementMgr( IAchievementMgr *pAchievementMgr )
{
	g_pAchievementMgr = pAchievementMgr;
}

//-----------------------------------------------------------------------------
// Gets achievement mgr
//-----------------------------------------------------------------------------
IAchievementMgr *CEngineClient::GetAchievementMgr() 
{
	return g_pAchievementMgr;
}


//-----------------------------------------------------------------------------
// Called by the client to determine violence settings for things like ragdoll
// fading.
//-----------------------------------------------------------------------------
bool CEngineClient::IsLowViolence()
{
	return g_bLowViolence;
}

const char *CEngineClient::GetMostRecentSaveGame( bool bEnsureExists )
{
	const char *pszResult = saverestore->GetMostRecentlyLoadedFileName();
	
	if ( pszResult && bEnsureExists && !saverestore->SaveFileExists( pszResult ) )
		pszResult = NULL;

	return pszResult;
}

void CEngineClient::SetMostRecentSaveGame( const char *lpszFilename )
{
	saverestore->SetMostRecentSaveGame( lpszFilename );
}

//-----------------------------------------------------------------------------
// Called by gameui to hint the engine that an exiting process has started.
// The Engine needs to stabilize to a safe quiet state. More frames are going
// to and have to run, but the true exit will occur.
//-----------------------------------------------------------------------------
void CEngineClient::StartXboxExitingProcess()
{
	if ( IsPC() )
	{
		// not for PC
		return;
	}

	g_pInputSystem->StopRumble();

	// save out the achievements
	g_pAchievementMgr->SaveGlobalStateIfDirty();

	// save out profile data
	if ( g_pMatchFramework )
	{
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnProfilesWriteOpportunity", "reason", "deactivation" ) );
	}

	S_StopAllSounds( true );

	// Shutdown QMS, need to go back to single threaded
	Host_AllowQueuedMaterialSystem( false );
}

bool CEngineClient::IsSaveInProgress()
{
	return saverestore->IsSaveInProgress();
}

bool CEngineClient::IsAutoSaveDangerousInProgress()
{
	return saverestore->IsAutoSaveDangerousInProgress();
}

bool CEngineClient::IsAutoSaveInProgress()
{
	return saverestore->IsAutoSaveInProgress();
}

const char *CEngineClient::GetSaveDirName() // get a pointer to the path where saves should go (with a trailing slash already added)
{
	return saverestore->GetSaveDir();
}


extern IXboxSystem *g_pXboxSystem;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
uint CEngineClient::OnStorageDeviceAttached( int iController )
{
	return g_pXboxSystem->OpenContainers( iController );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEngineClient::OnStorageDeviceDetached( int iController )
{
	XBX_SetStorageDeviceId( iController, XBX_INVALID_STORAGE_ID );
	g_pXboxSystem->CloseContainers( iController );
}

void CEngineClient::FinishContainerWrites( int iController )
{
	g_pXboxSystem->FinishContainerWrites( iController );
}

void CEngineClient::FinishAsyncSave()
{
	saverestore->FinishAsyncSave();
}

void CEngineClient::ResetDemoInterpolation( void )
{
	if( demorecorder->IsRecording() )
		demorecorder->ResetDemoInterpolation();
	if (demoplayer->IsPlayingBack() )
		demoplayer->ResetDemoInterpolation();
}

// For non-split screen games this will always be zero
int CEngineClient::GetActiveSplitScreenPlayerSlot()
{
	return GET_ACTIVE_SPLITSCREEN_SLOT();
}

int CEngineClient::SetActiveSplitScreenPlayerSlot( int slot )
{
	return splitscreen->SetActiveSplitScreenPlayerSlot( slot );
}

int CEngineClient::GetSplitScreenPlayer( int nSlot )
{
	return splitscreen->GetSplitScreenPlayerEntity( nSlot );
}

bool CEngineClient::SetLocalPlayerIsResolvable( char const *pchContext, int nLine, bool bResolvable )
{
	return splitscreen->SetLocalPlayerIsResolvable( pchContext, nLine, bResolvable );
}

bool CEngineClient::IsLocalPlayerResolvable()
{
	return splitscreen->IsLocalPlayerResolvable();
}

bool CEngineClient::IsSplitScreenActive()
{
	// Need a smarter way of doing this
	for ( int i = 1; i < splitscreen->GetNumSplitScreenPlayers(); i++ )
	{
		if ( splitscreen->IsValidSplitScreenSlot( i ) )
		{
			return true;
		}
	}

	return false;
}

bool CEngineClient::IsValidSplitScreenSlot( int nSlot )
{
	return splitscreen->IsValidSplitScreenSlot( nSlot );
}

int CEngineClient::FirstValidSplitScreenSlot()
{
	return splitscreen->FirstValidSplitScreenSlot();
}

int	CEngineClient::NextValidSplitScreenSlot( int nPreviousSlot )
{
	return splitscreen->NextValidSplitScreenSlot( nPreviousSlot );
}

ISPSharedMemory *CEngineClient::GetSinglePlayerSharedMemorySpace( const char *szName, int ent_num )
{
	return g_pSinglePlayerSharedMemoryManager->GetSharedMemory( szName, ent_num );
}


void CEngineClient::RegisterDemoCustomDataCallback( string_t szCallbackSaveID, pfnDemoCustomDataCallback pCallback )
{
	if( demorecorder->IsRecording() )
	{
		Warning( "Late registration of demo custom data callback.\n" );
		AssertMsg( false, "Late registration of demo custom data callback." );
	}

	//binary search for the callback address. Couldn't directly use UtlSortVector because of the need to pair data and sort by only the callback
	int start = 0, end = g_RegisteredDemoCustomDataCallbacks.Count() - 1;
	RegisteredDemoCustomDataCallbackPair_t *pEntries = g_RegisteredDemoCustomDataCallbacks.Base();
	while (start <= end)
	{
		int mid = (start + end) >> 1;
		if ( pEntries[mid].pCallback < pCallback )
		{
			start = mid + 1;
		}
		else if ( pCallback < pEntries[mid].pCallback )
		{
			end = mid - 1;
		}
		else
		{
			//found the entry already
			Warning( "Double registration of demo custom data callback.\n" );
			AssertMsg( false, "Double registration of demo custom data callback." );
			return;
		}
	}

	RegisteredDemoCustomDataCallbackPair_t addPair;
	addPair.pCallback = pCallback;
	addPair.szSaveID = szCallbackSaveID;
	g_RegisteredDemoCustomDataCallbacks.InsertBefore( start, addPair );
}


void CEngineClient::RecordDemoCustomData( pfnDemoCustomDataCallback pCallback, const void *pData, size_t iDataLength )
{
	Assert( demorecorder->IsRecording() );
	if( !demorecorder->IsRecording() )
	{
		Warning( "IEngineClient::RecordDemoCustomData(): Not recording a demo.\n" );
		AssertMsg( false, "IEngineClient::RecordDemoCustomData(): Not recording a demo." );
	}

	//binary search for the callback address. Couldn't directly use UtlSortVector because of the need to pair data and sort by only the callback
	int start = 0, end = g_RegisteredDemoCustomDataCallbacks.Count() - 1;
	RegisteredDemoCustomDataCallbackPair_t *pEntries = g_RegisteredDemoCustomDataCallbacks.Base();
	while (start <= end)
	{
		int mid = (start + end) >> 1;
		if ( pEntries[mid].pCallback < pCallback )
		{
			start = mid + 1;
		}
		else if ( pCallback < pEntries[mid].pCallback )
		{
			end = mid - 1;
		}
		else
		{
			//record the data
			demorecorder->RecordCustomData( mid, pData, iDataLength );
			return;
		}
	}

	Warning( "Demo recording custom data for unregistered callback.\n" );
	AssertMsg( false, "Demo recording custom data for unregistered callback." );
}

void CEngineClient::SetTimescale( float flTimescale )
{
	sv.SetTimescale( flTimescale );
}

extern CGamestatsData *g_pGamestatsData;
void CEngineClient::SetGamestatsData( CGamestatsData *pGamestatsData )
{
	g_pGamestatsData = pGamestatsData;
}

CGamestatsData *CEngineClient::GetGamestatsData()
{
	return g_pGamestatsData;
}

const char *CEngineClient::Key_LookupBindingEx( const char *pBinding, int iUserId, int iStartCount, BindingLookupOption_t nFlags )
{
	return ::Key_NameForBinding( pBinding, iUserId, iStartCount, nFlags );
}

int	CEngineClient::Key_CodeForBinding( const char *pBinding, int iUserId, int iStartCount, BindingLookupOption_t nFlags )
{
	return ::Key_CodeForBinding( pBinding, iUserId, iStartCount, nFlags );
}


// --------------------------------------------------------------------
// ADSP
// --------------------------------------------------------------------
bool CEngineClient::DSPGetCurrentDASRoomNew(void)
{
	return S_DSPGetCurrentDASRoomNew();
}
bool CEngineClient::DSPGetCurrentDASRoomChanged(void)
{
	return S_DSPGetCurrentDASRoomChanged();
}
bool CEngineClient::DSPGetCurrentDASRoomSkyAbove(void)
{
	return S_DSPGetCurrentDASRoomSkyAbove();
}
float CEngineClient::DSPGetCurrentDASRoomSkyPercent(void)
{
	return S_DSPGetCurrentDASRoomSkyPercent();
}

// --------------------------------------------------------------------
// soundmixer
// --------------------------------------------------------------------
void CEngineClient::SetMixGroupOfCurrentMixer( const char *szgroupname, const char *szparam, float val, int setMixerType )
{
	S_SetMixGroupOfCurrentMixer (szgroupname, szparam, val, setMixerType );
}

int CEngineClient::GetMixGroupIndex( const char *szmixgroupname )
{
	return  S_GetMixGroupIndex( szmixgroupname );
}
int CEngineClient::GetMixLayerIndex( const char *szmixlayername )
{
	return  S_GetMixLayerIndex( szmixlayername );
}

void CEngineClient::SetMixLayerLevel( int index, float level )
{
	S_SetMixLayerLevel( index, level );
}

void CEngineClient::SetMixLayerTriggerFactor( const char *pLayerName, const char *pMixGroupName, float flFactor )
{
	S_SetMixLayerTriggerFactor( pLayerName, pMixGroupName, flFactor );
}

void CEngineClient::SetMixLayerTriggerFactor( int nLayerIndex, int nMixGroupIndex, float flFactor )
{
	S_SetMixLayerTriggerFactor( nLayerIndex, nMixGroupIndex, flFactor );
}

bool CEngineClient::SOSSetOpvarFloat( const char *pOpVarName, float flValue )
{
	return S_SOSSetOpvarFloat( pOpVarName, flValue );
}
bool CEngineClient::SOSGetOpvarFloat( const char *pOpVarName, float &flValue )
{
	return S_SOSGetOpvarFloat( pOpVarName, flValue );
}

bool CEngineClient::IsSubscribedMap( const char *pchMapName, bool bOnlyOnDisk )
{
	return g_ClientDLL->IsSubscribedMap( pchMapName, bOnlyOnDisk );
}

bool CEngineClient::IsFeaturedMap( const char *pchMapName, bool bOnlyOnDisk )
{
	return g_ClientDLL->IsFeaturedMap( pchMapName, bOnlyOnDisk );
}

// --------------------------------------------------------------------
// 
// --------------------------------------------------------------------

bool CEngineClient::IsCreatingReslist()
{
	return MapReslistGenerator().IsEnabled();
}
bool CEngineClient::IsCreatingXboxReslist()
{
	return MapReslistGenerator().IsCreatingForXbox();
}

void CEngineClient::UpdateDAndELights( void )
{
	CL_UpdateDAndELights( false );
}

extern IEngineBugReporter *bugreporter;

int CEngineClient::GetBugSubmissionCount( void ) const
{
	if ( bugreporter )
	{
		return bugreporter->GetBugSubmissionCount();
	}
	
	return 0;
}

void CEngineClient::ClearBugSubmissionCount( void )
{
	if ( bugreporter )
	{
		bugreporter->ClearBugSubmissionCount();
	}
}

bool CEngineClient::DoesLevelContainWater()	const
{
	return host_state.worldbrush->numleafwaterdata != 0;
}

float Host_GetServerSimulationFrameTime();
float CEngineClient::GetServerSimulationFrameTime() const
{
	return Host_GetServerSimulationFrameTime();
}

//-----------------------------------------------------------------------------
// Adds a handle to the list of entities to update when a partition query occurs
//-----------------------------------------------------------------------------
void CEngineClient::SolidMoved( IClientEntity *pSolidEnt, ICollideable *pSolidCollide, const Vector* pPrevAbsOrigin, bool accurateBboxTriggerChecks )
{
	CL_SolidMoved( pSolidEnt, pSolidCollide, pPrevAbsOrigin, accurateBboxTriggerChecks );
}

void CEngineClient::TriggerMoved( IClientEntity *pTriggerEnt, bool accurateBboxTriggerChecks )
{
	CL_TriggerMoved( pTriggerEnt, accurateBboxTriggerChecks );
}

void CEngineClient::ComputeLeavesConnected( const Vector &vecOrigin, int nCount, const int *pLeaves, bool *pIsConnected )
{
	CM_LeavesConnected( vecOrigin, nCount, pLeaves, pIsConnected );
}

void CEngineClient::SetBlurFade( float scale )
{
	g_ClientDLL->SetBlurFade( scale );
}

bool CEngineClient::IsTransitioningToLoad( void )
{
	return HostState_IsTransitioningToLoad();
}

void CEngineClient::SearchPathsChangedAfterInstall( void )
{
	// close caption system needs to re-establish
	g_ClientDLL->ResetHudCloseCaption();
}

void CEngineClient::ConfigureSystemLevel( int nCPULevel, int nGPULevel )
{
	StaticPropMgr()->ConfigureSystemLevel( nCPULevel, nGPULevel );
	OverlayMgr()->UpdateOverlayRenderLevels( nCPULevel, nGPULevel );
}

void CEngineClient::SetConnectionPassword( char const *pchCurrentPW )
{
	GetBaseLocalClient().SetConnectionPassword( pchCurrentPW );
}

CSteamAPIContext* CEngineClient::GetSteamAPIContext()
{
	return &Steam3Client();
}

void CEngineClient::SubmitStatRecord( char const *szMapName, uint uiBlobVersion, uint uiBlobSize, const void *pvBlob )
{
	AsyncUpload_QueueData( szMapName, uiBlobVersion, uiBlobSize, pvBlob );
}

void CEngineClient::ServerCmdKeyValues( KeyValues *pKeyValues )
{
	GetLocalClient().SendServerCmdKeyValues( pKeyValues );
}

void CEngineClient::SendMessageToServer( INetMessage *pMessage, bool bForceReliable, bool bVoice )
{
	GetLocalClient().SendNetMsg( *pMessage, bForceReliable, bVoice );
}


void CEngineClient::SetPitchScale( float flPitchScale )
{
	S_SoundSetPitchScale( flPitchScale );
}

float CEngineClient::GetPitchScale( void )
{
	return S_SoundGetPitchScale();
}
bool CEngineClient::LoadFilmmaker()
{
	return toolframework->LoadFilmmaker();
}

void CEngineClient::UnloadFilmmaker()
{
	toolframework->UnloadFilmmaker();
}

bool CEngineClient::SpherePaintSurface( const model_t *pModel, const Vector& vPosition, BYTE colorIndex, float flSphereRadius, float flPaintCoatPercent )
{
	return ShootPaintSphere( pModel, vPosition, colorIndex, flSphereRadius, flPaintCoatPercent );
}

bool CEngineClient::HasPaintmap()
{
	return g_PaintManager.m_bShouldRegister;
}


void CEngineClient::PaintAllSurfaces( BYTE color )
{
	g_PaintManager.PaintAllSurfaces( color );
}


void CEngineClient::EnablePaintmapRender()
{
	ConVar *cv = g_pCVar->FindVar("mat_paint_enabled");
	if (cv)
	{
		cv->SetValue("1");
	}
}

void CEngineClient::SphereTracePaintSurface( const model_t *pModel, const Vector& vPosition, const Vector& vContactNormal, float flSphereRadius, CUtlVector<BYTE>& surfColors )
{
	TracePaintSphere( pModel, vPosition, vContactNormal, flSphereRadius, surfColors );
}


void CEngineClient::RemoveAllPaint()
{
	g_PaintManager.RemoveAllPaint();
}


void CEngineClient::RemovePaint( const model_t *pModel )
{
	g_PaintManager.RemovePaint( pModel );
}


bool CEngineClient::IsClientLocalToActiveServer()
{
	return sv.IsActive() || sv.IsLoading();
}

bool CEngineClient::IsActiveApp( void )
{
	return game->IsActiveApp();
}

// Callback for LevelInit to tick the progress bar during time consuming operations
void CEngineClient::TickProgressBar()
{
	EngineVGui()->UpdateProgressBar( PROGRESS_DEFAULT );
}

// Returns the requested input context
InputContextHandle_t CEngineClient::GetInputContext( EngineInputContextId_t id )
{
	switch( id )
	{
	case ENGINE_INPUT_CONTEXT_GAME:
		return GetGameInputContext();

	case ENGINE_INPUT_CONTEXT_GAMEUI:
		return EngineVGui()->GetGameUIInputContext();
	}

	return INPUT_CONTEXT_HANDLE_INVALID;
}

#define MAX_GENERIC_MEMORY_STATS 64
GenericMemoryStat_t g_EngineMemStats[MAX_GENERIC_MEMORY_STATS];
int g_nEngineMemStats = 0;
static inline int AddGenericMemoryStat( const char *name, int value )
{
	Assert( g_nEngineMemStats < MAX_GENERIC_MEMORY_STATS );
	if ( g_nEngineMemStats < MAX_GENERIC_MEMORY_STATS )
	{
		g_EngineMemStats[ g_nEngineMemStats ].name  = name;
		g_EngineMemStats[ g_nEngineMemStats ].value = value;
		g_nEngineMemStats++;
	}
	return g_nEngineMemStats;
}

int CEngineClient::GetGenericMemoryStats( GenericMemoryStat_t **ppMemoryStats )
{
	if ( !ppMemoryStats )
		return 0;
	g_nEngineMemStats = 0;

	AddGenericMemoryStat( "Hunk", Hunk_Size() );

#ifdef _GAMECONSOLE
	if ( host_state.worldbrush )
	{
		AddGenericMemoryStat( "BSP",     host_state.worldbrush->m_nBSPFileSize );
		AddGenericMemoryStat( "LM_lump", host_state.worldbrush->m_nLightingDataSize );
	}
#endif // _GAMECONSOLE

	*ppMemoryStats = &g_EngineMemStats[0];
	return g_nEngineMemStats;
}

void CEngineClient::AudioLanguageChanged()
{
	S_PurgeSoundsDueToLanguageChange();
}

void CEngineClient::StartLoadingScreenForCommand( const char* command )
{
	EngineVGui()->StartLoadingScreenForCommand( command );
}

void CEngineClient::StartLoadingScreenForKeyValues( KeyValues* keyValues )
{
	EngineVGui()->StartLoadingScreenForKeyValues( keyValues );
}

#if defined(_PS3)
void* CEngineClient::GetHostStateWorldBrush( void )
{
	return host_state.worldbrush;
}
#endif

int	CEngineClient::GetClientVersion() const
{
	return ::GetClientVersion();
}

float CEngineClient::GetSafeZoneXMin( void ) const
{
	float flMin = 0.85f;

	int nHeight = videomode->GetModeHeight();
	int nWidth = videomode->GetModeWidth();
	if ( (float)nHeight / (float)nWidth < 0.26f )
		flMin = 0.28f;
	else if ( (float)nHeight / (float)nWidth < 0.56f )
		flMin = 0.475f;

	return flMin;
}

bool CEngineClient::IsVoiceRecording() const
{
	return Voice_IsRecording();
}

void CEngineClient::ForceVoiceRecordOn() const
{
#if !defined( NO_VOICE )
	if ( GetBaseLocalClient().IsActive() && Voice_IsRecording() == false )
	{
		const char *pUncompressedFile = NULL;
		const char *pDecompressedFile = NULL;
		const char *pInputFile = NULL;

		//if (voice_recordtofile.GetInt())
		//{
		//	pUncompressedFile = "voice_micdata.wav";
		//	pDecompressedFile = "voice_decompressed.wav";
		//}

		//if (voice_inputfromfile.GetInt())
		//{
		//	pInputFile = "voice_input.wav";
		//}

		Voice_RecordStart( pUncompressedFile, pDecompressedFile, pInputFile );
	}
#endif
}

const char* CEngineClient::AliasToCommandString( const char* szAliasName )
{
	return Cmd_AliasToCommandString( szAliasName );
}

//-----------------------------------------------------------------------------
// The client DLL serves out this interface
//-----------------------------------------------------------------------------
IBaseClientDLL *g_ClientDLL = NULL;
IPrediction	*g_pClientSidePrediction = NULL;
IClientRenderTargets *g_pClientRenderTargets = NULL;
IClientEntityList *entitylist = NULL;
IClientLeafSystemEngine *clientleafsystem = NULL;
IClientAlphaPropertyMgr *g_pClientAlphaPropertyMgr = NULL;
ClientClass *g_pClientClassHead = NULL;

ClientClass *ClientDLL_GetAllClasses( void )
{
	if ( g_ClientDLL )
		return g_ClientDLL->GetAllClasses();
	else
		return g_pClientClassHead;
}

static void ClientDLL_InitRecvTableMgr()
{
	// Register all the receive tables.
	RecvTable *pRecvTables[MAX_DATATABLES];
	int nRecvTables = 0;
	for ( ClientClass *pCur = ClientDLL_GetAllClasses(); pCur; pCur=pCur->m_pNext )
	{
		ErrorIfNot( 
			nRecvTables < ARRAYSIZE( pRecvTables ), 
			("ClientDLL_InitRecvTableMgr: overflowed MAX_DATATABLES")
			);
		
		pRecvTables[nRecvTables] = pCur->m_pRecvTable;
		++nRecvTables;
	}

	RecvTable_Init( pRecvTables, nRecvTables );
}


static void ClientDLL_ShutdownRecvTableMgr()
{
	RecvTable_Term();
}

CreateInterfaceFn ClientDLL_GetFactory( void )
{
	return g_ClientFactory;
}

//-----------------------------------------------------------------------------
// Purpose: Loads the client DLL. Must return false if failed on PS3, to let the game output error screen before quitting to XMB
// Input  :  - 
//-----------------------------------------------------------------------------
bool ClientDLL_Load()
{
	Assert ( !g_ClientDLLModule );

	// Check the signature on the client dll.  If this fails we load it anyway but put this client
	// into insecure mode so it won't connect to secure servers and get VAC banned
	// #if 0 the following block and rebuild engine.dll if you want to build your own noCEG client.dll and run on Steam Public in secure mode!
#if 1
	if ( !Host_AllowLoadModule( "client" DLL_EXT_STRING, "GAMEBIN", false ) )
	{
		// not supposed to load this but we will anyway
		Host_DisallowSecureServers();
	}
#endif

	// loads the client.dll, but ensures that the client dll is running under Steam
	// this will have to be undone when we want mods to be able to run
	g_ClientDLLModule = g_pFileSystem->LoadModule( "client" DLL_EXT_STRING, "GAMEBIN", false );
	if ( g_ClientDLLModule )
	{
		g_ClientFactory = Sys_GetFactory( g_ClientDLLModule );
		if ( g_ClientFactory )
		{
			g_ClientDLL = (IBaseClientDLL *)g_ClientFactory( CLIENT_DLL_INTERFACE_VERSION, NULL );
			// this is to ensure the old format of the string table is used for clients version 13 and older.
			// when the client version gets revved, there will need to be an else that sets this bool to true
			// TERROR: g_bClientGameDLLGreaterThanV13 is true, so we get better stringtables
			g_bClientGameDLLGreaterThanV13 = true;
			if ( !g_ClientDLL )
			{
				if( IsPS3() )
				{
					return false;
				}
				else
				{
					Sys_Error( "Could not get client.dll interface from library client" );
				}
			}
		}
		else
		{
			if( IsPS3() )
			{
				return false;
			}
			else
			{
				Sys_Error( "Could not find factory interface in library client" );
			}
		}
	}
	else
	{	
		// library failed to load
		if( IsPS3() )
		{
			return false;
		}
		else
		{
			Sys_Error( "Could not load library client" );
		}
	}

	// Load the client render targets interface from the client .dll
	// NOTE: Its OK if this returns NULL, as some mods won't provide the interface and will just use the default behavior of the engine
	g_pClientRenderTargets = ( IClientRenderTargets * )g_ClientFactory( CLIENTRENDERTARGETS_INTERFACE_VERSION, NULL );
	return g_pClientRenderTargets != NULL;
}

void ClientDLL_GameInit()
{
	if ( g_ClientDLL )
	{
//		g_ClientDLL->GameInit();
	}
}

void ClientDLL_GameShutdown()
{
	if ( g_ClientDLL )
	{
//		g_ClientDLL->GameShutdown();
	}
}

void ClientDLL_Connect( void )
{
	if ( g_ClientDLL )
	{
		g_ClientDLL->Connect( g_AppSystemFactory, &g_ClientGlobalVariables );
	}
}

void ClientDLL_Disconnect()
{
	if( g_ClientDLL )
	{
		g_ClientDLL->Disconnect();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Inits the client .dll
//-----------------------------------------------------------------------------
void ClientDLL_Init( void )
{
	extern void CL_SetSteamCrashComment();

	// Assert ClientDLL_Load successfully created these interfaces, as we need them to init properly
	Assert ( g_ClientDLL );
	Assert ( g_ClientFactory );

	// this will get updated after we load a map, but this gets video info if we sys_error() prior to loading a map
	CL_SetSteamCrashComment();

	if ( g_ClientDLL )
	{
		COM_TimestampedLog( "g_ClientDLL->Init" );

		if ( !g_ClientDLL->Init( g_GameSystemFactory, &g_ClientGlobalVariables ) )
		{
			Sys_Error("Client.dll Init() in library client failed.");
		}

		if ( g_ClientFactory )
		{
			COM_TimestampedLog( "g_pClientSidePrediction->Init" );

			// Load the prediction interface from the client .dll
			g_pClientSidePrediction = (IPrediction *)g_ClientFactory( VCLIENT_PREDICTION_INTERFACE_VERSION, NULL );
			if ( !g_pClientSidePrediction )
			{
				Sys_Error( "Could not get IPrediction interface from library client" );
			}
			g_pClientSidePrediction->Init();

			entitylist = ( IClientEntityList  *)g_ClientFactory( VCLIENTENTITYLIST_INTERFACE_VERSION, NULL );
			if ( !entitylist )
			{
				Sys_Error( "Could not get client entity list interface from library client" );
			}

			clientleafsystem = ( IClientLeafSystemEngine *)g_ClientFactory( CLIENTLEAFSYSTEM_INTERFACE_VERSION, NULL );
			if ( !clientleafsystem )
			{
				Sys_Error( "Could not get client leaf system interface from library client" );
			}

			g_pClientAlphaPropertyMgr = ( IClientAlphaPropertyMgr* )g_ClientFactory( CLIENT_ALPHA_PROPERTY_MGR_INTERFACE_VERSION, NULL );
			if ( !g_pClientAlphaPropertyMgr )
			{
				Sys_Error( "Could not get client alpha property mgr interface from library client" );
			}

			toolframework->ClientInit( g_ClientFactory );
		}
		 		
		if ( g_pMaterialSystemHardwareConfig && !IsGameConsole( ) )
		{
			char pMessage[1024];
			pMessage[0] = '\0';
			bool bFailed = false;
		
			// We only run on hardware that supports shader model 3.0 (dxlevel 95) or later
			// @wge: HACK FIXME - Not doing this on MacOSX for now...
			if ( ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() < 95 ) && IsPC() && !IsOSX() && !IsOpenGL() ) // TODO: Need to remove the IsPC() before shipping once the Mac work is complete (mac is 92 right now)
			{
				wchar_t wcMessage[512];
				g_pVGuiLocalize->ConstructString( wcMessage, sizeof( wcMessage ), g_pVGuiLocalize->Find( "#Valve_MinShaderModel3" ), 0 );

				g_pVGuiLocalize->ConvertUnicodeToANSI( wcMessage, pMessage, sizeof( pMessage ) );
				
				bFailed = true;
			}
#ifdef CSTRIKE15
			// CS:GO requires CSM support for fairness. (This is primarily here in case the user is hacking/copying their moddefaults.txt or dxsupport.cfg from another product).
			else if ( !g_pMaterialSystemHardwareConfig->SupportsCascadedShadowMapping() )
			{
				wchar_t wcMessage[512];
				g_pVGuiLocalize->ConstructString( wcMessage, sizeof( wcMessage ), g_pVGuiLocalize->Find( "#Valve_CardMustSupportCSM" ), 0 );

				g_pVGuiLocalize->ConvertUnicodeToANSI( wcMessage, pMessage, sizeof( pMessage ) );

				bFailed = true;
			}
#endif
			// Allow the user to disable this check when testing internally (but not on steam public), typically when using remote desktop.
			// FIXME: Don't ship this
			//if ( ( GetSteamUniverse() != k_EUniversePublic ) && ( CommandLine()->CheckParm( "-nodevicechecks" ) ) )
			/*
			if ( CommandLine()->CheckParm( "-nodevicechecks" ) )
			{
				bFailed = false;
			}
			*/
						
			if ( bFailed )
			{
#ifdef _WIN32
				if ( g_pMaterialSystemConfig && materials )
				{
					MaterialAdapterInfo_t info;
					materials->GetDisplayAdapterInfo( materials->GetCurrentAdapter(), info );

					char pDeviceInfo[1024];

					if ( g_pMaterialSystemHardwareConfig )
					{
						sprintf_s( pDeviceInfo, "\n\nDevice Info:\nMarked unsupported: %i\nSupports PCF Sampling: %i\nDriverName: \"%s\"\nVendorID: 0x%04X, DeviceID: 0x%04X\nDriverHigh: 0x%08X, DriverLow: 0x%08X\nDXLevel: %u, MinDXSupportLevel: %u, MaxDXSupportLevel: %u\n", 
							g_pMaterialSystemConfig->IsUnsupported() || (g_pMaterialSystemHardwareConfig->IsUnsupported()),
							g_pMaterialSystemHardwareConfig->SupportsBilinearPCFSampling(),
							info.m_pDriverName ? info.m_pDriverName : "?",
							info.m_VendorID,
							info.m_DeviceID,
							info.m_nDriverVersionHigh,
							info.m_nDriverVersionLow,
							g_pMaterialSystemHardwareConfig->GetDXSupportLevel(),
							g_pMaterialSystemHardwareConfig->GetMinDXSupportLevel(),
							g_pMaterialSystemHardwareConfig->GetMaxDXSupportLevel() );
					}
					else
					{
						sprintf_s( pDeviceInfo, "\n\nDevice Info:\nMarked unsupported: %i\nDriverName: \"%s\"\nVendorID: 0x%04X, DeviceID: 0x%04X\nDriverHigh: 0x%08X, DriverLow: 0x%08X\n\n", 
							g_pMaterialSystemConfig->IsUnsupported(),
							info.m_pDriverName ? info.m_pDriverName : "?",
							info.m_VendorID,
							info.m_DeviceID,
							info.m_nDriverVersionHigh,
							info.m_nDriverVersionLow
							);

					}
					V_strcat( pMessage, pDeviceInfo, sizeof( pMessage ) );
				}
#endif
				Sys_Error( "%s", pMessage );
			}

			// Display a warning message (purposely not an error) if the card has been marked as unsupported in dxsupport.cfg.
			// If we got this far then the card is SM3 capable and supports bilinear PCF sampling, so let them try.
			if ( !CommandLine()->CheckParm( "-nounsupportedgpuchecks" ) )
			{
				if ( ( g_pMaterialSystemConfig && g_pMaterialSystemConfig->IsUnsupported() ) || ( g_pMaterialSystemHardwareConfig && g_pMaterialSystemHardwareConfig->IsUnsupported() ) )
				{
					wchar_t wcMessage[512];
					g_pVGuiLocalize->ConstructString( wcMessage, sizeof( wcMessage ), g_pVGuiLocalize->Find( "#Valve_UnsupportedCard" ), 0 );

					g_pVGuiLocalize->ConvertUnicodeToANSI( wcMessage, pMessage, sizeof( pMessage ) );

					// Make sure the warning message is visible in full-screen mode (otherwise the game appears like it's locked up).
					const bool bIsFullScreen = (videomode && !videomode->IsWindowedMode());
					if ( bIsFullScreen )
						videomode->ReleaseVideo();
					
					Sys_MessageBox( pMessage, NULL, false );
					
					if ( bIsFullScreen )
						videomode->RestoreVideo();
				}
			}
		}
	}

	COM_TimestampedLog( "ClientDLL_InitRecvTableMgr" );

	ClientDLL_InitRecvTableMgr();
}

//-----------------------------------------------------------------------------
// Purpose: Shuts down the client .dll
//-----------------------------------------------------------------------------
void ClientDLL_Shutdown( void )
{
	toolframework->ClientShutdown();

	ClientDLL_ShutdownRecvTableMgr();

	{
		FORCE_DEFAULT_SPLITSCREEN_PLAYER_GUARD;
		vgui::ivgui()->RunFrame();
		materials->UncacheAllMaterials();
		vgui::ivgui()->RunFrame();
	}

	if( g_pClientSidePrediction )
	{
		g_pClientSidePrediction->Shutdown();
	}

	entitylist = NULL;
	g_pClientSidePrediction = NULL;
	g_ClientFactory = NULL;

	g_ClientDLL->Shutdown();
}

//-----------------------------------------------------------------------------
// Purpose: Unloads the client .dll
// Input  :  - 
//-----------------------------------------------------------------------------
void ClientDLL_Unload()
{
	// Unfortunately, appsystem framework does not disconnect client in the order opposite 
	// of the creation, because the client is initialized and created/connected 
	// in special code path. So we disconnect it here for good measure: when
	// scenefilecache fails to load, the client is fully connected, but app framework
	// doesn't know about it.
	ClientDLL_Disconnect();

	FileSystem_UnloadModule( g_ClientDLLModule );

	g_ClientDLL = NULL;
	g_ClientDLLModule = NULL;
	g_pClientRenderTargets = NULL;

}

//-----------------------------------------------------------------------------
// Purpose: Called when the game initializes and whenever the vid_mode is changed
//   so the HUD can reinitialize itself.
//-----------------------------------------------------------------------------
void ClientDLL_HudVidInit( void )
{
	g_ClientDLL->HudVidInit();
}

//-----------------------------------------------------------------------------
// Purpose: Allow client .dll to modify input data
//-----------------------------------------------------------------------------

void ClientDLL_ProcessInput( void )
{
	SNPROF("ClientDLL_ProcessInput");

	if ( !g_ClientDLL )
		return;

	VPROF("ClientDLL_ProcessInput");
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		g_ClientDLL->HudProcessInput( GetLocalClient().IsConnected() );
	}
	
#ifdef _PS3
	if( g_pDebugInputThread && !g_pDebugInputThread->m_inputString.IsEmpty() )
	{
		AUTO_LOCK( g_pDebugInputThread->m_mx );
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), g_pDebugInputThread->m_inputString.Get(), kCommandSrcConsoleBuffer );
		g_pDebugInputThread->m_inputString.Purge();
	}
#endif
}


void ClientDLL_FrameStageNotify( ClientFrameStage_t frameStage )
{
	if ( !g_ClientDLL )
		return;

	g_ClientDLL->FrameStageNotify( frameStage );
}


//-----------------------------------------------------------------------------
// Purpose: Allow client .dll to think
//-----------------------------------------------------------------------------
void ClientDLL_Update( void )
{
	if ( sv.IsDedicated() )
		return;

	if ( g_ClientDLL )
	{
		g_ClientDLL->HudUpdate( true );
	}
}

void ClientDLL_VoiceStatus( int entindex, int iSsSlot, bool bTalking )
{
	if( g_ClientDLL )
	{
		g_ClientDLL->VoiceStatus( entindex, iSsSlot, bTalking );
	}
}

bool ClientDLL_IsPlayerAudible( int iPlayerIndex )
{
	if( g_ClientDLL )
	{
		return g_ClientDLL->PlayerAudible( iPlayerIndex );
	}
	return false;
}

void ClientDLL_OnActiveSplitscreenPlayerChanged( int slot )
{
	if( g_ClientDLL )
	{
		g_ClientDLL->OnActiveSplitscreenPlayerChanged( slot );
	}
}

void ClientDLL_OnSplitScreenStateChanged()
{
	if( g_ClientDLL )
	{
		g_ClientDLL->OnSplitScreenStateChanged();
	}
}

int  ClientDLL_GetSpectatorTarget( ClientDLLObserverMode_t *pObserverMode )
{
	if( g_ClientDLL )
	{
		return g_ClientDLL->GetSpectatorTarget( pObserverMode );
	}
	if ( pObserverMode )
	{
		*pObserverMode = CLIENT_DLL_OBSERVER_NONE;
	}
	return -1;
}

vgui::VPANEL ClientDLL_GetFullscreenClientDLLVPanel( void )
{
	if ( g_ClientDLL )
	{
		return g_ClientDLL->GetFullscreenClientDLLVPanel();
	}
	return false;
}

#if defined ( _PS3 )

// note:  We assume if we aren't connected or initialized, that the chat is NOT restricted
bool CEngineClient::PS3_IsUserRestrictedFromChat( void )
{
	return EngineHelperPS3::PS3_IsUserRestrictedFromChat();
}

// NOTE:  If we're not signed in yet, or not initialized, we consider this as not restricted from online
bool CEngineClient::PS3_IsUserRestrictedFromOnline( void )
{
	return EngineHelperPS3::PS3_IsUserRestrictedFromOnline();
}

bool CEngineClient::PS3_PendingInvitesFound( void )
{
	return EngineHelperPS3::PS3_PendingInvitesFound();
}

void CEngineClient::PS3_ShowInviteOverlay( void )
{
	EngineHelperPS3::PS3_ShowInviteOverlay();
}

#endif // _PS3

