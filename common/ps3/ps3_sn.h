//========= Copyright © , Valve LLC, All rights reserved. ============
//
// These are the workarounds for snTuner inability to patch prx'es, 
// as well as access prx symbols after attaching to a running game 
// snTuner functions are exposed through pointers to functions in ELF
// 
//////////////////////////////////////////////////////////////////////
#ifndef VALVE_PS3_SN_HDR
#define VALVE_PS3_SN_HDR

// Currently these are only defined on PS3, but there's no harm in declaring them on other platforms


#ifdef _PS3
extern "C" void(*g_pfnPushMarker)( const char * pName );
extern "C" void(*g_pfnPopMarker)();
extern "C" void(*g_pfnSwapBufferMarker)();
#else
inline void g_pfnPushMarker( const char * pName ){}
inline void g_pfnPopMarker(){}
inline void g_pfnSwapBufferMarker(){}
#endif

#endif