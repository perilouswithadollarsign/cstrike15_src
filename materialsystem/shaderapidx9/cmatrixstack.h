//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//==========================================================================//

#ifndef CMATRIXSTACK_H
#define CMATRIXSTACK_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"
#include "mathlib/vmatrix.h"


// TODO: switch the PS3/OGL shaderapi implementations to use this (replace the incomplete implementation of ID3DXMatrixStack in dxabstract.h)


struct D3DXMATRIX;
struct D3DXVECTOR3;

//-----------------------------------------------------------------------------
// CMatrixStack
// A direct replacement for ID3DXMatrixStack, which has bugs
// NOTE: it uses *TRANSPOSED* VMatrixes internally, to match D3D conventions
class CMatrixStack
{
public:

	// NOTE: The stack starts with one matrix, initialized to identity
	CMatrixStack();
	~CMatrixStack();

    D3DXMATRIX *GetTop();
	void Push();
	void Pop();
	void LoadIdentity();
	void LoadMatrix( const D3DXMATRIX *pMat );
	void MultMatrix( const D3DXMATRIX *pMat );
	void MultMatrixLocal( const D3DXMATRIX *pMat );

	// Left multiply the current matrix with the computed scale matrix
    // (scaling is about the local origin of the object)
    bool ScaleLocal( float x, float y, float z );

	// Left multiply the current matrix with the computed rotation
    // matrix, counterclockwise about the given axis with the given angle.
    // (rotation is about the local origin of the object)
    bool RotateAxisLocal( const D3DXVECTOR3 *pV, float angleInRadians );

	// Left multiply the current matrix with the computed translation
    // matrix. (transformation is about the local origin of the object)
    bool TranslateLocal( float x, float y, float z );

private:
	CUtlVector<VMatrix>	m_Stack; // 'Top' of the stack is at m_stack[count-1] (push increases count, pop decreases)
};

#endif // CMATRIXSTACK_H
