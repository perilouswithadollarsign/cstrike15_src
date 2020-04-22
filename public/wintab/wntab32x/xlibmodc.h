#define MODULE_TEMPLATEC2(mod)											\
																		\
																		\
	extern void __ ## mod ## unlink(void);								\
	void __cdecl _Unlink ## mod(void) {__ ## mod ## unlink();}

#define MODULE_TEMPLATEC(mod) MODULE_TEMPLATEC2(mod)

