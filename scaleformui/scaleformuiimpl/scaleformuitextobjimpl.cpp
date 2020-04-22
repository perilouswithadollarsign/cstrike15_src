//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"

#include "tier1/fmtstr.h"

// NOTE: This must be the last file included!!!
#include "tier0/memdbgon.h"

using namespace Scaleform::GFx;

#define TEMP_BUFFER_LENGTH 1024

class TextBoxArrayVisitor: public Value::ArrayVisitor
{
public:
	CUtlBlockVector<Value> *m_pTextBoxes;

	void Visit( unsigned int idx, const Value& val )
	{
		m_pTextBoxes->AddToTail( val );
	}
};

/**********************************************
 * Implementation of the text object.  It accepts
 * an SFText object or a TextBox from flash
 * and keeps referenes to all the text boxes so
 * that they can be set directly without needing
 * to invoke the SetText function.
 */

class SFTextObjectImpl: public ISFTextObject
{
protected:
	// Using a CUtlBlockVector to avoid realloc (memcpy of Scaleform::Value)
	// when the vector grow which would perform a shallow copy
	// (Need to use Scaleform::Value copy constructor performing a deep copy)
	CUtlBlockVector<Value> m_TextBoxes;

public:
	SFTextObjectImpl()
	{
	}

	SFTextObjectImpl( Value &value )
	{
		Init( value );
	}

	bool Init( Value &value )
	{
		m_TextBoxes.RemoveAll();

		if ( value.GetType() != Value::VT_DisplayObject )
		{
			return false;
		}

		Value boxArray;
		if ( value.GetMember( "SFText_TextBoxList", &boxArray ) )
		{
			TextBoxArrayVisitor vis;
			vis.m_pTextBoxes = &m_TextBoxes;
			boxArray.VisitElements( &vis );

		}
		else
		{
			m_TextBoxes.AddToTail( value );
		}

		return true;
	}

	virtual void SetText( int value )
	{
		SetText( CFmtStr( "%d", value ) );
	}

	virtual void SetText( float value )
	{
		SetText( CFmtStr( "%0.f", value ) );
	}

	virtual void SetText( const char* pszText )
	{
		FOR_EACH_VEC( m_TextBoxes, i )
		{
			m_TextBoxes[i].SetText( pszText );
		}
	}

	virtual void SetTextHTML( const char* pszText )
	{
		FOR_EACH_VEC( m_TextBoxes, i )
		{
			m_TextBoxes[i].SetTextHTML( pszText );
		}
	}

	virtual void SetText( const wchar_t* pwszText )
	{
		FOR_EACH_VEC( m_TextBoxes, i )
		{
			m_TextBoxes[i].SetText( pwszText );
		}
	}

	virtual void SetTextHTML( const wchar_t* pwszText )
	{
		FOR_EACH_VEC( m_TextBoxes, i )
		{
			m_TextBoxes[i].SetTextHTML( pwszText );
		}
	}

	virtual void SetVisible( bool visible )
	{
		FOR_EACH_VEC( m_TextBoxes, i )
		{
			SFINST.SetVisible( ToSFVALUE( &m_TextBoxes[i] ), visible);
		}
	}

	virtual bool IsValid( void )
	{
		return m_TextBoxes.Count() > 0;
	}

	virtual void Release( void )
	{
		delete this;
	}

};

/************************************************
 * methods in IScalformUI for creating these objects
 */

ISFTextObject* ScaleformUIImpl::TextObject_MakeTextObject( SFVALUE value )
{
	SFTextObjectImpl* pResult = new SFTextObjectImpl( *FromSFVALUE( value ) );

	if ( !pResult->IsValid() )
	{
		pResult->Release();
		pResult = NULL;
	}

	return pResult;

}

ISFTextObject* ScaleformUIImpl::TextObject_MakeTextObjectFromMember( SFVALUE value, const char * pName )
{
	Value* pValue = FromSFVALUE( value );

	Value member;
	if ( pValue->GetMember( pName, &member ) )
	{
		return TextObject_MakeTextObject( ToSFVALUE( &member ) );
	}
	else
	{
		return NULL;
	}
}
