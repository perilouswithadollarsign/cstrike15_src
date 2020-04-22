//========= Copyright (c), Valve Corporation, All rights reserved. ============//

#ifndef HLTV_BROADCAST_PLAYER_HDR
#define HLTV_BROADCAST_PLAYER_HDR


#include "tier1/utlincrementalvector.h"
#include "steam/steam_api.h"
#include "steam/isteamhttp.h"
#include "tier1/utlhashtable.h"
#include "tier1/smartptr.h"
#include "demostream.h"
#include "demofile/gotvhttpstream.h"

struct HTTPRequestCompleted_t;

class CDemoStreamHttp: public IDemoStream
{
public:
	CDemoStreamHttp();
	void SetClient( IDemoStreamClient *pClient ) { m_pClient = pClient; }

	struct SyncParams_t
	{
		SyncParams_t(int nStartFragment = 0) :m_nStartFragment( nStartFragment ){}
		int m_nStartFragment;
		void PrintSyncRequest(char *buffer, int nBufSize ) const;
	};

	void StartStreaming( const char *pUrl, SyncParams_t syncParams = SyncParams_t() );
	void StartStreamingCached( const char *pUrl, int nFragment = 1);

	bool PrepareForStreaming( const char * pUrl );

	void SendSync( int nResync = 0 );
	bool OnEngineGotvSyncPacket( const CEngineGotvSyncPacket *pPkt );

	void StopStreaming();
	bool IsIdle()const { return m_nState == STATE_IDLE; }
	void Resync( );
	void Update();

	virtual const char* GetUrl( void ) OVERRIDE { return m_Url.Get(); }
	virtual float GetTicksPerSecond( void )OVERRIDE { return m_SyncResponse.flTicksPerSecond; }
	int GetDemoProtocol()const { return m_nDemoProtocol; }
	virtual float GetTicksPerFrame( void ) OVERRIDE { return 1.0f; } // 1 network frame per 1 tick in broadcast - there's not much reason to do otherwise
	virtual void Close() OVERRIDE { StopStreaming(); }
	virtual int GetTotalTicks( void ) { return 0; }

	enum FragmentTypeEnum_t
	{
		//FRAGMENT_START,
		FRAGMENT_DELTA,
		FRAGMENT_FULL,
		FRAGMENT_COUNT
	};
	static const char *FragmentTypeToString( FragmentTypeEnum_t nType );
	static const char *AsString( FragmentTypeEnum_t nType ){ return nType == FRAGMENT_DELTA ? "delta" : "full"; }
	struct Buffer_t
	{
		Buffer_t()
		{
			m_nRefCount = 0;
			m_nSize = 0;
		}

		static void AddRef( Buffer_t *pObj )
		{
			pObj->m_nRefCount++;
		}

		static void Release( Buffer_t *pObj )
		{
			if ( 0 == --pObj->m_nRefCount )
			{
				delete[]( uint8* )pObj;
			}
		}

		uint m_nRefCount;
		uint m_nSize;
		uint8 *Base() { return ( uint8* )( this + 1 ); }
		uint8 *End() { return Base() + m_nSize; }
	};
	typedef CSmartPtr< Buffer_t, Buffer_t > BufferRef;
	Buffer_t *GetStreamSignupBuffer() { return m_pStreamSignup.GetObject(); }
	Buffer_t *GetFragmentBuffer( int nFragment, FragmentTypeEnum_t nFragmentType );
	void RequestFragment( int nFragment, FragmentTypeEnum_t nType );
	void ReleaseFragment( int nFragment );

	static GotvHttpStreamId_t GetStreamId( const char *pUrl );
	float GetBroadcastKeyframeInterval() const { return m_flBroadcastKeyframeInterval; }

	IDemoStreamClient::DemoStreamReference_t GetStreamStartReference( bool bLagCompensation = false );

	void BeginBuffering( int nFragment );
protected:

	struct SyncResponse_t
	{
		int nStartTick ;
		float flKeyframeInterval;
		float flRealTimeDelay ;
		float flReceiveAge;
		int nFragment ;
		int nSignupFragment;
		float flTicksPerSecond;
		double dPlatTimeReceived;
	};
	bool OnSync( int nResync );

	void OnStart( HTTPRequestHandle hRequest );
	void OnFragmentRequestSuccess( HTTPRequestHandle hRequest, int nFragment, FragmentTypeEnum_t nType );
	void OnFragmentRequestFailure( EHTTPStatusCode nErrorCode, int nFragment, FragmentTypeEnum_t nType );

	bool OnSync( const char *pBuffer, int nBufferSize, int nResync );

	void RequestDeltaFrame( int nFragment );


protected:

	typedef void( CDemoStreamHttp::*FnHttpCallback )( const HTTPRequestCompleted_t * pResponse );

	class CPendingRequest : public CCallbackBase
	{
	public:
		CPendingRequest( );
		void Init( CDemoStreamHttp *pParent, HTTPRequestHandle hRequest, SteamAPICall_t hCall );
		virtual void Run( void *pvParam ) OVERRIDE; // success; HTTPRequestCompleted_t
		virtual void Run( void *pvParam, bool bIOFailure, SteamAPICall_t hSteamAPICall ) OVERRIDE; // result; HTTPRequestCompleted_t
		void Cancel();

		virtual void OnSuccess( const HTTPRequestCompleted_t * pResponse ) = 0;
		virtual void OnFailure( const HTTPRequestCompleted_t * pResponse );
	public:
		int m_nIncrementalVectorIndex;

	protected:
		~CPendingRequest(); // only this object can delete itself
		virtual int GetCallbackSizeBytes() OVERRIDE { return sizeof( HTTPRequestCompleted_t ); }
		CDemoStreamHttp *m_pParent;
		HTTPRequestHandle m_hRequest;
		SteamAPICall_t m_hCall;
	};
	friend class CPendingRequest;

	class CSyncRequest : public CPendingRequest
	{
		int m_nResync;
		SyncParams_t m_SyncParams;
	public:
		CSyncRequest( SyncParams_t syncParams, int nResync = 0 ) : m_nResync( nResync ), m_SyncParams( syncParams ) { }
		virtual void OnSuccess( const HTTPRequestCompleted_t * pResponse ) OVERRIDE;
		virtual void OnFailure( const HTTPRequestCompleted_t * pResponse ) OVERRIDE;
	};

	class CStartRequest : public CPendingRequest
	{
	public:
		virtual void OnSuccess( const HTTPRequestCompleted_t * pResponse ) OVERRIDE;
		//virtual void OnFailure( const HTTPRequestCompleted_t * pResponse ) OVERRIDE;
	};


	class CFragmentRequest : public CPendingRequest
	{
		int m_nFragment;
		FragmentTypeEnum_t m_nType;
	public:
		CFragmentRequest( int nFragment , FragmentTypeEnum_t nType ) : m_nFragment( nFragment ), m_nType ( nType ){}
		virtual void OnSuccess( const HTTPRequestCompleted_t * pResponse ) OVERRIDE;
		virtual void OnFailure( const HTTPRequestCompleted_t * pResponse ) OVERRIDE;
	};


	enum StateEnum_t
	{
		STATE_IDLE,
		STATE_SYNC,
		STATE_RANDOM_WAIT_AND_SYNC,
		STATE_START,
		STATE_STREAMING
	};

	static Buffer_t *MakeBuffer( HTTPRequestHandle hRequest ); // returns a buffer with 0 refcount

	struct Fragment_t
	{
	protected:
		Buffer_t *m_pField[ FRAGMENT_COUNT ];
		uint m_nStreaming;
	public:

		Fragment_t()
		{
			for ( int i = 0; i < FRAGMENT_COUNT; ++i )
				m_pField[ i ] = NULL;
			m_nStreaming = 0;
		}

		void ResetBuffers();

		
		//const uint8 *Start() const { return pField[ FRAGMENT_START ] ? pField[ FRAGMENT_START ]->Base() : NULL; }
		//uint StartSize()const { return pField[ FRAGMENT_START ] ? pField[ FRAGMENT_START ]->nSize : 0; }
		void SetField( FragmentTypeEnum_t nFragment, Buffer_t *pBuffer );
		Buffer_t *GetField( FragmentTypeEnum_t nFragment ) { return m_pField[ nFragment ]; }
		const uint8 *Delta() const { return m_pField[ FRAGMENT_DELTA ] ? m_pField[ FRAGMENT_DELTA ]->Base() : NULL; }
		uint DeltaSize()const { return m_pField[ FRAGMENT_DELTA ] ? m_pField[ FRAGMENT_DELTA ]->m_nSize : 0; }
		const uint8 *Full() const { return m_pField[ FRAGMENT_FULL ] ? m_pField[ FRAGMENT_FULL ]->Base() : NULL; }
		uint FullSize()const { return m_pField[ FRAGMENT_FULL ] ? m_pField[ FRAGMENT_FULL ]->m_nSize : 0; }

		uint IsStreaming( FragmentTypeEnum_t nType ){ return ( m_nStreaming >> nType ) & 1; }
		void SetStreaming( FragmentTypeEnum_t nType ){ m_nStreaming |= ( 1 << nType ); }
		void ClearStreaming( FragmentTypeEnum_t nType ){ m_nStreaming &= ~( 1 << nType ); }
	};


protected:
	void SendGet( const char *pPath, CPendingRequest *pRequest );
	Fragment_t &Fragment( int nFragment );
protected:
	CUtlString m_Url;
	bool m_bSyncFromGc;
	CUtlIncrementalVector< CPendingRequest > m_PendingRequests;
	StateEnum_t m_nState;
	
	BufferRef m_pStreamSignup;
	int	m_nStreamSignupFragment; // the fragment where the signup/start fragment starts

	int m_nDemoProtocol;
	IDemoStreamClient *m_pClient;

	CUtlHashtable< int, Fragment_t, IdentityHashFunctor > m_FragmentCache; // TODO: implement freeing old buffers

	// STATE_SYNC                : when to stop streaming
	// STATE_RANDOM_WAIT_AND_SYNC: when to retry sync
	double m_dSyncTimeoutEnd;

	SyncParams_t m_SyncParams;
	SyncResponse_t m_SyncResponse;

	float m_flBroadcastKeyframeInterval;

	//int m_nFragment;
};




#endif // HLTV_BROADCAST_PLAYER_HDR