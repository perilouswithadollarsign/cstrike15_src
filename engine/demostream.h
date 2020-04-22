//========= Copyright (c), Valve Corporation, All rights reserved. ============//

#ifndef DEMO_STREAM_HDR
#define DEMO_STREAM_HDR

class CDemoFile;

// this is both an interface and a default, empty implementation of a demo stream 
// The empty implementation is needed to avoid having NULL demostream - all the legacy d
class IDemoStream
{
public:
	virtual ~IDemoStream() {}
	virtual bool IsOpen() { return false; }
	virtual CDemoFile *IsDemoFile() { return NULL;  } // by default, it is NOT a demofile

	virtual void Close() {}
	virtual const char* GetUrl( void ) { return ""; }
	virtual float GetTicksPerSecond( void ) { return 64; }
	virtual float GetTicksPerFrame( void ) { return 1; }
	virtual int	GetTotalTicks( void ) { return 0; }
};


class IDemoStreamClient
{
public:
	struct DemoStreamReference_t
	{
		int nTick;
		int nSkipTicks;
		int nFragment;
	};

	virtual void OnDemoStreamStart( const DemoStreamReference_t &start, int nResync ) {}
	virtual bool OnDemoStreamRestarting(){ return false; }
	virtual void OnDemoStreamStop() {}
	virtual void OnDemoStreamDelta() {}
};


#endif // DEMO_STREAM_HDR