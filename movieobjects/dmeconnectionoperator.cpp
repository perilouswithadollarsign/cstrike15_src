//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======
// 
// Purpose: Declaration of the CDmeConnectionOperator class, a CDmeOperator 
// which copies one attribute value to another, providing similar functionality
// to CDmeChannel, but does not store a log and is not effected by the 
// recording mode.
//
//=============================================================================
#include "movieobjects/dmeconnectionoperator.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "tier1/fmtstr.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-------------------------------------------------------------------------------------------------
// Expose this class to the scene database 
//-------------------------------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeConnectionOperator, CDmeConnectionOperator );

//-------------------------------------------------------------------------------------------------
// Purpose: Constructor, initializes attributes, create the embedded target
//-------------------------------------------------------------------------------------------------
void CDmeConnectionOperator::OnConstruction()
{
	m_Input.InitAndCreate( this, "input" );
	m_Outputs.Init( this, "outputs" );
}


//-------------------------------------------------------------------------------------------------
// Purpose: Perform destruction tasks, destroy the internal elements of the constraint.
//-------------------------------------------------------------------------------------------------
void CDmeConnectionOperator::OnDestruction()
{
	g_pDataModel->DestroyElement( m_Input.GetHandle() );

	int nOutputs = m_Outputs.Count();
	for ( int i = 0 ;i < nOutputs; ++i )
	{
		if ( m_Outputs[ i ] )
		{
			g_pDataModel->DestroyElement( m_Outputs[ i ]->GetHandle() );
		}
	}

	m_Outputs.RemoveAll();
}


//-------------------------------------------------------------------------------------------------
// Purpose: Run the operator, which copies the value from the source attribute to the destination 
// attributes.
//-------------------------------------------------------------------------------------------------
void CDmeConnectionOperator::Operate()
{
	if ( !m_Input->IsValid() )
		return;

	int nOutputs = m_Outputs.Count();
	if ( nOutputs == 0 )
		return;
			
	DmAttributeType_t inputType = AT_UNKNOWN;
	const void *pValue = m_Input->GetAttributeValue( inputType );

	for ( int iOutput = 0; iOutput < nOutputs; ++iOutput )
	{
		m_Outputs[ iOutput ]->SetAttributeValue( pValue, inputType );
	}
}


//-------------------------------------------------------------------------------------------------
// Purpose: Determine if data has changed and the operator needs to be updated
//-------------------------------------------------------------------------------------------------
bool CDmeConnectionOperator::IsDirty()
{
	CDmAttribute* pAttr = m_Input->GetReferencedAttribute();

	if ( pAttr )
	{
		return pAttr->IsFlagSet( FATTRIB_DIRTY );
	}

	return false;
}


//-------------------------------------------------------------------------------------------------
// Purpose: Add the input attribute used by the operator to the provided list of attributes, This 
// is generally used by the evaluation process to find the attributes an operator is dependent on.
//-------------------------------------------------------------------------------------------------
void CDmeConnectionOperator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	CDmAttribute *pInputAttr = m_Input->GetReferencedAttribute();
	if ( pInputAttr )
	{
		attrs.AddToTail( pInputAttr );
	}
}


//-------------------------------------------------------------------------------------------------
// Purpose: Add each of attributes the connection operator outputs to the provided list of 
// attributes. This is generally used by the evaluation process to find out what attributes are 
// written by the operator in order to determine what other operators are dependent on this 
// operator.
//-------------------------------------------------------------------------------------------------
void CDmeConnectionOperator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	int nOutputs = m_Outputs.Count();
	for ( int iOutput = 0; iOutput < nOutputs; ++iOutput )
	{
		CDmAttribute *pOutputAttr = m_Outputs[ iOutput ]->GetReferencedAttribute();
		if ( pOutputAttr )
		{
			attrs.AddToTail( pOutputAttr );
		}
	}
}


//-------------------------------------------------------------------------------------------------
// Purpose: Set the input attribute of the connection.
//-------------------------------------------------------------------------------------------------
void CDmeConnectionOperator::SetInput( CDmElement* pElement, const char* pchAttributeName, int index )
{
	m_Input->SetAttribute( pElement, pchAttributeName, index );
}


//-------------------------------------------------------------------------------------------------
// Purpose: Add an attribute to be written to by the connection.
//-------------------------------------------------------------------------------------------------
void CDmeConnectionOperator::AddOutput( CDmElement* pElement, const char* pchAttributeName, int index )
{
	if ( ( pElement == NULL ) || ( pchAttributeName == NULL ) )
		return;

	CDmeAttributeReference *pAttrRef = CreateElement< CDmeAttributeReference >( CFmtStr( "%s_%s", pElement->GetName() , pchAttributeName ), GetFileId() );
	if ( pAttrRef )
	{
		if ( pAttrRef->SetAttribute( pElement, pchAttributeName, index ) )
		{
			// Add the new reference to the list of outputs of the connection.
			m_Outputs.AddToTail( pAttrRef );
		}
		else
		{
			// If the specified attribute was not valid destroy the reference.
			g_pDataModel->DestroyElement( pAttrRef->GetHandle() );
		}
	}
}


//-------------------------------------------------------------------------------------------------
// Purpose: Get the number of output attributes
//-------------------------------------------------------------------------------------------------
int CDmeConnectionOperator::NumOutputAttributes() const
{
	return m_Outputs.Count();
}


//-------------------------------------------------------------------------------------------------
// Purpose: Get the specified output attribute
//-------------------------------------------------------------------------------------------------
CDmAttribute *CDmeConnectionOperator::GetOutputAttribute( int index ) const
{
	if ( index >= m_Outputs.Count() )
		return NULL;

	return m_Outputs[ index ]->GetReferencedAttribute();	
}


//-------------------------------------------------------------------------------------------------
// Purpose: Get the input attribute
//-------------------------------------------------------------------------------------------------
CDmAttribute *CDmeConnectionOperator::GetInputAttribute()
{
	return m_Input.GetAttribute();
}

