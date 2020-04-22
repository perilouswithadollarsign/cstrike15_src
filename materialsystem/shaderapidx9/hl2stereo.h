//--------------------------------------------------------------------------------------
// File: hl2stereo.h
// Authors: John McDonald
// Email: devsupport@nvidia.com
//
// Utility classes for stereo
//
// Copyright (c) 2009 NVIDIA Corporation. All rights reserved.
//
// NOTE: This file is provided as-is, with no warranty either expressed or implied.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef __HL2STEREO__
#define __HL2STEREO__ 1

#include "nvapi.h"

namespace nv
{
	namespace stereo
	{
		typedef struct  _Nv_Stereo_Image_Header
		{
			unsigned int dwSignature;
			unsigned int dwWidth;
			unsigned int dwHeight;
			unsigned int dwBPP;
			unsigned int dwFlags;
		} NVSTEREOIMAGEHEADER, *LPNVSTEREOIMAGEHEADER;

		#define NVSTEREO_IMAGE_SIGNATURE 0x4433564e //NV3D
		#define NVSTEREO_SWAP_EYES 0x00000001

		inline void PopulateTextureData( float *leftEye, float *rightEye, LPNVSTEREOIMAGEHEADER header, unsigned int width, unsigned int height, unsigned int pixelBytes, float eyeSep, float sep, float conv )
		{
			// Normally sep is in [0, 100], and we want the fractional part of 1.
			float finalSeparation = eyeSep * sep * 0.005f;
			leftEye[0] = -finalSeparation;
			leftEye[1] = conv;
			leftEye[2] = -1.0f;

			rightEye[0] = -leftEye[0];
			rightEye[1] = leftEye[1];
			rightEye[2] = -leftEye[2];

			// Fill the header
			header->dwSignature = NVSTEREO_IMAGE_SIGNATURE;
			header->dwWidth = width;
			header->dwHeight = height;
			header->dwBPP = pixelBytes * 8;
			header->dwFlags = 0;
		}

		// This is expensive...may take more than 1ms to return. Only call this once at startup.
		inline bool IsStereoEnabled()
		{
			NvU8 stereoEnabled = 0;
			if ( NVAPI_OK != NvAPI_Stereo_IsEnabled( &stereoEnabled ) )
			{
				// Only try to call initialize once here...doing this just in case their library always returns
				// one of the other error codes continually.
				static bool s_bFirstTime = true;
				if ( s_bFirstTime )
				{
					s_bFirstTime = false;
					NvAPI_Initialize();
					NvAPI_Stereo_CreateConfigurationProfileRegistryKey( NVAPI_STEREO_DX9_REGISTRY_PROFILE );
					if ( NVAPI_OK != NvAPI_Stereo_IsEnabled( &stereoEnabled ) )
					{
						return false;
					}
				}
				else
				{
					return false;
				}
			}

			return stereoEnabled != 0;
		}

#ifndef NO_STEREO_D3D9
		// The D3D9 "Driver" for stereo updates, encapsulates the logic that is Direct3D9 specific.
		struct D3D9Type
		{
			typedef IDirect3DDevice9 Device;
			typedef IDirect3DTexture9 Texture;
			typedef IDirect3DSurface9 StagingResource;

			static const NV_STEREO_REGISTRY_PROFILE_TYPE RegistryProfileType = NVAPI_STEREO_DX9_REGISTRY_PROFILE;

			static const int StereoTexWidth = 8;
			static const int StereoTexHeight = 1;
			static const D3DFORMAT StereoTexFormat = D3DFMT_R32F;
			static const int StereoBytesPerPixel = 4;

			static StagingResource *CreateStagingResource( Device *pDevice, float eyeSep, float sep, float conv )
			{
				StagingResource *staging = 0;
				unsigned int stagingWidth = StereoTexWidth * 2;
				unsigned int stagingHeight = StereoTexHeight + 1;

				pDevice->CreateOffscreenPlainSurface( stagingWidth, stagingHeight, StereoTexFormat, D3DPOOL_SYSTEMMEM, &staging, NULL );

				if ( !staging )
				{
					return 0;
				}

				D3DLOCKED_RECT lr;
				staging->LockRect( &lr, NULL, 0 );
				unsigned char *sysData = ( unsigned char * ) lr.pBits;
				unsigned int sysMemPitch = stagingWidth * StereoBytesPerPixel;

				float *leftEyePtr = ( float * )sysData;
				float *rightEyePtr = leftEyePtr + StereoTexWidth;
				LPNVSTEREOIMAGEHEADER header = ( LPNVSTEREOIMAGEHEADER )( sysData + sysMemPitch );
				PopulateTextureData( leftEyePtr, rightEyePtr, header, stagingWidth, stagingHeight, StereoBytesPerPixel, eyeSep, sep, conv );
				staging->UnlockRect();

				return staging;
			}

			static void UpdateTextureFromStaging( Device *pDevice, Texture *tex, StagingResource *staging )
			{
				RECT stereoSrcRect;
				stereoSrcRect.top = 0;
				stereoSrcRect.bottom = StereoTexHeight;
				stereoSrcRect.left = 0;
				stereoSrcRect.right = StereoTexWidth;

				POINT stereoDstPoint;
				stereoDstPoint.x = 0;
				stereoDstPoint.y = 0;

				IDirect3DSurface9 *texSurface;
				tex->GetSurfaceLevel( 0, &texSurface );

				pDevice->UpdateSurface( staging, &stereoSrcRect, texSurface, &stereoDstPoint );
				texSurface->Release();
			}
		};
#endif // NO_STEREO_D3D9

#ifndef NO_STEREO_D3D10
		// The D3D10 "Driver" for stereo updates, encapsulates the logic that is Direct3D10 specific.
		struct D3D10Type
		{
			typedef ID3D10Device Device;
			typedef ID3D10Texture2D Texture;
			typedef ID3D10Texture2D StagingResource;

			static const NV_STEREO_REGISTRY_PROFILE_TYPE RegistryProfileType = NVAPI_STEREO_DX10_REGISTRY_PROFILE;

			static const int StereoTexWidth = 8;
			static const int StereoTexHeight = 1;
			static const DXGI_FORMAT StereoTexFormat = DXGI_FORMAT_R32_FLOAT;
			static const int StereoBytesPerPixel = 4;

			static StagingResource *CreateStagingResource( Device *pDevice, float eyeSep, float sep, float conv )
			{
				StagingResource *staging = 0;
				unsigned int stagingWidth = StereoTexWidth * 2;
				unsigned int stagingHeight = StereoTexHeight + 1;

				// Allocate the buffer sys mem data to write the stereo tag and stereo params
				D3D10_SUBRESOURCE_DATA sysData;
				sysData.SysMemPitch = StereoBytesPerPixel * stagingWidth;
				sysData.pSysMem = new unsigned char[sysData.SysMemPitch * stagingHeight];

				float *leftEyePtr = ( float * )sysData.pSysMem;
				float *rightEyePtr = leftEyePtr + StereoTexWidth;
				LPNVSTEREOIMAGEHEADER header = ( LPNVSTEREOIMAGEHEADER )( ( unsigned char * )sysData.pSysMem + sysData.SysMemPitch );
				PopulateTextureData( leftEyePtr, rightEyePtr, header, stagingWidth, stagingHeight, StereoBytesPerPixel, eyeSep, sep, conv );

				D3D10_TEXTURE2D_DESC desc;
				memset( &desc, 0, sizeof( D3D10_TEXTURE2D_DESC ) );
				desc.Width = stagingWidth;
				desc.Height = stagingHeight;
				desc.MipLevels = 1;
				desc.ArraySize = 1;
				desc.Format = StereoTexFormat;
				desc.SampleDesc.Count = 1;
				desc.Usage = D3D10_USAGE_DEFAULT;
				desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
				desc.CPUAccessFlags = 0;
				desc.MiscFlags = 0;

				pDevice->CreateTexture2D( &desc, &sysData, &staging );
				delete [] sysData.pSysMem;
				return staging;
			}

			static void UpdateTextureFromStaging( Device *pDevice, Texture *tex, StagingResource *staging )
			{
				D3D10_BOX stereoSrcBox;
				stereoSrcBox.front = 0;
				stereoSrcBox.back = 1;
				stereoSrcBox.top = 0;
				stereoSrcBox.bottom = StereoTexHeight;
				stereoSrcBox.left = 0;
				stereoSrcBox.right = StereoTexWidth;

				pDevice->CopySubresourceRegion( tex, 0, 0, 0, 0, staging, 0, &stereoSrcBox );
			}
		};
#endif // NO_STEREO_D3D10

		// The HL2 Stereo class, which can work for either D3D9 or D3D10, depending on which type it's specialized for
		// Note that both types can live side-by-side in two seperate instances as well.
		// Also note that there are convenient typedefs below the class definition.
		template < class D3DType >
		class HL2Stereo
		{
		public:
			typedef typename D3DType Parms;
			typedef typename D3DType::Device Device;
			typedef typename D3DType::Texture Texture;
			typedef typename D3DType::StagingResource StagingResource;

			HL2Stereo() :
			mEyeSeparation( 0 ),
				mSeparation( 0 ),
				mConvergence( 0 ),
				mStereoHandle( 0 ),
				mInitialized( false ),
				mActive( false ),
				mDeviceLost( true ) // mDeviceLost is set to true to initialize the texture with good data at app startup.
			{
				NvAPI_Initialize();
				NvAPI_Stereo_CreateConfigurationProfileRegistryKey( D3DType::RegistryProfileType );
			}

			~HL2Stereo()
			{
				if ( mStereoHandle )
				{
					NvAPI_Stereo_DestroyHandle( mStereoHandle );
					mStereoHandle = 0;
				}
			}

			void Init( Device *dev )
			{
				NvAPI_Stereo_CreateHandleFromIUnknown( dev, &mStereoHandle );

				// Set that we've initialized regardless --we'll only try to init once.
				mInitialized = true;
			}

			// Not const because we will update the various values if an update is needed.
			bool RequiresUpdate( bool deviceLost )
			{
				bool active = IsStereoActive();
				bool updateRequired;
				float eyeSep, sep, conv;
				if ( active )
				{
					if ( NVAPI_OK != NvAPI_Stereo_GetEyeSeparation( mStereoHandle, &eyeSep ) )
						return false;
					if ( NVAPI_OK != NvAPI_Stereo_GetSeparation( mStereoHandle, &sep ) )
						return false;
					if ( NVAPI_OK != NvAPI_Stereo_GetConvergence( mStereoHandle, &conv ) )
						return false;

					// clamp the convergence to prevent wallhack exploit
					if ( conv > 31.0f )
					{
						conv = 31.0f;
						NvAPI_Stereo_SetConvergence( mStereoHandle, conv );
						DevMsg( "[NVIDIA Stereo 3D] Clamping convergence: %.2f\n", conv);
					}

					updateRequired = ( eyeSep != mEyeSeparation )
						|| ( sep != mSeparation )
						|| ( conv != mConvergence )
						|| ( active != mActive );
				}
				else
				{
					eyeSep = sep = conv = 0;
					updateRequired = active != mActive;
				}

				// If the device was lost and is now restored, need to update the texture contents again.
				updateRequired = updateRequired || ( !deviceLost && mDeviceLost );
				mDeviceLost = deviceLost;

				if ( updateRequired )
				{
					//Msg( "*** NV_STEREO - UpdateRequired == true\n" );
					mEyeSeparation = eyeSep;
					mSeparation = sep;
					mConvergence = conv;
					mActive = active;
					return true;
				}

				return false;
			}

			bool IsStereoActive() const
			{
				NvU8 stereoActive = 0;
				if ( NVAPI_OK != NvAPI_Stereo_IsActivated( mStereoHandle, &stereoActive ) )
				{
					return false;
				}

				return stereoActive != 0;
			}

			void UpdateStereoTexture( Device *dev, Texture *tex, bool deviceLost )
			{
				if ( !mInitialized )
				{
					Init( dev );
				}

				if ( !RequiresUpdate( deviceLost ) )
				{
					return;
				}

				DevMsg( "[NVIDIA Stereo 3D] UpdateStereoTexture: EyeSep: %.2f, Sep: %.2f, Conv: %.2f\n", mEyeSeparation, mSeparation, mConvergence);

				StagingResource *staging = D3DType::CreateStagingResource( dev, mEyeSeparation, mSeparation, mConvergence );
				if ( staging )
				{
					D3DType::UpdateTextureFromStaging( dev, tex, staging );
					staging->Release();
				}
			}

		private:
			float mEyeSeparation;
			float mSeparation;
			float mConvergence;

			StereoHandle mStereoHandle;
			bool mInitialized;
			bool mActive;
			bool mDeviceLost;
		};

#ifndef NO_STEREO_D3D9
		typedef HL2Stereo< D3D9Type > HL2StereoD3D9;
#endif

#ifndef NO_STEREO_D3D10
		typedef HL2Stereo< D3D10Type > HL2StereoD3D10;
#endif
	};
};

#endif /* __HL2STEREO__ */
