#ifndef GLMDEBUG_H
#define	GLMDEBUG_H

// include this anywhere you need to be able to compile-out code related specifically to GLM debugging.

// we expect DEBUG to be driven by the build system so you can include this header anywhere.
// when we come out, GLMDEBUG will be defined to a value - 0, 1, or 2
// 0 means no GLM debugging is possible
// 1 means it's possible and resulted from being a debug build
// 2 means it's possible and resulted from being manually forced on for a release build

#if defined( POSIX ) && !defined( _PS3 ) 
	#ifndef GLMDEBUG
		#ifdef DEBUG
			#define GLMDEBUG 1	// normally 1 here, testing
		#else
			// #define GLMDEBUG 2			// don't check this in enabled..
		#endif
		
		#ifndef GLMDEBUG
			#define GLMDEBUG 0
		#endif
	#endif
#else
	#ifndef GLMDEBUG
		#define GLMDEBUG 0
	#endif
#endif

// helpful macro if you are in a position to call GLM functions directly (i.e. you live in materialsystem / shaderapidx9)
#if GLMDEBUG
	#define	GLMPRINTF(args)			GLMPrintf args
	#define	GLMPRINTSTR(args)		GLMPrintStr args
	#define	GLMPRINTTEXT(args)		GLMPrintText args
	#define	GLMBEGINPIXEVENT(args)	GLMBeginPIXEvent args
	#define	GLMENDPIXEVENT(args)	GLMEndPIXEvent args
#else
	#define	GLMPRINTF(args)	
	#define	GLMPRINTSTR(args)
	#define	GLMPRINTTEXT(args)
	#define	GLMBEGINPIXEVENT(args)
	#define	GLMENDPIXEVENT(args)
#endif

#endif
