//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======
// 
// Purpose: Declaration of the CDmeAttributeReference class, a simple DmElement
// used to store references to attributes within other DmElements.
//
//=============================================================================

#ifndef DMEATTRIBUTEREFERENCE_H
#define DMEATTRIBUTEREFERENCE_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmattributevar.h"

//-------------------------------------------------------------------------------------------------
// The CDmeAttributeReference class provides storage of the information needed to access an 
// attribute in any element. It also provides an interface for accessing the attribute which
// automatically looks up the attribute and caches the handle.
//-------------------------------------------------------------------------------------------------
class CDmeAttributeReference : public CDmElement
{
	DEFINE_ELEMENT( CDmeAttributeReference, CDmElement );

	public:

		// Process notifications of changed attributes and update the attribute handles if needed.
		virtual void OnAttributeChanged( CDmAttribute *pAttribute );
		
		// Set the attribute by specifying the element, name of the attribute and optionally the array index of the attribute.
		bool SetAttribute( CDmElement* pElement, const char* pchAttributeName, int index = 0 );

		// Get the attribute using the cached handle or looking up the handle if needed.
		CDmAttribute* GetReferencedAttribute() const;

		// Get the value of the referenced attribute
		const void *GetAttributeValue( DmAttributeType_t &type ) const;
		
		// Set the value of the referenced attribute
		void SetAttributeValue( const void *pValue, DmAttributeType_t type ) const;

		// Determine if the attribute reference points to a valid attribute
		bool IsValid() const;
		
	private:
	
		// Lookup and cache the attribute handle
		CDmAttribute* LookupAttributeHandle() const;

		CDmaElement< CDmElement >	m_Element;
		CDmaString					m_AttributeName;
		CDmaVar< int >				m_AttributeIndex;

		mutable DmAttributeHandle_t	m_AttributeHandle;
};


#endif // DMEATTRIBUTEREFERENCE_H
