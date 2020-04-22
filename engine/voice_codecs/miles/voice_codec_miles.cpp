//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "quakedef.h"
#include "iframeencoder.h"
#include "interface.h"
#include "milesbase.h"
#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void Con_Printf( const char *pMsg, ... )
{
}


class FrameEncoder_Miles : public IFrameEncoder
{
protected:
	virtual			~FrameEncoder_Miles();

public:
					FrameEncoder_Miles();

	virtual bool	Init(int quality, int &rawFrameSize, int &encodedFrameSize);
	virtual void	Release();
	virtual void	EncodeFrame(const char *pUncompressed, char *pCompressed);
	virtual void	DecodeFrame(const char *pCompressed, char *pDecompressed);
	virtual bool	ResetState();


public:

	void			Shutdown();

	static S32 AILCALLBACK EncodeStreamCB(
		UINTa user,				// User value passed to ASI_open_stream()
		void *dest,				// Location to which stream data should be copied by app
		S32 bytes_requested,	// # of bytes requested by ASI codec
		S32 offset				// If not -1, application should seek to this point in stream
		);

	static S32 AILCALLBACK DecodeStreamCB(
		UINTa user,				// User value passed to ASI_open_stream()
		void *dest,				// Location to which stream data should be copied by app
		S32 bytes_requested,	// # of bytes requested by ASI codec
		S32 offset				// If not -1, application should seek to this point in stream
		);

	void			FigureOutFrameSizes();
	

private:

	// Encoder stuff.	
	ASISTRUCT			m_Encoder;

	// Decoder stuff.
	ASISTRUCT			m_Decoder;

	// Destination for encoding and decoding.
	const char			*m_pSrc;
	int					m_SrcLen;
	int					m_CurSrcPos;

	// Frame sizes..
	int					m_nRawBytes;
	int					m_nEncodedBytes;
};



// ------------------------------------------------------------------------ //
// Helper functions.
// ------------------------------------------------------------------------ //
void Convert16UnsignedToSigned(short *pDest, int nSamples)
{
	for(int i=0; i < nSamples; i++)
	{
		int val = *((unsigned short*)&pDest[i]) - (1 << 15);
		pDest[i] = (short)val;
	}
}


void Convert16SignedToUnsigned(short *pDest, int nSamples)
{
	for(int i=0; i < nSamples; i++)
	{
		int val = *((short*)&pDest[i]) + (1 << 15);
		*((unsigned short*)&pDest[i]) = (unsigned short)val;
	}
}


// ------------------------------------------------------------------------ //
// FrameEncoder_Miles functions.
// ------------------------------------------------------------------------ //
FrameEncoder_Miles::FrameEncoder_Miles()
{
}


FrameEncoder_Miles::~FrameEncoder_Miles()
{
	Shutdown();
}

bool FrameEncoder_Miles::Init(int quality, int &rawFrameSize, int &encodedFrameSize)
{
	Shutdown();


	// This tells what protocol we're using.
	C8 suffix[128] = ".v12"; // (.v12, .v24, .v29, or .raw)

	// encoder converts from RAW to v12
	if ( !m_Encoder.Init( (void *)this, ".RAW", suffix, &FrameEncoder_Miles::EncodeStreamCB ) )
	{
		Con_Printf("(FrameEncoder_Miles): Can't initialize ASI encoder.\n");
		Shutdown();
		return false;
	}
	
	// decoder converts from v12 to RAW
	if ( !m_Decoder.Init( (void *)this, suffix, ".RAW", &FrameEncoder_Miles::DecodeStreamCB ) )
	{
		Con_Printf("(FrameEncoder_Miles): Can't initialize ASI decoder.\n");
		Shutdown();
		return false;
	}


	FigureOutFrameSizes();

	
	// Output..	
	rawFrameSize = m_nRawBytes * 2; // They'll be using 16-bit samples and we're quantizing to 8-bit.
	encodedFrameSize = m_nEncodedBytes;

	return true;
}


void FrameEncoder_Miles::Release()
{
	delete this;
}


void FrameEncoder_Miles::EncodeFrame(const char *pUncompressedBytes, char *pCompressed)
{
	char samples[1024];

	if(!m_Encoder.IsActive() || m_nRawBytes > sizeof(samples))
		return;

	const short *pUncompressed = (const short*)pUncompressedBytes;
	for(int i=0; i < m_nRawBytes; i++)
		samples[i] = (char)(pUncompressed[i] >> 8);

	m_pSrc = samples;
	m_SrcLen = m_nRawBytes;
	m_CurSrcPos = 0;

	U32 len = m_Encoder.Process( pCompressed, m_nEncodedBytes );
	if ( len != (U32)m_nEncodedBytes )
	{
		Assert(0);
	}
}


void FrameEncoder_Miles::DecodeFrame(const char *pCompressed, char *pDecompressed)
{
	if(!m_Decoder.IsActive())
		return;

	m_pSrc = pCompressed;
	m_SrcLen = m_nEncodedBytes;
	m_CurSrcPos = 0;

	U32 outputSize = m_nRawBytes*2;
	U32 len = m_Decoder.Process( pDecompressed, outputSize );
	
	if (len != outputSize)
	{
		Assert(0);
	}
}


void FrameEncoder_Miles::Shutdown()
{
	m_Decoder.Shutdown();
	m_Encoder.Shutdown();
}


bool FrameEncoder_Miles::ResetState()
{
	if(!m_Decoder.IsActive() || !m_Encoder.IsActive())
		return true;

	for(int i=0; i < 2; i++)
	{
		char data[2048], compressed[2048];
		memset(data, 0, sizeof(data));
		m_pSrc = data;
		m_SrcLen = m_nRawBytes;
		m_CurSrcPos = 0;
		
		U32 len = m_Encoder.Process( compressed, m_nEncodedBytes );
		if ( len != (U32)m_nEncodedBytes )
		{
			Assert(0);
		}

		m_pSrc = compressed;
		m_SrcLen = m_nEncodedBytes;
		m_CurSrcPos = 0;

		m_Decoder.Process( data, m_nRawBytes * 2 );
	}

	// Encode and decode a couple frames of zeros.
	return true;
}


S32 AILCALLBACK FrameEncoder_Miles::EncodeStreamCB(
	UINTa user,				// User value passed to ASI_open_stream()
	void *dest,				// Location to which stream data should be copied by app
	S32 bytes_requested,	// # of bytes requested by ASI codec
	S32 offset				// If not -1, application should seek to this point in stream
	)
{
	FrameEncoder_Miles *pThis = (FrameEncoder_Miles*)user;
	Assert(pThis && offset == -1);

	// Figure out how many samples we can safely give it.
	int maxSamples = pThis->m_SrcLen - pThis->m_CurSrcPos;
	int samplesToGive = MIN(maxSamples, bytes_requested/2);

	// Convert to 16-bit signed mono.
	short *pOut = (short*)dest;
	for(int i=0; i < samplesToGive; i++)
	{
		pOut[i] = pThis->m_pSrc[pThis->m_CurSrcPos+i] << 8;
	}

	pThis->m_CurSrcPos += samplesToGive;
	return samplesToGive * 2;
}

S32 AILCALLBACK FrameEncoder_Miles::DecodeStreamCB(
	UINTa user,				// User value passed to ASI_open_stream()
	void *dest,				// Location to which stream data should be copied by app
	S32 bytes_requested,	// # of bytes requested by ASI codec
	S32 offset				// If not -1, application should seek to this point in stream
	)
{
	FrameEncoder_Miles *pThis = (FrameEncoder_Miles*)user;
	Assert(pThis && offset == -1);

	int maxBytes = pThis->m_SrcLen - pThis->m_CurSrcPos;
	int bytesToGive = MIN(maxBytes, bytes_requested);
	memcpy(dest, &pThis->m_pSrc[pThis->m_CurSrcPos], bytesToGive);

	pThis->m_CurSrcPos += bytesToGive;
	return bytesToGive;
}


void FrameEncoder_Miles::FigureOutFrameSizes()
{
	// Figure out the frame sizes. It is probably not prudent in general to assume fixed frame sizes with Miles codecs
	// but it works with the voxware codec right now and simplifies things a lot.
	m_nRawBytes = (int)m_Encoder.GetProperty( m_Encoder.INPUT_BLOCK_SIZE );
	
	char uncompressed[1024];
	char compressed[1024];

	Assert(m_nRawBytes <= sizeof(uncompressed));
	
	m_pSrc = uncompressed;
	m_SrcLen = m_nRawBytes;
	m_CurSrcPos = 0;

	m_nEncodedBytes = (int)m_Encoder.Process( compressed, sizeof(compressed) );
}



class IVoiceCodec;
extern IVoiceCodec* CreateVoiceCodec_Frame(IFrameEncoder *pEncoder);
void* CreateVoiceCodec_Miles()
{
	IFrameEncoder *pEncoder = new FrameEncoder_Miles;
	if(!pEncoder)
		return NULL;

	return CreateVoiceCodec_Frame(pEncoder);
}

EXPOSE_INTERFACE_FN(CreateVoiceCodec_Miles, IVoiceCodec, "vaudio_miles")


