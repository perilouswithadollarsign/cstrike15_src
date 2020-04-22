//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: couple of helper functions for  win32 PE modules
//
//=============================================================================//

#pragma once

enum EOpCodeOffsetType
{
	k_ENoRelativeOffsets,
	k_EDWORDOffsetAtByteTwo,
	k_EDWORDOffsetAtByteThree,
	k_EDWORDOffsetAtByteFour,
	k_EBYTEOffsetAtByteTwo,
};

bool ParseOpcode( unsigned char *pOpcode, int &nLength, EOpCodeOffsetType &eOffsetType );

#define CALCULATE_ADDRESS(base, offset) (((DWORD)(base)) + (offset))
#define GET_HEADER_DICTIONARY(module, idx)	&(module)->pNtHeaders->OptionalHeader.DataDirectory[idx]


// This structure describes an opcode parsed by our disassembler
typedef unsigned __int32 uint32;
typedef unsigned char uint8;
typedef struct
{
	int bOpcode;
	int cubOpcode;
	int cubImmed;
	uint32 uJump;
	uint32 uImmed; // not filled in
	bool bModRM;
	bool bRelative;
	bool bCantContinue;
	bool bJumpOrCall;
	bool bURJ;
} OPCODE_t;

bool ParseCode( uint8 *pubCode, OPCODE_t *pOpcode, int cubLeft );
uint32 ComputeJumpAddress( OPCODE_t *pOpcode, uint32 uVACurrent );
bool OpcodeText( OPCODE_t *pOpcode, char *rgchText );
bool LikelyValid( OPCODE_t *pOpcode );
bool LikelyNewValid( OPCODE_t *pOpcode );
int DisassembleSingleFunction( unsigned char *pubStart, int cub );


