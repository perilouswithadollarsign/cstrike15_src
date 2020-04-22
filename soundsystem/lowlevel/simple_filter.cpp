#include <math.h>
#include "mathlib/ssemath.h"
#include "mix.h"
#include "simple_filter.h"


#define V_powf(x, y) powf(x, y)
#define V_sinf(x) sinf(x)
#define V_cosf(x) cosf(x)
#define V_sinhf(x) sinhf(x)
#define V_sqrtf(x) sqrtf(x)

// natural log of 2
#ifndef M_LN2
#define M_LN2	   0.69314718055994530942
#endif


// slow, just for test
static inline fltx4 LoadSIMD( float flR0, float flR1, float flR2, float flR3 )
{
	fltx4 t;
	SubFloat( t, 0 ) = flR0;
	SubFloat( t, 1 ) = flR1;
	SubFloat( t, 2 ) = flR2;
	SubFloat( t, 3 ) = flR3;
	return t;
}

// scalar implemetation
#if 0
void SimpleFilter_ProcessBuffer( float flSamples[MIX_BUFFER_SIZE], float flOutput[MIX_BUFFER_SIZE], filterstate_t *pFilter )
{
	float x1 = pFilter->m_flFIRState[0];
	float x2 = pFilter->m_flFIRState[1];
	float y1 = pFilter->m_flIIRState[0];
	float y2 = pFilter->m_flIIRState[1];
	float sample, out;

	float fir0 = pFilter->m_flFIRCoeff[0];
	float fir1 = pFilter->m_flFIRCoeff[1];
	float fir2 = pFilter->m_flFIRCoeff[2];
	float iir0 = -pFilter->m_flIIRCoeff[0];
	float iir1 = -pFilter->m_flIIRCoeff[1];
	for ( int i = 0; i < MIX_BUFFER_SIZE; i++ )
	{
		sample = flSamples[i];
		// FIR part of the filter
		out = fir0 * sample + fir1 * x1 + fir2 * x2;
		// IIR part of the filter
		out += iir0 * y1 + iir1 * y2;
		// write FIR delay line of input
		x2 = x1;
		x1 = sample;

		// write IIR delay line of output
		y2 = y1;
		y1 = out;
		// write filtered sample
		flOutput[i] = out;
	}
	// write state back to the filter
	pFilter->m_flFIRState[0] = x1;
	pFilter->m_flFIRState[1] = x2;
	pFilter->m_flIIRState[0] = y1;
	pFilter->m_flIIRState[1] = y2;
}
#endif

void SimpleFilter_ProcessFIR4( const float flSamples[MIX_BUFFER_SIZE], float flOutput[MIX_BUFFER_SIZE], filterstate_t *pFilter )
{
	fltx4 fl4FIR0 = ReplicateX4( pFilter->m_flFIRCoeff[0] );
	fltx4 fl4FIR1 = ReplicateX4( pFilter->m_flFIRCoeff[1] );
	fltx4 fl4FIR2 = ReplicateX4( pFilter->m_flFIRCoeff[2] );
	fltx4 fl4FIR3 = ReplicateX4( pFilter->m_flFIRCoeff[3] );

	fltx4 fl4PrevSample = pFilter->m_fl4prevInputSamples;
	const fltx4 *RESTRICT pInput = (const fltx4 *)&flSamples[0];
	fltx4 *RESTRICT pOutput = (fltx4 *)&flOutput[0];

	for ( int i = 0; i < MIX_BUFFER_SIZE/4; i++ )
	{
		fltx4 fl4Sample = LoadAlignedSIMD( pInput );
		pInput++;
		fltx4 fl4fx2 = _mm_shuffle_ps( fl4PrevSample, fl4Sample, MM_SHUFFLE_REV(2,3,0,1) );
		fltx4 fl4fx1 = _mm_shuffle_ps( fl4fx2, fl4Sample, MM_SHUFFLE_REV(1,2,1,2) );
		fltx4 fl4fx3 = _mm_shuffle_ps( fl4PrevSample, fl4fx2, MM_SHUFFLE_REV(1,2,1,2) );

		// FIR part of the filter
		//out = fir0 * sample + fir1 * x1 + fir2 * x2; + fir3 * x3
		fltx4 fl4t0 = MulSIMD( fl4FIR0, fl4Sample );
		fltx4 fl4t1 = MulSIMD( fl4FIR1, fl4fx1 );
		fltx4 fl4t2 = MaddSIMD( fl4FIR3, fl4fx3, fl4t0 );
		fltx4 fl4out = AddSIMD( MaddSIMD( fl4FIR2, fl4fx2, fl4t1 ), fl4t2 );

		// write FIR delay line of input
		fl4PrevSample = fl4Sample;

		StoreAlignedSIMD( (float *)pOutput, fl4out );
		pOutput++;
	}

	pFilter->m_fl4prevInputSamples = fl4PrevSample;
}

void SimpleFilter_ProcessBuffer( const float flSamples[MIX_BUFFER_SIZE], float flOutput[MIX_BUFFER_SIZE], filterstate_t *pFilter )
{
	if ( pFilter->m_nFilterType == 1 )
	{
		SimpleFilter_ProcessFIR4( flSamples, flOutput, pFilter );
		return;
	}
	fltx4 fl4FIR0 = ReplicateX4( pFilter->m_flFIRCoeff[0] );
	fltx4 fl4FIR1 = ReplicateX4( pFilter->m_flFIRCoeff[1] );
	fltx4 fl4FIR2 = ReplicateX4( pFilter->m_flFIRCoeff[2] );

	// UNDONE: Store in filterstate this way
	fltx4 fl4PrevSample = pFilter->m_fl4prevInputSamples;
	const fltx4 *RESTRICT pInput = (const fltx4 *)&flSamples[0];
	fltx4 *RESTRICT pOutput = (fltx4 *)&flOutput[0];

	// iir exapansion from intel paper
	//			[y3, y2, y1,  0] * [b3, b3,		    b3,				          0]
	//			[y2, y1,  0, v2] * [b2, b2,			0,				         b1]  row2
	//		+	[y1,  0, v1, v1] * [b1,  0,			b1,	             (b1*b1)+b2]  row1
	//		+	[0,  v0, v0, v0] * [ 0, b1, (b1*b1)+b2,	b1*b1*b1 + 2*b1*b2 + b3]  row0
	// NOTE: b3/y3 are always zero in our case because we only have two taps (drop the first row and b3 term in the fourth row)
	fltx4 iirRow2 = pFilter->m_fl4iirRow2;
	fltx4 iirRow1 = pFilter->m_fl4iirRow1;
	fltx4 iirRow0 = pFilter->m_fl4iirRow0;
	fltx4 fl4PrevOutput = pFilter->m_fl4prevOutputSamples;

	for ( int i = 0; i < MIX_BUFFER_SIZE/4; i++ )
	{
		fltx4 fl4Sample = LoadAlignedSIMD( pInput );
		pInput++;
		fltx4 fx2 = _mm_shuffle_ps( fl4PrevSample, fl4Sample, MM_SHUFFLE_REV(2,3,0,1) );
		fltx4 fx1 = _mm_shuffle_ps( fx2, fl4Sample, MM_SHUFFLE_REV(1,2,1,2) );

		// FIR part of the filter
		//out = fl4FIR0 * fl4Sample + fl4FIR1 * x1 + fl4FIR2 * x2;
		fltx4 t0 = MulSIMD( fl4FIR0, fl4Sample );
		fltx4 t1 = MulSIMD( fl4FIR1, fx1 );
		fltx4 out = AddSIMD( MaddSIMD( fl4FIR2, fx2, t0 ), t1 );

		// write FIR delay line of input
		fl4PrevSample = fl4Sample;

		// IIR part of the filter
		fltx4 fl4OutRow = _mm_shuffle_ps( fl4PrevOutput, out, MM_SHUFFLE_REV(2,3,0,2) );
		fltx4 v = MaddSIMD( fl4OutRow, iirRow2, out );
		fl4OutRow = _mm_shuffle_ps( fl4PrevOutput, v, MM_SHUFFLE_REV(3,0,1,1) );
		v = MaddSIMD( fl4OutRow, iirRow1, v );
		fl4OutRow = SplatXSIMD(v);
		out = MaddSIMD( fl4OutRow, iirRow0, v );

		// write IIR delay line of output
		fl4PrevOutput = out;

		StoreAlignedSIMD( (float *)pOutput, out );
		pOutput++;
	}

	pFilter->m_fl4prevInputSamples = fl4PrevSample;
	pFilter->m_fl4prevOutputSamples = fl4PrevOutput;
}

void SimpleFilter_Coefficients( biquad_filter_coefficients_t *pCoeffs, int nFilterType, float fldbGain, float flCenterFrequency, float flBandwidth, float flSamplingRate )
{
	float flA0, flA1, flA2, flB0, flB1, flB2;

	/* setup variables */
	float flGain = V_powf( 10, fldbGain / 40 );
	float flOmega = float( 2 * M_PI * flCenterFrequency / flSamplingRate );
	float flSinOmega = V_sinf( flOmega );
	float flCosOmega = V_cosf( flOmega );
	float flAlpha = flSinOmega * (float)V_sinhf( M_LN2 / 2 * flBandwidth * flOmega / flSinOmega );
	float flBeta = V_sqrtf( flGain + flGain );

	switch ( nFilterType )
	{
	default:
	case FILTER_LOWPASS:
		flB0 = ( 1 - flCosOmega ) / 2;
		flB1 = 1 - flCosOmega;
		flB2 = ( 1 - flCosOmega ) / 2;
		flA0 = 1 + flAlpha;
		flA1 = -2 * flCosOmega;
		flA2 = 1 - flAlpha;
		break;
	case FILTER_HIGHPASS:
		flB0 = ( 1 + flCosOmega ) / 2;
		flB1 = -( 1 + flCosOmega );
		flB2 = ( 1 + flCosOmega ) / 2;
		flA0 = 1 + flAlpha;
		flA1 = -2 * flCosOmega;
		flA2 = 1 - flAlpha;
		break;
	case FILTER_BANDPASS:
		flB0 = flAlpha;
		flB1 = 0;
		flB2 = -flAlpha;
		flA0 = 1 + flAlpha;
		flA1 = -2 * flCosOmega;
		flA2 = 1 - flAlpha;
		break;
	case FILTER_NOTCH:
		flB0 = 1;
		flB1 = -2 * flCosOmega;
		flB2 = 1;
		flA0 = 1 + flAlpha;
		flA1 = -2 * flCosOmega;
		flA2 = 1 - flAlpha;
		break;
	case FILTER_PEAKING_EQ:
		flB0 = 1 + ( flAlpha * flGain );
		flB1 = -2 * flCosOmega;
		flB2 = 1 - ( flAlpha * flGain );
		flA0 = 1 + ( flAlpha / flGain );
		flA1 = -2 * flCosOmega;
		flA2 = 1 - ( flAlpha / flGain );
		break;
	case FILTER_LOW_SHELF:
		flB0 = flGain * ( ( flGain + 1 ) - ( flGain - 1 ) * flCosOmega + flBeta * flSinOmega );
		flB1 = 2 * flGain * ( ( flGain - 1 ) - ( flGain + 1 ) * flCosOmega );
		flB2 = flGain * ( ( flGain + 1 ) - ( flGain - 1 ) * flCosOmega - flBeta * flSinOmega );
		flA0 = ( flGain + 1 ) + ( flGain - 1 ) * flCosOmega + flBeta * flSinOmega;
		flA1 = -2 * ( ( flGain - 1 ) + ( flGain + 1 ) * flCosOmega );
		flA2 = ( flGain + 1 ) + ( flGain - 1 ) * flCosOmega - flBeta * flSinOmega;
		break;
	case FILTER_HIGH_SHELF:
		flB0 = flGain * ( ( flGain + 1 ) + ( flGain - 1 ) * flCosOmega + flBeta * flSinOmega );
		flB1 = -2 * flGain * ( ( flGain - 1 ) + ( flGain + 1 ) * flCosOmega );
		flB2 = flGain * ( ( flGain + 1 ) + ( flGain - 1 ) * flCosOmega - flBeta * flSinOmega );
		flA0 = ( flGain + 1 ) - ( flGain - 1 ) * flCosOmega + flBeta * flSinOmega;
		flA1 = 2 * ( ( flGain - 1 ) - ( flGain + 1 ) * flCosOmega );
		flA2 = ( flGain + 1 ) - ( flGain - 1 ) * flCosOmega - flBeta * flSinOmega;
		break;
	}
	pCoeffs->m_flA[0] = flA0;
	pCoeffs->m_flA[1] = flA1;
	pCoeffs->m_flA[2] = flA2;
	pCoeffs->m_flB[0] = flB0;
	pCoeffs->m_flB[1] = flB1;
	pCoeffs->m_flB[2] = flB2;
}

void SimpleFilter_Init( filterstate_t *pFilter, int nFilterType, float fldbGain, float flCenterFrequency, float flBandwidth, float flSamplingRate )
{
	biquad_filter_coefficients_t coeffs;
	SimpleFilter_Coefficients( &coeffs, nFilterType, fldbGain, flCenterFrequency, flBandwidth, flSamplingRate );

    // compute biquad coefficients
	pFilter->m_flFIRCoeff[0] = coeffs.m_flB[0] / coeffs.m_flA[0];
    pFilter->m_flFIRCoeff[1] = coeffs.m_flB[1] / coeffs.m_flA[0];
    pFilter->m_flFIRCoeff[2] = coeffs.m_flB[2] / coeffs.m_flA[0];
    pFilter->m_flIIRCoeff[0] = coeffs.m_flA[1] / coeffs.m_flA[0];
    pFilter->m_flIIRCoeff[1] = coeffs.m_flA[2] / coeffs.m_flA[0];

    // zero out initial state
	pFilter->m_flFIRState[0] = 0;
	pFilter->m_flFIRState[1] = 0;
	pFilter->m_flIIRState[0] = 0;
	pFilter->m_flIIRState[1] = 0;
	pFilter->m_fl4prevInputSamples = Four_Zeros;

	float iir0 = -pFilter->m_flIIRCoeff[0];
	float iir1 = -pFilter->m_flIIRCoeff[1];
	float iir0_sqr = iir0*iir0;
	pFilter->m_fl4iirRow2 = LoadSIMD( iir1, iir1, 0, iir0 );
	pFilter->m_fl4iirRow1 = LoadSIMD( iir0, 0, iir0, iir0_sqr+iir1 );
	pFilter->m_fl4iirRow0 = LoadSIMD( 0, iir0, iir0_sqr+iir1, iir0_sqr*iir0 + (2*iir0*iir1) );

	pFilter->m_fl4prevOutputSamples = Four_Zeros;
	pFilter->m_nFilterType = 0;
}

void SimpleFilter_Update( filterstate_t *pFilter, int nFilterType, float fldbGain, float flCenterFrequency, float flBandwidth, float flSamplingRate )
{
	biquad_filter_coefficients_t coeffs;
	SimpleFilter_Coefficients( &coeffs, nFilterType, fldbGain, flCenterFrequency, flBandwidth, flSamplingRate );

	// compute biquad coefficients
	pFilter->m_flFIRCoeff[0] = coeffs.m_flB[0] / coeffs.m_flA[0];
	pFilter->m_flFIRCoeff[1] = coeffs.m_flB[1] / coeffs.m_flA[0];
	pFilter->m_flFIRCoeff[2] = coeffs.m_flB[2] / coeffs.m_flA[0];
	pFilter->m_flIIRCoeff[0] = coeffs.m_flA[1] / coeffs.m_flA[0];
	pFilter->m_flIIRCoeff[1] = coeffs.m_flA[2] / coeffs.m_flA[0];

	float iir0 = -pFilter->m_flIIRCoeff[0];
	float iir1 = -pFilter->m_flIIRCoeff[1];
	float iir0_sqr = iir0*iir0;
	pFilter->m_fl4iirRow2 = LoadSIMD( iir1, iir1, 0, iir0 );
	pFilter->m_fl4iirRow1 = LoadSIMD( iir0, 0, iir0, iir0_sqr + iir1 );
	pFilter->m_fl4iirRow0 = LoadSIMD( 0, iir0, iir0_sqr + iir1, iir0_sqr*iir0 + ( 2 * iir0*iir1 ) );

	pFilter->m_nFilterType = 0;
}

void SimpleFilter_InitLowPass( filterstate_t *pFilter, float dbGain, float flCenterFrequency, float flBandWidth, float flSamplingRate )
{
	SimpleFilter_Init( pFilter, FILTER_LOWPASS, dbGain, flCenterFrequency, flBandWidth, flSamplingRate );
}

void SimpleFilter_InitHighPass( filterstate_t *pFilter, float dbGain, float flCenterFrequency, float flBandWidth, float flSamplingRate )
{
	SimpleFilter_Init( pFilter, FILTER_HIGHPASS, dbGain, flCenterFrequency, flBandWidth, flSamplingRate );
}

void SimpleFilter_InitBandPass( filterstate_t *pFilter, float dbGain, float flCenterFrequency, float flBandWidth, float flSamplingRate )
{
	SimpleFilter_Init( pFilter, FILTER_BANDPASS, dbGain, flCenterFrequency, flBandWidth, flSamplingRate );
}

void SimpleFilter_InitNotch( filterstate_t *pFilter, float dbGain, float flCenterFrequency, float flBandWidth, float flSamplingRate )
{
	SimpleFilter_Init( pFilter, FILTER_NOTCH, dbGain, flCenterFrequency, flBandWidth, flSamplingRate );
}

void SimpleFilter_InitPeakingEQ( filterstate_t *pFilter, float dbGain, float flCenterFrequency, float flBandWidth, float flSamplingRate )
{
	SimpleFilter_Init( pFilter, FILTER_PEAKING_EQ, dbGain, flCenterFrequency, flBandWidth, flSamplingRate );
}

void SimpleFilter_InitLowShelf( filterstate_t *pFilter, float dbGain, float flCenterFrequency, float flBandWidth, float flSamplingRate )
{
	SimpleFilter_Init( pFilter, FILTER_LOW_SHELF, dbGain, flCenterFrequency, flBandWidth, flSamplingRate );
}

void SimpleFilter_InitHighShelf( filterstate_t *pFilter, float dbGain, float flCenterFrequency, float flBandWidth, float flSamplingRate )
{
	SimpleFilter_Init( pFilter, FILTER_HIGH_SHELF, dbGain, flCenterFrequency, flBandWidth, flSamplingRate );
}


