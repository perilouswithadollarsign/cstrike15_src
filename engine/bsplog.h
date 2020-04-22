//===== Copyright © Valve Corporation, All rights reserved. ========//
#ifndef BSP_LOG_HDR
#define BSP_LOG_HDR

#include "filesystem.h"

struct cbrushside_t;
class CBspDebugLog
{
	FileHandle_t m_File;
	int m_nBaseVertex;
	bool m_bFlush;
	int m_nBrushCount;
	int m_nBoxCount;
public:
	CBspDebugLog( const char *pName );
	~CBspDebugLog();
	void AddBox( const char *pName, const char *pMtl, const Vector &mins, const Vector &maxs );
	void AddBrush( const char *pName, const char *pMtl, cbrushside_t * RESTRICT pSides, int nSides );

	int GetPrimCount()const { return m_nBrushCount + m_nBoxCount; }
	void ResetPrimCount() { m_nBrushCount = m_nBoxCount = 0; }
};


#endif 
