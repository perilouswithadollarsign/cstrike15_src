//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//

#ifndef IIEXTPROPPORTALLOCATORHINC
#define IIEXTPROPPORTALLOCATORHINC

#define IEXTPROPPORTALLOCATOR_INTERFACE_NAME "PORTAL_SERVER_DLL_PROPPORTAL_LOCATOR"

abstract_class IPortalServerDllPropPortalLocator
{
public:
	struct PortalInfo_t
	{
		int iLinkageGroupId;
		int nPortal;
		Vector vecOrigin;
		QAngle vecAngle;
	};
	virtual void LocateAllPortals( CUtlVector<PortalInfo_t> &arrPortals ) = 0;
};

#endif
