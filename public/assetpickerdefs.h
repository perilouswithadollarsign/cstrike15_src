//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
// Structures used when invoking the asset picker.
//
//==================================================================================================

#ifndef ASSETPICKERDEFS_H
#define ASSETPICKERDEFS_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Used when providing the asset picker with a list of used assets.
//-----------------------------------------------------------------------------
struct AssetUsageInfo_t
{
	CUtlString m_assetName;
	int m_nTimesUsed;
};


#endif // ASSETPICKERDEFS_H
