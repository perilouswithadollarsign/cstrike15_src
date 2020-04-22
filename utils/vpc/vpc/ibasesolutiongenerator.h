//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#pragma once


#include "dependencies.h"


class IBaseProjectGenerator;

//I know this defeats the idea of abstraction. But VisualStudio has required aliases for platforms and something has to give to make a mapping. And solution generators have less surface area
enum SolutionType_t
{
	ST_VISUALSTUDIO,
	ST_MAKEFILE,
	ST_XCODE,
};

class IBaseSolutionGenerator
{
public:
	virtual void GenerateSolutionFile( const char *pSolutionFilename, CUtlVector<CDependency_Project*> &projects ) = 0;
    virtual const char *GetSolutionFileExtension() { return NULL; }
    virtual void ProjectEnd( IBaseProjectGenerator *pCurGen ) {}
	virtual SolutionType_t GetSolutionType( void ) = 0;
};
