//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_H
#define SOS_OP_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_system.h"
#include "snd_channels.h"

// Externs the logging channel
DECLARE_LOGGING_CHANNEL( LOG_SND_OPERATORS );



#define THREAD_LOCK_SOUND() AUTO_LOCK( g_SndMutex )




#define VALUE_REF_CHAR '@'
#define CLOSE_SQ_BRACKET ']'

#define SOS_INPUT_FLOAT( memberName, memberCount ) \
	float memberName[memberCount]; \
	int memberName##Offset[memberCount];

#define SOS_OUTPUT_FLOAT( memberName, memberCount ) \
	float memberName[memberCount];

#define SOS_INIT_INPUT_VAR( memberName, memberCount, value ) \
	for( int i = 0; i < memberCount; i++ ) \
{ pStructMem->memberName[i] = value; pStructMem->memberName##Offset[i] = -1; }

#define SOS_INIT_OUTPUT_VAR( memberName, memberType, value ) \
	for( int i = 0; i < memberType; i++ ) \
{ pStructMem->memberName[i] = value; }

#define SOS_REGISTER_INPUT( className, memberName, memberCount, memberString, type )	\
	{																					\
		InputData_t inputData;															\
		inputData.m_nValueOffset = offsetof( className##_t, memberName );					\
		inputData.m_nOffsetOffset = offsetof( className##_t, memberName##Offset );		\
		inputData.m_Type = type;														\
		inputData.m_nCount = memberCount;												\
		int nIndex = m_vInputs.AddToTail( inputData );									\
		m_vInputMap.Insert( memberString, nIndex );										\
	}

#define SOS_REGISTER_INPUT_FLOAT( className, memberName, memberCount, memberString ) \
	SOS_REGISTER_INPUT( className, memberName, memberCount, memberString, SO_FLOAT )

#define SOS_REGISTER_OUTPUT( className, memberName, memberCount, memberString, type )	\
	{																					\
		OutputData_t outputData;														\
		outputData.m_nValueOffset = offsetof( className##_t, memberName );					\
		outputData.m_Type = type;														\
		outputData.m_nCount = memberCount;												\
		int nIndex = m_vOutputs.AddToTail( outputData );								\
		m_vOutputMap.Insert( memberString, nIndex );									\
	}

#define SOS_REGISTER_OUTPUT_FLOAT( className, memberName, memberCount, memberString ) \
	SOS_REGISTER_OUTPUT( className, memberName, memberCount, memberString, SO_FLOAT )



#define SOS_BEGIN_OPERATOR_CONSTRUCTOR( classname, operatorstring ) \
classname::classname() \
{


#define SOS_END_OPERATOR_CONSTRUCTOR( classname, operatorstring ) \
	g_pSoundOperatorSystem->m_vOperatorCollection.Insert(operatorstring, this ); \
} \
classname g_##classname;


#define SOS_HEADER_DESC( classname ) \
public: \
	classname(); \
	virtual void	SetDefaults( void *pVoidMem ) const ; \
	virtual size_t	GetSize() const { return sizeof( classname##_t ); } \
	virtual void	Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const ; \
	virtual void	Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const ; \
	virtual void    OpHelp( ) const ; \
	virtual void	ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pParamKV ) const ; \



//-----------------------------------------------------------------------------
// The CSosOperator class:
//
// Base class for all sound operators, each operator must override "execute"
//
//-----------------------------------------------------------------------------
struct CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flExecute, SO_SINGLE )
	bool m_bExecuteOnce;
	bool m_bHasExecuted;
};


class CSosOperator
{
public:

							CSosOperator();
	virtual					~CSosOperator() { } ;
	int						GetOutputOffset( const char *pOutputName ) const;
	// Proposal to optimize this:
	// For each InputData_t, we store a function pointer to handle the resolution.
	// We would create a different resolve function for the major cases (write a float, write 3 floats, generic case - same as current implementation).
	// When filling the inputs initially, we would store the optimal function pointer.
	// Cost:	4 bytes per InputData_t
	//			One dynamic call instead of one static call (currently ResolveInputValue).
	// But then in most cases, the implementation would more efficient (estimated at 4 to 10 times faster).
	void					ResolveInputValue( void *pInputDefault, int *pOffsetArray, short nDataCount, SODataType_t nDataType, void *pStackMem ) const;
	void					ResolveInputValues( void *pStructMem, void *pStackMem ) const;
	void					OffsetConnections( void *pVoidMem, size_t nOffset ) const;
	bool					BaseParseKV( CSosOperatorStack *pStack, CSosOperator_t *pStructMem, const char *pParamString, const char *pValueString ) const; 
	int						ParseValueRef( CSosOperatorStack *pStack, const char *pParamString, const char *pValueString ) const;
	void					PrintBaseParams( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const;
	void					PrintIO( CSosOperator_t *pStructMem, CSosOperatorStack *pStack, int nLevel ) const;
	int						BaseParseHasArrayIndice( const char *pParamString ) const;
	void					SetBaseDefaults( void *pVoidMem ) const;

	virtual void			SetDefaults( void *pVoidMem ) const = 0;
	virtual size_t			GetSize() const = 0;
	virtual void			Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const = 0;
	virtual void			Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const = 0;
	virtual void			OpHelp( ) const = 0;
	virtual void			ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pParamKV ) const = 0;

	virtual void            StackInit( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex );
	virtual void			StackShutdown( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex );


protected:
public:

	struct OutputData_t
	{
		size_t m_nValueOffset;
		SODataType_t m_Type;
		short m_nCount;
	};

	struct InputData_t
	{
		size_t m_nValueOffset;
		size_t m_nOffsetOffset;
		SODataType_t m_Type;
		short m_nCount;
	};

	CUtlDict <int, int> m_vOutputMap;
	CUtlVector < OutputData_t > m_vOutputs;

	CUtlDict <int, int> m_vInputMap;
	CUtlVector< InputData_t > m_vInputs;
};

#endif // SOS_OP_H