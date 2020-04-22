//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//http://www.nfl.com/gamecenter/2010010900/2009/POST18/eagles@cowboys#tab:watch
// Purpose: 
//
//=============================================================================

#ifndef DMELEMENTFACTORYHELPER_H
#define DMELEMENTFACTORYHELPER_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/idatamodel.h"
#include "datamodel/dmelement.h"
#include "datamodel/dmattribute.h"
#include "datamodel/dmattributevar.h"
#include "tier1/utlvector.h"
#include "tier1/utlsymbollarge.h"


//-----------------------------------------------------------------------------
// Internal interface for IDmElementFactory
//-----------------------------------------------------------------------------
class CDmElementFactoryInternal : public IDmElementFactory
{
public:
	virtual void SetElementTypeSymbol( CUtlSymbolLarge sym ) = 0;
	virtual bool IsAbstract() const = 0;

	virtual CUtlSymbolLarge GetElementTypeSymbol() const = 0;
	virtual CUtlSymbolLarge GetParentElementTypeSymbol() const = 0;

	virtual void AddOnElementCreatedCallback( IDmeElementCreated *pCallback );
	virtual void RemoveOnElementCreatedCallback( IDmeElementCreated *pCallback );
	virtual void OnElementCreated( CDmElement* pElement );
	
private:
	CUtlVector< IDmeElementCreated* > m_CallBackList;
};






//-----------------------------------------------------------------------------
// Class used to register factories into a global list
//-----------------------------------------------------------------------------
class CDmElementFactoryHelper
{
public:
	// Static list of helpers
	static CDmElementFactoryHelper *s_pHelpers[2];

	// Create all the hud elements
	static void InstallFactories( );

public:
	// Construction
	CDmElementFactoryHelper( const char *pClassName, CDmElementFactoryInternal *pFactory, bool bIsStandardFactory );

	// Accessors
	CDmElementFactoryHelper *GetNext   () { return m_pNext; }
	CDmElementFactoryHelper *GetParent () { return m_pParent; }
	CDmElementFactoryHelper *GetChild  () { return m_pChild; }
	CDmElementFactoryHelper *GetSibling() { return m_pSibling; }

	const char *GetClassname();
	CDmElementFactoryInternal *GetFactory();

private:
	CDmElementFactoryHelper() {}

	// Next factory in list
	CDmElementFactoryHelper	*m_pNext;

	// class hierarchy links
	CDmElementFactoryHelper *m_pParent;
	CDmElementFactoryHelper *m_pChild;
	CDmElementFactoryHelper *m_pSibling;

	// Creation function to use for this technology
	CDmElementFactoryInternal *m_pFactory;
	const char				*m_pszClassname;
};


//-----------------------------------------------------------------------------
// Inline methods 
//-----------------------------------------------------------------------------
inline const char *CDmElementFactoryHelper::GetClassname() 
{ 
	return m_pszClassname; 
}

inline CDmElementFactoryInternal *CDmElementFactoryHelper::GetFactory() 
{ 
	return m_pFactory; 
}


//-----------------------------------------------------------------------------
// Helper Template factory for simple creation of factories
//-----------------------------------------------------------------------------
template < class T >
class CDmElementFactory : public CDmElementFactoryInternal
{
public:
	CDmElementFactory( const char *pLookupName ) : m_pLookupName( pLookupName ) {}

	// Creation, destruction
	virtual CDmElement* Create( DmElementHandle_t handle, const char *pElementType, const char *pElementName, DmFileId_t fileid, const DmObjectId_t &id )
	{
		return new T( handle, m_pLookupName, id, pElementName, fileid );
	}

	virtual void Destroy( DmElementHandle_t hElement )
	{
		CDmElement *pElement = g_pDataModel->GetElement( hElement );
		if ( pElement )
		{
			T *pActualElement = static_cast< T* >( pElement );
			delete pActualElement;
		}
	}

	// Sets the type symbol, used for "isa" implementation
	virtual void SetElementTypeSymbol( CUtlSymbolLarge sym )
	{
		T::SetTypeSymbol( sym );
	}

	virtual bool IsAbstract() const { return false; }

	virtual CUtlSymbolLarge GetElementTypeSymbol() const
	{
		return T::GetStaticTypeSymbol();
	}
	virtual CUtlSymbolLarge GetParentElementTypeSymbol() const
	{
		CUtlSymbolLarge baseClassSym = T::BaseClass::GetStaticTypeSymbol();
		if ( baseClassSym == T::GetStaticTypeSymbol() )
			return UTL_INVAL_SYMBOL_LARGE; // only CDmElement has itself as it's BaseClass - this lets us know we're at the top of the hierarchy
		return baseClassSym;
	}

private:
	const char *m_pLookupName;
};


template < class T >
class CDmAbstractElementFactory : public CDmElementFactoryInternal
{
public:
	CDmAbstractElementFactory() {}

	// Creation, destruction
	virtual CDmElement* Create( DmElementHandle_t handle, const char *pElementType, const char *pElementName, DmFileId_t fileid, const DmObjectId_t &id )
	{
		return NULL;
	}

	virtual void Destroy( DmElementHandle_t hElement )
	{
	}

	// Sets the type symbol, used for "isa" implementation
	virtual void SetElementTypeSymbol( CUtlSymbolLarge sym )
	{
		T::SetTypeSymbol( sym );
	}

	virtual bool IsAbstract() const { return true; }

	virtual CUtlSymbolLarge GetElementTypeSymbol() const
	{
		return T::GetStaticTypeSymbol();
	}
	virtual CUtlSymbolLarge GetParentElementTypeSymbol() const
	{
		CUtlSymbolLarge baseClassSym = T::BaseClass::GetStaticTypeSymbol();
		if ( baseClassSym == T::GetStaticTypeSymbol() )
			return UTL_INVAL_SYMBOL_LARGE; // only CDmElement has itself as it's BaseClass - this lets us know we're at the top of the hierarchy
		return baseClassSym;
	}

private:
};


//-----------------------------------------------------------------------------
// Helper macro to create the class factory 
//-----------------------------------------------------------------------------
#if defined( MOVIEOBJECTS_LIB ) || defined ( DATAMODEL_LIB ) || defined ( DMECONTROLS_LIB ) || defined ( MDLOBJECTS_LIB )

#define IMPLEMENT_ELEMENT_FACTORY( lookupName, className )	\
	IMPLEMENT_ELEMENT( className )							\
	CDmElementFactory< className > g_##className##_Factory( #lookupName );							\
	CDmElementFactoryHelper g_##className##_Helper( #lookupName, &g_##className##_Factory, true );	\
	className *g_##className##LinkerHack = NULL;

#define IMPLEMENT_ABSTRACT_ELEMENT( lookupName, className )	\
	IMPLEMENT_ELEMENT( className )							\
	CDmAbstractElementFactory< className > g_##className##_Factory;									\
	CDmElementFactoryHelper g_##className##_Helper( #lookupName, &g_##className##_Factory, true );	\
	className *g_##className##LinkerHack = NULL;

#else

#define IMPLEMENT_ELEMENT_FACTORY( lookupName, className )	\
	IMPLEMENT_ELEMENT( className )							\
	CDmElementFactory< className > g_##className##_Factory( #lookupName );						\
	CDmElementFactoryHelper g_##className##_Helper( #lookupName, &g_##className##_Factory, false );	\
	className *g_##className##LinkerHack = NULL;

#define IMPLEMENT_ABSTRACT_ELEMENT( lookupName, className )	\
	IMPLEMENT_ELEMENT( className )							\
	CDmAbstractElementFactory< className > g_##className##_Factory;									\
	CDmElementFactoryHelper g_##className##_Helper( #lookupName, &g_##className##_Factory, false );	\
	className *g_##className##LinkerHack = NULL;

#endif


// Used by classes defined in movieobjects or scenedatabase that must be explicitly installed
#define IMPLEMENT_ELEMENT_FACTORY_INSTALL_EXPLICITLY( lookupName, className )	\
	IMPLEMENT_ELEMENT( className )							\
	CDmElementFactory< className > g_##className##_Factory( #lookupName );						\
	CDmElementFactoryHelper g_##className##_Helper( #lookupName, &g_##className##_Factory, false );	\
	className *g_##className##LinkerHack = NULL;


//-----------------------------------------------------------------------------
// Used to instantiate classes in libs from dlls/exes
//-----------------------------------------------------------------------------
#define USING_ELEMENT_FACTORY( className )			\
	extern C##className *g_##C##className##LinkerHack;		\
	C##className *g_##C##className##PullInModule = g_##C##className##LinkerHack;


//-----------------------------------------------------------------------------
// Installs dm element factories
//-----------------------------------------------------------------------------
void InstallDmElementFactories( );


#endif // DMELEMENTFACTORYHELPER_H
