/*
   Copyright (C) Impulsonic, Inc. All rights reserved.
*/

#ifndef IPL_PHONON_API_3D_H
#define IPL_PHONON_API_3D_H

#include "phonon_common.h"

#ifdef __cplusplus
extern "C" {
#endif


    /*************************************************************************/
    /* Context                                                               */
    /*************************************************************************/

    IPLAPI  IPLerror    iplCreate3DContext(IPLGlobalContext globalContext, IPLDspParams dspParams, IPLbyte* hrtfData, IPLhandle* context);
    IPLAPI  IPLvoid     iplDestroy3DContext(IPLhandle* context);


    /*************************************************************************/
    /* Panning Effect                                                        */
    /*************************************************************************/

    IPLAPI  IPLerror    iplCreatePanningEffect(IPLhandle context, IPLAudioFormat inputFormat, IPLAudioFormat outputFormat, IPLhandle* effect);
    IPLAPI  IPLvoid     iplDestroyPanningEffect(IPLhandle* effect);
    IPLAPI  IPLvoid     iplApplyPanningEffect(IPLhandle effect, IPLAudioBuffer inputAudio, IPLVector3 direction, IPLAudioBuffer outputAudio);


    /*************************************************************************/
    /* Object-Based Binaural Effect                                          */
    /*************************************************************************/

    /* HRTF interpolation schemes. */
    typedef enum {
        IPL_HRTFINTERPOLATION_NEAREST,
        IPL_HRTFINTERPOLATION_BILINEAR
    } IPLHrtfInterpolation;

    IPLAPI  IPLerror    iplCreateBinauralEffect(IPLhandle context, IPLAudioFormat inputFormat, IPLAudioFormat outputFormat, IPLhandle* effect);
    IPLAPI  IPLvoid     iplDestroyBinauralEffect(IPLhandle* effect);
    IPLAPI  IPLvoid     iplApplyBinauralEffect(IPLhandle effect, IPLAudioBuffer inputAudio, IPLVector3 direction, IPLHrtfInterpolation interpolation, IPLAudioBuffer outputAudio);


    /*************************************************************************/
    /* Virtual Surround Effect                                               */
    /*************************************************************************/

    IPLAPI  IPLerror    iplCreateVirtualSurroundEffect(IPLhandle context, IPLAudioFormat inputFormat, IPLAudioFormat outputFormat, IPLhandle* effect);
    IPLAPI  IPLvoid     iplDestroyVirtualSurroundEffect(IPLhandle* effect);
    IPLAPI  IPLvoid     iplApplyVirtualSurroundEffect(IPLhandle effect, IPLAudioBuffer inputAudio, IPLAudioBuffer outputAudio);


    /*************************************************************************/
    /* Ambisonics Binaural Effect                                            */
    /*************************************************************************/

    IPLAPI  IPLerror    iplCreateAmbisonicsBinauralEffect(IPLhandle context, IPLAudioFormat inputFormat, IPLAudioFormat outputFormat, IPLhandle* effect);
    IPLAPI  IPLvoid     iplDestroyAmbisonicsBinauralEffect(IPLhandle* effect);
    IPLAPI  IPLvoid     iplApplyAmbisonicsBinauralEffect(IPLhandle effect, IPLAudioBuffer inputAudio, IPLAudioBuffer outputAudio);


    /*************************************************************************/
    /* Mixing                                                                */
    /*************************************************************************/

    IPLAPI  IPLvoid     iplMixAudioBuffers(IPLint32 numBuffers, IPLAudioBuffer* inputAudio, IPLAudioBuffer outputAudio);


    /*************************************************************************/
    /* Format Conversion                                                     */
    /*************************************************************************/

    IPLAPI  IPLvoid     iplInterleaveAudioBuffer(IPLAudioBuffer inputAudio, IPLAudioBuffer outputAudio);
    IPLAPI  IPLvoid     iplDeinterleaveAudioBuffer(IPLAudioBuffer inputAudio, IPLAudioBuffer outputAudio);
    IPLAPI  IPLvoid     iplConvertAudioBufferFormat(IPLAudioBuffer inputAudio, IPLAudioBuffer outputAudio);


#ifdef __cplusplus
}
#endif

#endif
