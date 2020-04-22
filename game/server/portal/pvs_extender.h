//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef PVS_EXTENDER_H
#define PVS_EXTENDER_H
#ifdef _WIN32
#pragma once
#endif

class CPVS_Extender
{
public:
	CPVS_Extender( void ); //self-registration
	virtual ~CPVS_Extender( void ); //self-unregistration

	struct VisExtensionChain_t
	{
		VisExtensionChain_t *pParentChain;
		int m_nArea;
		CPVS_Extender *pExtender;
	};

	virtual CServerNetworkProperty *GetExtenderNetworkProp( void )= 0;
	virtual const edict_t	*GetExtenderEdict( void ) const = 0;
	virtual bool			IsExtenderValid( void ) = 0;
	virtual Vector			GetExtensionPVSOrigin( void ) = 0;

	//given an entity and initial pvs. Extend that pvs through any visible portals
	static void				ComputeExtendedPVS( const CBaseEntity *pViewEntity, const Vector &vVisOrigin, unsigned char *outputPVS, int pvssize, int iMaxRecursions );
	
	//This extender is decidedly visible, recursively extend the visibility problem
	virtual void			ComputeSubVisibility( CPVS_Extender **pExtenders, int iExtenderCount, unsigned char *outputPVS, int pvssize, const Vector &vVisOrigin, const VPlane *pVisFrustum, int iVisFrustumPlanes, VisExtensionChain_t *pVisChain, int iAreasNetworked[MAX_MAP_AREAS], int iMaxRecursionsLeft ) = 0;

	//cached data to make the algorithms a bit simpler in recursions
	struct ExtenderInstanceData_t
	{
		unsigned char iPVSBits[MAX_MAP_LEAFS/8];
		bool bAddedToPVSAlready; //no need to add our data to the PVS again, but should still recurse if we alter the frustum at all
	};

protected:
	ExtenderInstanceData_t *m_pExtenderData;
};




#endif //#ifndef PVS_EXTENDER_H
