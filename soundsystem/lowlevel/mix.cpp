#include "basetypes.h"
#include "mathlib/ssemath.h"
#include "soundsystem/lowlevel.h"
#include "mix.h"
#include "tier0/vprof.h"

// simple inline to test alignemnt of a value
inline bool IsAlign4( uint nAlign )
{
	return ( nAlign & 3 ) == 0;
}

inline bool IsAligned16Bytes( void *p )
{
	return ( uintp( p ) & 0xF ) ? false : true;
}

// this processes the low-level mix command list and produces pResults
void ProcessAudioMix( CAudioMixResults *pResults, const CAudioMixState &mixState, CAudioMixDescription &mixSetup )
{
	// set up with current counts
	pResults->m_pOutput.RemoveAll();

	pResults->m_pOutput.SetCount( mixSetup.m_nMixBufferMax );
	pResults->m_debugOutputs.SetCount( mixSetup.m_nDebugOutputCount );
	pResults->m_flOutputLevels.SetCount( mixSetup.m_nOutputLevelCount );

	// now run the commands
	VPROF("IAudioMix::Process");
	for ( int i = 0; i < mixSetup.m_commands.Count(); i++ )
	{
		audio_mix_command_t &cmd = mixSetup.m_commands[i];
		switch( cmd.m_nCommandId )
		{
		case AUDIO_MIX_CLEAR:
			SilenceBuffer( pResults->m_pOutput[ cmd.m_nOutput ].m_flData );
			break;
		
		case AUDIO_MIX_EXTRACT_SOURCE:
			ConvertSourceToFloat( *mixState.GetInput( cmd.m_nInput0 ), cmd.m_flParam1, pResults->m_pOutput[cmd.m_nOutput].m_flData, mixState.GetOutput( cmd.m_nInput0 ) );
			break;
		
		case AUDIO_MIX_ADVANCE_SOURCE:
			AdvanceSource( *mixState.GetInput( cmd.m_nInput0 ), cmd.m_flParam1, mixState.GetOutput( cmd.m_nInput0 ) );
			break;
		
		case AUDIO_MIX_MULTIPLY:
			ScaleBuffer( pResults->m_pOutput[cmd.m_nOutput].m_flData, pResults->m_pOutput[cmd.m_nInput0].m_flData, cmd.m_flParam0 );
			break;

		case AUDIO_MIX_PROCESS:
			{
				CAudioProcessor *pProc = mixSetup.m_processors[cmd.m_nInput1];
				pProc->Process( &pResults->m_pOutput[cmd.m_nInput0], &pResults->m_pOutput[cmd.m_nOutput], int(cmd.m_flParam0), mixState.DSPGlobals() );
			}
			break;

		case AUDIO_MIX_ACCUMULATE:
			MixBuffer( pResults->m_pOutput[cmd.m_nOutput].m_flData, pResults->m_pOutput[cmd.m_nInput0].m_flData, cmd.m_flParam0 );
			break;
		case AUDIO_MIX_ACCUMULATE_RAMP:
			MixBufferRamp( pResults->m_pOutput[cmd.m_nOutput].m_flData, pResults->m_pOutput[cmd.m_nInput0].m_flData, cmd.m_flParam0, cmd.m_flParam1 );
			break;

		case AUDIO_MIX_SUM:
			SumBuffer2x1( pResults->m_pOutput[cmd.m_nOutput].m_flData, pResults->m_pOutput[cmd.m_nInput0].m_flData, cmd.m_flParam0, pResults->m_pOutput[cmd.m_nInput1].m_flData, cmd.m_flParam1 );
			break;
		case AUDIO_MIX_SWAP:
			SwapBuffersInPlace( pResults->m_pOutput[cmd.m_nOutput].m_flData, pResults->m_pOutput[cmd.m_nInput0].m_flData );
			break;
		case AUDIO_MIX_MEASURE_DEBUG_LEVEL:
			{
				int nChannelCount = cmd.m_nInput1;
				mix_debug_outputs_t &debugOut = pResults->m_debugOutputs[cmd.m_nOutput];
				debugOut.m_flLevel = 0.0f;
				const float flScale = 1.0f / 32768.0f;
				for ( int nChan = 0; nChan < nChannelCount; nChan++ )
				{
					debugOut.m_flChannelLevels[nChan] = flScale * BufferLevel( pResults->m_pOutput[cmd.m_nInput0 + nChan].m_flData );
					debugOut.m_flLevel = Max( debugOut.m_flLevel, debugOut.m_flChannelLevels[nChan] );
				}
				debugOut.m_nChannelCount = nChannelCount;
			}
			break;
		case AUDIO_MIX_OUTPUT_LEVEL:
			{
				int nChannelCount = cmd.m_nInput1;
				float flLevel = 0.0f;
				const float flScale = 1.0f / 32768.0f;

				for ( int nChan = 0; nChan < nChannelCount; nChan++ )
				{
					float flOut = flScale * AvergeBufferAmplitude( pResults->m_pOutput[cmd.m_nInput0 + nChan].m_flData );
					flLevel = Max( flLevel, flOut );
				}
				pResults->m_flOutputLevels[cmd.m_nOutput] = clamp( flLevel, 0.0f, 1.0f );
			}
			break;
		default:
			Assert( 0 );
			//AssertMsg( 0, "Unknown mix command %d\n", int(cmd.m_nCommandId) );
			break;
		}
	}
}

void CAudioMixCommandList::ClearMultichannel( uint16 nTarget, int nCount ) 
{
	for ( int i = 0; i < nCount; i++ )
	{
		audio_mix_command_t cmd;
		cmd.Init( AUDIO_MIX_CLEAR, nTarget + i );
		m_commands.AddToTail( cmd );
	}
}

void CAudioMixCommandList::ScaleMultichannel( uint16 nOutput, uint16 nInput, int nCount, float flVolume )
{
	for ( int i = 0; i < nCount; i++ )
	{
		audio_mix_command_t cmd;
		cmd.Init( AUDIO_MIX_MULTIPLY, nOutput + i, nInput + i, flVolume );
		m_commands.AddToTail( cmd );
	}
}


void CAudioMixCommandList::AccumulateMultichannel( uint16 nOutput, int nOutputChannels, uint16 nInput, int nInputChannels, float flInputVolume )
{
	if ( nOutputChannels == nInputChannels )
	{
		for ( int i = 0; i < nInputChannels; i++ )
		{
			AccumulateToBuffer( nOutput + i, nInput + i, flInputVolume );
		}
	}
	else
	{
		// need to downmix or expand channels
		if ( nOutputChannels == 2 )
		{
			// downmix 6 ch to 2 ch
			Assert( nInputChannels == 6 ); // other cases should have been handled above or there's more code to write
			// out.left += 0.5 * (in.left + in.center*0.5) + 0.5 * in.rear_left
			AccumulateToBuffer( nOutput + 0, nInput + 0, flInputVolume * 0.5f );
			AccumulateToBuffer( nOutput + 0, nInput + 2, flInputVolume * 0.25f );
			AccumulateToBuffer( nOutput + 0, nInput + 4, flInputVolume * 0.5f );
			// out.right += 0.5 * (in.right + in.center*0.5) + 0.5 * in.rear_right
			AccumulateToBuffer( nOutput + 1, nInput + 1, flInputVolume * 0.5f );
			AccumulateToBuffer( nOutput + 1, nInput + 2, flInputVolume * 0.25f );
			AccumulateToBuffer( nOutput + 1, nInput + 5, flInputVolume * 0.5f );
		}
		else if ( nOutputChannels == 6 )
		{
			// expand 2ch to 6 ch
			Assert( nInputChannels == 2 );
			// out.left += in.left
			AccumulateToBuffer( nOutput + 0, nInput + 0, flInputVolume );
			// out.right += in.right
			AccumulateToBuffer( nOutput + 1, nInput + 1, flInputVolume );
			// out.center = 0.5f * (in.left + in.right)
			AccumulateToBuffer( nOutput + 2, nInput + 0, flInputVolume * 0.5f );
			AccumulateToBuffer( nOutput + 2, nInput + 1, flInputVolume * 0.5f );
			// out.rear_left += in.left
			AccumulateToBuffer( nOutput + 4, nInput + 0, flInputVolume );
			// out.rear_right += in.right
			AccumulateToBuffer( nOutput + 5, nInput + 1, flInputVolume );
		}
		else if ( nOutputChannels == 8 && (nInputChannels == 2 || nInputChannels == 6) )
		{
			// right now we just use this for solo/debug, copy
			for ( int i = 0; i < nInputChannels; i++ )
			{
				AccumulateToBuffer( nOutput + i, nInput + i, flInputVolume );
			}
		}
		else
		{
			// some other case we haven't implemented
			Assert(0);
		}
	}

}

FORCEINLINE shortx8 ShiftRightShortSIMD( const shortx8 &inputValue, const shortx8 &shiftBitCount )
{
	return _mm_srl_epi16( inputValue, shiftBitCount );
}

FORCEINLINE shortx8 SignedExtractLowAsInt32( const shortx8 &a )
{
	shortx8 signExtend = _mm_cmplt_epi16( a, _mm_setzero_si128() );
	return _mm_unpacklo_epi16( a, signExtend );
}

FORCEINLINE shortx8 SignedExtractHighAsInt32( const shortx8 &a )
{
	shortx8 signExtend = _mm_cmplt_epi16( a, _mm_setzero_si128() );
	return _mm_unpackhi_epi16( a, signExtend );
}

FORCEINLINE shortx8 RoundtFloatToInt32( const fltx4 &input )
{
	return _mm_cvtps_epi32( input );
}

FORCEINLINE shortx8 PackInt32x2ToShortx8( const shortx8 &input0, const shortx8 &input1 )
{
	return _mm_packs_epi32( input0, input1 );
}

// Load 4 aligned words into a SIMD register
FORCEINLINE shortx8 LoadAlignedShortx8SIMD( const void * RESTRICT pSIMD )
{
	return _mm_load_si128( reinterpret_cast<const __m128i *>( pSIMD ) );
}

// Load 4 unaligned words into a SIMD register
FORCEINLINE shortx8 LoadUnalignedShortx8SIMD( const void * RESTRICT pSIMD )
{
	return _mm_loadu_si128( reinterpret_cast<const __m128i *>( pSIMD ) );
}

// create a stereo interleaved signed-16 buffer from two float-32 buffers
void ConvertFloat32Int16_Clamp_Interleave2_Unaligned( short *pOut, float *pflInputLeft, float *pflInputRight, int nSampleCount )
{
	if ( nSampleCount >= 8 )
	{
		int nSampleQuads = nSampleCount >> 2;
		// truncate sample count to remainder after 4-bundles
		nSampleCount &= 3;

		short *pWrite = pOut;
		for ( int i = 0; i < nSampleQuads; i++ )
		{
			// load 4 samples from left and four from right
			fltx4 leftSamples = LoadAlignedSIMD( pflInputLeft );
			pflInputLeft += 4;
			fltx4 rightSamples = LoadAlignedSIMD( pflInputRight );
			pflInputRight += 4;
			shortx8 nLeft = RoundtFloatToInt32( leftSamples );
			shortx8 nRight = RoundtFloatToInt32( rightSamples );
			// interleave into L/R pairs
			shortx8 nInterleavedLow = _mm_unpacklo_epi32( nLeft, nRight );
			shortx8 nInterleavedHigh = _mm_unpackhi_epi32( nLeft, nRight );
			// pack 
			shortx8 nOut = PackInt32x2ToShortx8( nInterleavedLow, nInterleavedHigh );
			StoreUnalignedSIMD( pWrite, nOut );
			pWrite += 8;
		}
	}

	// now convert and clamp any remaining samples (not in SIMD 4-bundles)
	for ( int i = 0; i < nSampleCount; i++ )
	{
		int l = (int)pflInputLeft[i];
		if ( l < -32768 ) l = -32768;
		if ( l > 32767 ) l = 32767;
		int r = (int)pflInputRight[i];
		if ( r < -32768 ) r = -32768;
		if ( r > 32767 ) r = 32767;
		pOut[0] = l;
		pOut[1] = r;
		pOut += 2;
	}
}

void ConvertFloat32Int16_Clamp_Interleave2( short *pOut, float *pflInputLeft, float *pflInputRight, int nSampleCount )
{
	if ( !IsAligned16Bytes(pOut) )
	{
		ConvertFloat32Int16_Clamp_Interleave2_Unaligned( pOut, pflInputLeft, pflInputRight, nSampleCount );
		return;
	}
	if ( nSampleCount >= 8 )
	{
		int nSampleQuads = nSampleCount >> 2;

		// truncate sample count to remainder after 4-bundles
		nSampleCount &= 3;

		short *pWrite = pOut;
		for ( int i = 0; i < nSampleQuads; i++ )
		{
			// load 4 samples from left and four from right
			fltx4 leftSamples = LoadAlignedSIMD( pflInputLeft );
			pflInputLeft += 4;
			fltx4 rightSamples = LoadAlignedSIMD( pflInputRight );
			pflInputRight += 4;
			shortx8 nLeft = RoundtFloatToInt32( leftSamples );
			shortx8 nRight = RoundtFloatToInt32( rightSamples );
			shortx8 nInterleavedLow = _mm_unpacklo_epi32( nLeft, nRight );
			shortx8 nInterleavedHigh = _mm_unpackhi_epi32( nLeft, nRight );
			shortx8 nOut = PackInt32x2ToShortx8( nInterleavedLow, nInterleavedHigh );
			StoreAlignedSIMD( pWrite, nOut );

			pWrite += 8;
		}
	}

	// now convert and clamp any remaining samples (not in SIMD 4-bundles)
	for ( int i = 0; i < nSampleCount; i++ )
	{
		int l = (int)pflInputLeft[i];
		if ( l < -32768 ) l = -32768;
		if ( l > 32767 ) l = 32767;
		int r = (int)pflInputRight[i];
		if ( r < -32768 ) r = -32768;
		if ( r > 32767 ) r = 32767;
		pOut[0] = l;
		pOut[1] = r;
		pOut += 2;
	}
}

// Faster SIMD version for 6-in, 6-out
void ConvertFloat32Int16_Clamp_Interleave6( short *pOut, int nOutputChannelCount, int nChannelStrideFloats, float *pflChannel0, int nInputChannelCount, int nSampleCount )
{
	Assert( nOutputChannelCount == 6 && nInputChannelCount == 6 && IsAligned16Bytes( pflChannel0 ) );
	const float *pInput0 = pflChannel0;
	const float *pInput1 = pflChannel0 + nChannelStrideFloats;
	const float *pInput2 = pflChannel0 + 2*nChannelStrideFloats;
	const float *pInput3 = pflChannel0 + 3*nChannelStrideFloats;
	const float *pInput4 = pflChannel0 + 4*nChannelStrideFloats;
	const float *pInput5 = pflChannel0 + 5*nChannelStrideFloats;
	short *pWrite = pOut;
	// process 24 samples per loop, grab 6 bundles of 4, write out 3 bundles of 8
	for ( int i = 0; i < nSampleCount; i += 4 )
	{
		// grab 6 bundles of 4 samples
		fltx4 fl4Samples0 = LoadAlignedSIMD( pInput0 + i ); //  0  6 12 18
		fltx4 fl4Samples1 = LoadAlignedSIMD( pInput1 + i ); //  1  7 13 19
		fltx4 fl4Samples2 = LoadAlignedSIMD( pInput2 + i ); //  2  8 14 20
		fltx4 fl4Samples3 = LoadAlignedSIMD( pInput3 + i ); //  3  9 15 21
		fltx4 fl4Samples4 = LoadAlignedSIMD( pInput4 + i ); //  4 10 16 22
		fltx4 fl4Samples5 = LoadAlignedSIMD( pInput5 + i ); //  5 11 17 23

		// interleave into pairs
		fltx4 fl4Pair0 = _mm_shuffle_ps( fl4Samples0, fl4Samples1, MM_SHUFFLE_REV( 0, 1, 0, 1 ) );	//  0  6  1  7
		fltx4 fl4Pair1 = _mm_shuffle_ps( fl4Samples0, fl4Samples1, MM_SHUFFLE_REV( 2, 3, 2, 3 ) );	// 12 18 13 19
		fltx4 fl4Pair2 = _mm_shuffle_ps( fl4Samples2, fl4Samples3, MM_SHUFFLE_REV( 0, 1, 0, 1 ) );	//  2  8  3  9
		fltx4 fl4Pair3 = _mm_shuffle_ps( fl4Samples2, fl4Samples3, MM_SHUFFLE_REV( 2, 3, 2, 3 ) );	// 14 20 15 21
		fltx4 fl4Pair4 = _mm_shuffle_ps( fl4Samples4, fl4Samples5, MM_SHUFFLE_REV( 0, 1, 0, 1 ) );	//  4 10  5 11
		fltx4 fl4Pair5 = _mm_shuffle_ps( fl4Samples4, fl4Samples5, MM_SHUFFLE_REV( 2, 3, 2, 3 ) );	// 16 22 17 23

		// now put in final order
		fltx4 fl4Out0 = _mm_shuffle_ps( fl4Pair0, fl4Pair2, MM_SHUFFLE_REV( 0, 2, 0, 2 ) );	//  0  1  2  3
		fltx4 fl4Out1 = _mm_shuffle_ps( fl4Pair4, fl4Pair0, MM_SHUFFLE_REV( 0, 2, 1, 3 ) );	//  4  5  6  7
		fltx4 fl4Out2 = _mm_shuffle_ps( fl4Pair2, fl4Pair4, MM_SHUFFLE_REV( 1, 3, 1, 3 ) );	//  8  9 10 11
		fltx4 fl4Out3 = _mm_shuffle_ps( fl4Pair1, fl4Pair3, MM_SHUFFLE_REV( 0, 2, 0, 2 ) );	// 12 13 14 15
		fltx4 fl4Out4 = _mm_shuffle_ps( fl4Pair5, fl4Pair1, MM_SHUFFLE_REV( 0, 2, 1, 3 ) );	// 16 17 18 19
		fltx4 fl4Out5 = _mm_shuffle_ps( fl4Pair3, fl4Pair5, MM_SHUFFLE_REV( 1, 3, 1, 3 ) );	// 20 21 22 23

		// pack into 3 bundles of 8
		shortx8 nOut0 = PackInt32x2ToShortx8( RoundtFloatToInt32( fl4Out0 ), RoundtFloatToInt32( fl4Out1 ) );
		shortx8 nOut1 = PackInt32x2ToShortx8( RoundtFloatToInt32( fl4Out2 ), RoundtFloatToInt32( fl4Out3 ) );
		shortx8 nOut2 = PackInt32x2ToShortx8( RoundtFloatToInt32( fl4Out4 ), RoundtFloatToInt32( fl4Out5 ) );
		// NOTE: Optimize alignment?
		StoreUnalignedSIMD( pWrite, nOut0 );
		StoreUnalignedSIMD( pWrite + 8, nOut1 );
		StoreUnalignedSIMD( pWrite + 16, nOut2 );
		pWrite += 24;
	}
}


// Faster SIMD version for 8-in, 8-out
void ConvertFloat32Int16_Clamp_Interleave8( short *pOut, int nOutputChannelCount, int nChannelStrideFloats, float *pflChannel0, int nInputChannelCount, int nSampleCount )
{
	Assert( nOutputChannelCount == 8 && nInputChannelCount == 8 && IsAligned16Bytes( pflChannel0 ) );
	const float *pInput0 = pflChannel0;
	const float *pInput1 = pflChannel0 + nChannelStrideFloats;
	const float *pInput2 = pflChannel0 + 2 * nChannelStrideFloats;
	const float *pInput3 = pflChannel0 + 3 * nChannelStrideFloats;
	const float *pInput4 = pflChannel0 + 4 * nChannelStrideFloats;
	const float *pInput5 = pflChannel0 + 5 * nChannelStrideFloats;
	const float *pInput6 = pflChannel0 + 6 * nChannelStrideFloats;
	const float *pInput7 = pflChannel0 + 7 * nChannelStrideFloats;
	short *pWrite = pOut;
	// process 32 samples per loop, grab 6 bundles of 4, write out 4 bundles of 8
	for ( int i = 0; i < nSampleCount; i += 4 )
	{
		// grab 8 bundles of 4 samples
		fltx4 fl4Samples0 = LoadAlignedSIMD( pInput0 + i ); //  0  8 16 24
		fltx4 fl4Samples1 = LoadAlignedSIMD( pInput1 + i ); //  1  9 17 25
		fltx4 fl4Samples2 = LoadAlignedSIMD( pInput2 + i ); //  2 10 18 26
		fltx4 fl4Samples3 = LoadAlignedSIMD( pInput3 + i ); //  3 11 19 27
		fltx4 fl4Samples4 = LoadAlignedSIMD( pInput4 + i ); //  4 12 20 28
		fltx4 fl4Samples5 = LoadAlignedSIMD( pInput5 + i ); //  5 13 21 29
		fltx4 fl4Samples6 = LoadAlignedSIMD( pInput6 + i ); //  6 14 22 30 
		fltx4 fl4Samples7 = LoadAlignedSIMD( pInput7 + i ); //  7 15 23 31

		// interleave into pairs
		fltx4 fl4Pair0 = _mm_shuffle_ps( fl4Samples0, fl4Samples1, MM_SHUFFLE_REV( 0, 1, 0, 1 ) );	//  0  8  1  9
		fltx4 fl4Pair1 = _mm_shuffle_ps( fl4Samples0, fl4Samples1, MM_SHUFFLE_REV( 2, 3, 2, 3 ) );	// 16 24 17 25
		fltx4 fl4Pair2 = _mm_shuffle_ps( fl4Samples2, fl4Samples3, MM_SHUFFLE_REV( 0, 1, 0, 1 ) );	//  2 10  3 11
		fltx4 fl4Pair3 = _mm_shuffle_ps( fl4Samples2, fl4Samples3, MM_SHUFFLE_REV( 2, 3, 2, 3 ) );	// 18 26 19 27
		fltx4 fl4Pair4 = _mm_shuffle_ps( fl4Samples4, fl4Samples5, MM_SHUFFLE_REV( 0, 1, 0, 1 ) );	//  4 12  5 13
		fltx4 fl4Pair5 = _mm_shuffle_ps( fl4Samples4, fl4Samples5, MM_SHUFFLE_REV( 2, 3, 2, 3 ) );	// 20 28 21 29
		fltx4 fl4Pair6 = _mm_shuffle_ps( fl4Samples6, fl4Samples7, MM_SHUFFLE_REV( 0, 1, 0, 1 ) );	//  6 14  7 15
		fltx4 fl4Pair7 = _mm_shuffle_ps( fl4Samples6, fl4Samples7, MM_SHUFFLE_REV( 2, 3, 2, 3 ) );	// 22 30 23 31

		// now put in final order
		fltx4 fl4Out0 = _mm_shuffle_ps( fl4Pair0, fl4Pair2, MM_SHUFFLE_REV( 0, 2, 0, 2 ) );	//  0  1  2  3
		fltx4 fl4Out1 = _mm_shuffle_ps( fl4Pair4, fl4Pair6, MM_SHUFFLE_REV( 0, 2, 0, 2 ) );	//  4  5  6  7
		fltx4 fl4Out2 = _mm_shuffle_ps( fl4Pair0, fl4Pair2, MM_SHUFFLE_REV( 1, 3, 1, 3 ) );	//  8  9 10 11
		fltx4 fl4Out3 = _mm_shuffle_ps( fl4Pair4, fl4Pair6, MM_SHUFFLE_REV( 1, 3, 1, 3 ) );	// 12 13 14 15
		fltx4 fl4Out4 = _mm_shuffle_ps( fl4Pair1, fl4Pair3, MM_SHUFFLE_REV( 0, 2, 0, 2 ) );	// 16 17 18 19
		fltx4 fl4Out5 = _mm_shuffle_ps( fl4Pair5, fl4Pair7, MM_SHUFFLE_REV( 0, 2, 0, 2 ) );	// 20 21 22 23
		fltx4 fl4Out6 = _mm_shuffle_ps( fl4Pair1, fl4Pair3, MM_SHUFFLE_REV( 1, 3, 1, 3 ) );	// 24 25 26 27
		fltx4 fl4Out7 = _mm_shuffle_ps( fl4Pair5, fl4Pair7, MM_SHUFFLE_REV( 1, 3, 1, 3 ) );	// 28 29 30 31

		// pack into 4 bundles of 8
		shortx8 nOut0 = PackInt32x2ToShortx8( RoundtFloatToInt32( fl4Out0 ), RoundtFloatToInt32( fl4Out1 ) );
		shortx8 nOut1 = PackInt32x2ToShortx8( RoundtFloatToInt32( fl4Out2 ), RoundtFloatToInt32( fl4Out3 ) );
		shortx8 nOut2 = PackInt32x2ToShortx8( RoundtFloatToInt32( fl4Out4 ), RoundtFloatToInt32( fl4Out5 ) );
		shortx8 nOut3 = PackInt32x2ToShortx8( RoundtFloatToInt32( fl4Out6 ), RoundtFloatToInt32( fl4Out7 ) );
		// NOTE: Optimize alignment?
		StoreUnalignedSIMD( pWrite, nOut0 );
		StoreUnalignedSIMD( pWrite + 8, nOut1 );
		StoreUnalignedSIMD( pWrite + 16, nOut2 );
		StoreUnalignedSIMD( pWrite + 24, nOut3 );
		pWrite += 32;
	}
}

// slow version to support 4/6/8 channel devices
void ConvertFloat32Int16_Clamp_InterleaveStride( short *pOut, int nOutputChannelCount, int nChannelStrideFloats, float *pflChannel0, int nInputChannelCount, int nSampleCount )
{
	// detect optimizable cases and call fast code
	if ( nInputChannelCount == 6 && nOutputChannelCount == 6 && IsAlign4( nSampleCount ) )
	{
		ConvertFloat32Int16_Clamp_Interleave6( pOut, nOutputChannelCount, nChannelStrideFloats, pflChannel0, nInputChannelCount, nSampleCount );
		return;
	}
	if ( nInputChannelCount == 8 && nOutputChannelCount == 8 && IsAlign4( nSampleCount ) )
	{
		ConvertFloat32Int16_Clamp_Interleave8( pOut, nOutputChannelCount, nChannelStrideFloats, pflChannel0, nInputChannelCount, nSampleCount );
		return;
	}

	// run the slower code in this case
	if ( nOutputChannelCount > nInputChannelCount )
	{
		for ( int i = 0; i < nSampleCount; i++ )
		{
			float *pIn = pflChannel0 + i;
			for ( int j = 0; j < nInputChannelCount; j++ )
			{
				int nOut = int( pIn[0] );
				nOut = clamp( nOut, -32768, 32767 );
				*pOut++ = nOut;
				pIn += nChannelStrideFloats;
			}
			for ( int j = nInputChannelCount; j < nOutputChannelCount; j++ )
			{
				*pOut++ = 0;
			}
		}
	}
	else
	{
		int nCopyChannels = MIN(nOutputChannelCount, nInputChannelCount);
		for ( int i = 0; i < nSampleCount; i++ )
		{
			float *pIn = pflChannel0 + i;
			for ( int j = 0; j < nCopyChannels; j++ )
			{
				int nOut = int( pIn[0] );
				nOut = clamp( nOut, -32768, 32767 );
				*pOut++ = nOut;
				pIn += nChannelStrideFloats;
			}
		}
	}
	Assert( nOutputChannelCount >= nInputChannelCount );
}

static void ConvertShortToFloatx8( float flOutput[MIX_BUFFER_SIZE], const short *pIn )
{
	fltx4 *pOutput = reinterpret_cast<fltx4 *>(&flOutput[0]);
	const shortx8 *pInput = reinterpret_cast<const shortx8 *>(pIn);
	for ( int i = 0; i < (MIX_BUFFER_SIZE/8); i++ )
	{
		shortx8 samples = LoadUnalignedShortSIMD( pInput );
		pInput++;
		fltx4 lo = SignedIntConvertToFltSIMD( SignedExtractLowAsInt32( samples ) );
		fltx4 hi = SignedIntConvertToFltSIMD( SignedExtractHighAsInt32( samples ) );
		StoreAlignedSIMD( (float *)pOutput, lo );
		pOutput++;
		StoreAlignedSIMD( (float *)pOutput, hi );
		pOutput++;
	}
}

// use 15-bit fixed point fractions for resampling
#define FIX_BITS 15
#define FIX_MASK ((1ul<<FIX_BITS)-1)

FORCEINLINE int FLOAT_TO_FIXED( float flVal )
{
	return int( flVal * float( 1ul << FIX_BITS ) );
}

// UNDONE: This can be trivially optimized to not loop
static int CalcAdvanceSamples( int nOutCount, float sampleRatio, uint *pInputOffsetFrac )
{
	uint nRateScaleFix = FLOAT_TO_FIXED( sampleRatio );
	uint nSampleFrac  = *pInputOffsetFrac;
	uint nSampleIndex = 0;

	for ( int i = 0; i < nOutCount; i++ )
	{	
		nSampleFrac += nRateScaleFix;
		nSampleIndex += nSampleFrac >> FIX_BITS;
		nSampleFrac = nSampleFrac & FIX_MASK;
	}
	*pInputOffsetFrac = nSampleFrac;
	return nSampleIndex;
}

// resample 16-bit audio data at the given ratio using linear interpolation
// output is 32-bits per sample float
static uint Resample16to32( float *pOut, const short *pWaveData, float sampleRatio, uint *pInputOffsetFrac )
{
	uint nRateScaleFix = FLOAT_TO_FIXED( sampleRatio );
	uint nSampleFrac = *pInputOffsetFrac;
	Assert( nSampleFrac < ( 1ul << FIX_BITS ) );
	uint nSampleIndex = 0;

	int nFirst, nSecond, nInterp;
	for ( int i = 0; i < MIX_BUFFER_SIZE; i++ )
	{
		nFirst = (int)( pWaveData[nSampleIndex] );
		nSecond = (int)( pWaveData[nSampleIndex + 1] );
#if 0
		// this expression doesn't truncate the value to 16-bits and preserves fractional samples in the float
		// output.  It is a bit slower and the improved precision won't be audible unless the sample is amplified
		// or processed in some way because the output stage will simply round these back to 16-bit values
		// so disable this until we find a reason that we need it
		nInterp = ( nFirst << FIX_BITS ) + ( ( ( nSecond - nFirst ) * int( nSampleFrac ) ) );
		pOut[i] = float( nInterp ) * ( 1.0f / float( 1ul << FIX_BITS ) );
#else
		nInterp = nFirst + ( ( ( nSecond - nFirst ) * int( nSampleFrac ) ) >> FIX_BITS );
		pOut[i] = float( nInterp );
#endif

		nSampleFrac += nRateScaleFix;
		nSampleIndex += nSampleFrac >> FIX_BITS;
		nSampleFrac = nSampleFrac & FIX_MASK;
	}

	*pInputOffsetFrac = nSampleFrac;
	return nSampleIndex;
}

const fltx4 g_fl4LinerInterp2x_lo={1.0,0.5,1.0,0.5};
const fltx4 g_fl4LinerInterp2x_hi={0.0,0.5,0.0,0.5};

static uint Resample16to32_2x( float flOutput[MIX_BUFFER_SIZE], const short *pWaveData, uint *pInputOffsetFrac )
{
	fltx4 *pOutput = reinterpret_cast<fltx4 *>(&flOutput[0]);
	const shortx8 *pInput = reinterpret_cast<const shortx8 *>(pWaveData);
	fltx4 flAllOne = LoadAlignedSIMD( (float *)g_SIMD_AllOnesMask );
	fltx4 fl4FirstTwo = LoadAlignedSIMD( (float *)&g_SIMD_SkipTailMask[2] );
	fltx4 fl4LastTwo = AndNotSIMD( fl4FirstTwo, flAllOne );
	for ( int i = 0; i < (MIX_BUFFER_SIZE/16); i++ )
	{
		shortx8 samples = LoadUnalignedShortSIMD( pInput );
		pInput++;
		fltx4 lo = SignedIntConvertToFltSIMD( SignedExtractLowAsInt32( samples ) );
		fltx4 hi = SignedIntConvertToFltSIMD( SignedExtractHighAsInt32( samples ) );
		shortx8 samplesNext = LoadUnalignedShortSIMD( pInput );
		// LAME: Only need one value for this but I can't be bothered to unroll this yet
		fltx4 hi4 = SplatXSIMD( SignedIntConvertToFltSIMD( SignedExtractLowAsInt32( samplesNext ) ) );

		fltx4 samp0 = SplatXSIMD( lo );
		fltx4 samp1 = SplatYSIMD( lo );
		fltx4 samp0011 = OrSIMD( AndSIMD( fl4FirstTwo, samp0 ), AndSIMD( fl4LastTwo, samp1 ) );
		fltx4 samp2 = SplatZSIMD( lo );
		fltx4 samp1122 = OrSIMD( AndSIMD( fl4FirstTwo, samp1 ), AndSIMD( fl4LastTwo, samp2 ) );
		StoreAlignedSIMD( (float *)pOutput, MaddSIMD( g_fl4LinerInterp2x_lo, samp0011, MulSIMD( g_fl4LinerInterp2x_hi, samp1122 ) ) ); // 4
		pOutput++;

		fltx4 samp3 = SplatWSIMD( lo );
		fltx4 samp2233 = OrSIMD( AndSIMD( fl4FirstTwo, samp2 ), AndSIMD( fl4LastTwo, samp3 ) );
		fltx4 samp4 = SplatXSIMD( hi );
		fltx4 samp3344 = OrSIMD( AndSIMD( fl4FirstTwo, samp3 ), AndSIMD( fl4LastTwo, samp4 ) );
		StoreAlignedSIMD( (float *)pOutput, MaddSIMD( g_fl4LinerInterp2x_lo, samp2233, MulSIMD( g_fl4LinerInterp2x_hi, samp3344 ) ) ); // 8
		pOutput++;

		fltx4 samp5 = SplatYSIMD( hi );
		fltx4 samp4455 = OrSIMD( AndSIMD( fl4FirstTwo, samp4 ), AndSIMD( fl4LastTwo, samp5 ) );
		fltx4 samp6 = SplatZSIMD( hi );
		fltx4 samp5566 = OrSIMD( AndSIMD( fl4FirstTwo, samp5 ), AndSIMD( fl4LastTwo, samp6 ) );
		StoreAlignedSIMD( (float *)pOutput, MaddSIMD( g_fl4LinerInterp2x_lo, samp4455, MulSIMD( g_fl4LinerInterp2x_hi, samp5566 ) ) ); // 12
		pOutput++;

		fltx4 samp7 = SplatWSIMD( hi );
		fltx4 samp6677 = OrSIMD( AndSIMD( fl4FirstTwo, samp6 ), AndSIMD( fl4LastTwo, samp7 ) );
		fltx4 samp8 = SplatXSIMD( hi4 );
		fltx4 samp7788 = OrSIMD( AndSIMD( fl4FirstTwo, samp7 ), AndSIMD( fl4LastTwo, samp8 ) );
		StoreAlignedSIMD( (float *)pOutput, MaddSIMD( g_fl4LinerInterp2x_lo, samp6677, MulSIMD( g_fl4LinerInterp2x_hi, samp7788 ) ) ); // 16
		pOutput++;
	}
	return MIX_BUFFER_SIZE / 2;
}

const fltx4 g_fl4LinerInterp4x_lo={1.0,0.75,0.5,0.25};
const fltx4 g_fl4LinerInterp4x_hi={0.0,0.25,0.5,0.75};

static uint Resample16to32_4x( float flOutput[MIX_BUFFER_SIZE], const short *pWaveData, uint *pInputOffsetFrac )
{
	fltx4 *pOutput = reinterpret_cast<fltx4 *>(&flOutput[0]);
	const shortx8 *pInput = reinterpret_cast<const shortx8 *>(pWaveData);
	for ( int i = 0; i < (MIX_BUFFER_SIZE/32); i++ )
	{
		shortx8 samples = LoadUnalignedShortSIMD( pInput );
		pInput++;
		fltx4 lo = SignedIntConvertToFltSIMD( SignedExtractLowAsInt32( samples ) );
		fltx4 hi = SignedIntConvertToFltSIMD( SignedExtractHighAsInt32( samples ) );
		shortx8 samplesNext = LoadUnalignedShortSIMD( pInput );
		// LAME: Only need one value for this but I can't be bothered to unroll this yet
		fltx4 hi4 = SplatXSIMD( SignedIntConvertToFltSIMD( SignedExtractLowAsInt32( samplesNext ) ) );

		fltx4 samp0 = SplatXSIMD( lo );
		fltx4 samp1 = SplatYSIMD( lo );
		StoreAlignedSIMD( (float *)pOutput, MaddSIMD( g_fl4LinerInterp4x_lo, samp0, MulSIMD( g_fl4LinerInterp4x_hi, samp1 ) ) ); // 4
		pOutput++;

		fltx4 samp2 = SplatZSIMD( lo );
		StoreAlignedSIMD( (float *)pOutput, MaddSIMD( g_fl4LinerInterp4x_lo, samp1, MulSIMD( g_fl4LinerInterp4x_hi, samp2 ) ) ); // 8
		pOutput++;

		fltx4 samp3 = SplatWSIMD( lo );
		StoreAlignedSIMD( (float *)pOutput, MaddSIMD( g_fl4LinerInterp4x_lo, samp2, MulSIMD( g_fl4LinerInterp4x_hi, samp3 ) ) ); // 12
		pOutput++;

		fltx4 samp4 = SplatXSIMD( hi );
		StoreAlignedSIMD( (float *)pOutput, MaddSIMD( g_fl4LinerInterp4x_lo, samp3, MulSIMD( g_fl4LinerInterp4x_hi, samp4 ) ) ); // 16
		pOutput++;

		fltx4 samp5 = SplatYSIMD( hi );
		StoreAlignedSIMD( (float *)pOutput, MaddSIMD( g_fl4LinerInterp4x_lo, samp4, MulSIMD( g_fl4LinerInterp4x_hi, samp5 ) ) ); // 20
		pOutput++;

		fltx4 samp6 = SplatZSIMD( hi );
		StoreAlignedSIMD( (float *)pOutput, MaddSIMD( g_fl4LinerInterp4x_lo, samp5, MulSIMD( g_fl4LinerInterp4x_hi, samp6 ) ) ); // 24
		pOutput++;

		fltx4 samp7 = SplatWSIMD( hi );
		StoreAlignedSIMD( (float *)pOutput, MaddSIMD( g_fl4LinerInterp4x_lo, samp6, MulSIMD( g_fl4LinerInterp4x_hi, samp7 ) ) ); // 28
		pOutput++;

		fltx4 samp8 = SplatXSIMD( hi4 );
		StoreAlignedSIMD( (float *)pOutput, MaddSIMD( g_fl4LinerInterp4x_lo, samp7, MulSIMD( g_fl4LinerInterp4x_hi, samp8 ) ) ); // 32
		pOutput++;
	}
	return MIX_BUFFER_SIZE / 4;
}


static void Convert32ToFloatx4( float flOutput[MIX_BUFFER_SIZE], int *pIn )
{
	fltx4 *pOutput = reinterpret_cast<fltx4 *>(&flOutput[0]);
	const shortx8 *pInput = reinterpret_cast<const shortx8 *>(pIn);

	for ( int i = 0; i < (MIX_BUFFER_SIZE/4); i++ )
	{
		shortx8 n4Samples = LoadAlignedShortx8SIMD( pInput );
		pInput++;
		fltx4 fl4Output = SignedIntConvertToFltSIMD( n4Samples );
		StoreAlignedSIMD( (float *)pOutput, fl4Output );
		pOutput++;
	}
}

inline void ZeroFill( short *pBuffer, int nCount )
{
	short *pLast = pBuffer + nCount;
	while ( pBuffer < pLast )
	{
		*pBuffer++ = 0;
	}
}

// Join buffer list into a contiguous sample list
const short *GetContiguousSamples_8Mono( const audio_source_input_t &source, const audio_source_indexstate_t *pState, int nSamplesNeeded, short *pTemp, int nTempSampleCount )
{
	Assert( nSamplesNeeded < nTempSampleCount );

	int nSampleIndex = pState->m_nBufferSampleOffset;
	uint nPacketIndex = pState->m_nPacketIndex;
	int nOutIndex = 0;
	for ( ; nPacketIndex < source.m_nPacketCount; nPacketIndex++ )
	{
		const uint8 *pSourceData = (uint8 *)(source.m_pPackets[nPacketIndex].m_pSamples) + nSampleIndex;
		int nSamplesAvailable = source.m_pPackets[nPacketIndex].m_nSampleCount - nSampleIndex;
		Assert( nSamplesAvailable > 0 );
		int nCopy = Min(nSamplesAvailable, nSamplesNeeded);
		for ( int i = 0; i < nCopy; i++ )
		{
			// 8-bit PCM is unsigned, but we assume it has been converted to signed on load
			uint32 nSample = (uint8)((int32) pSourceData[i]);
			pTemp[nOutIndex+i] = (nSample<<8) | nSample;
		}
		nSamplesNeeded -= nCopy;
		nOutIndex += nCopy;
		Assert(nSamplesNeeded >= 0);
		if ( nSamplesNeeded <= 0 )
			break;
		nSampleIndex = 0;
	}
	if ( nSamplesNeeded )
	{
		ZeroFill( &pTemp[nOutIndex], nSamplesNeeded );
	}
	return pTemp;
}

const short *GetContiguousSamples_8Stereo( const audio_source_input_t &source, const audio_source_indexstate_t *pState, int nSamplesNeeded, short *pTemp, int nTempSampleCount, int nChannel )
{
	Assert( nSamplesNeeded < nTempSampleCount );

	uint nSampleIndex = pState->m_nBufferSampleOffset;
	uint nPacketIndex = pState->m_nPacketIndex;
	int nOutIndex = 0;
	for ( ; nPacketIndex < source.m_nPacketCount; nPacketIndex++ )
	{
		const uint8 *pSourceData = (uint8 *)(source.m_pPackets[nPacketIndex].m_pSamples) + (nSampleIndex<<1) + nChannel;
		int nSamplesAvailable = source.m_pPackets[nPacketIndex].m_nSampleCount - nSampleIndex;
		Assert( nSamplesAvailable > 0 );
		int nCopy = Min(nSamplesAvailable, nSamplesNeeded);
		for ( int i = 0; i < nCopy; i++ )
		{
			// 8-bit PCM is unsigned, but we assume it has been converted to signed on load
			uint32 nSample = (uint8)( (int32)pSourceData[i << 1] );
			pTemp[nOutIndex+i] = (nSample<<8) | nSample;
		}
		nSamplesNeeded -= nCopy;
		nOutIndex += nCopy;
		Assert(nSamplesNeeded >= 0);
		if ( nSamplesNeeded <= 0 )
			break;
		nSampleIndex = 0;
	}
	if ( nSamplesNeeded )
	{
		ZeroFill( &pTemp[nOutIndex], nSamplesNeeded );
	}
	return pTemp;
}

const short *GetContiguousSamples_16Mono( const audio_source_input_t &source, const audio_source_indexstate_t *pState, int nSamplesNeeded, short *pTemp, int nTempSampleCount )
{
	Assert( nSamplesNeeded <= nTempSampleCount );

	uint nSampleIndex = pState->m_nBufferSampleOffset;
	uint nPacketIndex = pState->m_nPacketIndex;

	if ( nPacketIndex < source.m_nPacketCount )
	{
		int nSamplesAvailable = source.m_pPackets[nPacketIndex].m_nSampleCount - nSampleIndex;

		// optimization: if the entire request can be satisfied by the current packet, just point to that (don't copy)
		if ( nSamplesAvailable >= nSamplesNeeded )
		{
			Assert( source.m_pPackets[nPacketIndex].m_pSamples != NULL );
			return source.m_pPackets[nPacketIndex].m_pSamples + nSampleIndex;
		}

		int nOutIndex = 0;
		for ( ; nPacketIndex < source.m_nPacketCount; nPacketIndex++ )
		{
			const short *pSourceData = source.m_pPackets[nPacketIndex].m_pSamples + nSampleIndex;
			nSamplesAvailable = source.m_pPackets[nPacketIndex].m_nSampleCount - nSampleIndex;
			Assert( nSamplesAvailable > 0 );
			int nCopy = Min(nSamplesAvailable, nSamplesNeeded);
			V_memcpy( &pTemp[nOutIndex], pSourceData, nCopy * sizeof(short) );
			nSamplesNeeded -= nCopy;
			nOutIndex += nCopy;
			Assert(nSamplesNeeded >= 0);
			if ( nSamplesNeeded <= 0 )
				break;
			nSampleIndex = 0;
		}
		if ( nSamplesNeeded )
		{
			// pad with zeros
			ZeroFill( &pTemp[nOutIndex], nSamplesNeeded );
		}
		return pTemp;
	}
	return NULL;
}

const short *GetContiguousSamples_16Stereo( const audio_source_input_t &source, const audio_source_indexstate_t *pState, int nSamplesNeeded, short *pTemp, int nTempSampleCount, int nChannel )
{
	Assert( nSamplesNeeded < nTempSampleCount );

	uint nSampleIndex = pState->m_nBufferSampleOffset;
	uint nPacketIndex = pState->m_nPacketIndex;
	int nOutIndex = 0;
	for ( ; nPacketIndex < source.m_nPacketCount; nPacketIndex++ )
	{
		const short *pSourceData = source.m_pPackets[nPacketIndex].m_pSamples + (nSampleIndex<<1) + nChannel;
		int nSamplesAvailable = source.m_pPackets[nPacketIndex].m_nSampleCount - nSampleIndex;
		Assert( nSamplesAvailable > 0 );
		int nCopy = MIN(nSamplesAvailable, nSamplesNeeded);
		for ( int i = 0; i < nCopy; i++ )
		{
			// copy every other sample to drop one channel.  Note that pSourceData is already offset to the appropriate channel
			pTemp[nOutIndex + i] = pSourceData[ i<<1 ];
		}
		nSamplesNeeded -= nCopy;
		nOutIndex += nCopy;
		Assert(nSamplesNeeded >= 0);
		if ( nSamplesNeeded <= 0 )
			break;
		nSampleIndex = 0;
	}
	if ( nSamplesNeeded )
	{
		// pad with zeros
		ZeroFill( &pTemp[nOutIndex], nSamplesNeeded );
	}
	return pTemp;
}

// has this source finished playing its sample data
bool IsFinished( const audio_source_input_t &source, const audio_source_indexstate_t *pCurrentState )
{
	return pCurrentState->m_nPacketIndex >= source.m_nPacketCount ? true : false;
}

// Move the source offset by some number of samples
// If necessary also advance the packet index
uint AdvanceSourceIndex( audio_source_indexstate_t *pOut, const audio_source_input_t &source, uint nAdvance )
{
	for ( ; pOut->m_nPacketIndex < source.m_nPacketCount; pOut->m_nPacketIndex++ )
	{
		nAdvance += pOut->m_nBufferSampleOffset;
		pOut->m_nBufferSampleOffset = nAdvance;
		// We can skip entirely within this packet by adjusting the offset, so return
		if ( nAdvance < source.m_pPackets[pOut->m_nPacketIndex].m_nSampleCount )
			return 0;

		nAdvance -= source.m_pPackets[pOut->m_nPacketIndex].m_nSampleCount;
		pOut->m_nBufferSampleOffset = 0;
	}
	return nAdvance;
}


int ConvertSourceToFloat( const audio_source_input_t &source, float flPitch, float flOutput[MIX_BUFFER_SIZE], audio_source_indexstate_t *pOut )
{
	//TestResample();
	VPROF("ConvertSourceToFloat");

	// if float
	//	 join, resample
	//	 return;
	// if 8 bit
	//	if stereo - extract/join/updepth
	//	if mono - join/updepth
	// if 16 bit
	//	if stereo - extract/join
	//	if mono - join
	// now we have 16-bit joined mono data
	// resample and convert to float
	// for now assume 16-bit mono, joined
	short nJoinedData[MIX_BUFFER_SIZE*2 + 8];

	float flSampleRatio = 1.0f;
	int nSamplesNeeded = MIX_BUFFER_SIZE;
	float flSampleRate = float(source.m_nSamplingRate) * flPitch;
	bool bResample = flSampleRate != MIX_DEFAULT_SAMPLING_RATE ? true : false;
 
	if ( bResample )
	{
		flSampleRatio = flSampleRate * (1.0f / MIX_DEFAULT_SAMPLING_RATE);
		flSampleRatio = clamp(flSampleRatio, 0.125f, 2.0f);
		nSamplesNeeded = int( (MIX_BUFFER_SIZE * flSampleRatio) + 0.5f ) + 2;  // add 2 for rounding, interpolate to next neighbor

		// some of the resampling code processes in blocks of 8 samples with SSE2 instructions, so align to nearest 8
		nSamplesNeeded = AlignValue( nSamplesNeeded, 8 );
#if _DEBUG
		uint64 nSampleRefCount = ( ( ( MIX_BUFFER_SIZE * FLOAT_TO_FIXED( flSampleRatio ) ) + pOut->m_nSampleFracOffset ) >> FIX_BITS ) + 1;
		Assert( nSampleRefCount <= nSamplesNeeded );
#endif
	}

	const short *pSourceData = NULL;
	// Grab a pointer to a joined set of sample data at the right length
	if ( source.m_nSampleFormat == SAMPLE_INT8_MONO )
	{
		pSourceData = GetContiguousSamples_8Mono( source, pOut, nSamplesNeeded, nJoinedData, Q_ARRAYSIZE(nJoinedData) );
	}
	else if ( source.m_nSampleFormat == SAMPLE_INT16_MONO )
	{
		pSourceData = GetContiguousSamples_16Mono( source, pOut, nSamplesNeeded, nJoinedData, Q_ARRAYSIZE(nJoinedData) );
	}
	else if ( source.m_nSampleFormat == SAMPLE_INT16_STEREO_L )
	{
		pSourceData = GetContiguousSamples_16Stereo( source, pOut, nSamplesNeeded, nJoinedData, Q_ARRAYSIZE(nJoinedData), 0 );
	}
	else if ( source.m_nSampleFormat == SAMPLE_INT16_STEREO_R )
	{
		pSourceData = GetContiguousSamples_16Stereo( source, pOut, nSamplesNeeded, nJoinedData, Q_ARRAYSIZE(nJoinedData), 1 );
	}
	else if ( source.m_nSampleFormat == SAMPLE_INT8_STEREO_L )
	{
		pSourceData = GetContiguousSamples_8Stereo( source, pOut, nSamplesNeeded, nJoinedData, Q_ARRAYSIZE(nJoinedData), 0 );
	}
	else if ( source.m_nSampleFormat == SAMPLE_INT8_STEREO_R )
	{
		pSourceData = GetContiguousSamples_8Stereo( source, pOut, nSamplesNeeded, nJoinedData, Q_ARRAYSIZE(nJoinedData), 1 );
	}

	if ( pSourceData )
	{
		if ( bResample )
		{
			if ( flSampleRate == 11025.0f )
			{
				nSamplesNeeded = Resample16to32_4x( flOutput, pSourceData, &pOut->m_nSampleFracOffset );
			}
			else if ( flSampleRate == 22050.0f )
			{
				nSamplesNeeded = Resample16to32_2x( flOutput, pSourceData, &pOut->m_nSampleFracOffset );
			}
			else
			{
				// slow path, resample arbitrary ratio
				VPROF("Resample_Ratio");
				nSamplesNeeded = Resample16to32( flOutput, pSourceData, flSampleRatio, &pOut->m_nSampleFracOffset );
			}
		}
		else
		{
			ConvertShortToFloatx8( flOutput, pSourceData );
		}
		// update the index state
		AdvanceSourceIndex( pOut, source, nSamplesNeeded );
		return 1;
	}

	return 0;
}

int AdvanceSource( const audio_source_input_t &source, float flPitch, audio_source_indexstate_t *pOut )
{
	float flSampleRatio = 1.0f;
	int nSamplesNeeded = MIX_BUFFER_SIZE;
	float flSampleRate = float(source.m_nSamplingRate) * flPitch;
	if ( flSampleRate != MIX_DEFAULT_SAMPLING_RATE )
	{
		flSampleRatio = flSampleRate * (1.0f / MIX_DEFAULT_SAMPLING_RATE);
		flSampleRatio = clamp(flSampleRatio, 0.125f, 2.0f);
		nSamplesNeeded = CalcAdvanceSamples( nSamplesNeeded, flSampleRatio, &pOut->m_nSampleFracOffset );
	}

	// update the index state
	AdvanceSourceIndex( pOut, source, nSamplesNeeded );
	return nSamplesNeeded;
}

// constants for linear ramping
const float flMixBufferSizeInv = 1.0f / MIX_BUFFER_SIZE;
const fltx4 g_fl4_MixBufferSizeInv = { flMixBufferSizeInv, flMixBufferSizeInv, flMixBufferSizeInv, flMixBufferSizeInv };
const fltx4 g_fl4_Sequence1234 = { 1.0, 2.0, 3.0, 4.0 };


void ScaleBuffer( float flOutput[MIX_BUFFER_SIZE], const float input[MIX_BUFFER_SIZE], float scale )
{
	fltx4 volume = ReplicateX4(scale);
	fltx4 * RESTRICT pOut = (fltx4 *)&flOutput[0];
	fltx4 * RESTRICT pIn = (fltx4 *)&input[0];
	for ( int i = 0; i < MIX_BUFFER_SIZE/4; i++ )
	{
		fltx4 sample = LoadAlignedSIMD( pIn );
		StoreAlignedSIMD( (float *)pOut, MulSIMD( volume, sample ) );
		pOut++;
		pIn++;
	}
}

void ScaleBufferRamp( float flOutput[MIX_BUFFER_SIZE], const float flInput[MIX_BUFFER_SIZE], float flScaleStart, float flScaleEnd )
{
	fltx4 fl4Volume = ReplicateX4( flScaleStart );
	fltx4 fl4VolumeStep = MulSIMD( g_fl4_MixBufferSizeInv, SubSIMD( ReplicateX4( flScaleEnd ), fl4Volume ) );

	// offset volume by first ramp steps
	fl4Volume = AddSIMD( fl4Volume, MulSIMD( fl4VolumeStep, g_fl4_Sequence1234 ) );

	fltx4 fl4VolumeInc = MulSIMD( fl4VolumeStep, Four_Fours );

	fltx4 * RESTRICT pOut = (fltx4 *)&flOutput[0];
	fltx4 * RESTRICT pIn = (fltx4 *)&flInput[0];
	for ( int i = 0; i < MIX_BUFFER_SIZE / 4; i++ )
	{
		fltx4 fl4Sample = LoadAlignedSIMD( pIn );
		StoreAlignedSIMD( (float *)pOut, MulSIMD( fl4Volume, fl4Sample ) );
		pOut++;
		pIn++;
		fl4Volume = AddSIMD( fl4VolumeInc, fl4Volume );
	}
}

void SilenceBuffer( float flBuffer[MIX_BUFFER_SIZE] )
{
	fltx4 * RESTRICT pOut = (fltx4 *)&flBuffer[0];
	fltx4 fl4Zero = LoadZeroSIMD();
	for ( int i = 0; i < MIX_BUFFER_SIZE/4; i++ )
	{
		StoreAlignedSIMD( (float *)pOut, fl4Zero );
		pOut++;
	}
}

void SilenceBuffers( CAudioMixBuffer *pBuffers, int nBufferCount )
{
	for ( int i = 0; i < nBufferCount; i++ )
	{
		SilenceBuffer( pBuffers[i].m_flData );
	}
}

void MixBuffer( float flOutput[MIX_BUFFER_SIZE], const float flInput[MIX_BUFFER_SIZE], float scale )
{
	fltx4 fl4Volume = ReplicateX4(scale);
	fltx4 * RESTRICT pOut = (fltx4 *)&flOutput[0];
	fltx4 * RESTRICT pIn = (fltx4 *)&flInput[0];
	for ( int i = 0; i < MIX_BUFFER_SIZE/4; i++ )
	{
		fltx4 fl4Sample = LoadAlignedSIMD( pIn );
		fltx4 fl4Mix = LoadAlignedSIMD( pOut );
		StoreAlignedSIMD( (float *)pOut, MaddSIMD( fl4Volume, fl4Sample, fl4Mix ) );
		pOut++;
		pIn++;
	}
}

void MixBufferRamp( float flOutput[MIX_BUFFER_SIZE], const float flInput[MIX_BUFFER_SIZE], float flScaleStart, float flScaleEnd )
{
	fltx4 fl4Volume = ReplicateX4( flScaleStart );
	fltx4 fl4VolumeStep = MulSIMD( g_fl4_MixBufferSizeInv, SubSIMD( ReplicateX4( flScaleEnd ), fl4Volume ) );

	// offset volume by first ramp steps
	fl4Volume = AddSIMD( fl4Volume, MulSIMD( fl4VolumeStep, g_fl4_Sequence1234 ) );

	fltx4 fl4VolumeInc = MulSIMD( fl4VolumeStep, Four_Fours );

	fltx4 * RESTRICT pOut = (fltx4 *)&flOutput[0];
	fltx4 * RESTRICT pIn = (fltx4 *)&flInput[0];
	for ( int i = 0; i < MIX_BUFFER_SIZE / 4; i++ )
	{
		fltx4 fl4Sample = LoadAlignedSIMD( pIn );
		fltx4 fl4Mix = LoadAlignedSIMD( pOut );
		StoreAlignedSIMD( (float *)pOut, MaddSIMD( fl4Volume, fl4Sample, fl4Mix ) );
		pOut++;
		pIn++;
		fl4Volume = AddSIMD( fl4VolumeInc, fl4Volume );
	}
}

void SumBuffer2x1( float flOutput[MIX_BUFFER_SIZE], float flInput0[MIX_BUFFER_SIZE], float flScale0, float flInput1[MIX_BUFFER_SIZE], float flScale1 )
{
	fltx4 fl4Scale0 = ReplicateX4(flScale0);
	fltx4 fl4Scale1 = ReplicateX4(flScale1);
	fltx4 * RESTRICT pOut = (fltx4 *)&flOutput[0];
	fltx4 * RESTRICT pIn0 = (fltx4 *)&flInput0[0];
	fltx4 * RESTRICT pIn1 = (fltx4 *)&flInput1[0];
	for ( int i = 0; i < MIX_BUFFER_SIZE/4; i++ )
	{
		fltx4 fl4Sample0 = LoadAlignedSIMD( pIn0 );
		fltx4 fl4Sample1 = LoadAlignedSIMD( pIn1 );
		StoreAlignedSIMD( (float *)pOut, MaddSIMD( fl4Scale0, fl4Sample0, MulSIMD( fl4Scale1, fl4Sample1 ) ) );
		pOut++;
		pIn0++;
		pIn1++;
	}
}


void SwapBuffersInPlace( float flInput0[MIX_BUFFER_SIZE], float flInput1[MIX_BUFFER_SIZE] )
{
	fltx4 * RESTRICT pIn0 = (fltx4 *)&flInput0[0];
	fltx4 * RESTRICT pIn1 = (fltx4 *)&flInput1[0];
	for ( int i = 0; i < MIX_BUFFER_SIZE/4; i++ )
	{
		fltx4 fl4Sample0 = LoadAlignedSIMD( pIn0 );
		fltx4 fl4Sample1 = LoadAlignedSIMD( pIn1 );
		StoreAlignedSIMD( (float *)pIn0, fl4Sample1 );
		StoreAlignedSIMD( (float *)pIn1, fl4Sample0 );
		pIn0++;
		pIn1++;
	}
}

// UNDONE: OPTIMIZE: SIMD implementation
float BufferLevel( float flInput0[MIX_BUFFER_SIZE] )
{
	float flAbsMax = 0.0f;
	for ( int i = 0; i < MIX_BUFFER_SIZE; i++ )
	{
		flAbsMax = Max( flAbsMax, (float)fabs(flInput0[i]) );
	}
	return flAbsMax;
}

float AvergeBufferAmplitude( float flInput0[MIX_BUFFER_SIZE] )
{
	float flTotal = 0;
	for ( int i = 0; i < MIX_BUFFER_SIZE; i++ )
	{
		flTotal += fabs( flInput0[i] );
	}
	return flTotal * ( 1.0f / MIX_BUFFER_SIZE );
}
