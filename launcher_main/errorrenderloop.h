//========= Copyright 2010, Valve Corporation, All rights reserved. ============//
#ifndef ERRORRENDERLOOP_HDR
#define ERRORRENDERLOOP_HDR


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timer.h>
#include <sys/return_code.h>
#include <cell/gcm.h>
#include <stddef.h>
#include <math.h>
#include <sys/memory.h>
#include <sysutil/sysutil_sysparam.h>
#include "ps3/ps3_helpers.h"

class ErrorRenderLoop
{
public:
	typedef struct
	{
		float x, y, z;
		float nx, ny, nz;
		uint32_t rgba; 
	} Vertex_t;
	
	ErrorRenderLoop();
	int Run( void );
	
protected:
	void *localMemoryAlloc(const uint32_t size) ;
	void *localMemoryAlign(const uint32_t alignment, 
		const uint32_t size);
	void setRenderState(void);
	void setDrawEnv(void);
	int32_t setRenderObject(void);
	int32_t initDisplay(void);
	void initShader(void);
	void setRenderTarget(const uint32_t Index);
	void flip(void);
	void setVertex(Vertex_t* vertex_buffer);

	void setTransforms(float *tm);
public:

	/* double buffering */
	enum ConstEnum_t{ COLOR_BUFFER_NUM = 2, HOST_SIZE = (1*1024*1024) };
	
	bool m_keepRunning;
	uint32_t m_nLocalMemHeap;

	uint32_t display_width;
	uint32_t display_height; 

	float    display_aspect_ratio;
	uint32_t color_pitch;
	uint32_t depth_pitch;
	uint32_t color_offset[COLOR_BUFFER_NUM];
	uint32_t depth_offset;

	uint32_t frame_index;

	unsigned char *vertex_program_ptr;
	unsigned char *fragment_program_ptr;

	CGprogram vertex_program;
	CGprogram fragment_program;
	CGparameter model_view_projection;

	void *vertex_program_ucode;
	void *fragment_program_ucode;
	uint32_t fragment_offset;
	uint32_t m_nVertPositionOffset, m_nVertNormalOffset, m_nVertColorOffset, m_nIndexBufferLocalMemoryOffset;
	uint32_t color_index ;
	uint32_t position_index ;
	uint32_t normal_index;
	float MVP[16];
	Vertex_t *m_pVertexBuffer;
	uint16_t *m_pIndexBuffer;
};

extern PS3_GcmSharedData g_gcmSharedData;


#endif