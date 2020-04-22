//////////////////////////////////////////////////////////////////////////////
//
//  ATI Technologies Inc.
//  1 Commerce Valley Drive East
//  Markham, Ontario
//  CANADA  L3T 7X6
//
//  File Name:   ATI_Compress.h
//  Description: A library to compress/decompress textures
//
//  Copyright (c) 2004-2006 ATI Technologies Inc.
//
//	Version:	1.4
//
//  Developer:	Seth Sowerby	
//  Email:		gputools.support@amd.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef ATI_COMPRESS
#define ATI_COMPRESS

#define ATI_COMPRESS_VERSION_MAJOR 1
#define ATI_COMPRESS_VERSION_MINOR 4

typedef unsigned long ATI_TC_DWORD;
typedef unsigned short ATI_TC_WORD;
typedef unsigned char ATI_TC_BYTE;

#if defined(WIN32) || defined(_WIN64)
#	define ATI_TC_API __cdecl
#else
#	define ATI_TC_API 
#endif

#ifdef ATI_COMPRESS_INTERNAL_BUILD
#include "ATI_Compress_Internal.h"
#else // ATI_COMPRESS_INTERNAL_BUILD

typedef enum
{
	ATI_TC_FORMAT_ARGB_8888,
	ATI_TC_FORMAT_ARGB_2101010,
    ATI_TC_FORMAT_ARGB_16,
	ATI_TC_FORMAT_ARGB_16F,
	ATI_TC_FORMAT_ARGB_32F,
    ATI_TC_FORMAT_DXT1,
    ATI_TC_FORMAT_DXT3,
    ATI_TC_FORMAT_DXT5,
	ATI_TC_FORMAT_DXT5_xGBR,
	ATI_TC_FORMAT_DXT5_RxBG,
	ATI_TC_FORMAT_DXT5_RBxG,
	ATI_TC_FORMAT_DXT5_xRBG,
	ATI_TC_FORMAT_DXT5_RGxB,
	ATI_TC_FORMAT_DXT5_xGxR,
	ATI_TC_FORMAT_ATI1N,
    ATI_TC_FORMAT_ATI2N,
    ATI_TC_FORMAT_ATI2N_XY,
    ATI_TC_FORMAT_ATI2N_DXT5,
    ATI_TC_FORMAT_MAX = ATI_TC_FORMAT_ATI2N_DXT5
} ATI_TC_FORMAT;

typedef struct _ATI_TC_CompressOptions
{
	ATI_TC_DWORD	dwSize;					/* Size of this structure */

	/* Channel Weightings */
	/* With swizzled formats the weighting applies to the data within the specified channel */ 
	/* not the channel itself. */
	BOOL			bUseChannelWeighting;
	double			fWeightingRed;			/* Weighting of the Red or X Channel */
	double			fWeightingGreen;		/* Weighting of the Green or Y Channel */
	double			fWeightingBlue;			/* Weighting of the Blue or Z Channel */
	BOOL			bUseAdaptiveWeighting;	/* Adapt weighting on a per-block basis */
	BOOL			bDXT1UseAlpha;
	ATI_TC_BYTE		nAlphaThreshold;
} ATI_TC_CompressOptions;
#endif // !ATI_COMPRESS_INTERNAL_BUILD

typedef struct _ATI_TC_Texture
{
	ATI_TC_DWORD	dwSize;				/* Size of this structure */
	ATI_TC_DWORD	dwWidth;			/* Width of the texture */
	ATI_TC_DWORD	dwHeight;			/* Height of the texture */
	ATI_TC_DWORD	dwPitch;			/* Distance to start of next line - necessary only for uncompressed textures */
	ATI_TC_FORMAT	format;				/* Format of the texture */
	ATI_TC_DWORD	dwDataSize;			/* Size of the allocated texture data */
	ATI_TC_BYTE*	pData;				/* Pointer to the texture data */
} ATI_TC_Texture;

typedef enum
{
    ATI_TC_OK = 0,
    ATI_TC_ABORTED,
    ATI_TC_ERR_INVALID_SOURCE_TEXTURE,
    ATI_TC_ERR_INVALID_DEST_TEXTURE,
    ATI_TC_ERR_UNSUPPORTED_SOURCE_FORMAT,
    ATI_TC_ERR_UNSUPPORTED_DEST_FORMAT,
    ATI_TC_ERR_SIZE_MISMATCH,
    ATI_TC_ERR_UNABLE_TO_INIT_CODEC,
    ATI_TC_ERR_GENERIC
} ATI_TC_ERROR;

#define MINIMUM_WEIGHT_VALUE 0.01f


#ifdef __cplusplus
extern "C" {
#endif

/*
**	ATI_TC_Feedback_Proc
**	Feedback proc for conversion
**	Return non-NULL(true) value to abort conversion
*/

typedef bool (ATI_TC_API * ATI_TC_Feedback_Proc)(float fProgress, uint32* pUser1, uint32* pUser2);

/*
**	ATI_TC_CalculateBufferSize
**	Calculates the required buffer size for the specified texture
*/

ATI_TC_DWORD ATI_TC_API ATI_TC_CalculateBufferSize(const ATI_TC_Texture* pTexture);


/*
**	ATI_TC_ConvertTexture
**	Converts the source texture to the destination texture
*/

ATI_TC_ERROR ATI_TC_API ATI_TC_ConvertTexture(const ATI_TC_Texture* pSourceTexture,	/* [in]  - Pointer to the source texture */
											ATI_TC_Texture* pDestTexture,			/* [out] - Pointer to the destination texture */
											const ATI_TC_CompressOptions* pOptions, /* [in]  - Pointer to the compression options - can be NULL */
											ATI_TC_Feedback_Proc pFeedbackProc,		/* [in]  - Pointer to the feedback proc - can be NULL */
											uint32* pUser1,							/* [in]  - User data to pass to the feedback proc */
											uint32* pUser2);						/* [in]  - User data to pass to the feedback proc */


#ifdef __cplusplus
};
#endif

#endif // !ATI_COMPRESS
