/***************************************************************************
 *
 *  Copyright (C) Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       xact2wb.h
 *  Content:    XACT 2 wave bank definitions.
 *
 ****************************************************************************/

#ifndef __XACTWB_H__
#define __XACTWB_H__

#ifdef _XBOX
#   include <xauddefs.h>
#else
#   include <math.h>
#endif

#include <xact.h>

#pragma warning(push)
#pragma warning(disable:4201)
#pragma warning(disable:4214)   // nonstandard extension used : bit field types other than int

#pragma pack(push, 1)

#ifdef _M_PPCBE
#pragma bitfield_order(push, lsb_to_msb)
#endif

#define WAVEBANK_HEADER_SIGNATURE               'DNBW'      // WaveBank  RIFF chunk signature
#define WAVEBANK_HEADER_VERSION                 42          // Current wavebank file version

#define WAVEBANK_BANKNAME_LENGTH                64          // Wave bank friendly name length, in characters
#define WAVEBANK_ENTRYNAME_LENGTH               64          // Wave bank entry friendly name length, in characters

#define WAVEBANK_MAX_DATA_SEGMENT_SIZE          0xFFFFFFFF  // Maximum wave bank data segment size, in bytes
#define WAVEBANK_MAX_COMPACT_DATA_SEGMENT_SIZE  0x001FFFFF  // Maximum compact wave bank data segment size, in bytes

typedef DWORD WAVEBANKOFFSET;

//
// Bank flags
//

#define WAVEBANK_TYPE_BUFFER         0x00000000      // In-memory buffer
#define WAVEBANK_TYPE_STREAMING      0x00000001      // Streaming
#define WAVEBANK_TYPE_MASK           0x00000001

#define WAVEBANK_FLAGS_ENTRYNAMES    0x00010000      // Bank includes entry names
#define WAVEBANK_FLAGS_COMPACT       0x00020000      // Bank uses compact format
#define WAVEBANK_FLAGS_SYNC_DISABLED 0x00040000      // Bank is disabled for audition sync
#define WAVEBANK_FLAGS_SEEKTABLES    0x00080000      // Bank includes seek tables.
#define WAVEBANK_FLAGS_MASK          0x000F0000

//
// Entry flags
//

#define WAVEBANKENTRY_FLAGS_READAHEAD       0x00000001  // Enable stream read-ahead
#define WAVEBANKENTRY_FLAGS_LOOPCACHE       0x00000002  // One or more looping sounds use this wave
#define WAVEBANKENTRY_FLAGS_REMOVELOOPTAIL  0x00000004  // Remove data after the end of the loop region
#define WAVEBANKENTRY_FLAGS_IGNORELOOP      0x00000008  // Used internally when the loop region can't be used
#define WAVEBANKENTRY_FLAGS_MASK            0x00000008

//
// Entry wave format identifiers
//

#define WAVEBANKMINIFORMAT_TAG_PCM      0x0     // PCM data
#define WAVEBANKMINIFORMAT_TAG_XMA      0x1     // XMA data
#define WAVEBANKMINIFORMAT_TAG_ADPCM    0x2     // ADPCM data

#define WAVEBANKMINIFORMAT_BITDEPTH_8   0x0     // 8-bit data (PCM only)
#define WAVEBANKMINIFORMAT_BITDEPTH_16  0x1     // 16-bit data (PCM only)

//
// Arbitrary fixed sizes
//
#define WAVEBANKENTRY_XMASTREAMS_MAX          3   // enough for 5.1 channel audio
#define WAVEBANKENTRY_XMACHANNELS_MAX         6   // enough for 5.1 channel audio (cf. XAUDIOCHANNEL_SOURCEMAX)

//
// DVD data sizes
//

#define WAVEBANK_DVD_SECTOR_SIZE    2048
#define WAVEBANK_DVD_BLOCK_SIZE     (WAVEBANK_DVD_SECTOR_SIZE * 16)

//
// Bank alignment presets
//

#define WAVEBANK_ALIGNMENT_MIN  4                           // Minimum alignment
#define WAVEBANK_ALIGNMENT_DVD  WAVEBANK_DVD_SECTOR_SIZE    // DVD-optimized alignment

//
// Wave bank segment identifiers
//

typedef enum WAVEBANKSEGIDX
{
    WAVEBANK_SEGIDX_BANKDATA = 0,       // Bank data
    WAVEBANK_SEGIDX_ENTRYMETADATA,      // Entry meta-data
    WAVEBANK_SEGIDX_SEEKTABLES,         // Storage for seek tables for the encoded waves.
    WAVEBANK_SEGIDX_ENTRYNAMES,         // Entry friendly names
    WAVEBANK_SEGIDX_ENTRYWAVEDATA,      // Entry wave data
    WAVEBANK_SEGIDX_COUNT
} WAVEBANKSEGIDX, *LPWAVEBANKSEGIDX;

typedef const WAVEBANKSEGIDX *LPCWAVEBANKSEGIDX;

//
// Endianness
//

#ifdef __cplusplus

namespace XACTWaveBank
{
    __inline void SwapBytes(DWORD &dw)
    {

#ifdef _X86_

        __asm
        {
            mov edi, dw
            mov eax, [edi]
            bswap eax
            mov [edi], eax
        }

#else // _X86_

        dw = _byteswap_ulong(dw);

#endif // _X86_

    }

    __inline void SwapBytes(WORD &w)
    {

#ifdef _X86_

        __asm
        {
            mov edi, w
            mov ax, [edi]
            xchg ah, al
            mov [edi], ax
        }

#else // _X86_

        w = _byteswap_ushort(w);

#endif // _X86_

    }

}

#endif // __cplusplus

//
// Wave bank region in bytes.
//

typedef struct WAVEBANKREGION
{
    DWORD       dwOffset;               // Region offset, in bytes.
    DWORD       dwLength;               // Region length, in bytes.

#ifdef __cplusplus

    void SwapBytes(void)
    {
        XACTWaveBank::SwapBytes(dwOffset);
        XACTWaveBank::SwapBytes(dwLength);
    }

#endif // __cplusplus

} WAVEBANKREGION, *LPWAVEBANKREGION;

typedef const WAVEBANKREGION *LPCWAVEBANKREGION;


//
// Wave bank region in samples.
//

typedef struct WAVEBANKSAMPLEREGION
{
    DWORD       dwStartSample;          // Start sample for the region.
    DWORD       dwTotalSamples;         // Region length in samples.

#ifdef __cplusplus

    void SwapBytes(void)
    {
        XACTWaveBank::SwapBytes(dwStartSample);
        XACTWaveBank::SwapBytes(dwTotalSamples);
    }

#endif // __cplusplus

} WAVEBANKSAMPLEREGION, *LPWAVEBANKSAMPLEREGION;

typedef const WAVEBANKSAMPLEREGION *LPCWAVEBANKSAMPLEREGION;


//
// Wave bank file header
//

typedef struct WAVEBANKHEADER
{
    DWORD           dwSignature;                        // File signature
    DWORD           dwVersion;                          // Version of the tool that created the file
    DWORD           dwHeaderVersion;                    // Version of the file format
    WAVEBANKREGION  Segments[WAVEBANK_SEGIDX_COUNT];    // Segment lookup table

#ifdef __cplusplus

    void SwapBytes(void)
    {
        XACTWaveBank::SwapBytes(dwSignature);
        XACTWaveBank::SwapBytes(dwVersion);
        XACTWaveBank::SwapBytes(dwHeaderVersion);

        for(int i = 0; i < WAVEBANK_SEGIDX_COUNT; i++)
        {
            Segments[i].SwapBytes();
        }
    }

#endif // __cplusplus

} WAVEBANKHEADER, *LPWAVEBANKHEADER;

typedef const WAVEBANKHEADER *LPCWAVEBANKHEADER;

//
// Entry compressed data format
//

typedef union WAVEBANKMINIWAVEFORMAT
{
    struct
    {
        DWORD       wFormatTag      : 2;        // Format tag
        DWORD       nChannels       : 3;        // Channel count (1 - 6)
        DWORD       nSamplesPerSec  : 18;       // Sampling rate
        DWORD       wBlockAlign     : 8;        // Block alignment
        DWORD       wBitsPerSample  : 1;        // Bits per sample (8 vs. 16, PCM only)
    };

    DWORD           dwValue;

#ifdef __cplusplus

    void SwapBytes(void)
    {
        XACTWaveBank::SwapBytes(dwValue);
    }

    WORD BitsPerSample() const
    {
        return wBitsPerSample == WAVEBANKMINIFORMAT_BITDEPTH_16 ? 16 : 8;
    }

    #define ADPCM_MINIWAVEFORMAT_BLOCKALIGN_CONVERSION_OFFSET 22
    DWORD BlockAlign() const
    {
        return wFormatTag != WAVEBANKMINIFORMAT_TAG_ADPCM ? wBlockAlign :
               (wBlockAlign + ADPCM_MINIWAVEFORMAT_BLOCKALIGN_CONVERSION_OFFSET) * nChannels;
    }

#endif // __cplusplus

} WAVEBANKMINIWAVEFORMAT, *LPWAVEBANKMINIWAVEFORMAT;

typedef const WAVEBANKMINIWAVEFORMAT *LPCWAVEBANKMINIWAVEFORMAT;

//
// Entry meta-data
//

typedef struct WAVEBANKENTRY
{
    union
    {
        struct
        {
            // Entry flags
            DWORD                   dwFlags  :  4;

            // Duration of the wave, in units of one sample.
            // For instance, a ten second long wave sampled
            // at 48KHz would have a duration of 480,000.
            // This value is not affected by the number of
            // channels, the number of bits per sample, or the
            // compression format of the wave.
            DWORD                   Duration : 28;
        };
        DWORD dwFlagsAndDuration;
    };

    WAVEBANKMINIWAVEFORMAT  Format;         // Entry format.
    WAVEBANKREGION          PlayRegion;     // Region within the wave data segment that contains this entry.
    WAVEBANKSAMPLEREGION    LoopRegion;     // Region within the wave data (in samples) that should loop.

#ifdef __cplusplus

    void SwapBytes(void)
    {
        XACTWaveBank::SwapBytes(dwFlagsAndDuration);
        Format.SwapBytes();
        PlayRegion.SwapBytes();
        LoopRegion.SwapBytes();
    }

#endif // __cplusplus

} WAVEBANKENTRY, *LPWAVEBANKENTRY;

typedef const WAVEBANKENTRY *LPCWAVEBANKENTRY;

//
// Compact entry meta-data
//

typedef struct WAVEBANKENTRYCOMPACT
{
    DWORD       dwOffset            : 21;       // Data offset, in sectors
    DWORD       dwLengthDeviation   : 11;       // Data length deviation, in bytes

#ifdef __cplusplus

    void SwapBytes(void)
    {
        XACTWaveBank::SwapBytes(*(LPDWORD)this);
    }

#endif // __cplusplus

} WAVEBANKENTRYCOMPACT, *LPWAVEBANKENTRYCOMPACT;

typedef const WAVEBANKENTRYCOMPACT *LPCWAVEBANKENTRYCOMPACT;

//
// Bank data segment
//

typedef struct WAVEBANKDATA
{
    DWORD                   dwFlags;                                // Bank flags
    DWORD                   dwEntryCount;                           // Number of entries in the bank
    CHAR                    szBankName[WAVEBANK_BANKNAME_LENGTH];   // Bank friendly name
    DWORD                   dwEntryMetaDataElementSize;             // Size of each entry meta-data element, in bytes
    DWORD                   dwEntryNameElementSize;                 // Size of each entry name element, in bytes
    DWORD                   dwAlignment;                            // Entry alignment, in bytes
    WAVEBANKMINIWAVEFORMAT  CompactFormat;                          // Format data for compact bank
    FILETIME                BuildTime;                              // Build timestamp

#ifdef __cplusplus

    void SwapBytes(void)
    {
        XACTWaveBank::SwapBytes(dwFlags);
        XACTWaveBank::SwapBytes(dwEntryCount);
        XACTWaveBank::SwapBytes(dwEntryMetaDataElementSize);
        XACTWaveBank::SwapBytes(dwEntryNameElementSize);
        XACTWaveBank::SwapBytes(dwAlignment);
        CompactFormat.SwapBytes();
        XACTWaveBank::SwapBytes(BuildTime.dwLowDateTime);
        XACTWaveBank::SwapBytes(BuildTime.dwHighDateTime);
    }

#endif // __cplusplus

} WAVEBANKDATA, *LPWAVEBANKDATA;

typedef const WAVEBANKDATA *LPCWAVEBANKDATA;

#ifdef _M_PPCBE
#pragma bitfield_order(pop)
#endif

#pragma warning(pop)
#pragma pack(pop)

#endif // __XACTWB_H__

