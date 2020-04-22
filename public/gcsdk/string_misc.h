//====== Copyright ©, Valve Corporation, All rights reserved. =======
//
// Purpose: Misc string helpers
//
//=============================================================================

#ifndef GC_STRING_MISC_H
#define GC_STRING_MISC_H

namespace GCSDK
{

char *GetPchTempTextBuffer();								// Returns a short-lived text buffer
size_t GetCchTempTextBuffer();									// How big is my temp text buffer?

#ifdef DBGFLAG_VALIDATE
void ValidateTempTextBuffers( CValidator & validator );
#endif

void UninitTempTextBuffers( );


} // namespace GCSDK

#endif // GC_STRING_MISC_H

