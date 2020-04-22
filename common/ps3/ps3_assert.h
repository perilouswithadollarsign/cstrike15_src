// PS3 Assert : As of SDK 084, the PS3 assert found in assert.h will simply break execution
//              by calling __abort: this will exit the application instead of breaking
//              execution in the debugger.

#if defined(_PS3) && defined(_DEBUG)
#ifdef assert
#undef assert
#endif
#define assert(v)   
//if(!(v)) { *(int*)0x1 = 0; }
#endif
