//======= Copyright © 1996-2006, Valve Corporation, All rights reserved. ======
//
// Utility classes for creating maya commands
//
//=============================================================================

#ifndef VSMAYACOMMAND_H
#define VSMAYACOMMAND_H

#if defined( _WIN32 )
#pragma once
#endif

#include "tier0/dbg.h"
#include "xsi_status.h"
#include "idccmain.h"
#include "xsi_context.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
namespace XSI
{
	class Context;
	class CRef;
	class PluginRegistrar;
}


//-----------------------------------------------------------------------------
// Base class for maya commands
//-----------------------------------------------------------------------------
class CVsMayaCommand
{
public:
	CVsMayaCommand();

	virtual void OnRegister( XSI::PluginRegistrar& in_reg ) {}

	virtual XSI::CStatus doIt( XSI::Context &ctx ) = 0;

	// Derived classes must specify this to override syntax
	virtual void SpecifySyntax( const XSI::Context &ctx ) {}
};


//-----------------------------------------------------------------------------
// Base class for maya command factories
//-----------------------------------------------------------------------------
class CVsMayaCommandFactoryBase
{
public:
	// Returns the commandname associated with the factory
	const char *GetCommandName();

	// Called when we register
	virtual void OnRegister( XSI::PluginRegistrar& in_reg ) {}

	// Registers/deregisters all commands
	static bool RegisterAllCommands( XSI::PluginRegistrar& in_reg );
	static void DeregisterAllCommands( const XSI::PluginRegistrar& in_reg );

protected:
	// Constructor
	CVsMayaCommandFactoryBase( const char *pCommandName );

private:
	CVsMayaCommandFactoryBase* m_pNextFactory;
	const char* m_CommandName;

	static CVsMayaCommandFactoryBase *s_pFirstCommandFactory;
};


template < class T >
class CVsDCCCommandFactory : public CVsMayaCommandFactoryBase
{
	typedef CVsMayaCommandFactoryBase BaseClass;

public:
	// Constructor
	CVsDCCCommandFactory( const char *pCommandName ) : BaseClass( pCommandName ) {}

	// Called when we register
	virtual void OnRegister( XSI::PluginRegistrar& in_reg )
	{
		m_Singleton.OnRegister( in_reg );	
	}

	T m_Singleton;
};


//-----------------------------------------------------------------------------
// Helper macro to instantiate a command 
//-----------------------------------------------------------------------------
#define INSTALL_MAYA_COMMAND( _commandClassName, _stringName )							\
	static CVsDCCCommandFactory< _commandClassName > s_##_commandClassName##_Factory( #_stringName ); \
	XSIPLUGINCALLBACK XSI::CStatus _stringName##_Init( const XSI::CRef& in_context )	\
	{																					\
		if ( !g_pDCCMain->IsInitialized() )												\
			return XSI::CStatus::Fail;													\
		XSI::Context ctx(in_context);													\
		s_##_commandClassName##_Factory.m_Singleton.SpecifySyntax( ctx );				\
		return XSI::CStatus::OK;														\
	}																					\
	XSIPLUGINCALLBACK XSI::CStatus _stringName##_Execute( XSI::CRef& in_context )		\
	{																					\
		if ( !g_pDCCMain->IsInitialized() )												\
			return XSI::CStatus::Fail;													\
		XSI::Context ctxt( in_context );												\
		s_##_commandClassName##_Factory.m_Singleton.doIt( ctxt );						\
	}																					\


#endif // VSMAYACOMMAND_H
