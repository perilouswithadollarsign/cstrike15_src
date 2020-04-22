//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======
// 
// Purpose: Declaration of the CDmeConnectionOperator class, a CDmeOperator 
// which copies one attribute value to another, providing similar functionality
// to CDmeChannel, but does not store a log and is not effected by the 
// recording mode.
//
//=============================================================================

#ifndef DMECONNECTIONOPERATOR_H
#define DMECONNECTIONOPERATOR_H
#ifdef _WIN32
#pragma once
#endif

#include "movieobjects/dmeattributereference.h"
#include "movieobjects/dmeoperator.h" 


//-------------------------------------------------------------------------------------------------
// The CDmeConnectionOperator class is a CDmeOperator which copies the value from one attribute
// to another. The CDmeConnectionOperator is frequently used in combination with the 
// CDmeExpressionOperator to allow a single value, often on controlled by a channel, to drive the
// value of multiple targets. The connection operator may have multiple outputs, but only one 
// input. Only a single input is allowed because allowing multiple inputs would mean that the 
// operator could actually represent multiple unrelated connections, and this would could cause
// various dependency and evaluation issues. Multiple outputs are allowed as it reduces the number
// of individual operators required to accomplish many setups, all of the connections must be 
// related, and the dependency chain is essentially the same as having individual operators for
// each connection, as the operator with multiple outputs can always be evaluated immediately 
// following the evaluation of its single input.
//-------------------------------------------------------------------------------------------------
class CDmeConnectionOperator : public CDmeOperator
{
	DEFINE_ELEMENT( CDmeConnectionOperator, CDmeOperator );


public:

	// Run the operator, which copies the value from the source attribute to the destination attributes.
	virtual void Operate();

	// Add the input attribute used by the operator to the provided list of attributes, This is 
	// generally used by the evaluation process to find the attributes an operator is dependent on.
	virtual void GetInputAttributes( CUtlVector< CDmAttribute * > &attrs );

	// Add each of attributes the connection operator outputs to the provided list of attributes.
	// This is generally used by the evaluation process to find out what attributes are written by
	// the operator in order to determine what other operators are dependent on this operator.
	virtual void GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs );

	// Determine if data has changed and the operator needs to be updated
	virtual bool IsDirty();

	// Set the input attribute of the connection.
	void SetInput( CDmElement* pElement, const char* pchAttributeName, int index = 0 );

	// Add an attribute to be written to by the connection.
	void AddOutput( CDmElement* pElement, const char* pchAttributeName, int index = 0 );

	// Get the number of output attributes
	int NumOutputAttributes() const;

	// Get the specified output attribute
	CDmAttribute *GetOutputAttribute( int index ) const;

	// Get the input attribute
	CDmAttribute *GetInputAttribute();


protected:

	CDmaElement< CDmeAttributeReference	>		m_Input;	// Reference to the input attribute
	CDmaElementArray< CDmeAttributeReference >	m_Outputs;	// Array of references to output attributes
			
};


#endif // DMECONNECTIONOPERATOR_H
