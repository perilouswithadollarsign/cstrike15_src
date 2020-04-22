//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Implementation for various PE helper functions
//
//=============================================================================//

#ifdef _WIN32
#include "disassembler.h"
#include "windows.h"

#pragma warning (disable: 4311) // warning C4311: 'type cast' : pointer truncation from 'bla *' to 'unsigned int'
#pragma warning (disable: 4127) // warning C4127: conditional expression is constant
#pragma warning (disable: 4244) // warning C4244: 'initializing' : conversion from 'unsigned int' to 'char', possible loss of data
#pragma warning (disable: 4996) // warning C4996: 'function': This function or variable may be unsafe. Consider using function_s instead.
#pragma warning (disable: 4100) // warning C4100: 'format' : unreferenced formal parameter


typedef struct 
{
	unsigned char m_OpCodeB1;	// first opcode byte
	unsigned char m_OpCodeB2;	// second opcode byte 
	unsigned char m_OpCodeB3;	// third opcode byte 
	unsigned char m_OpCodeB4;	// fourth opcode byte
	unsigned char m_TotalLength; // total length of opcodes and data
	int m_cOpCodeBytesToMatch; // Normally 1, 2 if this is a 2 byte opcode, 3 if it's 3 bytes (ie, has x64 prefix or such)
	EOpCodeOffsetType m_EOffsetType;  // if true, this opcode has IP relative data
} KnownOpCode_t;

// Set of opcodes that we know how to relocate safely from function preambles when
// installing function detours.
static const KnownOpCode_t s_rgKnownOpCodes[] =
{
#ifndef _WIN64
	{ 0x08, 0xE9, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // or cl,ch
	{ 0x0F, 0x57, 0xC0, 0x00, 3, 3, k_ENoRelativeOffsets }, // xorps xmm0,xmm0 (simd)
	{ 0x31, 0xC0, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // xor eax,eax
	{ 0x31, 0xD2, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // xor edx,edx 
	{ 0x31, 0xED, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // xor ebp,ebp
	{ 0x31, 0xF6, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // xor esi,esi 
	{ 0x32, 0xC0, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // xor al,al
	{ 0x33, 0xC0, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // xor eax,eax
	{ 0x33, 0xF6, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // xor esi,esi
	{ 0x3C, 0x00, 0x00, 0x00, 2, 1, k_ENoRelativeOffsets }, // cmp al,immediate byte
	{ 0x39, 0x74, 0x00, 0x00, 4, 2, k_ENoRelativeOffsets }, // cmp dword ptr [rega+regb*coefficient+imm8],esi -- rega, regb, and coefficient depend on value of byte 3
	{ 0x3D, 0x00, 0x00, 0x00, 5, 1, k_ENoRelativeOffsets }, // cmp eax,immediate dword
	{ 0x3F, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // aas (ascii adjust al after subtraction)

	{ 0x40, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // inc eax
	{ 0x41, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // inc ecx
	{ 0x42, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // inc edx
	{ 0x43, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // inc ebx
	{ 0x44, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // inc esp
	{ 0x45, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // inc ebp
	{ 0x46, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // inc esi
	{ 0x47, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // inc edi

	{ 0x48, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // dec eax
	{ 0x49, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // dec ecx
	{ 0x4A, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // dec edx
	{ 0x4B, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // dec ebx
	{ 0x4C, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // dec esp
	{ 0x4D, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // dec ebp
	{ 0x4E, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // dec esi
	{ 0x4F, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // dec edi

	{ 0x50, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // push eax
	{ 0x51, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // push ecx
	{ 0x52, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // push edx
	{ 0x53, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // push ebx
	{ 0x54, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // push esp
	{ 0x55, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // push ebp
	{ 0x56, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // push esi
	{ 0x57, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // push edi

	{ 0x58, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // pop eax
	{ 0x59, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // pop ecx
	{ 0x5A, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // pop edx
	{ 0x5B, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // pop ebx
	{ 0x5C, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // pop esp
	{ 0x5D, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // pop ebp
	{ 0x5E, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // pop esi
	{ 0x5F, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // pop edi


	{ 0x60, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // pushad
	{ 0x61, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // popad

	{ 0x64, 0xA1, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr fs:[imm32] 

	{ 0x68, 0x00, 0x00, 0x00, 5, 1, k_ENoRelativeOffsets }, // push immediate doubleword
	{ 0x6A, 0x00, 0x00, 0x00, 2, 1, k_ENoRelativeOffsets }, // push immediate byte

	{ 0x80, 0x3D, 0x00, 0x00, 7, 2, k_ENoRelativeOffsets }, // cmp byte ptr ds:[dword],imm8
	{ 0x81, 0xEC, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // sub esp immediate dword
	{ 0x81, 0xF9, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // cmp ecx, immediate dword
	{ 0x83, 0x3D, 0x00, 0x00, 7, 2, k_ENoRelativeOffsets }, // cmp dword ptr to immediate byte
	{ 0x83, 0x40, 0x00, 0x00, 4, 2, k_ENoRelativeOffsets }, // add dword ptr [eax+imm8],imm8
	{ 0x83, 0x41, 0x00, 0x00, 4, 2, k_ENoRelativeOffsets }, // add dword ptr [ecx+imm8],imm8
	{ 0x83, 0x6C, 0x00,	0x00, 5, 2, k_ENoRelativeOffsets },	// sub dword ptr [rega+regb*coefficient+imm8a],imm8b -- rega, regb, and coefficient depend on value of byte 3
	{ 0x83, 0x7C, 0x00, 0x00, 5, 2, k_ENoRelativeOffsets }, // cmp dword ptr [rega+regb*coefficient+imm8a],imm8b -- rega, regb, and coefficient depend on value of byte 3
	{ 0x83, 0x7D, 0x00, 0x00, 4, 2, k_ENoRelativeOffsets }, // cmp dword ptr [ebp+imm8],imm8

	{ 0x83, 0xC0, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // add eax immediate byte
	{ 0x83, 0xC1, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // add ecx immediate byte
	{ 0x83, 0xC2, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // add edx immediate byte
	{ 0x83, 0xC3, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // add ebx immediate byte
	{ 0x83, 0xC4, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // add esp immediate byte
	{ 0x83, 0xC5, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // add ebp immediate byte
	{ 0x83, 0xC6, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // add esi immediate byte
	{ 0x83, 0xC7, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // add edi immediate byte

	{ 0x83, 0xE4, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // and esp,0FFFFFF00+immediate byte
	{ 0x83, 0xE8, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // sub eax immediate byte
	{ 0x83, 0xE9, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // sub ecx immediate byte
	{ 0x83, 0xEA, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // sub edx immediate byte
	{ 0x83, 0xEB, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // sub ebx immediate byte
	{ 0x83, 0xEC, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // sub esp immediate byte
	{ 0x83, 0xED, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // sub ebp immediate byte
	{ 0x83, 0xEE, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // sub esi immediate byte
	{ 0x83, 0xEF, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // sub edi immediate byte

	{ 0x83, 0xFA, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // lock cmp edx,imm8
	
	{ 0x85, 0xC7, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // test edi,eax 
	{ 0x85, 0xC8, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // test eax,ecx 
	{ 0x85, 0xC9, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // test ecx,ecx 
	{ 0x85, 0xCA, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // test edx,ecx 

	{ 0x87, 0x05, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // xchg eax, dword ptr
	{ 0x89, 0xE5, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ebp,esp 
	{ 0x89, 0x5C, 0x24, 0x00, 4, 3, k_ENoRelativeOffsets }, // mov dword ptr [esp+imm8],ebx 

	{ 0x8B, 0x00, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [eax]
	{ 0x8B, 0x01, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [ecx]
	{ 0x8B, 0x02, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [edx]
	{ 0x8B, 0x03, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [ebx]
	
	{ 0x8B, 0x06, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [esi]
	{ 0x8B, 0x07, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [edi]
	{ 0x8B, 0x08, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ecx,dword ptr [eax]
	{ 0x8B, 0x09, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ecx,dword ptr [ecx]
	{ 0x8B, 0x0B, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ecx,dword ptr [ebx]
	{ 0x8B, 0x0D, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov ecx,dword ptr [_gpsi]
	{ 0x8B, 0x0E, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ecx,dword ptr [esi]
	{ 0x8B, 0x0F, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ecx,dword ptr [edi]

	{ 0x8B, 0x10, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edx,dword ptr [eax]
	{ 0x8B, 0x11, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edx,dword ptr [ecx]
	{ 0x8B, 0x12, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edx,dword ptr [edx]
	{ 0x8B, 0x13, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edx,dword ptr [ebx]

	{ 0x8B, 0x16, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edx,dword ptr [esi]
	{ 0x8B, 0x17, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edx,dword ptr [edi]

	{ 0x8B, 0x18, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ebx,dword ptr [eax]
	{ 0x8B, 0x19, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ebx,dword ptr [ecx]
	{ 0x8B, 0x1B, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ebx,dword ptr [ebx]
	{ 0x8B, 0x1E, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ebx,dword ptr [edi]
	{ 0x8B, 0x1F, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ebx,dword ptr [esi]

	{ 0x8B, 0x30, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [eax]
	{ 0x8B, 0x31, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [ecx]
	{ 0x8B, 0x32, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [edx]
	{ 0x8B, 0x33, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [ebx]
	{ 0x8B, 0x34, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [edi+eax], 3rd byte determines ptr
	{ 0x8B, 0x35, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [_gpsi]
	{ 0x8B, 0x36, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [esi]
	{ 0x8B, 0x37, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [edi]

	{ 0x8B, 0x38, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edi,dword ptr [eax]
	{ 0x8B, 0x39, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edi,dword ptr [ecx]
	{ 0x8B, 0x3B, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edi,dword ptr [ebx]
	{ 0x8B, 0x3E, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edi,dword ptr [esi]
	{ 0x8B, 0x3F, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edi,dword ptr [edi]

	{ 0x8B, 0x40, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [eax+rawbyte]
	{ 0x8B, 0x41, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [ecx+rawbyte]
	{ 0x8B, 0x42, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [edx+rawbyte]
	{ 0x8B, 0x43, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [ebx+rawbyte]
	{ 0x8B, 0x44, 0x00, 0x00, 4, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [esp+rawbyte]
	{ 0x8B, 0x45, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [ebp+rawbyte]
	{ 0x8B, 0x46, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [esi+rawbyte]
	{ 0x8B, 0x47, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [edi+rawbyte]

	{ 0x8B, 0x48, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov ecx,dword ptr [eax+rawbyte]
	{ 0x8B, 0x49, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov ecx,dword ptr [ecx+rawbyte]
	{ 0x8B, 0x4A, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov ecx,dword ptr [edx+rawbyte]
	{ 0x8B, 0x4B, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov ecx,dword ptr [ebx+rawbyte]
	{ 0x8B, 0x4C, 0x00, 0x00, 4, 2, k_ENoRelativeOffsets }, // mov ecx,dword ptr [esp+rawbyte]
	{ 0x8B, 0x4D, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov ecx,dword ptr [ebp+rawbyte]
	{ 0x8B, 0x4E, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov ecx,dword ptr [esi+rawbyte]
	{ 0x8B, 0x4F, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov ecx,dword ptr [edi+rawbyte]

	{ 0x8B, 0x50, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov edx,dword ptr [eax+rawbyte]
	{ 0x8B, 0x51, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov edx,dword ptr [ecx+rawbyte]
	{ 0x8B, 0x52, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov edx,dword ptr [edx+rawbyte]
	{ 0x8B, 0x53, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov edx,dword ptr [ebx+rawbyte]
	{ 0x8B, 0x54, 0x00, 0x00, 4, 2, k_ENoRelativeOffsets }, // mov edx,dword ptr [esp+rawbyte]
	{ 0x8B, 0x55, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov edx,dword ptr [ebp+rawbyte]
	{ 0x8B, 0x56, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov edx,dword ptr [esi+rawbyte]
	{ 0x8B, 0x57, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov edx,dword ptr [edi+rawbyte]

	{ 0x8B, 0x58, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov ebx,dword ptr [eax+rawbyte]
	{ 0x8B, 0x59, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov ebx,dword ptr [ecx+rawbyte]
	{ 0x8B, 0x5A, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov ebx,dword ptr [edx+rawbyte]
	{ 0x8B, 0x5B, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov ebx,dword ptr [ebx+rawbyte]
	{ 0x8B, 0x5C, 0x00, 0x00, 4, 2, k_ENoRelativeOffsets }, // mov ebx,dword ptr [esp+rawbyte]
	{ 0x8B, 0x5D, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov ebx,dword ptr [ebp+rawbyte]
	{ 0x8B, 0x5E, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov ebx,dword ptr [esi+rawbyte]
	{ 0x8B, 0x5F, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov ebx,dword ptr [edi+rawbyte]

	{ 0x8B, 0x60, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov esp,dword ptr [eax+rawbyte]
	{ 0x8B, 0x61, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov esp,dword ptr [ecx+rawbyte]
	{ 0x8B, 0x62, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov esp,dword ptr [edx+rawbyte]
	{ 0x8B, 0x63, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov esp,dword ptr [ebx+rawbyte]
	{ 0x8B, 0x64, 0x00, 0x00, 4, 2, k_ENoRelativeOffsets }, // mov esp,dword ptr [esp+rawbyte]
	{ 0x8B, 0x65, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov esp,dword ptr [ebp+rawbyte]
	{ 0x8B, 0x66, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov esp,dword ptr [esi+rawbyte]
	{ 0x8B, 0x67, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov esp,dword ptr [edi+rawbyte]

	{ 0x8B, 0x68, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov ebp,dword ptr [eax+rawbyte]
	{ 0x8B, 0x69, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov ebp,dword ptr [ecx+rawbyte]
	{ 0x8B, 0x6A, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov ebp,dword ptr [edx+rawbyte]
	{ 0x8B, 0x6B, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov ebp,dword ptr [ebx+rawbyte]
	{ 0x8B, 0x6C, 0x00, 0x00, 4, 2, k_ENoRelativeOffsets }, // mov ebp,dword ptr [esp+rawbyte]
	{ 0x8B, 0x6D, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov ebp,dword ptr [ebp+rawbyte]
	{ 0x8B, 0x6E, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov ebp,dword ptr [esi+rawbyte]
	{ 0x8B, 0x6F, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov ebp,dword ptr [edi+rawbyte]

	{ 0x8B, 0x70, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [eax+rawbyte]
	{ 0x8B, 0x71, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [ecx+rawbyte]
	{ 0x8B, 0x72, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [edx+rawbyte]
	{ 0x8B, 0x73, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [ebx+rawbyte]
	{ 0x8B, 0x74, 0x00, 0x00, 4, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [esp+rawbyte]
	{ 0x8B, 0x75, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [ebp+rawbyte]
	{ 0x8B, 0x76, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [esi+rawbyte]
	{ 0x8B, 0x77, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [edi+rawbyte]

	{ 0x8B, 0x78, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov edi,dword ptr [eax+rawbyte]
	{ 0x8B, 0x79, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov edi,dword ptr [ecx+rawbyte]
	{ 0x8B, 0x7A, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov edi,dword ptr [edx+rawbyte]
	{ 0x8B, 0x7B, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov edi,dword ptr [ebx+rawbyte]
	{ 0x8B, 0x7C, 0x00, 0x00, 4, 2, k_ENoRelativeOffsets }, // mov edi,dword ptr [esp+rawbyte]
	{ 0x8B, 0x7D, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov edi,dword ptr [ebp+rawbyte]
	{ 0x8B, 0x7E, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov edi,dword ptr [esi+rawbyte]
	{ 0x8B, 0x7F, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov edi,dword ptr [edi+rawbyte]

	{ 0x8B, 0x80, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [eax+rawdword]
	{ 0x8B, 0x81, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [ecx+rawdword]
	{ 0x8B, 0x82, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [edx+rawdword]
	{ 0x8B, 0x83, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [ebx+rawdword]
	{ 0x8B, 0x84, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [esp+rawdword]
	{ 0x8B, 0x85, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [ebp+rawdword]
	{ 0x8B, 0x86, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [esi+rawdword]
	{ 0x8B, 0x87, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [edi+rawdword]

	{ 0x8B, 0x88, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov ecx,dword ptr [eax+rawdword]
	{ 0x8B, 0x89, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov ecx,dword ptr [ecx+rawdword]
	{ 0x8B, 0x8A, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov ecx,dword ptr [edx+rawdword]
	{ 0x8B, 0x8B, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov ecx,dword ptr [ebx+rawdword]
	{ 0x8B, 0x8C, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov ecx,dword ptr [esp+rawdword]
	{ 0x8B, 0x8D, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov ecx,dword ptr [ebp+rawdword]
	{ 0x8B, 0x8E, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov ecx,dword ptr [esi+rawdword]
	{ 0x8B, 0x8F, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov ecx,dword ptr [edi+rawdword]

	{ 0x8B, 0x90, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov edx,dword ptr [eax+rawdword]
	{ 0x8B, 0x91, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov edx,dword ptr [ecx+rawdword]
	{ 0x8B, 0x92, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov edx,dword ptr [edx+rawdword]
	{ 0x8B, 0x93, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov edx,dword ptr [ebx+rawdword]
	{ 0x8B, 0x94, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov edx,dword ptr [esp+rawdword]
	{ 0x8B, 0x95, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov edx,dword ptr [ebp+rawdword]
	{ 0x8B, 0x96, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov edx,dword ptr [esi+rawdword]
	{ 0x8B, 0x97, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov edx,dword ptr [edi+rawdword]

	{ 0x8B, 0x98, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov ebx,dword ptr [eax+rawdword]
	{ 0x8B, 0x99, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov ebx,dword ptr [ecx+rawdword]
	{ 0x8B, 0x9A, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov ebx,dword ptr [edx+rawdword]
	{ 0x8B, 0x9B, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov ebx,dword ptr [ebx+rawdword]
	{ 0x8B, 0x9C, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov ebx,dword ptr [esp+rawdword]
	{ 0x8B, 0x9D, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov ebx,dword ptr [ebp+rawdword]
	{ 0x8B, 0x9E, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov ebx,dword ptr [esi+rawdword]
	{ 0x8B, 0x9F, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov ebx,dword ptr [edi+rawdword]

	{ 0x8B, 0xB0, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [eax+rawdword]
	{ 0x8B, 0xB1, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [ecx+rawdword]
	{ 0x8B, 0xB2, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [edx+rawdword]
	{ 0x8B, 0xB3, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [ebx+rawdword]
	{ 0x8B, 0xB4, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [esp+rawdword]
	{ 0x8B, 0xB5, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [ebp+rawdword]
	{ 0x8B, 0xB6, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [esi+rawdword]
	{ 0x8B, 0xB7, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov esi,dword ptr [edi+rawdword]

	{ 0x8B, 0xB8, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov edi,dword ptr [eax+rawdword]
	{ 0x8B, 0xB9, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov edi,dword ptr [ecx+rawdword]
	{ 0x8B, 0xBA, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov edi,dword ptr [edx+rawdword]
	{ 0x8B, 0xBB, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov edi,dword ptr [ebx+rawdword]
	{ 0x8B, 0xBC, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov edi,dword ptr [ebx+rawdword]
	{ 0x8B, 0xBD, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov edi,dword ptr [ebp+rawdword]
	{ 0x8B, 0xBE, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov edi,dword ptr [esi+rawdword]
	{ 0x8B, 0xBF, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov edi,dword ptr [edi+rawdword]

	{ 0x8B, 0xC0, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov eax,eax
	{ 0x8B, 0xC1, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov eax,ecx
	{ 0x8B, 0xC2, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov eax,edx
	{ 0x8B, 0xC3, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov eax,ebx
	{ 0x8B, 0xC4, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov eax,esp
	{ 0x8B, 0xC5, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov eax,ebp
	{ 0x8B, 0xC6, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov eax,esi
	{ 0x8B, 0xC7, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov eax,edi
	
	{ 0x8B, 0xC8, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ecx,eax
	{ 0x8B, 0xC9, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ecx,ecx
	{ 0x8B, 0xCA, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ecx,edx
	{ 0x8B, 0xCB, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ecx,ebx
	{ 0x8B, 0xCC, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ecx,esp
	{ 0x8B, 0xCD, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ecx,ebp
	{ 0x8B, 0xCE, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ecx,esi
	{ 0x8B, 0xCF, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ecx,edi

	{ 0x8B, 0xD0, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edx,eax
	{ 0x8B, 0xD1, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edx,ecx
	{ 0x8B, 0xD2, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edx,edx
	{ 0x8B, 0xD3, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edx,ebx
	{ 0x8B, 0xD4, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edx,esp
	{ 0x8B, 0xD5, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edx,ebp
	{ 0x8B, 0xD6, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edx,esi
	{ 0x8B, 0xD7, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edx,edi

	{ 0x8B, 0xD8, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ebx,eax
	{ 0x8B, 0xD9, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ebx,ecx
	{ 0x8B, 0xDA, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ebx,edx
	{ 0x8B, 0xDB, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ebx,ebx
	{ 0x8B, 0xDC, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ebx,ebx
	{ 0x8B, 0xDD, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ebx,ebp
	{ 0x8B, 0xDE, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ebx,esi
	{ 0x8B, 0xDF, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ebx,edi

	{ 0x8B, 0xE0, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov esp,eax
	{ 0x8B, 0xE1, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov esp,ecx
	{ 0x8B, 0xE2, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov esp,edx
	{ 0x8B, 0xE3, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov esp,ebx
	{ 0x8B, 0xE4, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov esp,ebx
	{ 0x8B, 0xE5, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov esp,ebp
	{ 0x8B, 0xE6, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov esp,esi
	{ 0x8B, 0xE7, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov esp,edi

	{ 0x8B, 0xE8, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ebp,eax
	{ 0x8B, 0xE9, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ebp,eax
	{ 0x8B, 0xEA, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ebp,edx
	{ 0x8B, 0xEB, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ebp,ebx
	{ 0x8B, 0xEC, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ebp,esp
	{ 0x8B, 0xED, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ebp,ebp
	{ 0x8B, 0xEE, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ebp,esi
	{ 0x8B, 0xEF, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov ebp,edi

	{ 0x8B, 0xD3, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edx,ebx
	{ 0x8B, 0xD5, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edx,ebp
	{ 0x8B, 0xD6, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edx,esi
	{ 0x8B, 0xD7, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edx,edi

	{ 0x8B, 0xF0, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov esi,eax
	{ 0x8B, 0xF1, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov esi,ecx
	{ 0x8B, 0xF2, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov esi,edx
	{ 0x8B, 0xF3, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov esi,ebx
	{ 0x8B, 0xF4, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov esi,esp
	{ 0x8B, 0xF5, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov esi,ebp
	{ 0x8B, 0xF6, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov esi,esi
	{ 0x8B, 0xF7, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov esi,edi

	{ 0x8B, 0xF8, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edi,eax
	{ 0x8B, 0xF9, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edi,ecx
	{ 0x8B, 0xFA, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edi,edx
	{ 0x8B, 0xFB, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edi,ebx
	{ 0x8B, 0xFC, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edi,esp
	{ 0x8B, 0xFD, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edi,ebp
	{ 0x8B, 0xFE, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edi,esi
	{ 0x8B, 0xFF, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edi,edi
	
	{ 0x8D, 0x44, 0x24, 0x00, 4, 3, k_ENoRelativeOffsets }, // lea eax,[esp+imm8] 
	{ 0x8D, 0x45, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // lea eax,[ebp+imm8] 
	{ 0x8D, 0x4C, 0x24, 0x00, 4, 3, k_ENoRelativeOffsets }, // lea ecx,[esp+imm8] 
	{ 0x8D, 0x64, 0x24, 0x00, 4, 3, k_ENoRelativeOffsets }, // lea esp,[esp+imm8] 
	{ 0x8D, 0xA4, 0x24, 0x00, 7, 3, k_ENoRelativeOffsets }, // lea esp,[esp+imm32] 
	{ 0x8D, 0xAC, 0x24, 0x00, 7, 3, k_ENoRelativeOffsets }, // lea ebp,[esp+imm32] 
	
	{ 0x90, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // nop 
	{ 0x97, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // xchg eax,edi
	{ 0x9C, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // pushfd
	{ 0x9D, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // popfd

	{ 0xA0, 0x00, 0x00, 0x00, 5, 1, k_ENoRelativeOffsets }, // mov al,byte ptr ds:[imm32]

	{ 0xB9, 0x00, 0x00, 0x00, 5, 1, k_ENoRelativeOffsets }, // mov immediate doubleword into ecx
	{ 0xBA, 0x00, 0x00, 0x00, 5, 1, k_ENoRelativeOffsets }, // mov immediate doubleword into edx
	{ 0xBB, 0x00, 0x00, 0x00, 5, 1, k_ENoRelativeOffsets }, // mov immediate doubleword into ebx
	{ 0xBC, 0x00, 0x00, 0x00, 5, 1, k_ENoRelativeOffsets }, // mov immediate doubleword into esp
	{ 0xBD, 0x00, 0x00, 0x00, 5, 1, k_ENoRelativeOffsets }, // mov immediate doubleword into ebp
	{ 0xBE, 0x00, 0x00, 0x00, 5, 1, k_ENoRelativeOffsets }, // mov immediate doubleword into esi
	{ 0xB8, 0x00, 0x00, 0x00, 5, 1, k_ENoRelativeOffsets }, // mov immediate doubleword into eax
	{ 0xBF, 0x00, 0x00, 0x00, 5, 1, k_ENoRelativeOffsets }, // mov immediate doubleword into edi
	{ 0xA1, 0x00, 0x00, 0x00, 5, 1, k_ENoRelativeOffsets }, // mov eax, dword ptr
	{ 0xA2, 0x00, 0x00, 0x00, 5, 1, k_ENoRelativeOffsets }, // mov byte ptr, al 
	{ 0xA3, 0x00, 0x00, 0x00, 5, 1, k_ENoRelativeOffsets }, // mov dword ptr, eax 
	{ 0xC3, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // ret
	{ 0xC7, 0x05, 0x00, 0x00, 10, 1, k_ENoRelativeOffsets }, // mov dword ptr ds:[dword],dword 
	{ 0xC9, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets },	// leave
	{ 0xCC, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // int3 

	{ 0xD0, 0x00, 0x00, 0x00, 2, 1, k_ENoRelativeOffsets }, // shr, sar, or rcr (shift right style operations on registers)

	// 0xF0 is the lock prefix
	{ 0xF0, 0x0F, 0xBA, 0x2D, 9, 4, k_ENoRelativeOffsets }, // lock bts dword ptr ds:[dword], imm byte

	{ 0xFA, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // cli
	{ 0xF8,	0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // clc
	{ 0xFC, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // cld
	{ 0xFF, 0x15, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // call dword ptr ds:[imm32]
	{ 0xFF, 0x48, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // dec dword ptr [eax+imm8] 
	{ 0xFF, 0x61, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // jmp dword ptr [ecx+imm8] 
	{ 0xFF, 0x74, 0x00, 0x00, 4, 2, k_ENoRelativeOffsets }, // push dword ptr
	{ 0xFF, 0x75, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // push dword ptr type 2
	{ 0xFF, 0x25, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // jmp dword ptr -- this is often at the start of win32 api functions that are just stubs to call some __imp__func call
	
	{ 0xE8, 0x00, 0x00, 0x00, 5, 1, k_EDWORDOffsetAtByteTwo }, // call DWORD rel
	{ 0xE9, 0x00, 0x00, 0x00, 5, 1, k_EDWORDOffsetAtByteTwo }, // jmp DWORD rel
	{ 0xEB, 0x00, 0x00, 0x00, 2, 1, k_EBYTEOffsetAtByteTwo }, // jmp byte rel

#else
	//
	// 64 bit specific opcodes
	//
	{ 0x0F, 0x1F, 0x00, 0x00, 3, 3,	k_ENoRelativeOffsets }, // nop dword ptr[rax] (canonical 3-byte NOP)
	{ 0x0F, 0x1F, 0x40, 0x00, 4, 3,	k_ENoRelativeOffsets }, // nop dword ptr[rax+imm8] (canonical 4-byte NOP)
	{ 0x0F, 0x1F, 0x44, 0x00, 5, 3,	k_ENoRelativeOffsets }, // nop dword ptr[rax+rax+imm8] (canonical 5-byte NOP)
	{ 0x0F, 0x1F, 0x80, 0x00, 7, 3,	k_ENoRelativeOffsets }, // nop dword ptr[rax+0x0] (canonical 7-byte NOP)
	{ 0x0F, 0xB6, 0x53, 0x00, 4, 3, k_ENoRelativeOffsets }, // movzx edx,byte ptr[rbx+byte]	
	{ 0x33, 0xD2, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // xor edx,edx

	// 0x40 indicates 64bit operands
	{ 0x40, 0x50, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push rax
	{ 0x40, 0x51, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push rcx
	{ 0x40, 0x52, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push rdx
	{ 0x40, 0x53, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push rbx
	{ 0x40, 0x54, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push rsp
	{ 0x40, 0x55, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push rbp
	{ 0x40, 0x56, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push rsi

	{ 0x41, 0x50, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push r8
	{ 0x41, 0x51, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push r9
	{ 0x41, 0x52, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push r10
	{ 0x41, 0x53, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push r11
	{ 0x41, 0x54, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push r12
	{ 0x41, 0x55, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push r13
	{ 0x41, 0x56, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push r14
	{ 0x41, 0x57, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push r15
	{ 0x41, 0x58, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // pop r8
	{ 0x41, 0x59, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // pop r9
	{ 0x41, 0x5A, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // pop r10
	{ 0x41, 0x5B, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // pop r11
	{ 0x41, 0x5C, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // pop r12
	{ 0x41, 0x5D, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // pop r13
	{ 0x41, 0x5E, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // pop r14
	{ 0x41, 0x5F, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // pop r15

	{ 0x41, 0x8B, 0xC0, 0x00, 3, 3, k_ENoRelativeOffsets }, // mov eax,r8d 
	{ 0x41, 0x8B, 0xD8, 0x00, 3, 3, k_ENoRelativeOffsets }, // mov ebx,r8d
	
	{ 0x41, 0xB0, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov r8b, imm8
	{ 0x41, 0xB1, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov r9b, imm8
	
	{ 0x41, 0xB8, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov r8d, imm32
	{ 0x41, 0xB9, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // mov r9d, imm32
	
	// 44 is a prefix that indicates the mod r/m field is extended
	{ 0x44, 0x89, 0x44, 0x00, 5, 3, k_ENoRelativeOffsets }, // mov dword ptr [rsp+...], reg
	{ 0x44, 0x8D, 0x42, 0x00, 4, 3, k_ENoRelativeOffsets }, // lea r8d[rdx+...]

	{ 0x45, 0x33, 0xC0, 0x00, 3, 3, k_ENoRelativeOffsets }, // xor r8d,r8d
	{ 0x45, 0x33, 0xC9, 0x00, 3, 3, k_ENoRelativeOffsets }, // xor r9d,r9d

	// 48 is a prefix that indicates the operation takes 64 bit operands
	{ 0x48, 0x81, 0xEC, 0x00, 7, 3, k_ENoRelativeOffsets }, // sub rsp, imm32

	{ 0x48, 0x63, 0xC9, 0x00, 3, 3, k_ENoRelativeOffsets }, // movsxd rcx,ecx
	{ 0x48, 0x63, 0xD2, 0x00, 3, 3, k_ENoRelativeOffsets }, // movsxd rdx,edx

	{ 0x48, 0x83, 0x64, 0x00, 6, 3, k_ENoRelativeOffsets }, // and qword ptr [rsp+...], immediate
	{ 0x48, 0x83, 0xEC, 0x00, 4, 3, k_ENoRelativeOffsets }, // sub rsp, immediate
	{ 0x48, 0x83, 0xE9, 0x00, 4, 3, k_ENoRelativeOffsets }, // sub rcx, immediate byte
	{ 0x48, 0x83, 0xC1, 0x00, 4, 3, k_ENoRelativeOffsets }, // add rcx, immediate byte

	{ 0x48, 0x85, 0xC0, 0x00, 3, 3, k_ENoRelativeOffsets }, // test rax,rax
	{ 0x48, 0x85, 0xC9, 0x00, 3, 3, k_ENoRelativeOffsets }, // test rcx,rcx
	{ 0x48, 0x85, 0xD2, 0x00, 3, 3, k_ENoRelativeOffsets }, // text rdx,rdx

	{ 0x48, 0x89, 0x4C, 0x00, 5, 3, k_ENoRelativeOffsets }, // mov qword ptr[...+...+imm byte],rcx
	{ 0x48, 0x89, 0x54, 0x00, 5, 3, k_ENoRelativeOffsets }, // mov qword ptr[rsp+...],rdx
	{ 0x48, 0x89, 0x58, 0x00, 4, 3, k_ENoRelativeOffsets }, // mov qword ptr[rax+...],rbx
	{ 0x48, 0x89, 0x5C, 0x00, 5, 3, k_ENoRelativeOffsets }, // mov qword ptr[rsp+...], reg
	{ 0x48, 0x89, 0x68, 0x00, 4, 3, k_ENoRelativeOffsets }, // mov qword ptr[rax+...],rbp
	{ 0x48, 0x89, 0x6C, 0x00, 5, 3, k_ENoRelativeOffsets }, // mov qword ptr[rsp+...],rbp
	{ 0x48, 0x89, 0x70, 0x00, 4, 3, k_ENoRelativeOffsets }, // mov qword ptr[rax+...],rsi
	{ 0x48, 0x89, 0x74, 0x00, 5, 3, k_ENoRelativeOffsets }, // mov qword ptr[rsp+...],rsi

	{ 0x48, 0x8B, 0x01, 0x00, 3, 3, k_ENoRelativeOffsets }, // mov rax,qword ptr [rcx]
	{ 0x48, 0x8B, 0x04, 0x24, 4, 4, k_ENoRelativeOffsets }, // mov rax,qword ptr [rsp]
	{ 0x48, 0x8B, 0x44, 0x00, 5, 3, k_ENoRelativeOffsets }, // mov rax,qword ptr[...+...+imm byte]
	{ 0x48, 0x8B, 0x49, 0x00, 4, 3, k_ENoRelativeOffsets }, // mov rcx,qword ptr[rcx+im8]
	{ 0x48, 0x8B, 0x84, 0x00, 8, 3, k_ENoRelativeOffsets }, // mov rax,qword ptr[rsp+dword]
	{ 0x48, 0x8B, 0xC1, 0x00, 3, 3, k_ENoRelativeOffsets }, // mov rax,rcx
	{ 0x48, 0x8B, 0xC3, 0x00, 3, 3, k_ENoRelativeOffsets }, // mov ebx,r8d
	{ 0x48, 0x8B, 0xC4, 0x00, 3, 3, k_ENoRelativeOffsets }, // mov rax,rsp
	{ 0x48, 0x8B, 0xD9, 0x00, 3, 3, k_ENoRelativeOffsets }, // mov rbx,rbx
	{ 0x48, 0x8B, 0xEC, 0x00, 3, 3, k_ENoRelativeOffsets }, // mov rbp,rsp
	{ 0x48, 0x8B, 0xFA, 0x00, 3, 3, k_ENoRelativeOffsets }, // mov rdi,rdx

	{ 0x48, 0xB8, 0x00, 0x00, 10, 2, k_ENoRelativeOffsets }, // mov rax,imm64
	{ 0x48, 0xB9, 0x00, 0x00, 10, 2, k_ENoRelativeOffsets }, // mov rcx,imm64

	{ 0x48, 0xC7, 0x44, 0x00, 9, 3, k_ENoRelativeOffsets }, // mov qword ptr[rsp+...], dword immediate
	{ 0x48, 0xC7, 0xC0, 0x00, 7, 3, k_ENoRelativeOffsets }, // mov rax,dword ptr

	{ 0x48, 0x8D, 0x05, 0x00, 7, 3, k_EDWORDOffsetAtByteFour }, // lea rax, [imm dword offset]
	{ 0x48, 0x8D, 0x0D, 0x00, 7, 3, k_EDWORDOffsetAtByteFour }, // lea rcx, [imm dword offset]
	{ 0x48, 0x8D, 0x15, 0x00, 7, 3, k_EDWORDOffsetAtByteFour }, // lea rdx, [imm dword offset]
	{ 0x48, 0x8D, 0x1D, 0x00, 7, 3, k_EDWORDOffsetAtByteFour }, // lea rbx, [imm dword offset]
	{ 0x48, 0x8D, 0x44, 0x24, 5, 4, k_ENoRelativeOffsets }, // lea rax, [rsp+imm byte]
	{ 0x48, 0x8D, 0x4C, 0x24, 5, 4, k_ENoRelativeOffsets }, // lea rcx, [rsp+imm byte]
	{ 0x48, 0x8D, 0x54, 0x24, 5, 4, k_ENoRelativeOffsets }, // lea rdx, [rsp+imm byte]
	{ 0x48, 0x8D, 0x5C, 0x24, 5, 4, k_ENoRelativeOffsets }, // lea rbx, [rsp+imm byte]

	{ 0x48, 0xFF, 0x25, 0x00, 7, 3, k_EDWORDOffsetAtByteFour }, // jmp QWORD PTR [rip+dword] -- RIP-relative indirect jump

	{ 0x49, 0x89, 0x5B, 0x00, 4, 3, k_ENoRelativeOffsets }, // qword ptr[r11 + byte], rbx
	{ 0x49, 0x89, 0x73, 0x00, 4, 3, k_ENoRelativeOffsets }, // qword ptr[r11 + byte], rsi
	{ 0x49, 0x8B, 0xC1, 0x00, 3, 3, k_ENoRelativeOffsets }, // qword rax, r9

	{ 0x4C, 0x3B, 0xCF, 0x00, 3, 3, k_ENoRelativeOffsets }, // cmp r9,rdi

	{ 0x4C, 0x89, 0x40, 0x00, 4, 3, k_ENoRelativeOffsets }, // mov qword ptr [rax+immediate byte],r8
	{ 0x4C, 0x89, 0x48, 0x00, 4, 3, k_ENoRelativeOffsets }, // mov qword ptr [rax+immediate byte],r9 
	{ 0x4C, 0x89, 0x44, 0x00, 5, 3, k_ENoRelativeOffsets }, // mov qword ptr [rsp+imm byte],r8
	{ 0x4C, 0x89, 0x4C, 0x00, 5, 3, k_ENoRelativeOffsets }, // mov qword ptr [...+...+imm byte],r9
	{ 0x4C, 0x8B, 0xC2, 0x00, 3, 3, k_ENoRelativeOffsets }, // mov r8,rdx
	{ 0x4C, 0x8B, 0xD1, 0x00, 3, 3, k_ENoRelativeOffsets }, // mov r10,rcx
	{ 0x4C, 0x8B, 0xDC, 0x00, 3, 3, k_ENoRelativeOffsets }, // mov r11,rsp
	{ 0x4C, 0x8D, 0x44, 0x00, 5, 3, k_ENoRelativeOffsets }, // lea reg,[rsp+...]

	{ 0x4D, 0x85, 0xC0, 0x00, 3, 3, k_ENoRelativeOffsets }, // test r8,r8
	{ 0x4D, 0x85, 0xC9, 0x00, 3, 3, k_ENoRelativeOffsets }, // test r9,r9

	{ 0x50, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // push rax
	{ 0x51, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // push rcx
	{ 0x52, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // push rdx
	{ 0x53, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // push rbx
	{ 0x54, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // push rsp
	{ 0x55, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // push rbp
	{ 0x56, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // push rsi
	{ 0x57, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // push rdi

	// 0x64 and 0x65 are prefixes for FS or GS relative memory addressing
	{ 0x64, 0x48, 0x89, 0x04, 5, 4, k_ENoRelativeOffsets }, // mov qword ptr fs:[register-based offset], rax
	{ 0x65, 0x48, 0x8b, 0x00, 9, 3, k_ENoRelativeOffsets }, // mov reg,qword ptr gs:[dword]

	{ 0x66, 0x90, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // xchg ax,ax  - canonical 2-byte NOP
	{ 0x66, 0x0F, 0x1F, 0x44, 6, 4, k_ENoRelativeOffsets }, // nop word ptr[rax+...] - canonical 6-byte NOP

	{ 0x81, 0x3A, 0x00, 0x00, 6, 2, k_ENoRelativeOffsets }, // cmp prt[rdx], 4 bytes

	{ 0x89, 0x70, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov dword ptr[rax+...],esi
	{ 0x89, 0x4C, 0x00, 0x00, 4, 2, k_ENoRelativeOffsets }, // mov dword ptr[rsp+...],ecx

	{ 0x8B, 0x40, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [rax+rawbyte]
	{ 0x8B, 0x41, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [rcx+rawbyte]
	{ 0x8B, 0x42, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [rdx+rawbyte]
	{ 0x8B, 0x43, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [rbx+rawbyte]
	{ 0x8B, 0x44, 0x00, 0x00, 4, 2, k_ENoRelativeOffsets },	// mov eax,dword ptr [rsp+rawbyte] 
	{ 0x8B, 0x45, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [rbp+rawbyte]
	{ 0x8B, 0x46, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [rsi+rawbyte]
	{ 0x8B, 0x47, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // mov eax,dword ptr [rdi+rawbyte]

	{ 0x8B, 0xC0, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov eax,eax
	{ 0x8B, 0xC1, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov eax,ecx
	{ 0x8B, 0xC2, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov eax,edx
	{ 0x8B, 0xC3, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov eax,ebx
	{ 0x8B, 0xC5, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov eax,ebp
	{ 0x8B, 0xC6, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov eax,esi
	{ 0x8B, 0xC7, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov eax,edi
	
	{ 0x8B, 0xD3, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edx,ebx
	{ 0x8B, 0xD5, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edx,ebp
	// bugbug jmccaskey - the below is correct, but will break detours of LoadLibraryExW on x64.  
	// need to work out exactly what is wrong there, right now we work fine failing to detour it, and
	// there seems to be some set of interlinking stubs we may double detour between LoadLibarExA/W
	// somehow.
	//{ 0x8B, 0xD6, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edx,esi
	{ 0x8B, 0xD7, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // mov edx,edi

	{ 0xB8, 0x00, 0x00, 0x00, 5, 1, k_ENoRelativeOffsets }, // mov eax, immediate dword
	{ 0xB9, 0x00, 0x00, 0x00, 5, 1, k_ENoRelativeOffsets }, // mov ecx, immediate dword
	{ 0xBA, 0x00, 0x00, 0x00, 5, 1, k_ENoRelativeOffsets }, // mov edx, immediate dword

	{ 0xE8, 0x00, 0x00, 0x00, 5, 1, k_EDWORDOffsetAtByteTwo }, // call DWORD rel
	{ 0xE9, 0x00, 0x00, 0x00, 5, 1, k_EDWORDOffsetAtByteTwo }, // jmp DWORD rel
	{ 0xEB, 0x00, 0x00, 0x00, 2, 1, k_EBYTEOffsetAtByteTwo }, // jmp BYTE rel
	{ 0x90, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // nop
	{ 0xCC, 0x00, 0x00, 0x00, 1, 1, k_ENoRelativeOffsets }, // int 3

	// F0 is the lock prefix
	{ 0xF0, 0x83, 0x41, 0x00, 5, 3, k_ENoRelativeOffsets }, // lock add dword ptr[rcx+...], immediate
	{ 0xF0, 0x83, 0x05, 0x00, 8, 3, k_EDWORDOffsetAtByteFour }, // lock add dword ptr[rel], immediate byte

	{ 0xF6, 0xC1, 0x00, 0x00, 3, 2, k_ENoRelativeOffsets }, // test c1,byte
	
	{ 0xFF, 0x25, 0x00, 0x00, 6, 2, k_EDWORDOffsetAtByteThree }, // jmp dword offset
	{ 0xFF, 0xE0, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // jmp rax

	{ 0xFF, 0xF0, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push rax
	{ 0xFF, 0xF1, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push rcx
	{ 0xFF, 0xF2, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push rdx
	{ 0xFF, 0xF3, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push rbx
	{ 0xFF, 0xF4, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push rsp
	{ 0xFF, 0xF5, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push rbp
	{ 0xFF, 0xF6, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push rsi
	{ 0xFF, 0xF7, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push rdi
	{ 0xFF, 0xF8, 0x00, 0x00, 2, 2, k_ENoRelativeOffsets }, // push rsp



#endif
};


//-----------------------------------------------------------------------------
// Purpose: tries to parse the opcode and returns length in bytes if known
//-----------------------------------------------------------------------------
bool ParseOpcode( unsigned char *pOpcode, int &nLength, EOpCodeOffsetType &eOffsetType )
{
	for ( int i=0; i < sizeof(s_rgKnownOpCodes)/sizeof(s_rgKnownOpCodes[0]); i++ )
	{
		const KnownOpCode_t &opcode = s_rgKnownOpCodes[i];

		if ( pOpcode[0] == opcode.m_OpCodeB1 )
		{
			if ( opcode.m_cOpCodeBytesToMatch < 2 || pOpcode[1] == opcode.m_OpCodeB2 )
			{
				if ( opcode.m_cOpCodeBytesToMatch < 3 || pOpcode[2] == opcode.m_OpCodeB3 )
				{
					if ( opcode.m_cOpCodeBytesToMatch < 4 || pOpcode[3] == opcode.m_OpCodeB4 )
					{
						nLength = opcode.m_TotalLength;
						eOffsetType = opcode.m_EOffsetType;
						return true;
					}
				}
			}
		}
	}

	return false;
}

// New disassembler code

typedef unsigned char uint8;

// data from http://www.intel.com/design/processor/manuals/253667.pdf appendix A pages A-10 and up

// Table Key
// 0-9 number of bytes of data following opcode
// specials:
#define opcMODRMBYTE 0x10
#define M0 0x10 // ModRM byte that cannot have extra displacement
#define Mb 0x10 // ModRM byte that may have extra byte displcement
#define Mw 0x30 // ModRM byte that may have extra dword displacement
#define M1 0x11 // ModRM byte ( with possible displacement ) PLUS 1 byte immediate
#define M4 0x34 // ModRM byte ( with possible displacement ) PLUS 2 byte immediate
#define MF 0x70 // ModRM byte that defines opcode
#define UU 0x80 // I dont understand this instruction - disassembly will fail
#define opcRELATIVEJUMP 0x100
#define opcUNCONDITIONALJUMP 0x400
#define R1 0x301 // conditional relative jump/call - 1 byte offset
#define R4 0x304 // conditional relative jump/call - 4 byte offset
#define C1 0x201 // absolute jump/call - 1 byte offset ?
#define C4 0x204 // absolute jump/call - 4 byte offset ?
#define uC6 0x206 // absolute jump/call - 6 byte offset
#define CM 0x230 // jump with modrm byte ( probably jmp [eax] or jmp [eax+stuff]

#define uR1 0x701 // unconditional relative jump - 1 byte offset
#define uR4 0x704 // unconditional relative jump - 4 byte offset
#define uJ1 0x701 // unconditional jump - 1 byte offset
#define uJ4 0x704 // unconditional jump - 4 byte offset

#define opcRETURN 0x800
#define E0 0x800 // return ( a jump we cant follow )
#define E2 0x802 // return ( a jump we cant follow )
#define XC 0x1000 // coprocessor instruction - floating point - disassembly will fail
#define XX 0x2000 // start of 2 byte opcode
#define IS 0x4000
#define opcOPERANDSIZEOVERRIDE 0x4000
#define AS 0x8000
#define MP 0x3000 // meaningless prefix
#define MQ 0x5000 // meaningless prefix

// 1 byte opcode map
int rgOpData[256]=
{
//     0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
/*0*/ Mb, Mw, Mb, Mw,  1,  4,  0,  0, Mb, Mw, Mb, Mw,  1,  4,  0, XX,
/*1*/ Mb, Mw, Mb, Mw,  1,  4,  0,  0, Mb, Mw, Mb, Mw,  1,  4,  0,  0,
/*2*/ Mb, Mw, Mb, Mw,  1,  4,  0,  0, Mb, Mw, Mb, Mw,  1,  4,  0,  0,
/*3*/ Mb, Mw, Mb, Mw,  1,  4,  0,  0, Mb, Mw, Mb, Mw,  1,  4,  0,  0,
/*4*/  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
/*5*/  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
/*6*/  0,  0, UU, M0,  0,  0, IS, UU,  4, M4,  1, M1,  0,  0,  0,  0,
/*7*/ R1, R1, R1, R1, R1, R1, R1, R1, R1, R1, R1, R1, R1, R1, R1, R1, // relative jumps
/*8*/ M1, M4, M1, M1, Mb, Mw, Mb, Mw, Mb, Mw, Mb, Mw, Mw, Mw, Mw, Mw, 
/*9*/  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, C4,  0,  0,  0,  0,  0, 
/*a*/  4,  4,  4,  4,  0,  0,  0,  0,  1,  4,  0,  0,  0,  0,  0,  0,
/*b*/  1,  1,  1,  1,  1,  1,  1,  1,  4,  4,  4,  4,  4,  4,  4,  4,
/*c*/ M1, M1, E2, E0, UU, UU, M1, M4,  5,  0,  2,  0,  0,  1,  0,  0,
/*d*/  1,  1,  1,  1,  1,  1,  1,  1, XC, XC, XC, XC, XC, XC, XC, XC,
/*e*/ R1, R1, R1, R1,  1,  1,  1,  1, R4,uR4,uC6,uR1,  0,  0,  0,  0,
/*f*/ MP,  0, MP, MP,  0,  0, MF, MF,  0,  0,  0,  0,  0,  0, UU, CM
};
// e8 = call
// e9 = jump
// f2/f3 prefixes only a small subset of instructions - otherwise its part of a 3 byte opcode
// A6 A7 AA AB AD AE


// 2 byte opcode map
int rgOpData2[256]=
{
//     0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
/*0*/ UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU,
/*1*/ UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU,
/*2*/ UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU,
/*3*/  0,  0,  0,  0,  0,  0, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU,
/*4*/ Mw, Mw, Mw, Mw, Mw, Mw, Mw, Mw, Mw, Mw, Mw, Mw, Mw, Mw, Mw, Mw,
/*5*/ UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU,
/*6*/ UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU,
/*7*/ UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU,
/*8*/ R4, R4, R4, R4, R4, R4, R4, R4, R4, R4, R4, R4, R4, R4, R4, R4, // far relative jumps
/*9*/ M0, M0, M0, M0, M0, M0, M0, M0, M0, M0, M0, M0, M0, M0, M0, M0,
/*a*/  0,  0,  0, Mw, M1, Mw, UU, UU,  0,  0,  0, Mw, M1, Mw, UU, Mw,
/*b*/ Mb, Mw, UU, Mw, UU, UU, Mb, Mw, UU, UU, UU, Mw, Mw, Mw, Mb, Mw,
/*c*/ Mb, Mw, UU, UU, UU, UU, UU, UU,  0,  0,  0,  0,  0,  0,  0,  0,
/*d*/ UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU,
/*e*/ UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU,
/*f*/ UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU
};

#ifdef VACTOOL
#define OPCODE_NAMES
#endif
#ifdef OPCODE_NAMES
char *rgOpcodeNames[256]=
{
	"add",   "add",   "add",   "add",   "add",   "add",   "push",  "pop",  "or",   "or",   "or",   "or",   "or",   "or",   "push", "",
	"adc",   "adc",   "adc",   "adc",   "adc",   "adc",   "push",  "pop",  "sbb",  "sbb",  "sbb",  "sbb",  "sbb",  "sbb",  "push", "pop",
	"and",   "and",   "and",   "and",   "and",   "and",   "sege",  "daa",  "sub",  "sub",  "sub",  "sub",  "sub",  "sub",  "segc", "das",
	"xor",   "xor",   "xor",   "xor",   "xor",   "xor",   "segs",  "aaa",  "cmp",  "cmp",  "cmp",  "cmp",  "cmp",  "cmp",  "segd", "aas", 
	"inc",   "inc",   "inc",   "inc",   "inc",   "inc",   "inc",   "inc",  "dec",  "dec",  "dec",  "dec",  "dec",  "dec",  "dec",  "dec",  
	"push",  "push",  "push",  "push",  "push",  "push",  "push",  "push", "pop",  "pop",  "pop",  "pop",  "pop",  "pop",  "pop",  "pop",  
	"pusha", "popa",  "bound", "arpl",  "segf",  "segg",  "pre",   "pre",  "push", "imul", "push", "imul", "ins",  "ins",  "outs", "outs",
	"jo",    "jno",   "jb",    "jnb",   "je",    "jne",   "jbe",   "ja",   "js",   "jns",  "jp",   "jnp",  "jl",   "jge",  "jle",  "jg",
	"",      "",      "",      "",      "test",  "test",  "xchg",  "xchg", "mov",  "mov",  "mov",  "mov",  "mov",  "lea",  "mov",  "pgp", 
	"nop",   "xchg",  "xchg",  "xchg",  "xchg",  "xchg",  "xchg",  "xchg", "cbw",  "cwd",  "call", "wait", "pushf","popf", "sahf", "lahf",
	"mov",   "mov",   "mov",   "mov",   "movs",  "movs",  "cmps",  "cmps", "test", "test", "stos", "stos", "lods", "lods", "scas", "scas",
	"mov",   "mov",   "mov",   "mov",   "mov",   "mov",   "mov",   "mov",  "mov",  "mov",  "mov",   "mov",   "mov",   "mov",   "mov",   "mov",  
	"shlr",  "shlr",  "retn",  "retn",  "les",   "lds",   "mov",   "mov",  "enter","leave", "retf", "retf", "int3", "int", "into", "iret",
	"shlr",  "shlr",  "shlr",  "shlr",  "aam",   "aad",   "",      "xlat", "",    "",    "",    "",    "",    "",    "",    "",    
	"loop",  "loop",  "loop",  "jcxz",  "in",    "in",    "out",   "out",  "call", "jmp",  "jmp",  "jmp",  "in",  "in",  "out",  "out", 
	"lock",  "",      "repne", "repe",  "hlt",   "cmc",   "unry",  "unry", "clc",  "stc",  "cli",  "sti",  "cld", "std",  "dinc", "dinc",
};
#endif

uint8 rgOpcodeValid[256]=
{
//0	"add",   "add",   "add",   "add",   "add",   "add",   "push",  "pop",  "or",   "or",   "or",   "or",   "or",   "or",   "push", "",
	1,       1,       1,       1,       1,       1,       1,       1,      1,      1,      1,      1,      1,      1,      1,      1,
//1	"adc",   "adc",   "adc",   "adc",   "adc",   "adc",   "push",  "pop",  "sbb",  "sbb",  "sbb",  "sbb",  "sbb",  "sbb",  "push", "pop",
    1,       1,       1,       1,       1,       1,       1,       1,      1,      1,      1,      1,      1,      1,      1,      1,
//2	"and",   "and",   "and",   "and",   "and",   "and",   "sege",  "daa",  "sub",  "sub",  "sub",  "sub",  "sub",  "sub",  "segc", "das",
    1,       1,       1,       1,       1,       1,       1,       0,      1,      1,      1,      1,      1,      1,      1,      0,
//3	"xor",   "xor",   "xor",   "xor",   "xor",   "xor",   "segs",  "aaa",  "cmp",  "cmp",  "cmp",  "cmp",  "cmp",  "cmp",  "segd", "aas", 
    1,       1,       1,       1,       1,       1,       1,       1,      1,      1,      1,      1,      1,      1,      1,      1,
//4	"inc",   "inc",   "inc",   "inc",   "inc",   "inc",   "inc",   "inc",  "dec",  "dec",  "dec",  "dec",  "dec",  "dec",  "dec",  "dec",  
    1,       1,       1,       1,       1,       1,       1,       1,      1,      1,      1,      1,      1,      1,      1,      1,
//5	"push",  "push",  "push",  "push",  "push",  "push",  "push",  "push", "pop",  "pop",  "pop",  "pop",  "pop",  "pop",  "pop",  "pop",  
    2,       2,       2,       2,       2,       2,       2,       2,      1,      1,      1,      1,      1,      1,      1,      1,
//6	"pusha", "popa",  "bound", "arpl",  "segf",  "segg",  "pre",   "pre",  "push", "imul", "push", "imul", "ins",  "ins",  "outs", "outs",
    1,       1,       1,       1,       1,       1,       1,       1,      2,      1,      2,      1,      0,      0,      0,      0,
//7	"jo",    "jno",   "jb",    "jnb",   "je",    "jne",   "jbe",   "ja",   "js",   "jns",  "jp",   "jnp",  "jl",   "jge",  "jle",  "jg",
    1,       1,       1,       1,       1,       1,       1,       1,      1,      1,      1,      1,      1,      1,      1,      1,
//8	"",      "",      "",      "",      "test",  "test",  "xchg",  "xchg", "mov",  "mov",  "mov",  "mov",  "mov",  "lea",  "mov",  "pgp", 
    2,       2,       2,       2,       1,       1,       1,       1,      2,      2,      2,      2,      2,      2,      2,      1,
//9	"nop",   "xchg",  "xchg",  "xchg",  "xchg",  "xchg",  "xchg",  "xchg", "cbw",  "cwd",  "call", "wait", "pushf","popf", "sahf", "lahf",
    1,       1,       1,       1,       1,       1,       1,       1,      1,      1,      1,      1,      1,      1,      1,      1,
//A	"mov",   "mov",   "mov",   "mov",   "movs",  "movs",  "cmps",  "cmps", "test", "test", "stos", "stos", "lods", "lods", "scas", "scas",
    2,       2,       2,       2,       1,       1,       1,       1,      1,      1,      1,      1,      1,      1,      1,      1,
//B	"mov",   "mov",   "mov",   "mov",   "mov",   "mov",   "mov",   "mov",  "mov",   "mov", "mov",   "mov",   "mov",   "mov",   "mov",   "mov",  
    1,       1,       1,       1,       1,       1,       1,       1,      1,      1,      1,      1,      1,      1,      1,      1,
//C	"shlr",  "shlr",  "retn",  "retn",  "les",   "lds",   "mov",   "mov",  "enter","leave","retf", "retf", "int3", "int", "into", "iret",
    1,       1,       1,       1,       1,       1,       1,       1,      1,      1,      1,      1,      0,      1,      1,      1,
//D	"shlr",  "shlr",  "shlr",  "shlr",  "aam",   "aad",   "",      "xlat", "",    "",    "",    "",    "",    "",    "",    "",    
    1,       1,       1,       1,       1,       1,       1,       0,      1,      2,      1,      1,      1,      1,      1,      1,
//	"loop",  "loop",  "loop",  "jcxz",  "in",    "in",    "out",   "out",  "call", "jmp",  "jmp",  "jmp",  "in",  "in",  "out",  "out", 
    1,       1,       1,       1,       0,       0,       0,       0,      1,      1,      1,      1,      0,      0,      0,      0,
//	"lock",  "",      "repne", "repe",  "hlt",   "cmc",   "unry",  "unry", "clc",  "stc",  "cli",  "sti",  "cld", "std",  "dinc", "dinc",
    1,       1,       1,       1,       0,       1,       1,       1,      1,      1,      1,      1,      1,      1,      1,      1,
};

//-----------------------------------------------------------------------------
// Purpose: parse a modrm byte, determine size of data
// http://www.intel.com/design/processor/manuals/253666.pdf sections 2.1.3 to 2.1.5
//-----------------------------------------------------------------------------
bool BParseModRMByte( uint8 bOpcode, int w, uint8 *pubCode, OPCODE_t *pOpcode )
{
	// 2nd byte is a modrm byte
	int mod = (pubCode[0] >> 6) & 0x03;
	int other = (pubCode[0] >> 3 ) & 0x07;
	int rm = (pubCode[0]) & 0x07;
	if ( w == MF )
	{
		// the "other" bits define the opcode - we need a full table here
		// currently bOpcode == F6/F7 and other == 0 is the only thing we care about
		// because its the only thing we extra immediate data
		// http://www.intel.com/design/processor/manuals/253667.pdf table A-6 
		// opcode extensions by group number
		if ( bOpcode == 0xF6 && other == 0 )
		{
			pOpcode->cubOpcode += 1;
		}
		else if ( bOpcode == 0xF7 && other == 0 )
		{
			pOpcode->cubOpcode += 4;
		}
	}
	if ( mod == 0x03 )
	{
		return true;
	}
	if ( rm == 0x04 )
	{
		// There is a SIB byte following
		pOpcode->cubOpcode += 1;
		int sib = pubCode[1];
		// if the bottom 3 bits of the SIB = 5
		// then there is an additional displacement
		// which depends on the mod for its size
		if ( ( sib & 0x07 ) == 0x05 )
		{
			if ( mod == 0x01 )
			{
				pOpcode->cubOpcode += 1;
				pOpcode->cubImmed += 1;
			}
			else if ( mod == 0x10 || mod == 0x00 )
			{
				pOpcode->cubOpcode += 4;
				pOpcode->cubImmed += 4;
			}
			// bail out ?
			return true;
		}
	}
	if ( mod == 0x02 )
	{
		pOpcode->cubOpcode += 4;
		pOpcode->cubImmed = 4;
	}
	else if ( mod == 0x01 )
	{
		pOpcode->cubOpcode += 1;
	}
	else if ( mod == 0 && rm == 0x05 )
	{
		pOpcode->cubOpcode += 4;
		pOpcode->cubImmed = 4;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: parse an instruction
// cub = total size of instruction and data and displacements
// bRelative = relative jump?
// uJumpDestination = jump offset if jump
// cubLeft = fail if try to disassemble more bytes than are left
//-----------------------------------------------------------------------------
bool BParseCode( int w, uint8 *pubCode, OPCODE_t *pOpcode, int cubLeft )
{
	// how much immediate data is there?
	pOpcode->cubImmed = w & 0x0f;
	if ( w == opcOPERANDSIZEOVERRIDE )
	{
		pOpcode->cubOpcode++;
		if ( pOpcode->cubOpcode > cubLeft )
			return false;
		pubCode++;
		pOpcode->bOpcode = pubCode[0];
		w = rgOpData[ pubCode[0] ];
		pOpcode->cubImmed = ( w & 0x0f );
		// override dword to word
		// leave byte alone
		if ( pOpcode->cubImmed == 4 )
			pOpcode->cubImmed = 2;
	}
	if ( w == MQ )
	{
		pOpcode->cubOpcode++;
		if ( pOpcode->cubOpcode > cubLeft )
			return false;
		// we only understand rep/ne 
		switch ( pubCode[1] )
		{
			case 0xA6:
			case 0xA7:
			case 0xAA:
			case 0xAB:
			case 0xAD:
			case 0xAE:
			case 0xAF:
			return true;
		}
		return false;
	}
	if ( w == MP )
	{
		// skip meaningless prefix
		pOpcode->cubOpcode++;
		if ( pOpcode->cubOpcode > cubLeft )
			return false;
		pubCode++;
		pOpcode->bOpcode = pubCode[0];
		w = rgOpData[ pubCode[0] ];
		pOpcode->cubImmed = ( w & 0x0f );
	}
	pOpcode->cubOpcode += pOpcode->cubImmed;
	if ( pOpcode->cubOpcode > cubLeft )
		return false;
	pOpcode->bRelative = ( w & opcRELATIVEJUMP ) ? true : false;
	pOpcode->bCantContinue = false;
	if ( w & opcRETURN || w & opcUNCONDITIONALJUMP )
		pOpcode->bCantContinue = true;
	pOpcode->bURJ = w == uR1;
	pOpcode->uJump = 0;
	pOpcode->bJumpOrCall = ( w & 0x200 ) ? true : false;
	if ( w < 0x10 )
	{
		return true;
	}
	else if ( w & opcMODRMBYTE )
	{
		// modrm byte
		pOpcode->cubOpcode ++;
		if ( pOpcode->cubOpcode > cubLeft )
			return false;
		pOpcode->bModRM = true;
		if (! BParseModRMByte( pubCode[0], w, &pubCode[1], pOpcode ) )
			return false;
		if ( pOpcode->cubOpcode > cubLeft )
			return false;
	}
	if ( w == XX )
	{
		pOpcode->cubOpcode ++;
		if ( pOpcode->cubOpcode > cubLeft )
			return false;
		w = rgOpData2[ pubCode[1] ];
		return BParseCode( w, &pubCode[1], pOpcode, cubLeft );
	}
	else if ( w == XC )
	{
		// coprocessor instruction
		pOpcode->cubOpcode ++;
		if ( pOpcode->cubOpcode > cubLeft )
			return false;
		uint8 b = pubCode[1];
		if ( b <= 0xBF )
		{
			// if < BF then its a regular modrm byte ( I think )
			// size overrides?
			pOpcode->bModRM = true;
			if (! BParseModRMByte( pubCode[0], w, &pubCode[1], pOpcode ) )
				return false;
			if ( pOpcode->cubOpcode > cubLeft )
				return false;
		}
	}
	else if ( w == UU || w == XC )
	{
		return false;
	}
	if ( pOpcode->cubImmed )
	{
		int iubImmed = pOpcode->cubOpcode - pOpcode->cubImmed;
		pOpcode->uImmed = 0;
		if ( pOpcode->cubImmed == 4 )
			pOpcode->uImmed = *(DWORD *)(&pubCode[iubImmed]);
		else if ( pOpcode->cubImmed == 2 )
			pOpcode->uImmed = *(WORD *)(&pubCode[iubImmed]);
		else if ( pOpcode->cubImmed == 1 )
			pOpcode->uImmed = pubCode[iubImmed];

		// if its a jump or call and there is no modrm byte
		// we know where its going
		// if there is a modrm/sib then it could be 
		// jmp dword ptr [edx*4+1006982ch] or something
		// and we cant eval that
		if ( pOpcode->bJumpOrCall && !pOpcode->bModRM )
			pOpcode->uJump = pOpcode->uImmed;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: disassemble one instruction
//			returns false if not understood or we run out of bytes in cubLeft
// pubCode - code to disasm
// pOpcode - return info here
// cubLeft - max bytes to look at
//-----------------------------------------------------------------------------
bool ParseCode( uint8 *pubCode, OPCODE_t *pOpcode, int cubLeft )
{
	int w = rgOpData[ pubCode[0] ];
	pOpcode->bOpcode = pubCode[0];
	pOpcode->bRelative = false;
	pOpcode->bCantContinue = false;
	pOpcode->bModRM = false;
	pOpcode->bJumpOrCall = false;
	pOpcode->bURJ = false;
	pOpcode->cubImmed = 1;
	pOpcode->cubOpcode = 1;
	pOpcode->uJump = 0;
	pOpcode->uImmed = 0;
	return BParseCode( w, pubCode, pOpcode, cubLeft );
}

uint32 ComputeJumpAddress( OPCODE_t *pOpcode, uint32 uVACurrent )
{
	if ( pOpcode->bRelative && pOpcode->uJump )
	{
		switch ( pOpcode->cubImmed )
		{
		case 1:
			{
			char bOffset = pOpcode->uJump;
			uVACurrent += bOffset;
			}
			break;
		case 2:
			{
			short wOffset = pOpcode->uJump;
			uVACurrent += wOffset;
			}
			break;
		case 4:
			{
			int nOffset = pOpcode->uJump;
			uVACurrent += nOffset;
			}
			break;
		}
		pOpcode->uJump = uVACurrent + pOpcode->cubOpcode;
	}
	return pOpcode->uJump;
}

#ifdef OPCODE_NAMES
bool OpcodeText( OPCODE_t *pOpcode, char *rgchText )
{
	strncpy( rgchText, rgOpcodeNames[pOpcode->bOpcode], 32 );
	return true;
}
#endif

bool LikelyValid( OPCODE_t *pOpcode )
{
	return rgOpcodeValid[pOpcode->bOpcode] >= 1;
}

bool LikelyNewValid( OPCODE_t *pOpcode )
{
	return rgOpcodeValid[pOpcode->bOpcode] == 2;
}

//-----------------------------------------------------------------------------
// Purpose: disassemble - return how many bytes we understood
//-----------------------------------------------------------------------------
int Disassemble( unsigned char *pubStart, int cub )
{
	unsigned char *pub = pubStart;
	int cubLeft = cub;
	int cubUnderstood = 0;
	OPCODE_t opcode;
	while ( cubLeft > 0 && ParseCode( pub, &opcode, cubLeft ) )
	{
		pub += opcode.cubOpcode;
		cubLeft -= opcode.cubOpcode;
		cubUnderstood += opcode.cubOpcode;
	}
	return cubUnderstood;
}


//-----------------------------------------------------------------------------
// Purpose: disassemble - return how many bytes we understood
//-----------------------------------------------------------------------------
int DisassembleSingleFunction( unsigned char *pubStart, int cub )
{
	unsigned char *pub = pubStart;
	int cubLeft = cub;
	int cubUnderstood = 0;
	OPCODE_t opcode;
	while ( cubLeft > 0 && ParseCode( pub, &opcode, cubLeft ) )
	{
		pub += opcode.cubOpcode;
		cubLeft -= opcode.cubOpcode;
		cubUnderstood += opcode.cubOpcode;
		if ( opcode.bCantContinue )
			return cubUnderstood;
	}
	return cubUnderstood;
}


//-----------------------------------------------------------------------------
// Purpose: disassemble cubMin bytes
//-----------------------------------------------------------------------------
int CubDisasm( uint8 *pub, int cubLeft, int cubMin )
{
	int cubUnderstood = Disassemble( pub, cubLeft );
	if ( cubUnderstood < cubMin )
		return 0;
	return cubUnderstood;
}

#ifdef VACTOOL
//#define DASMTEST
#endif


#ifdef DASMTEST
#include <stdio.h>
#include "vstdlib/strtools.h"
char *PchNextItem( char *pchStart, char *pchCopy )
{
	*pchCopy = 0;
	while ( *pchStart == ' ' || *pchStart == '\t' )
	{
		pchStart++;
	}
	if ( *pchStart == 0 || *pchStart == '\n' )
		return NULL;
	while ( !( *pchStart == ' ' || *pchStart == '\t' || *pchStart == 0 || *pchStart == '\n' ) )
	{
		*pchCopy++ = *pchStart++;
	}
	*pchCopy = 0;
	return pchStart;
}

int ReadDisasmTextFile( const char *pszFileName )
{
	FILE *pFile = fopen( pszFileName, "rt" );
	if ( !pFile )
	{
		printf( "Failed to open file %s", pszFileName );
		return 0;
	}

	char rgchLine[ 1024 ];
	char rgchLine2[ 1024 ];
	char rgchLine3[ 1024 ];
	bool bNewInsLast = false;
	unsigned char rgubCode[32];
	int cubCode = 0;
	int nLine = 0;
	while ( fgets( rgchLine, sizeof(rgchLine), pFile ) )
	{
		nLine++;

		// read data, format: ID, type, class, name, hash
		// eg "1912","1","3","OGZ.dll","4ea1f025d3e5744f656e1822d72ea445"

		char *pchPos = rgchLine;
		char *pchColon = strstr( pchPos, ":" );
		bool bNewIns = pchColon != NULL;
		if ( bNewIns )
		{
			// this line has a colon - its a new opcode
			// so disasm the last opcode
			int cubUnderstood = CubDisasm( rgubCode, cubCode, cubCode );
			if ( cubUnderstood != cubCode )
			{
				if ( bNewInsLast )
					printf("Failed to disassemble %s \n", rgchLine2 );
				else
					printf("Failed to disassemble %s %s\n", rgchLine3, rgchLine2 );
				//__asm int 3;
			}
			cubCode = 0;
			pchPos = pchColon + 1;
		}
		char rgchCopy[16];
		rgchCopy[0] = '0';
		rgchCopy[1] = 'x';
		int ich = 0;
		while ( ich < 31 && pchPos )
		{
			pchPos = PchNextItem( pchPos, rgchCopy+2 );
			if ( !pchPos && pchColon )
			{
				cubCode = 0;
				break;
			}
			if ( !pchPos || rgchCopy[2] == 0 )
				break;
			ich = pchPos - rgchLine;
			if ( ich > 31 )
				break;
			rgubCode[cubCode] = V_atoi( rgchCopy );
			// ignore runs of CC - the file format doesnt make sense
			if ( cubCode == 0 && rgubCode[cubCode] == 0xCC )
				break;
			cubCode++;
		}

		bNewInsLast = bNewIns;
		memcpy( rgchLine3, rgchLine2, sizeof( rgchLine ) );
		memcpy( rgchLine2, rgchLine, sizeof( rgchLine ) );
	}

	fclose( pFile );
	return 1;
}


void TestDisassemble()
{
	//ReadDisasmTextFile( "\\schemacompiler.txt" );
	ReadDisasmTextFile( "\\serverdisasm.txt" );

	unsigned char *pubStartCode;
	int cubCode = 0;
	__asm mov eax, overcode
	__asm sub eax, startcode
	__asm mov cubCode, eax
	__asm mov eax, startcode
	__asm mov pubStartCode,eax
	__asm jmp overcode
startcode:
	__asm jmp         dword ptr [edx*4+1006982Ch]
	__asm jmp eax
	__asm _emit 0x55
	__asm _emit 0x8b
	__asm _emit 0xec
	__asm _emit 0x90
	__asm _emit 0x5d
	__asm _emit 0xe9
	__asm _emit 0x16
	__asm _emit 0x4d
	__asm _emit 0xdd
	__asm _emit 0xb9
	__asm _emit 0x90
	__asm _emit 0x90
	__asm _emit 0x90
	__asm _emit 0x90
	__asm _emit 0x90

overcode:
	Disassemble( pubStartCode, cubCode );
}

#endif
#endif // _WIN32
