//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "interface.h"
#include "milesbase.h"
#include "vaudio/ivaudio.h"
#include "dbg.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static S32 AILCALLBACK AudioStreamEventCB( UINTa user, void *dest, S32 bytes_requested, S32 offset )
{
	IAudioStreamEvent *pThis = static_cast<IAudioStreamEvent*>( (void *)user);
	
#if defined( OSX ) && !defined( PLATFORM_64BITS )
	// save of the args to local stack vars before we screw with ESP
	volatile void *newDest = dest;
	volatile S32 newBytes = bytes_requested;
	volatile S32 newOffset = offset;
	volatile void *oldESP;
	
	// now move ESP to be aligned to 16-bytes
	asm volatile(
	 "movl    %%esp,%0\n"
	"subl    $16,%%esp\n"
	"andl    $-0x10,%%esp\n"
	:  "=m" (oldESP) : : "%esp");
	int val = pThis->StreamRequestData( (void *)newDest, newBytes, newOffset );

	// undo the alignment
	asm( "movl    %0,%%esp\n" ::"m" (oldESP) );

	return val;
#else
	return pThis->StreamRequestData( dest, bytes_requested, offset );
#endif	
}

class CMilesMP3 : public IAudioStream
{
public:
	CMilesMP3();
	bool Init( IAudioStreamEvent *pHandler );
	virtual ~CMilesMP3();
	
	// IAudioStream functions
	virtual int	Decode( void *pBuffer, unsigned int bufferSize );
	virtual int GetOutputBits();
	virtual int GetOutputRate();
	virtual int GetOutputChannels();
	virtual unsigned int GetPosition();
	virtual void SetPosition( unsigned int position );
private:
	ASISTRUCT		m_decoder;
};

CMilesMP3::CMilesMP3()
{
}

bool CMilesMP3::Init( IAudioStreamEvent *pHandler )
{
	return m_decoder.Init( pHandler, ".MP3", ".RAW", &AudioStreamEventCB );
}


CMilesMP3::~CMilesMP3()
{
	m_decoder.Shutdown();
}
	

// IAudioStream functions
int	CMilesMP3::Decode( void *pBuffer, unsigned int bufferSize )
{
	return m_decoder.Process( pBuffer, bufferSize );
}


int CMilesMP3::GetOutputBits()
{
	return m_decoder.GetProperty( m_decoder.OUTPUT_BITS );
}


int CMilesMP3::GetOutputRate()
{
	return m_decoder.GetProperty( m_decoder.OUTPUT_RATE );
}


int CMilesMP3::GetOutputChannels()
{
	return m_decoder.GetProperty( m_decoder.OUTPUT_CHANNELS );
}


unsigned int CMilesMP3::GetPosition()
{
	return m_decoder.GetProperty( m_decoder.POSITION );
}

// NOTE: Only supports seeking forward right now
void CMilesMP3::SetPosition( unsigned int position )
{
	m_decoder.Seek( position );
}


class CVAudio : public IVAudio
{
public:
	CVAudio()
	{
		// Assume the user will be creating multiple miles objects, so
		// keep miles running while this exists
		IncrementRefMiles();
	}

	virtual ~CVAudio()
	{
		DecrementRefMiles();
	}

	IAudioStream *CreateMP3StreamDecoder( IAudioStreamEvent *pEventHandler )
	{
		CMilesMP3 *pMP3 = new CMilesMP3;
		if ( !pMP3->Init( pEventHandler ) )
		{
			delete pMP3;
			return NULL;
		}
		return pMP3;
	}

	void DestroyMP3StreamDecoder( IAudioStream *pDecoder )
	{
		delete pDecoder;
	}

	void *CreateMilesAudioEngine()
	{
		IncrementRefMiles();
		HDIGDRIVER hDriver = AIL_open_digital_driver( 44100, 16, MSS_MC_51_DISCRETE, 0 );
		return (void *)hDriver;
	}

	void DestroyMilesAudioEngine( void *pEngine )
	{
		HDIGDRIVER hDriver = HDIGDRIVER(pEngine);
		AIL_close_digital_driver( hDriver );
		DecrementRefMiles();
	}
};

void* CreateVoiceCodec_CVAudio()
{
	return new CVAudio;
}

EXPOSE_INTERFACE_FN(CreateVoiceCodec_CVAudio, IVAudio, VAUDIO_INTERFACE_VERSION );

