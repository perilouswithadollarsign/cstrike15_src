//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmeoperator.h"
#include "movieobjects/dmeattributereference.h"
#include "datamodel/dmelementfactoryhelper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ABSTRACT_ELEMENT( DmeOperator, CDmeOperator );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeOperator::OnConstruction()
{
	m_nSortKey = -1;
}

void CDmeOperator::OnDestruction()
{
}

//-----------------------------------------------------------------------------
// IsDirty - ie needs to operate
//-----------------------------------------------------------------------------
bool CDmeOperator::IsDirty()
{
	return BaseClass::IsDirty();
}


//-----------------------------------------------------------------------------
// Purpose : get a list of all of the operators whose evaluation affects the
// result of this operator's evaluation.
//-----------------------------------------------------------------------------
void CDmeOperator::GatherInputOperators( CUtlVector< CDmeOperator * > &operatorList )
{
	// Another operator will only affect this operator if one or more of its output attributes is 
	// an input attribute of this operator. So to find all of the input operators we first find all
	// of the input attributes of this operator, then find the elements which own those attributes 
	// and find all of the operators referencing those elements. Finally we check to see if any of 
	// the output attributes of the operators match any of the input attributes of this operator.

	// Find the input attributes of this operator.
	CUtlVector< CDmAttribute* > inputAttributes( 0, 32 );
	GetInputAttributes( inputAttributes );

	// Build a list of all the operators which are referencing any of the elements which own an input
	// attribute of this operator, these are all the operators which can possibly be input operators.
	int nInputAttributes = inputAttributes.Count();
	CUtlVector< CDmeOperator* > connectedOperators( 0, nInputAttributes );
	CUtlVector< CDmElement* > inputOwnerList( 0, nInputAttributes );

	for ( int iAttr = 0; iAttr < nInputAttributes; ++iAttr )
	{
		CDmAttribute *pInputAttr = inputAttributes[ iAttr ];
		if ( pInputAttr == NULL )
			continue;

		CDmElement *pInputOwner = pInputAttr->GetOwner();

		// If the owner of the input is another operator, add it directly to the connected operator list.
		if ( ( pInputOwner != this ) && ( pInputOwner->IsA( CDmeOperator::GetStaticTypeSymbol() ) ) )
		{			
			connectedOperators.AddToTail(  CastElement< CDmeOperator >( pInputOwner ) );
			continue;
		}

		// If the owner of the input is not an operator, check to see if it has any operators referring
		// to it. A list of these elements is kept so that the check is only done once per element.
		if ( inputOwnerList.Find( pInputOwner ) != inputOwnerList.InvalidIndex() )
			continue;			

		inputOwnerList.AddToTail( pInputOwner );


		for ( DmAttributeReferenceIterator_t it = g_pDataModel->FirstAttributeReferencingElement( pInputOwner->GetHandle() );
			it != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID;
			it = g_pDataModel->NextAttributeReferencingElement( it ) )
		{
			CDmAttribute *pAttr = g_pDataModel->GetAttribute( it );
			if ( pAttr == NULL )
				continue;

			CDmElement *pElement = pAttr->GetOwner();
			CDmeOperator *pOperator = CastElement< CDmeOperator >( pElement );

			if ( pOperator == NULL )
			{
				pOperator = FindAncestorReferencingElement< CDmeOperator >( pElement );
			}

			if ( pOperator != NULL )
			{		
				if ( connectedOperators.Find( pOperator ) == connectedOperators.InvalidIndex() )
				{
					connectedOperators.AddToTail( pOperator );
				}
			}
		}
	}

	// Now check each of the connected operators to determine if any of its output attributes is one
	// of this operator's input attributes. If so add it to the list of output operators if it is not
	// already there. Note, as soon as one attribute match is found there is no need to check the rest.
	CUtlVector< CDmAttribute* > outputAttributes( 0, 32 );

	int nConnectedOperators = connectedOperators.Count();
	for ( int iOper = 0; iOper < nConnectedOperators; ++iOper )
	{
		CDmeOperator *pOperator = connectedOperators[ iOper ];

		outputAttributes.RemoveAll();
		pOperator->GetOutputAttributes( outputAttributes );

		int nOutputAttributes = outputAttributes.Count();
		for ( int iAttr = 0; iAttr < nOutputAttributes; ++iAttr )
		{
			CDmAttribute *pOuputAttr = outputAttributes[ iAttr ];
			if ( inputAttributes.Find( pOuputAttr ) != inputAttributes.InvalidIndex() )
			{
				if ( operatorList.Find( pOperator ) == operatorList.InvalidIndex() )
				{
					pOperator->GatherInputOperators( operatorList );
					operatorList.AddToTail( pOperator );
				}
				break;
			}
		}
	}
	
}


void CDmeOperator::SetSortKey( int key )
{
	m_nSortKey = key;
}

int CDmeOperator::GetSortKey() const
{
	return m_nSortKey;
}

//-----------------------------------------------------------------------------
// Purpose : Gather a list of all of the operators referencing this element and
// all of the operators that they depend on.
//-----------------------------------------------------------------------------
void GatherOperatorsForElement( CDmElement *pRootElement, CUtlVector< CDmeOperator * > &operatorList )
{
	for ( DmAttributeReferenceIterator_t it = g_pDataModel->FirstAttributeReferencingElement( pRootElement->GetHandle() );
		it != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID;
		it = g_pDataModel->NextAttributeReferencingElement( it ) )
	{
		CDmAttribute *pAttr = g_pDataModel->GetAttribute( it );
		CDmElement *pOwnerElement = pAttr->GetOwner();

		if ( !g_pDataModel->GetElement( pOwnerElement->GetHandle() ) )
			continue;

		CDmeOperator *pOperator = CastElement< CDmeOperator >( pOwnerElement );

		if ( pOwnerElement->IsA( CDmeAttributeReference::GetStaticTypeSymbol() ) )
		{
			pOperator = FindAncestorReferencingElement< CDmeOperator >( pOwnerElement );
		}

		if ( pOperator == NULL )
			continue;
				
		if ( operatorList.Find( pOperator ) == operatorList.InvalidIndex() )
		{
			pOperator->GatherInputOperators( operatorList );
			operatorList.AddToTail( pOperator );
		}
	}
}
