//========= Copyright (c) 2010, Valve Corporation, All rights reserved. ============//
/*   SCE CONFIDENTIAL                                       */
/*   PlayStation(R)3 Programmer Tool Runtime Library 350.001 */
/*   Copyright (C) 2009 Sony Computer Entertainment Inc.    */
/*   All Rights Reserved.                                   */
/*   File: main.cpp
 *   Description:
 *     simple graphics to show how to use libgcm
 *
 */
#include "errorrenderloop.h"
#include <ppu_intrinsics.h>

#define ARRAYSIZE( ARRAY ) ( sizeof( ARRAY ) / sizeof( ( ARRAY )[0] ) )


#ifdef _DEBUG
#define CELL_GCMUTIL_ASSERT(X) do{ if( !( X ) ) {printf( "Assert(%s)\n%s:%d\n", #X, __FILE__, __LINE__ ); __builtin_snpause(); } }while(0)
#define CELL_GCMUTIL_CHECK_ASSERT(X) CELL_GCMUTIL_ASSERT( X == CELL_OK )
#define CELL_GCMUTIL_CG_PARAMETER_CHECK_ASSERT(X) CELL_GCMUTIL_ASSERT(X != 0 )
#else
#define CELL_GCMUTIL_ASSERT(X)
#define CELL_GCMUTIL_CHECK_ASSERT(X) (X)
#define CELL_GCMUTIL_CG_PARAMETER_CHECK_ASSERT(X) (void)(X)
#endif


// For exit routine
static void sysutil_exit_callback(uint64_t status, uint64_t param, void* userdata);

extern uint32_t _binary_errorshader_vpo_start;
extern uint32_t _binary_errorshader_vpo_end;
extern uint32_t _binary_errorshader_fpo_start;
extern uint32_t _binary_errorshader_fpo_end;

PS3_GcmSharedData g_gcmSharedData;

using namespace cell::Gcm;


ErrorRenderLoop::ErrorRenderLoop()
{
	m_keepRunning = true;
	m_nLocalMemHeap = 0;
	vertex_program_ptr = 
		(unsigned char *)&_binary_errorshader_vpo_start;
	fragment_program_ptr = 
		(unsigned char *)&_binary_errorshader_fpo_start;
	frame_index = 0;
}




/* local memory allocation */
void *ErrorRenderLoop::localMemoryAlloc(const uint32_t size) 
{
	uint32_t allocated_size = (size + 1023) & (~1023);
	uint32_t base = m_nLocalMemHeap;
	m_nLocalMemHeap += allocated_size;
	return (void*)base;
}

void *ErrorRenderLoop::localMemoryAlign(const uint32_t alignment, 
		const uint32_t size)
{
	m_nLocalMemHeap = (m_nLocalMemHeap + alignment-1) & (~(alignment-1));
	return (void*)localMemoryAlloc(size);
}




void ErrorRenderLoop::setRenderTarget(const uint32_t Index)
{
	CellGcmSurface sf;
	sf.colorFormat 	= CELL_GCM_SURFACE_A8R8G8B8;
	sf.colorTarget	= CELL_GCM_SURFACE_TARGET_0;
	sf.colorLocation[0]	= CELL_GCM_LOCATION_LOCAL;
	sf.colorOffset[0] 	= color_offset[Index];
	sf.colorPitch[0] 	= color_pitch;

	sf.colorLocation[1]	= CELL_GCM_LOCATION_LOCAL;
	sf.colorLocation[2]	= CELL_GCM_LOCATION_LOCAL;
	sf.colorLocation[3]	= CELL_GCM_LOCATION_LOCAL;
	sf.colorOffset[1] 	= 0;
	sf.colorOffset[2] 	= 0;
	sf.colorOffset[3] 	= 0;
	sf.colorPitch[1]	= 64;
	sf.colorPitch[2]	= 64;
	sf.colorPitch[3]	= 64;

	sf.depthFormat 	= CELL_GCM_SURFACE_Z24S8;
	sf.depthLocation	= CELL_GCM_LOCATION_LOCAL;
	sf.depthOffset	= depth_offset;
	sf.depthPitch 	= depth_pitch;

	sf.type		= CELL_GCM_SURFACE_PITCH;
	sf.antialias	= CELL_GCM_SURFACE_CENTER_1;

	sf.width 		= display_width;
	sf.height 		= display_height;
	sf.x 		= 0;
	sf.y 		= 0;
	cellGcmSetSurface(&sf);
	//cellGcmSetAntiAliasingControl(CELL_GCM_TRUE, CELL_GCM_FALSE, CELL_GCM_FALSE, 0xffff);
}

/* wait until flip */
static void waitFlip(void)
{
	while (cellGcmGetFlipStatus()!=0){
		sys_timer_usleep(300);
	}
	cellGcmResetFlipStatus();
}


void ErrorRenderLoop::flip(void)
{
	static int first=1;

	// wait until the previous flip executed
	if (first!=1) waitFlip();
	else cellGcmResetFlipStatus();

	if(cellGcmSetFlip(frame_index) != CELL_OK) return;
	cellGcmFlush();

	// resend status
	setDrawEnv();
	setRenderState();

	cellGcmSetWaitFlip();

	// New render target
	frame_index = (frame_index+1)%COLOR_BUFFER_NUM;
	setRenderTarget(frame_index);

	first=0;
}


void ErrorRenderLoop::initShader(void)
{
	vertex_program   = (CGprogram)vertex_program_ptr;
	fragment_program = (CGprogram)fragment_program_ptr;

	// init
	cellGcmCgInitProgram(vertex_program);
	cellGcmCgInitProgram(fragment_program);

	uint32_t ucode_size;
	void *ucode;
	cellGcmCgGetUCode(fragment_program, &ucode, &ucode_size);
	// 64B alignment required 
	void *ret = localMemoryAlign(64, ucode_size);
	fragment_program_ucode = ret;
	memcpy(fragment_program_ucode, ucode, ucode_size); 

	cellGcmCgGetUCode(vertex_program, &ucode, &ucode_size);
	vertex_program_ucode = ucode;
}

static void buildProjection(float *M, 
		const float top, 
		const float bottom, 
		const float left, 
		const float right, 
		const float near, 
		const float far)
{
	memset(M, 0, 16*sizeof(float)); 

	M[0*4+0] = (2.0f*near) / (right - left);
	M[1*4+1] = (2.0f*near) / (bottom - top);

	float A = (right + left) / (right - left);
	float B = (top + bottom) / (top - bottom);
	float C = -(far + near) / (far - near);
	float D = -(2.0f*far*near) / (far - near);

	M[0*4 + 2] = A;
	M[1*4 + 2] = B;
	M[2*4 + 2] = C;
	M[3*4 + 2] = -1.0f; 
	M[2*4 + 3] = D;
}

static void matrixMul(float *Dest, float *A, float *B)
{
	for (int i=0; i < 4; i++) {
		for (int j=0; j < 4; j++) {
			Dest[i*4+j] 
				= A[i*4+0]*B[0*4+j] 
				+ A[i*4+1]*B[1*4+j] 
				+ A[i*4+2]*B[2*4+j] 
				+ A[i*4+3]*B[3*4+j];
		}
	}
}

static void matrixTranslate(float *M, 
		const float x, 
		const float y, 
		const float z)
{
	memset(M, 0, sizeof(float)*16);
	M[0*4+3] = x;
	M[1*4+3] = y;
	M[2*4+3] = z;

	M[0*4+0] = 1.0f;
	M[1*4+1] = 1.0f;
	M[2*4+2] = 1.0f;
	M[3*4+3] = 1.0f;
}

/*
static void unitMatrix(float *M)
{
	M[0*4+0] = 1.0f;
	M[0*4+1] = 0.0f;
	M[0*4+2] = 0.0f;
	M[0*4+3] = 0.0f;

	M[1*4+0] = 0.0f;
	M[1*4+1] = 1.0f;
	M[1*4+2] = 0.0f;
	M[1*4+3] = 0.0f;

	M[2*4+0] = 0.0f;
	M[2*4+1] = 0.0f;
	M[2*4+2] = 1.0f;
	M[2*4+3] = 0.0f;

	M[3*4+0] = 0.0f;
	M[3*4+1] = 0.0f;
	M[3*4+2] = 0.0f;
	M[3*4+3] = 1.0f;
}

*/
#define CB_SIZE	(0x10000)

int32_t ErrorRenderLoop::initDisplay(void)
{
	uint32_t color_depth=4; // ARGB8
	uint32_t z_depth=4;     // COMPONENT24
	void *color_base_addr;
	void *depth_base_addr;
	void *color_addr[COLOR_BUFFER_NUM];
	void *depth_addr;
	CellVideoOutResolution resolution;

	// display initialize

	// read the current video status
	// INITIAL DISPLAY MODE HAS TO BE SET BY RUNNING SETMONITOR.SELF
	CellVideoOutState videoState;
	CELL_GCMUTIL_CHECK_ASSERT(cellVideoOutGetState(CELL_VIDEO_OUT_PRIMARY, 0, &videoState));
	CELL_GCMUTIL_CHECK_ASSERT(cellVideoOutGetResolution(videoState.displayMode.resolutionId, &resolution));

	display_width = resolution.width;
	display_height = resolution.height;
	color_pitch = display_width*color_depth;
	depth_pitch = display_width*z_depth;

	uint32_t color_size =   color_pitch*display_height;
	uint32_t depth_size =  depth_pitch*display_height;

	CellVideoOutConfiguration videocfg;
	memset(&videocfg, 0, sizeof(CellVideoOutConfiguration));
	videocfg.resolutionId = videoState.displayMode.resolutionId;
	videocfg.format = CELL_VIDEO_OUT_BUFFER_COLOR_FORMAT_X8R8G8B8;
	videocfg.pitch = color_pitch;

	// set video out configuration with waitForEvent set to 0 (4th parameter)
	CELL_GCMUTIL_CHECK_ASSERT(cellVideoOutConfigure(CELL_VIDEO_OUT_PRIMARY, &videocfg, NULL, 0));
	CELL_GCMUTIL_CHECK_ASSERT(cellVideoOutGetState(CELL_VIDEO_OUT_PRIMARY, 0, &videoState));

	switch (videoState.displayMode.aspect){
	  case CELL_VIDEO_OUT_ASPECT_4_3:
		  display_aspect_ratio=4.0f/3.0f;
		  break;
	  case CELL_VIDEO_OUT_ASPECT_16_9:
		  display_aspect_ratio=16.0f/9.0f;
		  break;
	  default:
		  printf("unknown aspect ratio %x\n", videoState.displayMode.aspect);
		  display_aspect_ratio=16.0f/9.0f;
	}

	cellGcmSetFlipMode(CELL_GCM_DISPLAY_VSYNC);

	// get config
	CellGcmConfig config;
	cellGcmGetConfiguration(&config);
	// buffer memory allocation
	m_nLocalMemHeap = (uint32_t)config.localAddress;

	color_base_addr = localMemoryAlign(64, COLOR_BUFFER_NUM*color_size);

	for (int i = 0; i < COLOR_BUFFER_NUM; i++) {
	    color_addr[i]= (void *)((uint32_t)color_base_addr+ (i*color_size));
	    CELL_GCMUTIL_CHECK_ASSERT(cellGcmAddressToOffset(color_addr[i], &color_offset[i]));
	}

	// regist surface
	for (int i = 0; i < COLOR_BUFFER_NUM; i++) {
		CELL_GCMUTIL_CHECK_ASSERT(cellGcmSetDisplayBuffer(i, color_offset[i], color_pitch, display_width, display_height));
	}

	depth_base_addr = localMemoryAlign(64, depth_size);
	depth_addr = depth_base_addr;
	CELL_GCMUTIL_CHECK_ASSERT(cellGcmAddressToOffset(depth_addr, &depth_offset));

	return 0;
}



//  65  verts in  polySurfaceShape1
ErrorRenderLoop::Vertex_t g_verts_polySurfaceShape1[65] = {
	{0.287,2.232,-0.144,  0.398,0.702,-0.590, 0xFFFF5FFF },
	{-0.287,2.232,-0.144,  -0.404,0.699,-0.590, 0xFFFF00FF },
	{-1.634,-0.134,-0.134,  -0.808,-0.006,-0.588, 0xFFFF00FF },
	{-1.366,-0.598,-0.134,  -0.413,-0.695,-0.589, 0xFFFF00FF },
	{1.345,-0.635,-0.155,  0.400,-0.700,-0.592, 0xFFFF00FF },
	{1.655,-0.097,-0.155,  0.806,0.001,-0.592, 0xFFFF00FF },
	{0.287,2.232,0.144,  0.455,0.835,0.308, 0xFFFF5FFF },
	{-0.287,2.232,0.144,  -0.462,0.832,0.309, 0xFFFF5FFF },
	{-1.634,-0.134,0.134,  -0.937,-0.034,0.347, 0xFFFF00FF },
	{-1.366,-0.598,0.134,  -0.484,-0.795,0.367, 0xFFFF5FFF },
	{1.345,-0.635,0.155,  0.470,-0.802,0.369, 0xFFFF5FFF },
	{1.655,-0.097,0.155,  0.937,-0.027,0.348, 0xFFFF00FF },
	{0.000,0.500,-0.257,  -0.007,0.000,-1.000, 0xFFFF00FF },
	{0.000,0.274,0.422,  0.000,0.962,0.272, 0xFF0076FF },
	{0.000,-0.085,0.422,  0.000,-0.962,0.272, 0xFF0076FF },
	{-0.173,0.094,0.422,  -0.964,0.000,0.266, 0xFF0076FF },
	{0.173,0.094,0.422,  0.964,0.000,0.266, 0xFF0076FF },
	{-0.058,0.445,0.422,  -0.592,-0.698,0.404, 0xFF0076FF },
	{0.058,0.445,0.422,  0.591,-0.698,0.404, 0xFF0076FF },
	{0.002,2.002,0.422,  0.019,0.915,0.403, 0xFF0000FF },
	{0.173,1.774,0.422,  0.871,0.278,0.405, 0xFF0000FF },
	{-0.172,1.789,0.422,  -0.864,0.296,0.407, 0xFF0000FF },
	{0.088,0.787,0.422,  0.963,-0.084,0.256, 0xFF0076FF },
	{-0.088,0.787,0.422,  -0.963,-0.082,0.256, 0xFF0076FF },
	{0.124,1.192,0.422,  0.949,-0.082,0.304, 0xFF0000FF },
	{-0.123,1.192,0.422,  -0.950,-0.081,0.302, 0xFF0000FF },
	{0.161,1.597,0.422,  0.935,-0.070,0.347, 0xFF0000FF },
	{-0.159,1.597,0.422,  -0.936,-0.071,0.345, 0xFF0000FF },
	{0.000,0.274,0.349,  0.000,0.818,-0.575, 0xFFFFFFFF },
	{-0.173,0.094,0.349,  -0.815,0.000,-0.579, 0xFFFFFFFF },
	{0.000,-0.085,0.349,  0.000,-0.818,-0.575, 0xFF0076FF },
	{0.173,0.094,0.349,  0.815,0.000,-0.579, 0xFF0076FF },
	{0.173,1.774,0.349,  0.739,0.219,-0.637, 0xFFFFFFFF },
	{0.002,2.002,0.349,  0.018,0.825,-0.566, 0xFFFFFFFF },
	{-0.172,1.789,0.349,  -0.738,0.235,-0.633, 0xFFFFFFFF },
	{0.058,0.445,0.349,  0.549,-0.600,-0.582, 0xFFFFFFFF },
	{0.088,0.787,0.349,  0.704,-0.063,-0.707, 0xFFFFFFFF },
	{-0.088,0.787,0.349,  -0.704,-0.062,-0.707, 0xFFFFFFFF },
	{0.124,1.192,0.349,  0.704,-0.063,-0.707, 0xFFFFFFFF },
	{-0.123,1.192,0.349,  -0.704,-0.062,-0.707, 0xFFFFFFFF },
	{0.161,1.597,0.349,  0.708,-0.055,-0.704, 0xFFFFFFFF },
	{-0.159,1.597,0.349,  -0.707,-0.056,-0.705, 0xFFFFFFFF },
	{-0.058,0.445,0.349,  -0.549,-0.600,-0.582, 0xFFFFFFFF },
	{0.280,2.195,0.188,  0.318,0.461,0.828, 0xFFFF5FFF },
	{-0.279,2.195,0.188,  -0.323,0.457,0.829, 0xFFFF5FFF },
	{0.000,0.508,0.298,  -0.007,0.000,1.000, 0xFFFF00FF },
	{-1.591,-0.110,0.178,  -0.535,0.082,0.841, 0xFFFF00FF },
	{-1.331,-0.561,0.178,  -0.213,-0.432,0.877, 0xFFFF5FFF },
	{1.310,-0.597,0.199,  0.199,-0.432,0.880, 0xFFFF5FFF },
	{1.612,-0.074,0.199,  0.529,0.089,0.844, 0xFFFF00FF },
	{-0.150,1.762,0.445,  -0.414,0.061,0.908, 0xFF0000FF },
	{0.150,1.747,0.445,  0.414,0.057,0.909, 0xFF0000FF },
	{0.002,1.965,0.445,  0.016,0.404,0.915, 0xFF0000FF },
	{-0.076,0.808,0.445,  -0.498,-0.043,0.866, 0xFF0076FF },
	{0.050,0.482,0.445,  0.464,-0.229,0.856, 0xFF0076FF },
	{0.077,0.808,0.445,  0.497,-0.043,0.866, 0xFF0076FF },
	{-0.107,1.194,0.445,  -0.459,-0.037,0.888, 0xFF0000FF },
	{0.108,1.194,0.445,  0.457,-0.037,0.888, 0xFF0000FF },
	{-0.138,1.580,0.445,  -0.423,-0.034,0.905, 0xFF0000FF },
	{0.140,1.580,0.445,  0.422,-0.034,0.906, 0xFF0000FF },
	{-0.050,0.482,0.445,  -0.464,-0.229,0.856, 0xFF0076FF },
	{0.000,-0.067,0.447,  0.000,-0.599,0.800, 0xFF0076FF },
	{0.000,0.255,0.447,  0.000,0.599,0.800, 0xFF0076FF },
	{-0.155,0.094,0.447,  -0.603,0.000,0.798, 0xFF0076FF },
	{0.155,0.094,0.447,  0.603,0.000,0.798, 0xFF0076FF },
};
//  118  triangles
uint16_t g_tris_polySurfaceShape1[118][3] = {
	{  0, 1, 6  }
	,  {  6, 1, 7  }
	,  {  1, 2, 7  }
	,  {  7, 2, 8  }
	,  {  2, 3, 8  }
	,  {  8, 3, 9  }
	,  {  3, 4, 9  }
	,  {  9, 4, 10  }
	,  {  4, 5, 10  }
	,  {  10, 5, 11  }
	,  {  5, 0, 11  }
	,  {  11, 0, 6  }
	,  {  1, 0, 12  }
	,  {  2, 1, 12  }
	,  {  3, 2, 12  }
	,  {  4, 3, 12  }
	,  {  5, 4, 12  }
	,  {  0, 5, 12  }
	,  {  43, 44, 45  }
	,  {  44, 46, 45  }
	,  {  46, 47, 45  }
	,  {  47, 48, 45  }
	,  {  48, 49, 45  }
	,  {  49, 43, 45  }
	,  {  28, 30, 29  }
	,  {  30, 28, 31  }
	,  {  32, 34, 33  }
	,  {  35, 37, 36  }
	,  {  36, 39, 38  }
	,  {  38, 41, 40  }
	,  {  34, 40, 41  }
	,  {  40, 34, 32  }
	,  {  37, 35, 42  }
	,  {  39, 36, 37  }
	,  {  41, 38, 39  }
	,  {  62, 63, 61  }
	,  {  61, 64, 62  }
	,  {  51, 52, 50  }
	,  {  54, 55, 53  }
	,  {  55, 57, 56  }
	,  {  57, 59, 58  }
	,  {  50, 58, 59  }
	,  {  59, 51, 50  }
	,  {  53, 60, 54  }
	,  {  56, 53, 55  }
	,  {  58, 56, 57  }
	,  {  13, 28, 15  }
	,  {  28, 29, 15  }
	,  {  15, 29, 14  }
	,  {  29, 30, 14  }
	,  {  14, 30, 16  }
	,  {  30, 31, 16  }
	,  {  16, 31, 13  }
	,  {  31, 28, 13  }
	,  {  20, 32, 19  }
	,  {  32, 33, 19  }
	,  {  19, 33, 21  }
	,  {  33, 34, 21  }
	,  {  18, 35, 22  }
	,  {  35, 36, 22  }
	,  {  22, 36, 24  }
	,  {  36, 38, 24  }
	,  {  24, 38, 26  }
	,  {  38, 40, 26  }
	,  {  21, 34, 27  }
	,  {  34, 41, 27  }
	,  {  26, 40, 20  }
	,  {  40, 32, 20  }
	,  {  23, 37, 17  }
	,  {  37, 42, 17  }
	,  {  17, 42, 18  }
	,  {  42, 35, 18  }
	,  {  25, 39, 23  }
	,  {  39, 37, 23  }
	,  {  27, 41, 25  }
	,  {  41, 39, 25  }
	,  {  7, 44, 6  }
	,  {  6, 44, 43  }
	,  {  8, 46, 7  }
	,  {  7, 46, 44  }
	,  {  9, 47, 8  }
	,  {  8, 47, 46  }
	,  {  9, 10, 47  }
	,  {  47, 10, 48  }
	,  {  10, 11, 48  }
	,  {  48, 11, 49  }
	,  {  11, 6, 49  }
	,  {  49, 6, 43  }
	,  {  19, 21, 52  }
	,  {  21, 50, 52  }
	,  {  19, 52, 20  }
	,  {  52, 51, 20  }
	,  {  18, 22, 54  }
	,  {  22, 55, 54  }
	,  {  22, 24, 55  }
	,  {  24, 57, 55  }
	,  {  26, 59, 24  }
	,  {  59, 57, 24  }
	,  {  21, 27, 50  }
	,  {  27, 58, 50  }
	,  {  20, 51, 26  }
	,  {  51, 59, 26  }
	,  {  18, 54, 17  }
	,  {  54, 60, 17  }
	,  {  17, 60, 23  }
	,  {  60, 53, 23  }
	,  {  23, 53, 25  }
	,  {  53, 56, 25  }
	,  {  27, 25, 58  }
	,  {  25, 56, 58  }
	,  {  14, 61, 15  }
	,  {  61, 63, 15  }
	,  {  13, 15, 62  }
	,  {  15, 63, 62  }
	,  {  13, 62, 16  }
	,  {  62, 64, 16  }
	,  {  14, 16, 61  }
	,  {  16, 64, 61  }
};



void ErrorRenderLoop::setDrawEnv(void)
{
	cellGcmSetColorMask(CELL_GCM_COLOR_MASK_B|
			CELL_GCM_COLOR_MASK_G|
			CELL_GCM_COLOR_MASK_R|
			CELL_GCM_COLOR_MASK_A);

	cellGcmSetColorMaskMrt(0);
	uint16_t x,y,w,h;
	float min, max;
	float scale[4],offset[4];

	x = 0;
	y = 0;
	w = display_width;
	h = display_height;
	min = 0.0f;
	max = 1.0f;
	scale[0] = w * 0.5f;
	scale[1] = h * -0.5f;
	scale[2] = (max - min) * 0.5f;
	scale[3] = 0.0f;
	offset[0] = x + scale[0];
	offset[1] = y + h * 0.5f;
	offset[2] = (max + min) * 0.5f;
	offset[3] = 0.0f;

	cellGcmSetViewport(x, y, w, h, min, max, scale, offset);
	cellGcmSetClearColor((64<<0)|(64<<8)|(64<<16)|(64<<24));

	cellGcmSetDepthTestEnable(CELL_GCM_TRUE);
	cellGcmSetDepthFunc(CELL_GCM_LESS);

}

void ErrorRenderLoop::setRenderState(void)
{
	cellGcmSetVertexProgram(vertex_program, vertex_program_ucode);
	cellGcmSetVertexDataArray(position_index,
			0, 
			sizeof(Vertex_t), 
			3, 
			CELL_GCM_VERTEX_F, 
			CELL_GCM_LOCATION_LOCAL, 
			m_nVertPositionOffset);
	cellGcmSetVertexDataArray(normal_index,
		0, 
		sizeof(Vertex_t), 
		3, 
		CELL_GCM_VERTEX_F, 
		CELL_GCM_LOCATION_LOCAL, 
		m_nVertNormalOffset);
	cellGcmSetVertexDataArray(color_index,
			0, 
			sizeof(Vertex_t), 
			4, 
			CELL_GCM_VERTEX_UB, 
			CELL_GCM_LOCATION_LOCAL, 
			m_nVertColorOffset );



	cellGcmSetFragmentProgram(fragment_program, fragment_offset);

}


void AngleMatrix( float yaw, float pitch, float roll, float matrix[16] )
{
	float sy = sinf(yaw),cy = cosf( yaw ),sp = sinf( pitch ),cp = cosf( pitch ),sr = sinf( roll ),cr = cosf( roll );

	// matrix = (YAW * PITCH) * ROLL
	matrix[0*4+0] = cp*cy;
	matrix[1*4+0] = cp*sy;
	matrix[2*4+0] = -sp;

	// NOTE: Do not optimize this to reduce multiplies! optimizer bug will screw this up.
	matrix[0*4+1] = sr*sp*cy+cr*-sy;
	matrix[1*4+1] = sr*sp*sy+cr*cy;
	matrix[2*4+1] = sr*cp;
	matrix[0*4+2] = (cr*sp*cy+-sr*-sy);
	matrix[1*4+2] = (cr*sp*sy+-sr*cy);
	matrix[2*4+2] = cr*cp;

	matrix[0*4+3] = 0.0f;
	matrix[1*4+3] = 0.0f;
	matrix[2*4+3] = 0.0f;
	
	matrix[3*4+0] = 0;
	matrix[3*4+1] = 0;
	matrix[3*4+2] = 0;
	matrix[3*4+3] = 1;
}



// call this AFTER setRenderState and setRenderObject
void ErrorRenderLoop::setTransforms(float *M)
{
	// transform
	float P[16];
	float V[16];
	float VP[16];

	// projection 
	buildProjection(P, -1.0f, 1.0f, -1.0f, 1.0f, 1.0, 10000.0f); 

	// 16:9 scale or 4:3 scale
	matrixTranslate(V, 0.0f, 0.0f, -4.0);
	V[0*4 + 0] = 1.0f / display_aspect_ratio;
	V[1*4 + 1] = 1.0f; 

	// model view 
	matrixMul(VP, P, V);

	matrixMul(MVP, VP, M);

	cellGcmSetVertexProgramParameter(model_view_projection, MVP);
}

int32_t ErrorRenderLoop::setRenderObject(void)
{
	void *ret = localMemoryAlign(128, sizeof(Vertex_t) * ARRAYSIZE( g_verts_polySurfaceShape1 ) );
	m_pVertexBuffer = (Vertex_t*)ret;
	memcpy( m_pVertexBuffer, g_verts_polySurfaceShape1, sizeof( g_verts_polySurfaceShape1 ) );
	
	m_pIndexBuffer = (uint16_t *)localMemoryAlign( 128, sizeof( uint16_t ) * ARRAYSIZE( g_tris_polySurfaceShape1 ) * 3 );
	memcpy( m_pIndexBuffer, g_tris_polySurfaceShape1, sizeof( g_tris_polySurfaceShape1 ) );

	model_view_projection = cellGcmCgGetNamedParameter(vertex_program, "modelViewProj");
	CELL_GCMUTIL_CG_PARAMETER_CHECK_ASSERT(model_view_projection);
	CGparameter position = cellGcmCgGetNamedParameter(vertex_program, "position");
	CELL_GCMUTIL_CG_PARAMETER_CHECK_ASSERT(position);
	CGparameter normal = cellGcmCgGetNamedParameter(vertex_program, "normal");
	CELL_GCMUTIL_CG_PARAMETER_CHECK_ASSERT(normal);
	CGparameter color = cellGcmCgGetNamedParameter(vertex_program, "color");
	CELL_GCMUTIL_CG_PARAMETER_CHECK_ASSERT(color);

	// get Vertex Attribute index
	position_index = cellGcmCgGetParameterResource(vertex_program, position) - CG_ATTR0;
	normal_index = cellGcmCgGetParameterResource(vertex_program, normal) - CG_ATTR0;
	color_index = cellGcmCgGetParameterResource(vertex_program, color) - CG_ATTR0;

	// fragment program offset
	CELL_GCMUTIL_CHECK_ASSERT(cellGcmAddressToOffset(fragment_program_ucode, &fragment_offset));
	CELL_GCMUTIL_CHECK_ASSERT(cellGcmAddressToOffset(&m_pVertexBuffer->x, &m_nVertPositionOffset));
	CELL_GCMUTIL_CHECK_ASSERT(cellGcmAddressToOffset(&m_pVertexBuffer->nx, &m_nVertNormalOffset));
	CELL_GCMUTIL_CHECK_ASSERT(cellGcmAddressToOffset(&m_pVertexBuffer->rgba, &m_nVertColorOffset));
	CELL_GCMUTIL_CHECK_ASSERT(cellGcmAddressToOffset(m_pIndexBuffer, &m_nIndexBufferLocalMemoryOffset));
	
	return 0;
}


const uint32_t g_nInitCmdBuffer = 0x1000;

int32_t SimpleContextCallback( struct CellGcmContextData *pData, uint32_t nSize )
{
	*pData->current = CELL_GCM_JUMP( g_nInitCmdBuffer );
	__sync();
	CellGcmControl * pControlRegister = cellGcmGetControlRegister();
	pControlRegister->put = g_nInitCmdBuffer;
	do 
	{
		sys_timer_usleep( 60 );
	} while ( pControlRegister->get != g_nInitCmdBuffer );
	pData->current = pData->begin;
	return CELL_OK;
}


int ErrorRenderLoop::Run( void )
{
	// Exit routines
	{
		// register sysutil exit callback
		CELL_GCMUTIL_CHECK_ASSERT( cellSysutilRegisterCallback( 0, sysutil_exit_callback, this ) );
		
		CELL_GCMUTIL_CHECK_ASSERT( cellSysutilCheckCallback() );
		if ( !m_keepRunning )
			return 0;
	}
	
	if( g_gcmSharedData.m_pIoMemory )
	{
		// GCM has already been initialized, no need to reinitialize it.
		// But we have to  drive RSX to our new command buffer
		const uint32_t nCmdBufferOverfetchSlack = 1024;
		//printf( "Entering Error Render Loop, reusing IO memory @%p size 0x%X; initial ctx={%p,%p,%p;fn=%p}\n", g_gcmSharedData.m_pIoMemory, g_gcmSharedData.m_nIoMemorySize, gCellGcmCurrentContext->begin, gCellGcmCurrentContext->current, gCellGcmCurrentContext->end, gCellGcmCurrentContext->callback );
		cellGcmSetCurrentBuffer( 
			// start command buffer right after initialization fixed command buffer (always the first 4 k of IO memory)
			( uint32_t* )( uint32_t( g_gcmSharedData.m_pIoMemory ) + g_nInitCmdBuffer ), 
			// the size is all of IO memory, minus initialization command buffer, minus overfetch amount in the end of command buffer to avoid a crash,
			// minus 4 bytes to insert a JUMP to the start of command buffer when we run out of space
			g_gcmSharedData.m_nIoMemorySize - nCmdBufferOverfetchSlack - g_nInitCmdBuffer - sizeof( uint32_t ) );
		CellGcmContextData *pCurrentContext = gCellGcmCurrentContext;
		pCurrentContext->callback = SimpleContextCallback;
		
		CellGcmControl * pControlRegister = cellGcmGetControlRegister();
		uint32_t nPut = pControlRegister->put;
		uint32_t * pCurrentCmd = ( uint32_t* )( uint32_t( g_gcmSharedData.m_pIoMemory ) + nPut );
		pCurrentContext->current = pCurrentCmd; // de facto current
		
// 		#ifndef _CERT
// 		if ( pControlRegister->put != g_nInitCmdBuffer || gCellGcmCurrentContext->current != ( uint32_t* )( uint32_t( g_gcmSharedData.m_pIoMemory ) + g_nInitCmdBuffer ) )
// 		{
// 			__builtin_snpause();
// 		}
// 		#endif
	}
	else
	{
		sys_addr_t allocatedAddress = NULL;
		int smaResult = sys_memory_allocate( 1 * 1024 * 1024, SYS_MEMORY_PAGE_SIZE_1M, &allocatedAddress );
		if( smaResult != CELL_OK )
			return smaResult;
		void* host_addr = (void*)allocatedAddress;
		printf( "Entering Error Render loop, IO memory @%p...\n", host_addr );
		CELL_GCMUTIL_ASSERT(host_addr != NULL);
		CELL_GCMUTIL_CHECK_ASSERT(cellGcmInit(CB_SIZE, HOST_SIZE, host_addr));
	}

	if (initDisplay()!=0)	return -1;

	initShader();

	setDrawEnv();

	if (setRenderObject())
		return -1;

	setRenderState();

	// 1st time
	setRenderTarget(frame_index);

	// rendering loop
	CELL_GCMUTIL_ASSERT(m_keepRunning);
	float flTime = 0;
	uint32_t nFrames = 0;
	while (m_keepRunning) {
		// check system event
		CELL_GCMUTIL_CHECK_ASSERT( cellSysutilCheckCallback() );

		// clear frame buffer
		cellGcmSetClearSurface(
			CELL_GCM_CLEAR_Z |
			CELL_GCM_CLEAR_R |
			CELL_GCM_CLEAR_G |
			CELL_GCM_CLEAR_B |
			CELL_GCM_CLEAR_A
		);
		float tm[16];
		AngleMatrix(0.1f * sinf( flTime * 0.5f ), 0.5f * sinf( flTime * 0.45f ), 0.1f * sinf( flTime*0.02f ), tm);
		setTransforms(tm);
		// set draw command
		if( flTime > 4.0f )
		{
			cellGcmSetDrawIndexArray(CELL_GCM_PRIMITIVE_TRIANGLES, 3 * ARRAYSIZE( g_tris_polySurfaceShape1 ), CELL_GCM_DRAW_INDEX_ARRAY_TYPE_16, CELL_GCM_LOCATION_LOCAL, m_nIndexBufferLocalMemoryOffset );
		}

		// start reading the command buffer
		flip();
		flTime += 1/60.0f;
		nFrames ++;
	}
	printf( "Exiting Error Render loop, %u frames rendered", nFrames );
	cellSysutilUnregisterCallback( 0 );
	
	// Let RSX wait for final flip
	cellGcmSetWaitFlip();

	// Let PPU wait for all commands done (include waitFlip)
	cellGcmFinish(1);
	
	// let's just leak this memory to avoid problems due to initializing GCM in different places, it doesn't matter
	//sys_memory_free( g_gcmSharedData.m_pIoMemory );
	
	printf(".\n");

	return 0;
}

void sysutil_exit_callback(uint64_t status, uint64_t param, void* userdata)
{
	(void) param;
	(void) userdata;

	switch(status) {
	case CELL_SYSUTIL_REQUEST_EXITGAME:
		((ErrorRenderLoop*)userdata)->m_keepRunning = false;
		break;
	case CELL_SYSUTIL_DRAWING_BEGIN:
	case CELL_SYSUTIL_DRAWING_END:
	case CELL_SYSUTIL_SYSTEM_MENU_OPEN:
	case CELL_SYSUTIL_SYSTEM_MENU_CLOSE:
	case CELL_SYSUTIL_BGMPLAYBACK_PLAY:
	case CELL_SYSUTIL_BGMPLAYBACK_STOP:
	default:
		break;
	}
}


