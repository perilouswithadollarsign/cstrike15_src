//===== Copyright � 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef VTF_H
#define VTF_H

#ifdef _WIN32
#pragma once
#endif

#include "bitmap/imageformat.h"
#include "tier0/platform.h"

#include "vtf/vtf_declarations.h"

// #define VTF_FILE_FORMAT_ONLY to just include the vtf header and none of the code declaration
#ifndef VTF_FILE_FORMAT_ONLY



//-----------------------------------------------------------------------------
// Interface to get at various bits of a VTF texture
//-----------------------------------------------------------------------------
class IVTFTexture
{
public:
    virtual ~IVTFTexture() { }
	// Initializes the texture and allocates space for the bits
	// In most cases, you shouldn't force the mip count.
	virtual bool Init( int nWidth, int nHeight, int nDepth, ImageFormat fmt, int nFlags, int iFrameCount, int nForceMipCount = -1 ) = 0;

	// Methods to set other texture fields
	virtual void SetBumpScale( float flScale ) = 0;
	virtual void SetReflectivity( const Vector &vecReflectivity ) = 0;

	// Methods to initialize the low-res image
	virtual void InitLowResImage( int nWidth, int nHeight, ImageFormat fmt ) = 0;

	// set the resource data (for writers). pass size=0 to delete data. if pdata is not null,
	// the resource data will be copied from *pData
	virtual void *SetResourceData( uint32 eType, void const *pData, size_t nDataSize ) = 0;

	// find the resource data and return a pointer to it. The data pointed to by this pointer will
	// go away when the ivtftexture does. retruns null if resource not present
	virtual void *GetResourceData( uint32 eType, size_t *pDataSize ) const = 0;

	// Locates the resource entry info if it's present, easier than crawling array types
	virtual bool HasResourceEntry( uint32 eType ) const = 0;

	// Retrieve available resource types of this IVTFTextures
	//		arrTypesBuffer			buffer to be filled with resource types available.
	//		numTypesBufferElems		how many resource types the buffer can accomodate.
	// Returns:
	//		number of resource types available (can be greater than "numTypesBufferElems"
	//		in which case only first "numTypesBufferElems" are copied to "arrTypesBuffer")
	virtual unsigned int GetResourceTypes( uint32 *arrTypesBuffer, int numTypesBufferElems ) const = 0;

	// When unserializing, we can skip a certain number of mip levels,
	// and we also can just load everything but the image data
	// NOTE: If you load only the buffer header, you'll need to use the
	// VTFBufferHeaderSize() method below to only read that much from the file
	// NOTE: If you skip mip levels, the height + width of the texture will
	// change to reflect the size of the largest read in mip level
	virtual bool Unserialize( CUtlBuffer &buf, bool bHeaderOnly = false, int nSkipMipLevels = 0 ) = 0;
	virtual bool Serialize( CUtlBuffer &buf ) = 0;

	// These are methods to help with optimization:
	// Once the header is read in, they indicate where to start reading
	// other data (measured from file start), and how many bytes to read....
	virtual void LowResFileInfo( int *pStartLocation, int *pSizeInBytes) const = 0;
	virtual void ImageFileInfo( int nFrame, int nFace, int nMip, int *pStartLocation, int *pSizeInBytes) const = 0;
	virtual int FileSize( int nMipSkipCount = 0 ) const = 0;

	// Attributes...
	virtual int Width() const = 0;
	virtual int Height() const = 0;
	virtual int Depth() const = 0;
	virtual int MipCount() const = 0;

	// returns the size of one row of a particular mip level
	virtual int RowSizeInBytes( int nMipLevel ) const = 0;

	// returns the size of one face of a particular mip level
	virtual int FaceSizeInBytes( int nMipLevel ) const = 0;

	virtual ImageFormat Format() const = 0;
	virtual int FaceCount() const = 0;
	virtual int FrameCount() const = 0;
	virtual int Flags() const = 0;

	virtual float BumpScale() const = 0;

	virtual int LowResWidth() const = 0;
	virtual int LowResHeight() const = 0;
	virtual ImageFormat LowResFormat() const = 0;

	// NOTE: reflectivity[0] = blue, [1] = greem, [2] = red
	virtual const Vector &Reflectivity() const = 0;

	virtual bool IsCubeMap() const = 0;
	virtual bool IsNormalMap() const = 0;
	virtual bool IsVolumeTexture() const = 0;

	// Computes the dimensions of a particular mip level
	virtual void ComputeMipLevelDimensions( int iMipLevel, int *pMipWidth, int *pMipHeight, int *pMipDepth ) const = 0;

	// Computes the size (in bytes) of a single mipmap of a single face of a single frame 
	virtual int ComputeMipSize( int iMipLevel ) const = 0;

	// Computes the size of a subrect (specified at the top mip level) at a particular lower mip level
	virtual void ComputeMipLevelSubRect( Rect_t* pSrcRect, int nMipLevel, Rect_t *pSubRect ) const = 0;

	// Computes the size (in bytes) of a single face of a single frame
	// All mip levels starting at the specified mip level are included
	virtual int ComputeFaceSize( int iStartingMipLevel = 0 ) const = 0;

	// Computes the total size (in bytes) of all faces, all frames
	virtual int ComputeTotalSize() const = 0;

	// Returns the base address of the image data
	virtual uint8 *ImageData() = 0;

	// Returns a pointer to the data associated with a particular frame, face, and mip level
	virtual uint8 *ImageData( int iFrame, int iFace, int iMipLevel ) = 0;

	// Returns a pointer to the data associated with a particular frame, face, mip level, and offset
	virtual uint8 *ImageData( int iFrame, int iFace, int iMipLevel, int x, int y, int z = 0 ) = 0;

	// Returns the base address of the low-res image data
	virtual uint8 *LowResImageData() = 0;

	// Converts the textures image format. Use IMAGE_FORMAT_DEFAULT
	// if you want to be able to use various tool functions below
	virtual	void ConvertImageFormat( ImageFormat fmt, bool bNormalToDUDV, bool bNormalToDXT5GA = false ) = 0;

	// NOTE: The following methods only work on textures using the
	// IMAGE_FORMAT_DEFAULT!

	// Generate spheremap based on the current cube faces (only works for cubemaps)
	// The look dir indicates the direction of the center of the sphere
	// NOTE: Only call this *after* cube faces have been correctly
	// oriented (using FixCubemapFaceOrientation)
#if !defined (_PS3)
	virtual void GenerateSpheremap( LookDir_t lookDir = LOOK_DOWN_Z ) = 0;
#endif

	// Generate spheremap based on the current cube faces (only works for cubemaps)
	// The look dir indicates the direction of the center of the sphere
	// NOTE: Only call this *after* cube faces have been correctly
	// oriented (using FixCubemapFaceOrientation)
	virtual void GenerateHemisphereMap( uint8 *pSphereMapBitsRGBA, int targetWidth, 
		int targetHeight, LookDir_t lookDir, int iFrame ) = 0;

	// Fixes the cubemap faces orientation from our standard to the
	// standard the material system needs.
	virtual void FixCubemapFaceOrientation( ) = 0;

	// Generates mipmaps from the base mip levels
	virtual void GenerateMipmaps() = 0;

	// Put 1/miplevel (1..n) into alpha.
	virtual void PutOneOverMipLevelInAlpha() = 0;

	// Scale alpha by miplevel/mipcount
	virtual void PremultAlphaWithMipFraction() = 0;
	
	// Computes the reflectivity
	virtual void ComputeReflectivity( ) = 0;

	// Computes the alpha flags
	virtual void ComputeAlphaFlags() = 0;

	// Generate the low-res image bits
	virtual bool ConstructLowResImage() = 0;

	// Gets the texture all internally consistent assuming you've loaded
	// mip 0 of all faces of all frames
	virtual void PostProcess(bool bGenerateSpheremap, LookDir_t lookDir = LOOK_DOWN_Z, bool bAllowFixCubemapOrientation = true, bool bLoadedMiplevels = false) = 0;

	// Blends adjacent pixels on cubemap borders, since the card doesn't do it. If the texture
	// is S3TC compressed, then it has to do it AFTER the texture has been compressed to prevent
	// artifacts along the edges.
	//
	// If bSkybox is true, it assumes the faces are oriented in the way the engine draws the skybox
	// (which happens to be different from the way cubemaps have their faces).
	virtual void MatchCubeMapBorders( int iStage, ImageFormat finalFormat, bool bSkybox ) = 0;

	// Sets threshhold values for alphatest mipmapping
	virtual void SetAlphaTestThreshholds( float flBase, float flHighFreq ) = 0;

	virtual bool IsPreTiled() const = 0;

#if defined( _GAMECONSOLE )
	virtual int UpdateOrCreate( const char *pFilename, const char *pPathID = NULL, bool bForce = false ) = 0;
	virtual bool UnserializeFromBuffer( CUtlBuffer &buf, bool bBufferIsVolatile, bool bHeaderOnly, bool bPreloadOnly, int nMipSkipCount ) = 0;
	virtual int FileSize( bool bPreloadOnly, int nMipSkipCount ) const = 0;
	virtual int MappingWidth() const = 0;
	virtual int MappingHeight() const = 0;
	virtual int MappingDepth() const = 0;
	virtual int MipSkipCount() const = 0;
	virtual uint8 *LowResImageSample() = 0;
	virtual void ReleaseImageMemory() = 0;
#endif
#if defined ( _PS3 )
	virtual int GetImageOffset() const = 0;
#endif

	// Sets post-processing flags (settings are copied, pointer passed to distinguish between structure versions)
	virtual void SetPostProcessingSettings( VtfProcessingOptions const *pOptions ) = 0;
};

//-----------------------------------------------------------------------------
// Class factory
//-----------------------------------------------------------------------------
IVTFTexture *CreateVTFTexture();
void DestroyVTFTexture( IVTFTexture *pTexture );

//-----------------------------------------------------------------------------
// Allows us to only load in the first little bit of the VTF file to get info
// Clients should read this much into a UtlBuffer and then pass it in to
// Unserialize
//-----------------------------------------------------------------------------
int VTFFileHeaderSize( int nMajorVersion = -1, int nMinorVersion = -1 );

//-----------------------------------------------------------------------------
// 360 Conversion
//-----------------------------------------------------------------------------
typedef bool (*CompressFunc_t)( CUtlBuffer &inputBuffer, CUtlBuffer &outputBuffer );
bool ConvertVTFTo360Format( const char *pDebugName, CUtlBuffer &sourceBuf, CUtlBuffer &targetBuf, CompressFunc_t pCompressFunc, int nMaxMip = 0 );
bool ConvertVTFToPS3Format( const char *pDebugName, CUtlBuffer &sourceBuf, CUtlBuffer &targetBuf, CompressFunc_t pCompressFunc, int nMaxMip = 0 );
bool SRGBCorrectImage( uint8 *pImage,int imageSize, ImageFormat	imageFormat);

//-----------------------------------------------------------------------------
// 360 Preload
//-----------------------------------------------------------------------------
bool GetVTFPreload360Data( const char *pDebugName, CUtlBuffer &fileBufferIn, CUtlBuffer &preloadBufferOut );
bool GetVTFPreloadPS3Data( const char *pDebugName, CUtlBuffer &fileBufferIn, CUtlBuffer &preloadBufferOut );

#include "mathlib/vector.h"

#endif // VTF_FILE_FORMAT_ONLY

//-----------------------------------------------------------------------------
// Disk format for VTF files ver. 7.2 and earlier
//
// NOTE: After the header is the low-res image data
// Then follows image data, which is sorted in the following manner
//
//	for each mip level (starting with 1x1, and getting larger)
//		for each animation frame
//			for each face
//				store the image data for the face
//
// NOTE: In memory, we store the data in the following manner:
//	for each animation frame
//		for each face
//			for each mip level (starting with the largest, and getting smaller)
//				store the image data for the face
//
// This is done because the various image manipulation function we have
// expect this format
//-----------------------------------------------------------------------------
// Disk format for VTF files ver. 7.3
//
// NOTE: After the header is the array of ResourceEntryInfo structures,
// number of elements in the array is defined by "numResources".
// there are entries for:
//		eRsrcLowResImage	=	low-res image data
//		eRsrcSheet			=	sheet data
//		eRsrcImage			=	image data
// {
//	for each mip level (starting with 1x1, and getting larger)
//		for each animation frame
//			for each face
//				store the image data for the face
//
// NOTE: In memory, we store the data in the following manner:
//	for each animation frame
//		for each face
//			for each mip level (starting with the largest, and getting smaller)
//				store the image data for the face
// }
//
//-----------------------------------------------------------------------------


#include "datamap.h"

#pragma pack(1)

// version number for the disk texture cache
#define VTF_MAJOR_VERSION 7
#define VTF_MINOR_VERSION 5

//-----------------------------------------------------------------------------
// !!!!CRITICAL!!!! BEFORE YOU CHANGE THE FORMAT
//
// The structure sizes ARE NOT what they appear, regardless of Pack(1).
// The "VectorAligned" causes invisible padding in the FINAL derived structure.
// 
// Each VTF format has been silently plagued by this.
//
// LOOK AT A 7.3 FILE. The 7.3 structure ends at 0x48 as you would expect by
// counting structure bytes. But, the "Infos" start at 0x50! because the PC
// compiler pads, the 360 compiler does NOT.
//-----------------------------------------------------------------------------

struct VTFFileBaseHeader_t
{
	DECLARE_BYTESWAP_DATADESC();
	char fileTypeString[4]; // "VTF" Valve texture file
	int version[2]; 		// version[0].version[1]
	int headerSize;
};

struct VTFFileHeaderV7_1_t : public VTFFileBaseHeader_t 
{
	DECLARE_BYTESWAP_DATADESC();
	uint16	width;
	uint16	height;
	uint32	flags;
	uint16	numFrames;
	uint16	startFrame;
#if !defined( POSIX ) && !defined( _X360 )
	VectorAligned	reflectivity;
#else
	// must manually align in order to maintain pack(1) expected layout with existing binaries
	char			pad1[4];
	Vector			reflectivity;
	char			pad2[4];
#endif
	float			bumpScale;
	ImageFormat		imageFormat;
	uint8	numMipLevels;
	ImageFormat		lowResImageFormat;
	uint8	lowResImageWidth;
	uint8	lowResImageHeight;	
};

struct VTFFileHeaderV7_2_t : public VTFFileHeaderV7_1_t
{
	DECLARE_BYTESWAP_DATADESC();

	uint16 depth;
};

#define BYTE_POS( byteVal, shft )	uint32( uint32(uint8(byteVal)) << uint8(shft * 8) )
#if !defined( _X360 ) && !defined ( _PS3 )
#define MK_VTF_RSRC_ID(a, b, c)		uint32( BYTE_POS(a, 0) | BYTE_POS(b, 1) | BYTE_POS(c, 2) )
#define MK_VTF_RSRCF(d)				BYTE_POS(d, 3)
#else
#define MK_VTF_RSRC_ID(a, b, c)		uint32( BYTE_POS(a, 3) | BYTE_POS(b, 2) | BYTE_POS(c, 1) )
#define MK_VTF_RSRCF(d)				BYTE_POS(d, 0)
#endif

// Special section for stock resources types
enum ResourceEntryType
{
	// Legacy stock resources, readin/writing are handled differently (i.e. they do not have the length tag word!)
	VTF_LEGACY_RSRC_LOW_RES_IMAGE	= MK_VTF_RSRC_ID( 0x01, 0, 0 ),	// Low-res image data
	VTF_LEGACY_RSRC_IMAGE			= MK_VTF_RSRC_ID( 0x30, 0, 0 ),	// Image data

	// New extended resource
	VTF_RSRC_SHEET = MK_VTF_RSRC_ID( 0x10, 0, 0 ),			// Sheet data
};

// Bytes with special meaning when set in a resource type
enum ResourceEntryTypeFlag
{
	RSRCF_HAS_NO_DATA_CHUNK	= MK_VTF_RSRCF( 0x02 ),	// Resource doesn't have a corresponding data chunk
	RSRCF_MASK				= MK_VTF_RSRCF( 0xFF )	// Mask for all the flags
};

// Header details constants
enum HeaderDetails
{
	MAX_RSRC_DICTIONARY_ENTRIES = 32,		// Max number of resources in dictionary
	MAX_X360_RSRC_DICTIONARY_ENTRIES = 4,	// 360 needs this to be slim, otherwise preload size suffers
};

struct ResourceEntryInfo
{
	union
	{ 
		uint32	eType;		// Use MK_VTF_??? macros to be endian compliant with the type
		uint8	chTypeBytes[4];
	};
	uint32		resData;	// Resource data or offset from the beginning of the file
};

struct VTFFileHeaderV7_3_t : public VTFFileHeaderV7_2_t
{
	DECLARE_BYTESWAP_DATADESC();

	char			pad4[3];
	uint32	numResources;

#if defined( _X360 ) || defined( POSIX )
	// must manually align in order to maintain pack(1) expected layout with existing binaries
	char			pad5[8];
#endif
	
	// AFTER THE IMPLICIT PADDING CAUSED BY THE COMPILER....
	// *** followed by *** ResourceEntryInfo resources[0];
	// Array of resource entry infos sorted ascending by type
};

struct VTFFileHeader_t : public VTFFileHeaderV7_3_t
{
	DECLARE_BYTESWAP_DATADESC();
};

#define VTF_X360_MAJOR_VERSION	0x0360
#define VTF_X360_MINOR_VERSION	8
struct VTFFileHeaderX360_t : public VTFFileBaseHeader_t 
{
	DECLARE_BYTESWAP_DATADESC();
	uint32	flags;
	uint16	width;					// actual width of data in file
	uint16	height;					// actual height of data in file
	uint16	depth;					// actual depth of data in file
	uint16	numFrames;
	uint16	preloadDataSize;		// exact size of preload data (may extend into image!)
	uint8	mipSkipCount;			// used to resconstruct mapping dimensions
	uint8	numResources;
	Vector			reflectivity;			// Resides on 16 byte boundary!
	float			bumpScale;
	ImageFormat		imageFormat;
	uint8	lowResImageSample[4];
	uint32	compressedSize;

	// *** followed by *** ResourceEntryInfo resources[0];
};

// PS3 version of the vtf file header. This is just a copy of the 360 file header
#define VTF_PS3_MAJOR_VERSION	0x0333
#define VTF_PS3_MINOR_VERSION	8
struct ALIGN16 VTFFileHeaderPS3_t : public VTFFileBaseHeader_t 
{
	DECLARE_BYTESWAP_DATADESC();
	uint32	flags;
	uint16	width;					// actual width of data in file
	uint16	height;					// actual height of data in file
	uint16	depth;					// actual depth of data in file
	uint16	numFrames;
	uint16	preloadDataSize;		// exact size of preload data (may extend into image!)
	uint8	mipSkipCount;			// used to resconstruct mapping dimensions
	uint8	numResources;
	Vector			reflectivity;			// Resides on 16 byte boundary!
	float			bumpScale;
	ImageFormat		imageFormat;
	uint8	lowResImageSample[4];
	uint32	compressedSize;

	uint8 _padding_ps3[4];			// padding it to 64 bytes

	// *** followed by *** ResourceEntryInfo resources[0];
}
ALIGN16_POST;

///////////////////////////
//  Resource Extensions  //
///////////////////////////

// extended texture lod control:
#define VTF_RSRC_TEXTURE_LOD_SETTINGS ( MK_VTF_RSRC_ID( 'L','O','D' ) )
struct TextureLODControlSettings_t
{
	// What to clamp the dimenstions to, mip-map wise, when at picmip 0. keeps texture from
	// exceeding (1<<m_ResolutionClamp) at picmip 0.  at picmip 1, it won't exceed
	// (1<<(m_ResolutionClamp-1)), etc.
	uint8 m_ResolutionClampX;
	uint8 m_ResolutionClampY;

	uint8 m_ResolutionClampX_360;
	uint8 m_ResolutionClampY_360;
};

// Extended flags and settings:
#define VTF_RSRC_TEXTURE_SETTINGS_EX ( MK_VTF_RSRC_ID( 'T','S','0' ) )
struct TextureSettingsEx_t
{
	enum Flags0			// flags0 byte mask
	{
		UNUSED = 0x01,
	};

	uint8 m_flags0;		// a bitwise combination of Flags0
	uint8 m_flags1;		// set to zero. for future expansion.
	uint8 m_flags2;		// set to zero. for future expansion.
	uint8 m_flags3;		// set to zero. for future expansion.
};

#define VTF_RSRC_TEXTURE_CRC ( MK_VTF_RSRC_ID( 'C','R','C' ) )

#pragma pack()

#endif // VTF_H
