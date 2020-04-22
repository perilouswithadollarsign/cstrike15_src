//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#pragma once

void GenerateSolutionForPerforceChangelist( CProjectDependencyGraph &dependencyGraph, CUtlVector<int> &changelists, IBaseSolutionGenerator *pGenerator, const char *pSolutionFilename );
