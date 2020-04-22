#ifndef RENDERMECHANISM_H
#define RENDERMECHANISM_H

#if defined(DX_TO_GL_ABSTRACTION)

#undef PROTECTED_THINGS_ENABLE

#include <GL/gl.h>
#include <GL/glext.h>

#include "tier0/basetypes.h"
#include "tier0/platform.h"

#include "togl/glmdebug.h"
#include "togl/glbase.h"
#include "togl/glentrypoints.h"
#include "togl/glmdisplay.h"
#include "togl/glmdisplaydb.h"
#include "togl/glmgrbasics.h"
#include "togl/glmgrext.h"
#include "togl/cglmbuffer.h"
#include "togl/cglmtex.h"
#include "togl/cglmfbo.h"
#include "togl/cglmprogram.h"
#include "togl/cglmquery.h"
#include "togl/glmgr.h"
#include "togl/dxabstract_types.h"
#include "togl/dxabstract.h"

#else
	//USE_ACTUAL_DX
	#ifdef WIN32
		#ifdef _X360
			#include "d3d9.h"
			#include "d3dx9.h"
		#else
			#include <windows.h>
			#include "../../dx9sdk/include/d3d9.h"
			#include "../../dx9sdk/include/d3dx9.h"
		#endif
		typedef HWND VD3DHWND;
	#endif

	#define	GLMPRINTF(args)	
	#define	GLMPRINTSTR(args)
	#define	GLMPRINTTEXT(args)
#endif // defined(DX_TO_GL_ABSTRACTION)

#endif // RENDERMECHANISM_H
