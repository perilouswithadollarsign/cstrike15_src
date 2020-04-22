/////////////////////////////////////////////////////////////////////////
//XUIClientInfo.h 
//
//Copyright Certain Affinity 2007
// 

#ifndef _XUICLIENT_INFO_H_
#define _XUICLIENT_INFO_H_

#define INTERFACEVERSION_XUIClientInfo "XUIClientInterface1"

//This is intended to be an interface between the XUI system and the main game.
class IXUIClientInfo
{
public:
	virtual bool IsPlayerSurvivor() = 0;
	virtual bool IsPlayerInfected() = 0;
	virtual bool IsPlayerSpectator() = 0;
	// presence info functions
	virtual unsigned int GetScenarioCount() = 0;
	virtual const char* GetScenarioDisplayString(unsigned int index) = 0;
	virtual const char* GetScenarioValueString(unsigned int index) = 0;
	virtual unsigned int GetScenarioID(unsigned int index) = 0;
};

#endif
