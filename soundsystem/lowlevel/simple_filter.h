#include "mathlib/ssemath.h"

struct biquad_filter_coefficients_t
{
	float m_flA[3];
	float m_flB[3];
};

struct filterstate_t
{
	float m_flFIRCoeff[4];
	float m_flIIRCoeff[2];
	int		m_nFilterType;
	float m_flUnused1[1];
	float m_flFIRState[2];
	float m_flIIRState[2];

	fltx4 m_fl4iirRow0;
	fltx4 m_fl4iirRow1;
	fltx4 m_fl4iirRow2;
	fltx4 m_fl4prevInputSamples;
	fltx4 m_fl4prevOutputSamples;
};


extern void SimpleFilter_ProcessBuffer( const float flSamples[MIX_BUFFER_SIZE], float flOutput[MIX_BUFFER_SIZE], filterstate_t *pFilter );
extern void SimpleFilter_InitLowPass( filterstate_t *pFilter, float fldbGain, float flCenterFrequency, float flBandWidth, float flSamplingRate = MIX_DEFAULT_SAMPLING_RATE );
extern void SimpleFilter_InitHighPass( filterstate_t *pFilter, float fldbGain, float flCenterFrequency, float flBandWidth, float flSamplingRate = MIX_DEFAULT_SAMPLING_RATE );
extern void SimpleFilter_InitBandPass( filterstate_t *pFilter, float fldbGain, float flCenterFrequency, float flBandWidth, float flSamplingRate = MIX_DEFAULT_SAMPLING_RATE );
extern void SimpleFilter_InitNotch( filterstate_t *pFilter, float fldbGain, float flCenterFrequency, float flBandWidth, float flSamplingRate = MIX_DEFAULT_SAMPLING_RATE );
extern void SimpleFilter_InitPeakingEQ( filterstate_t *pFilter, float fldbGain, float flCenterFrequency, float flBandWidth, float flSamplingRate = MIX_DEFAULT_SAMPLING_RATE );
extern void SimpleFilter_InitLowShelf( filterstate_t *pFilter, float fldbGain, float flCenterFrequency, float flBandWidth, float flSamplingRate = MIX_DEFAULT_SAMPLING_RATE );
extern void SimpleFilter_InitHighShelf( filterstate_t *pFilter, float fldbGain, float flCenterFrequency, float flBandWidth, float flSamplingRate = MIX_DEFAULT_SAMPLING_RATE );
extern void SimpleFilter_Init( filterstate_t *pFilter, int nFilterType, float fldbGain, float flCenterFrequency, float flBandwidth, float flSamplingRate );

extern void SimpleFilter_Coefficients( biquad_filter_coefficients_t *pCoeffs, int nFilterType, float fldbGain, float flCenterFrequency, float flBandwidth, float flSamplingRate );


// update parameterization but don't change prev input state
extern void SimpleFilter_Update( filterstate_t *pFilter, int nFilterType, float fldbGain, float flCenterFrequency, float flBandwidth, float flSamplingRate );
