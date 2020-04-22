// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#if defined( COMPILER_MSVC )
#pragma once
#endif

#if defined ( DX_TO_GL_ABSTRACTION )
#include "togl/rendermechanism.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#if ( defined( WIN32 ) && !defined( DX_TO_GL_ABSTRACTION ) ) || defined( _X360 )
#include <d3d9.h>
#endif

#include "GFx.h"

// Disable "_force_inline not inlined" warning
#pragma warning(disable : 4714)

#include "Kernel/SF_Types.h"
#include "Kernel/SF_RefCount.h"

#if defined( WIN32 ) && !defined( DX_TO_GL_ABSTRACTION )
#include "GFx_Renderer_D3D9.h"
#else
#include "GFx_Renderer_GL.h"
#endif

#include "tier0/platform.h"
#include "tier0/dbg.h"    
#include "vprof.h"  

#pragma warning(disable : 4267)

#include "tier1/strtools.h"
#include "tier1/utlvector.h"
#include "tier3/tier3dm.h"
#include "vstdlib/random.h"
#include "interfaces/interfaces.h"

#include "shaderapi/IShaderDevice.h"
#include "inputsystem/ButtonCode.h"
#include "inputsystem/AnalogCode.h"
#include "inputsystem/iinputsystem.h"
#include "filesystem.h"
#include "IGameUIFuncs.h"

namespace SF = Scaleform;

#pragma warning(default : 4267)

#include "scaleformui/scaleformui.h"
#include "scaleformuiimpl/scaleformuiimpl.h"

