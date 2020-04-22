//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======
//
// Purpose: Implementation of the CDmeAttributeReference class, a simple 
// DmElement used to store references to attributes within other DmElements.
//
//=============================================================================
#include "movieobjects/dmeattributereference.h"
#include "datamodel/dmelementfactoryhelper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



//-------------------------------------------------------------------------------------------------
// Expose the CDmeAttributeReference class to the scene database 
//-------------------------------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeAttributeReference, CDmeAttributeReference );


//-------------------------------------------------------------------------------------------------
// Purpose: Perform construction tasks, initializes attributes
//-------------------------------------------------------------------------------------------------
void CDmeAttributeReference::OnConstruction()
{
	m_Element.Init(			this,	"element",		FATTRIB_HAS_CALLBACK | FATTRIB_NEVERCOPY );
	m_AttributeName.Init(	this,	"attribute",	FATTRIB_HAS_CALLBACK | FATTRIB_TOPOLOGICAL );
	m_AttributeIndex.Init(	this,	"index",		FATTRIB_TOPOLOGICAL );	
	m_AttributeHandle = DMATTRIBUTE_HANDLE_INVALID;
}


//-------------------------------------------------------------------------------------------------
// Purpose: Perform destruction tasks
//-------------------------------------------------------------------------------------------------
void CDmeAttributeReference::OnDestruction()
{

}


//-------------------------------------------------------------------------------------------------
// Purpose: Process notifications of changed attributes and update the attribute handles if needed.
//-------------------------------------------------------------------------------------------------
void CDmeAttributeReference::OnAttributeChanged( CDmAttribute *pAttribute )
{
	if ( ( pAttribute == m_Element.GetAttribute() ) || ( pAttribute == m_AttributeName.GetAttribute() ) )
	{
		// Invalidate the attribute handle so that it will be updated.
		m_AttributeHandle = DMATTRIBUTE_HANDLE_INVALID;
		return;
	}

	BaseClass::OnAttributeChanged( pAttribute );
}


//-------------------------------------------------------------------------------------------------
// Purpose: Set the attribute by specifying the element, name of the attribute and optionally the 
// array index of the attribute.
//-------------------------------------------------------------------------------------------------
bool CDmeAttributeReference::SetAttribute( CDmElement* pElement, const char* pchAttributeName, int index )
{
	m_Element = pElement;
	m_AttributeName = pchAttributeName;
	m_AttributeIndex = index;

	CDmAttribute *pAttr = LookupAttributeHandle();
	return ( pAttr != NULL );
}


//-------------------------------------------------------------------------------------------------
// Purpose: Lookup and cache the attribute handle
//-------------------------------------------------------------------------------------------------
CDmAttribute *CDmeAttributeReference::LookupAttributeHandle() const
{
	m_AttributeHandle = DMATTRIBUTE_HANDLE_INVALID;

	CDmElement *pElement = m_Element.GetElement();
	const char *pName= m_AttributeName.Get();

	if ( pElement == NULL || pName == NULL || !pName[0] )
		return NULL;

	CDmAttribute *pAttr = pElement->GetAttribute( pName );
	if ( !pAttr )
		return NULL;

	m_AttributeHandle = pAttr->GetHandle();
	return pAttr;
}


//-------------------------------------------------------------------------------------------------
// Purpose: Get the attribute using the cached handle or looking up the handle if needed.
//-------------------------------------------------------------------------------------------------
CDmAttribute* CDmeAttributeReference::GetReferencedAttribute() const
{
	CDmAttribute *pAttribute = g_pDataModel->GetAttribute( m_AttributeHandle );
	if ( !pAttribute )
	{
		pAttribute = LookupAttributeHandle();
	}
	return pAttribute;
}


//-------------------------------------------------------------------------------------------------
// Purpose: Get the value of the referenced attribute
//-------------------------------------------------------------------------------------------------
const void *CDmeAttributeReference::GetAttributeValue( DmAttributeType_t &type ) const
{
	CDmAttribute *pAttribute = GetReferencedAttribute();
	if ( pAttribute == NULL )
		return NULL;

	const void *pValue = NULL;

	type = pAttribute->GetType();
	if ( IsArrayType( type ) )
	{
		CDmrGenericArray array( pAttribute );
		pValue = array.GetUntyped( m_AttributeIndex.Get() );
		type = ArrayTypeToValueType( type );
	}
	else
	{
		pValue = pAttribute->GetValueUntyped();
	}
	
	return pValue;
}


//-------------------------------------------------------------------------------------------------
// Purpose: Set the value of the referenced attribute
//-------------------------------------------------------------------------------------------------
void CDmeAttributeReference::SetAttributeValue( const void *pValue, DmAttributeType_t type ) const
{
	CDmAttribute *pAttribute = GetReferencedAttribute();
	if ( pAttribute == NULL )
		return;

	if ( IsArrayType( pAttribute->GetType() ) )
	{
		CDmrGenericArray array( pAttribute );
		array.Set( m_AttributeIndex.Get(), type, pValue );
	}
	else
	{
		pAttribute->SetValue( type, pValue );
	}
}


//-------------------------------------------------------------------------------------------------
// Purpose: Determine if the attribute reference points to a valid attribute
//-------------------------------------------------------------------------------------------------
bool CDmeAttributeReference::IsValid() const
{
	return ( GetReferencedAttribute() != NULL );
}


