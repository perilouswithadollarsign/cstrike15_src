//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//==========================================================================//

#include "cmatrixstack.h"


// Define RUN_TEST_CODE to validate that CMatrixStack behaves the same as ID3DXMatrixStack
//#define RUN_TEST_CODE
#if defined( RUN_TEST_CODE )

#ifndef WIN32
#error sorry man
#endif
#ifdef _X360
#include "d3d9.h"
#include "d3dx9.h"
#else
#include <windows.h>
#include "../../dx9sdk/include/d3d9.h"
#include "../../dx9sdk/include/d3dx9.h"
#endif

#else // RUN_TEST_CODE

#if !defined( _X360 )
struct D3DXMATRIX  : public VMatrix{};
struct D3DXVECTOR3 : public Vector{};
#endif // _X360

#endif // RUN_TEST_CODE


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



CMatrixStack::CMatrixStack( void )
{
	m_Stack.AddToTail();
	m_Stack.Tail().Identity();
}

CMatrixStack::~CMatrixStack( void )
{
	Assert( m_Stack.Count() == 1 );			// Catch push/pop mismatch
	Assert( m_Stack.Tail().IsIdentity() );	// Modifying the root matrix is probably unintentional
}

D3DXMATRIX *CMatrixStack::GetTop( void )
{
	return (D3DXMATRIX *)&m_Stack.Tail();
}

void CMatrixStack::Push()
{
	AssertMsg( m_Stack.Count() < 100, "CMatrixStack - Push/Pop mismatch!" ); // Catch push/pop mismatch
	// Duplicate the current 'top' matrix (NOTE: AddToTail can realloc!)
	m_Stack.AddToTail();
	VMatrix *top = &m_Stack.Tail();
	top[0] = top[-1];
}

void CMatrixStack::Pop()
{
	AssertMsg( m_Stack.Count() > 1, "CMatrixStack - Push/Pop mismatch!" ); // Catch push/pop mismatch
	m_Stack.RemoveMultipleFromTail( 1 );
}

void CMatrixStack::LoadIdentity()
{
	m_Stack.Tail().Identity();
}

void CMatrixStack::LoadMatrix( const D3DXMATRIX *pMat )
{
	COMPILE_TIME_ASSERT( sizeof( VMatrix ) == sizeof( D3DXMATRIX ) );
	memcpy( &m_Stack.Tail(), pMat, sizeof( VMatrix ) );
}

void CMatrixStack::MultMatrix( const D3DXMATRIX *pMat )
{
	// Right-multiply
	VMatrix &top = m_Stack.Tail();
	VMatrix result;
	MatrixMultiply( top, *(const VMatrix *)pMat, result );
	top = result;
}

void CMatrixStack::MultMatrixLocal( const D3DXMATRIX *pMat )
{
	// Left-multiply
	VMatrix &top = m_Stack.Tail();
	VMatrix result;
	MatrixMultiply( *(const VMatrix *)pMat, top, result );
	top = result;
}

bool CMatrixStack::ScaleLocal( float x, float y, float z )
{
	VMatrix scale;
	MatrixBuildScale( scale, x, y, z );
	// scale = scale.Transpose(); // A no-op, in this case :)
	MultMatrixLocal( (D3DXMATRIX *)&scale );
	return true;
}

bool CMatrixStack::RotateAxisLocal( const D3DXVECTOR3 *pV, float angleInRadians )
{
	COMPILE_TIME_ASSERT( sizeof( Vector ) == sizeof( D3DXVECTOR3 ) );
	const Vector &axis = *(const Vector *)pV;
	VMatrix rotate;
	MatrixBuildRotationAboutAxis( rotate, axis, angleInRadians * 180 / M_PI );
	rotate = rotate.Transpose();
	MultMatrixLocal( (D3DXMATRIX *)&rotate );
	return true;
}

bool CMatrixStack::TranslateLocal( float x, float y, float z )
{
	VMatrix translate;
	MatrixBuildTranslation( translate, x, y, z );
	translate = translate.Transpose();
	MultMatrixLocal( (D3DXMATRIX *)&translate );
	return true;
}



//==========================================================================//
//
// Test code to ensure this produces the same results as ID3DMATRIXStack
//
//==========================================================================//

#if defined( RUN_TEST_CODE )

class CMatrixStack_Test
{
	enum TestOps
	{
		LOAD_IDENT		= 0,
		LOAD_MAT		= 1,
		PUSH			= 2,
		POP				= 3,
		MULT			= 4,
		MULT_LOCAL		= 5,
		// Only these local/left-multiply variants are used by ShaderAPI (not the right-multiply ROTATE/TRANSLATE/SCALE versions)
		SCALE_LOCAL		= 6,
		ROTATE_LOCAL	= 7,
		TRANSLATE_LOCAL	= 8,
		NUM_TEST_OPS
	};

public:
	CMatrixStack *m_pStack;
	ID3DXMatrixStack *m_pD3DStack;
	CMatrixStack_Test()
	{
		m_pStack = new CMatrixStack();
		HRESULT result = D3DXCreateMatrixStack( 0, &m_pD3DStack );
		if ( result == S_OK )
		{
			VMatrix testMatrix;
			testMatrix.Identity();
			MatrixTranslate( testMatrix, Vector( 1, 2, 3 ) );
			MatrixRotate( testMatrix, Vector( 1, 0, 0 ), 90 );
			MatrixTranslate( testMatrix, Vector( 4, 5, 6 ) );
			// CMatrixStack mimics D3DX's transposed matrix style
			testMatrix = testMatrix.Transpose();

			// Leave the top matrix unmodified
			m_pStack->Push();
			m_pD3DStack->Push();
			int depth = 2;

			Msg( "CMatrixStack test...\n" );
			srand(1352469);
			for ( int i = 0; i < 1000; i++ )
			{
				TestOps op = (TestOps)( rand() % NUM_TEST_OPS );
				switch( op )
				{
				case LOAD_IDENT:
					Msg( "LOAD_IDENT\n" );
					m_pStack->LoadIdentity();
					m_pD3DStack->LoadIdentity();
					break;
				case LOAD_MAT:
					Msg( "LOAD_MAT\n" );
					m_pStack->LoadMatrix( (D3DXMATRIX *) &testMatrix );
					m_pD3DStack->LoadMatrix( (D3DXMATRIX *) &testMatrix );
					break;
				case PUSH:
					Msg( "PUSH\n" );
					m_pStack->Push();
					m_pD3DStack->Push();
					depth++;
					break;
				case POP:
					if ( depth > 2 ) // Leave the top matrix unmodified
					{
						Msg( "POP\n" );
						m_pStack->Pop();
						m_pD3DStack->Pop();
						depth--;
					}
					break;
				case MULT:
					Msg( "MULT\n" );
					m_pStack->MultMatrix( (D3DXMATRIX *) &testMatrix );
					m_pD3DStack->MultMatrix( (D3DXMATRIX *) &testMatrix );
					break;
				case MULT_LOCAL:
					Msg( "MULT_LOCAL\n" );
					m_pStack->MultMatrixLocal( (D3DXMATRIX *) &testMatrix );
					m_pD3DStack->MultMatrixLocal( (D3DXMATRIX *) &testMatrix );
					break;
				case SCALE_LOCAL:
					Msg( "SCALE_LOCAL\n" );
					if ( i & 1 )
					{
						m_pStack->ScaleLocal( 2.0f, 2.0f, 2.0f );
						m_pD3DStack->ScaleLocal( 2.0f, 2.0f, 2.0f );
					}
					else
					{
						m_pStack->ScaleLocal( 0.5f, 0.5f, 0.5f );
						m_pD3DStack->ScaleLocal( 0.5f, 0.5f, 0.5f );
					}
					break;
				case ROTATE_LOCAL:
				{
					Msg( "ROTATE_LOCAL\n" );
					float angleInRadians = ( (i&1)?+1:-1 )*0.5f*M_PI;
					D3DXVECTOR3 axis(1,0,0);
					if ( (i%3) == 1 ) axis = D3DXVECTOR3(0,1,0);
					if ( (i%3) == 2 ) axis = D3DXVECTOR3(0,0,1);
					m_pStack->RotateAxisLocal( &axis, angleInRadians );
					m_pD3DStack->RotateAxisLocal( &axis, angleInRadians );
					break;
				}
				case TRANSLATE_LOCAL:
				{
					Msg( "TRANSLATE_LOCAL\n" );
					Vector delta = RandomVector( -10, +10 );
					m_pStack->TranslateLocal( delta.x, delta.y, delta.z );
					m_pD3DStack->TranslateLocal( delta.x, delta.y, delta.z );
					break;
				}
				}
				CompareTopMatrices( op );
			}
			while( depth > 1 )
			{
				m_pStack->Pop();
				m_pD3DStack->Pop();
				depth--;
				CompareTopMatrices( POP );
			}

			m_pD3DStack->Release();
		}
		delete m_pStack;
	}

	void CompareTopMatrices( TestOps op )
	{
		// Compare the top matrices
		float mat[4][4], d3dMat[4][4];
		COMPILE_TIME_ASSERT( sizeof( D3DXMATRIX ) == 16*sizeof( float ) );
		memcpy( mat, m_pStack->GetTop(), sizeof( D3DXMATRIX ) );
		memcpy( d3dMat, m_pD3DStack->GetTop(), sizeof( D3DXMATRIX ) );
		for ( int y = 0; y < 4; y++ )
		{
			for ( int x = 0; x < 4; x++ )
			{
				static float absEpsilon = 1.0e-5f, relEpsilon = 1.0e-4f;
				float absolute = fabsf( mat[y][x] - d3dMat[y][x] );
				float relative = 0;
				if ( fabsf( d3dMat[y][x] ) > 0.00001f )
				{
					relative = fabsf( ( mat[y][x] / d3dMat[y][x] ) - 1.0f );
				}
				AssertMsg9( ( absolute <= absEpsilon ) && ( relative <= relEpsilon ),
					"DIFFERENCE! CMatrixStack[%d][%d] = %10f, ID3DXMatrixStack[%d][%d] = %10f (OP: %d, Absolute diff: %10e, Relative diff: %10e)\n",
					y, x, mat[y][x],
					y, x, d3dMat[y][x],
					op, absolute, relative );
			}
		}

		// Copy the D3D version back onto ours, to negate accumulated numerical differences
		m_pStack->LoadMatrix( m_pD3DStack->GetTop() );
	}
};
static CMatrixStack_Test g_MatrixStackTest;

#endif // defined( RUN_TEST_CODE )
