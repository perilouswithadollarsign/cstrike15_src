/*
   Copyright (C) Impulsonic, Inc. All rights reserved.
*/

#ifndef IPL_PHONON_API_COMMON_H
#define IPL_PHONON_API_COMMON_H

#if (defined(_WIN32) || defined(_WIN64))
#define IPLAPI __declspec(dllexport)
#else
#define IPLAPI __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif


    /*************************************************************************/
    /* Basic Data Types                                                      */
    /*************************************************************************/

    /* Elementary data types. */
    typedef void                IPLvoid;
    typedef char                IPLint8;
    typedef unsigned char       IPLuint8;
    typedef short               IPLint16;
    typedef unsigned short      IPLuint16;
    typedef int                 IPLint32;
    typedef unsigned int        IPLuint32;
    typedef long long           IPLint64;
    typedef unsigned long long  IPLuint64;
    typedef float               IPLfloat32;
    typedef double              IPLfloat64;
    typedef unsigned char       IPLbyte;
    typedef size_t              IPLsize;
    typedef char*               IPLstring;

    /* Boolean values. */
    typedef enum {
        IPL_FALSE,
        IPL_TRUE
    } IPLbool;

    /* Opaque object handles. */
    typedef void*               IPLhandle;

    /* Status codes. */
    typedef enum {
        IPL_STATUS_SUCCESS,
        IPL_STATUS_FAILURE,
        IPL_STATUS_OUTOFMEMORY,
        IPL_STATUS_INITIALIZATION
    } IPLerror;


    /*************************************************************************/
    /* Context                                                               */
    /*************************************************************************/

    /* Logger function prototype. */
    typedef IPLvoid     (*IPLLogFunction)(char* message);

    /* Memory allocator function prototypes. */
    typedef IPLvoid*    (*IPLAllocateFunction)(IPLsize, IPLsize);
    typedef IPLvoid     (*IPLFreeFunction)(IPLvoid*);

    /* Context data structure. */
    typedef struct {
        IPLLogFunction      logCallback;
        IPLAllocateFunction allocateCallback;
        IPLFreeFunction     freeCallback;
    } IPLGlobalContext;


    /*************************************************************************/
    /* Geometric Types                                                       */
    /*************************************************************************/

    /* Points in 3D space. */
    typedef struct {
        IPLfloat32  x;
        IPLfloat32  y;
        IPLfloat32  z;
    } IPLVector3;


    /*************************************************************************/
    /* Audio Buffers                                                         */
    /*************************************************************************/

    /* Whether the audio buffer is Ambisonics or not. */
    typedef enum {
        IPL_CHANNELLAYOUTTYPE_SPEAKERS,
        IPL_CHANNELLAYOUTTYPE_AMBISONICS
    } IPLChannelLayoutType;

    /* For speaker-based layouts, the type of layout used. */
    typedef enum {
        IPL_CHANNELLAYOUT_MONO,
        IPL_CHANNELLAYOUT_STEREO,
        IPL_CHANNELLAYOUT_QUADRAPHONIC,
        IPL_CHANNELLAYOUT_FIVEPOINTONE,
        IPL_CHANNELLAYOUT_SEVENPOINTONE,
        IPL_CHANNELLAYOUT_CUSTOM
    } IPLChannelLayout;

    /* The different possible orderings of Ambisonics channels. */
    typedef enum {
        IPL_AMBISONICSORDERING_FURSEMALHAM,
        IPL_AMBISONICSORDERING_ACN
    } IPLAmbisonicsOrdering;

    /* The different possible normalization conventions for Ambisonics. */
    typedef enum {
        IPL_AMBISONICSNORMALIZATION_FURSEMALHAM,
        IPL_AMBISONICSNORMALIZATION_SN3D,
        IPL_AMBISONICSNORMALIZATION_N3D
    } IPLAmbisonicsNormalization;

    /* Whether the data is interleaved or deinterleaved. */
    typedef enum {
        IPL_CHANNELORDER_INTERLEAVED,
        IPL_CHANNELORDER_DEINTERLEAVED
    } IPLChannelOrder;

    /* The format of an audio buffer. */
    typedef struct {
        IPLChannelLayoutType        channelLayoutType;
        IPLChannelLayout            channelLayout;
        IPLint32                    numSpeakers;
        IPLVector3*                 speakerDirections;
        IPLint32                    ambisonicsOrder;
        IPLAmbisonicsOrdering       ambisonicsOrdering;
        IPLAmbisonicsNormalization  ambisonicsNormalization;
        IPLChannelOrder             channelOrder;
    } IPLAudioFormat;

    /* An audio buffer. */
    typedef struct {
        IPLAudioFormat  format;
        IPLint32        numSamples;
        IPLfloat32*     interleavedBuffer;
        IPLfloat32**    deinterleavedBuffer;
    } IPLAudioBuffer;


    /*************************************************************************/
    /* DSP Parameters                                                        */
    /*************************************************************************/

    /* Properties of the DSP pipeline. */
    typedef struct {
        IPLint32    samplingRate;
        IPLint32    frameSize;
    } IPLDspParams;


#ifdef __cplusplus
}
#endif

#endif
