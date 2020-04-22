//======= Copyright © 1996-2007, Valve Corporation, All rights reserved. ======
//
// Purpose: Utils for working with HyperShade in Maya
//
//=============================================================================


// Maya includes
#include <maya/MObject.h>


// Valve includes
#include "valveMaya/Undo.h"


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
namespace ValveMaya
{

	class CHyperShadeUtil
	{
	public:
		CHyperShadeUtil();

		CHyperShadeUtil( CUndo &undo );

		MStatus AddUtility( const MObject &utilityNode );

		MStatus AddShader( const MObject &shaderNode );

		MStatus AddTexture( const MObject &textureNode );

	protected:
		CUndo m_tmpUndo;
		CUndo &m_undo;

		MObject m_renderUtilityListObj;
		MObject m_shaderListObj;
		MObject m_textureListObj;

		void Init();
	};

}