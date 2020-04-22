//========= Copyright © 1996-2013, Valve Corporation, All rights reserved. ============//
//
// Purpose: Provide custom texture generation (compositing) 
//
// $NoKeywords: $
//=============================================================================//

#ifndef COMPOSITE_TEXTURE_H
#define COMPOSITE_TEXTURE_H

#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/itexture.h"
#include "materialsystem/icompositetexturegenerator.h"
#include "utllinkedlist.h"
#include "utlbuffer.h"

class IVisualsDataProcessor;
class ITextureInternal;

//
// Render Targets for Custom Materials
//

class IMaterialSystem;
class IMaterialSystemHardwareConfig;

struct SCompositeTextureRTData_t
{
	const char* pName;
	int nSize;
	bool bSRGB;
	bool bAvailable;	// true if the RT was created and can be used.
	IVTFTexture *pScratch;
};

// this should match the s_compositeTextureRTData array in composite_texture.cpp
enum CompositeTextureRTSizes_t
{
#if !defined( PLATFORM_OSX )
	COMPOSITE_TEXTURE_RT_2048,
#endif
	COMPOSITE_TEXTURE_RT_1024,
	COMPOSITE_TEXTURE_RT_512,
	COMPOSITE_TEXTURE_RT_256,
	COMPOSITE_TEXTURE_RT_128,
	COMPOSITE_TEXTURE_RT_COUNT
};

//
// Texture Regenerator for Custom Materials
//
// This is where the compositing material is created and used to make the procedural textures
// used with custom materials.  It also holds the result vtfs that are copied into the
// actual textures used for rendering.
//

enum CompositeTextureStages_t
{
	COMPOSITE_TEXTURE_STATE_NOT_STARTED = 0,
	COMPOSITE_TEXTURE_STATE_ASYNC_TEXTURE_LOAD,
	COMPOSITE_TEXTURE_STATE_WAITING_FOR_ASYNC_TEXTURE_LOAD_FINISH,
	COMPOSITE_TEXTURE_STATE_NEEDS_INIT,
	COMPOSITE_TEXTURE_STATE_WAITING_FOR_RENDER_TO_RT,
	COMPOSITE_TEXTURE_STATE_RENDERED_TO_RT,
	COMPOSITE_TEXTURE_STATE_WAITING_FOR_READ_RT,
	COMPOSITE_TEXTURE_STATE_REQUESTED_READ,
	COMPOSITE_TEXTURE_STATE_WAITING_FOR_GETRESULT,
	COMPOSITE_TEXTURE_STATE_REQUESTED_GETRESULT,
	COMPOSITE_TEXTURE_STATE_COPY_TO_VTF_COMPLETE,
	COMPOSITE_TEXTURE_STATE_WAITING_FOR_MATERIAL_CLEANUP,
	COMPOSITE_TEXTURE_STATE_COMPLETE
};

#define NUM_PRELOAD_TEXTURES 20

class CCompositeTexture;

class CCompositeTextureResult : public ITextureRegenerator
{
public:
	CCompositeTextureResult( CCompositeTexture *pOwner ) : m_pOwner( pOwner ), m_pTexture( NULL ) {}

	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect );
	virtual void Release();

	CCompositeTexture *m_pOwner;
	ITexture *m_pTexture;
};

class CCompositeTexture : public ICompositeTexture, public CRefCounted<>
{
public:
	CCompositeTexture( const CUtlBuffer &compareBlob, KeyValues *pCompositingMaterialKeyValues, CompositeTextureSize_t size, CompositeTextureFormat_t format, int nMaterialParamNameId, bool bSRGB, bool bIgnorePicMip );
	bool Init();

	void Refresh() 
	{ 
		m_nRegenerateStage = COMPOSITE_TEXTURE_STATE_NOT_STARTED; 
		m_bNeedsRegenerate = false;
		m_bNeedsFinalize = true;
	}
	bool IsReady() const { return ( !NeedsFinalize() && GenerationComplete() ); }
	bool GenerationComplete() const { return ( m_nRegenerateStage == COMPOSITE_TEXTURE_STATE_COMPLETE ); }
	void Usage( int &nTextures, int &nBackingTextures );

	bool NeedsAsyncTextureLoad() const { return m_nRegenerateStage == COMPOSITE_TEXTURE_STATE_ASYNC_TEXTURE_LOAD; }
	void DoAsyncTextureLoad();

	bool NeedsCompositingMaterial() const { return m_nRegenerateStage == COMPOSITE_TEXTURE_STATE_NEEDS_INIT; }
	void CreateCompositingMaterial();

	bool NeedsRender() const { return m_nRegenerateStage == COMPOSITE_TEXTURE_STATE_WAITING_FOR_RENDER_TO_RT; }
	void RenderToRT();

	bool HasRendered() const { return ( m_nRegenerateStage == COMPOSITE_TEXTURE_STATE_RENDERED_TO_RT ); }
	void AdvanceToReadRT();

	bool NeedsReadRT() const { return m_nRegenerateStage == COMPOSITE_TEXTURE_STATE_WAITING_FOR_READ_RT; }
	void ReadRT();

	bool NeedsGetResult() const { return m_nRegenerateStage == COMPOSITE_TEXTURE_STATE_WAITING_FOR_GETRESULT; }
	void GetReadRTResult();

	bool NeedsMaterialCleanup() const {	return m_nRegenerateStage == COMPOSITE_TEXTURE_STATE_WAITING_FOR_MATERIAL_CLEANUP; }
	void CleanupCompositingMaterial();

	void GenerationStep();
	bool NeedsRegenerate() const { return m_bNeedsRegenerate; }
	void ForceRegenerate();

	CompositeTextureFormat_t Format() const { return m_format; }
	CompositeTextureSize_t Size() const { return m_size; }
	int ActualSize() const { return m_nActualSize; }

	int GetMaterialParamNameId() const { return m_nMaterialParamNameId; }

	const char *GetName() { return m_szTextureName; }

	bool NeedsFinalize() const { return m_bNeedsFinalize; }
	void Finalize();

	IVTFTexture *GetResultVTF() { return m_pResultVTF; }

	bool ShouldRelease() { return ( ( GetRefCount() == 1 ) && IsReady() ); }

	const CUtlBuffer &GetVisualsDataCompareBlob() { return m_compareBlob; }

	void ReleaseResult() { m_ResultTexture.Release(); }

	bool IsSRGB() const { return m_bSRGB; }

	bool Compare( const SCompositeTextureInfo &textureInfo );

protected:
	virtual ~CCompositeTexture();
	void GenerateComposite();
	void ReleasePreloadedTextures();

	char m_szTextureName[ MAX_PATH ];
	CompositeTextureSize_t m_size;
	CompositeTextureFormat_t m_format;	// DXT1 or DXT5 only
	int m_nMaterialParamNameId;	// the material parameter that this texture will fill/replace in the eventual Custom Material, also used to interact with the VisualsDataProcessor
	bool m_bSRGB;
	CompositeTextureStages_t m_nRegenerateStage;
	CUtlBuffer m_compareBlob;

	IVTFTexture *m_pResultVTF;
	CCompositeTextureResult m_ResultTexture;

	int m_nActualSize;

	bool m_bNeedsRegenerate;
	bool m_bNeedsFinalize;
	IVTFTexture *m_pScratchVTF;
	ITexture *m_pCustomMaterialRT;
	KeyValues *m_pCompositingMaterialKeyValues;
	IMaterial *m_pCompositingMaterial;
	int m_nLastFrameCount;
	CThreadEvent *m_pPixelsReadEvent;

	bool m_bIgnorePicMip;

	static int m_nTextureCount;

private:
	CCompositeTexture( CCompositeTexture &);

	ITextureInternal* m_pPreLoadTextures[ NUM_PRELOAD_TEXTURES ];
};



//
// Composite Texture Generator
//
//  This is the class that the game talks to in order to make composite textures
//  You need a IVisualsDataProcessor based class to feed this.
//

class CCompositeTextureGenerator : public ICompositeTextureGenerator
{
public:
	CCompositeTextureGenerator( void );
	virtual ~CCompositeTextureGenerator( void );

	bool Init( void );
	void Shutdown( void );

	virtual bool Process( void );
	virtual ICompositeTexture *GetCompositeTexture( IVisualsDataProcessor *pVisualsDataProcessor, int nMaterialParamNameId, CompositeTextureSize_t size, CompositeTextureFormat_t format, bool bSRGB, bool bIgnorePicMip = false , bool bAllowCreate = true );
	virtual ICompositeTexture *GetCompositeTexture( const SCompositeTextureInfo &textureInfo, bool bIgnorePicMip = false , bool bAllowCreate = true );
	virtual bool ForceRegenerate( ICompositeTexture *pTexture );

private:
	CUtlVector< CCompositeTexture * > m_pCompositeTextures;
	CUtlVector< CCompositeTexture * > m_pPendingCompositeTextures;

	void DestroyTextures( void );

	void GenerateThread(); // never call this directly, it's running as another thread
	void CreateGenerateThread();
	void DestroyGenerateThread();

	CUtlLinkedList< CCompositeTexture * > m_pGenerateQueue;
	CThreadFastMutex m_GenerateQueueMutex;
	ThreadHandle_t m_hGenerateThread;
	bool m_bGenerateThreadExit;

	CTextureReference m_CompositeTextureManagerRTs[COMPOSITE_TEXTURE_RT_COUNT]; 

	ITextureInternal* m_pGunGrimeTexture;
	ITextureInternal* m_pPaintWearTexture;
};

#endif // COMPOSITE_TEXTURE_H
