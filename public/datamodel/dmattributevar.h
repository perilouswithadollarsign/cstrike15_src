//====== Copyright � 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef DMATTRIBUTEVAR_H
#define DMATTRIBUTEVAR_H
#ifdef _WIN32
#pragma once
#endif


#include "datamodel/dmattribute.h"
#include "tier1/utlvector.h"
#include "Color.h"
#include "mathlib/vector2d.h"
#include "mathlib/vector.h"
#include "mathlib/vector4d.h"
#include "mathlib/vmatrix.h"
#include "datamodel/dmelement.h"
#include "datamodel/dmehandle.h"


//-----------------------------------------------------------------------------
// Specialization for color
//-----------------------------------------------------------------------------
class CDmaColor : public CDmaVar< Color >
{
public:
	// Set methods
	void SetColor( int r, int g, int b, int a = 0 );
	void SetRed( int r );
	void SetGreen( int g );
	void SetBlue( int b );
	void SetAlpha( int a );

	// Sets the color as a 32-bit integer
	void SetRawColor( int color );

	// Get methods
	unsigned char r() const;
	unsigned char g() const;
	unsigned char b() const;
	unsigned char a() const;
	const unsigned char &operator[]( int index ) const;
};


//-----------------------------------------------------------------------------
// Specialization for DmeTime_t
//-----------------------------------------------------------------------------
class CDmaTime : public CDmaVar< DmeTime_t >
{
public:
	// Cast operators
	operator const DmeTime_t&() const { return this->Value(); }
	const DmeTime_t* operator->() const { return &this->Value(); }

	// Math utility operations
	const DmeTime_t& operator= ( const DmeTime_t &val ) { return Set( val ); }
	const DmeTime_t& operator+=( const DmeTime_t &val ) { return Set( this->Value() + val ); }
	const DmeTime_t& operator-=( const DmeTime_t &val ) { return Set( this->Value() - val ); }
	const DmeTime_t& operator/=( float f ) { return Set( this->Value() / f ); }
	const DmeTime_t& operator*=( float f ) { return Set( this->Value() * f ); }
	DmeTime_t operator++() { return Set( this->Value() + DMETIME_MINDELTA ); }
	DmeTime_t operator--() { return Set( this->Value() - DMETIME_MINDELTA ); }
	DmeTime_t operator++( int ) { DmeTime_t t = this->Value(); Set( t + DMETIME_MINDELTA ); return t; } // postfix version..
	DmeTime_t operator--( int ) { DmeTime_t t = this->Value(); Set( t - DMETIME_MINDELTA ); return t; } // postfix version..

	void SetTenthsOfMS( int tms );
	void SetSeconds( float seconds );
	int GetTenthsOfMS() const;
	float GetSeconds() const;
};


//-----------------------------------------------------------------------------
// Specialization for binary block
//-----------------------------------------------------------------------------
class CDmaBinaryBlock : public CDmaVar< CUtlBinaryBlock >
{
public:
	void Get( void *pValue, int nMaxLen ) const;
	void Set( const void *pValue, int nLen );
	const void *Get() const;
	const unsigned char& operator[]( int i ) const;

	// Returns buffer length
	int	Length() const;
};


//-----------------------------------------------------------------------------
// Specialization for elements
//-----------------------------------------------------------------------------
template <class T>
class CDmaElement : public CDmaVar< DmElementHandle_t >
{
	typedef CDmaVar< DmElementHandle_t > BaseClass;

public:
	// Used to initialize the attribute in an element's OnConstruction method
	void InitAndCreate( CDmElement *pOwner, const char *pAttributeName, const char *pElementName = NULL, int flags = 0 );
	void Init( CDmElement *pOwner, const char *pAttributeName, int flags = 0 );

	// Returns the type of elements allowed into this attribute. UTL_INVAL_SYMBOL allows everything.
	CUtlSymbolLarge GetElementType() const;

	// Get/set
	void Set( T* pElement );
	T* GetElement() const;
	const DmElementHandle_t& GetHandle() const
	{
		return this->Value();
	}

	// Cast
	T* operator->() const;
	operator T*() const;

	// NULL check
	bool operator!() const;

	// Assignment.. wish I knew how to un-inline these methods
	template <class S> CDmaElement<T> &operator=( S* pElement )
	{
		Set( static_cast<T*>( pElement ) );
		return *this;
	}

	template <class S> CDmaElement<T> &operator=( const CDmaElement<S>& src )
	{
		Set( static_cast<T*>( src.Get() ) );
		return *this;
	}

	template <class S> bool operator==( const CDmaElement<S>& src )	const
	{
		return this->Value() == src.Value();
	}

	template <class S> bool operator!=( const CDmaElement<S>& src )	const
	{
		return this->Value() != src.Value();
	}

protected:
	// hide this to avoid accidental use in code like this:
	// if ( m_var.Get() )
	// which returns a DmElementHandle_t, which is true for most handles, including the invalid one
	const DmElementHandle_t& Get() const
	{
		return this->Value();
	}
};


//-----------------------------------------------------------------------------
// Can access any array attribute, regardless of type
// See below for type-specific array accessors which have more features
//-----------------------------------------------------------------------------
class CDmrGenericArrayConst
{
public:
	CDmrGenericArrayConst( const CDmAttribute* pAttribute );
	CDmrGenericArrayConst( const CDmElement *pElement, const char *pAttributeName );

	// Array count
	int			Count() const;

	// Gets 
	const void*	GetUntyped( int i ) const;

	// String conversion
	const char* GetAsString( int i, char *pBuffer, size_t nBufLen ) const;

	const CDmAttribute *GetAttribute() const;
	bool IsValid() const;

protected:
	CDmrGenericArrayConst();
	void Init( const CDmAttribute *pAttribute );
	void Init( const CDmElement *pElement, const char *pAttributeName );

	CDmAttribute *m_pAttribute;
};

class CDmrGenericArray : public CDmrGenericArrayConst
{
public:
	CDmrGenericArray( CDmAttribute* pAttribute );
	CDmrGenericArray( CDmElement *pElement, const char *pAttributeName );

	void	EnsureCount( int num );

	// Sets multiple elements at the same time
	int		AddToTail();
	void	Remove( int elem );		// preserves order, shifts elements
	void	RemoveAll();				// doesn't deallocate memory
	void	SetMultiple( int i, int nCount, DmAttributeType_t valueType, const void *pValue );
	void	Set( int i, DmAttributeType_t valueType, const void *pValue );

	// String conversion
	void SetFromString( int i, const char *pValue );

	CDmAttribute *GetAttribute();
	const CDmAttribute *GetAttribute() const;
};


//-----------------------------------------------------------------------------
// Helper template for external array attribute vars
// NOTE: To use this class, don't use CDmaArrayBase directly. Instead, use
//		CDmaArray<T> var;	<- Instantiate an array attribute var as a member of a element class
//		CDmrArray<T> var;	<- Used to reference an existing array attribute + read/modify it
//		CDmrArrayConst<T> var;	<- Used to reference an existing array attribute + read it (no modify)
//
// Also, there is a CDmaStringArray/CDmrStringArray/CDmrStringArrayConst for strings
// and a CDmaElementArray/CDmrElementArray/CDmrElementArrayConst for elements
//-----------------------------------------------------------------------------
template< class T, class B >
class CDmaArrayConstBase : public B
{
public:
	// Accessors
	const CUtlVector<T> &Get() const;
	const T *Base() const;

	// Iteration
	int		Count() const;
	const T& operator[]( int i ) const;
	const T& Element( int i ) const;
	const T& Get( int i ) const;
	const void*	GetUntyped( int i ) const;
	bool	IsValidIndex( int i ) const;
	int		InvalidIndex( void ) const;

	// Search
	int		Find( const T &value ) const;

	// Attribute-related methods
	const CDmAttribute *GetAttribute() const;
	CDmElement *GetOwner();
	bool IsDirty() const;

protected:
	CDmaArrayConstBase( );

	CDmAttribute *m_pAttribute;
};

template< class T, class B >
class CDmaArrayBase : public CDmaArrayConstBase< T, B >
{
public:	
	// Insertion
	int		AddToTail();
	int		InsertBefore( int elem );
	int		AddToTail( const T& src );
	int		InsertBefore( int elem, const T& src );
	int		AddMultipleToTail( int num );
	int		InsertMultipleBefore( int elem, int num );
	void	EnsureCount( int num );

	// Element Modification
	void	Set( int i, const T& value );
	void	SetMultiple( int i, int nCount, const T* pValue );
	void	Swap( int i, int j );

	// Copy related methods
	void	CopyArray( const T *pArray, int size );

	// this is basically just a faster version of CopyArray which uses pointer swap
	// NOTE: This doesn't work for element arrays
	void	SwapArray( CUtlVector< T > &array );

	// Removal
	void	FastRemove( int elem );
	void	Remove( int elem );
	void	RemoveMultiple( int elem, int num );
	void	RemoveAll();

	// Memory management
	void	EnsureCapacity( int num );
	void	Purge();

	// Attribute-related methods
	CDmAttribute *GetAttribute();
	const CDmAttribute *GetAttribute() const;
};


//-----------------------------------------------------------------------------
// Specialization for string arrays
// NOTE: To use this class, don't use CDmaStringArrayBase directly. Instead, use
//		CDmaStringArray var;	<- Instantiate an array attribute var as a member of a element class
//		CDmrStringArray var;	<- Used to reference an existing array attribute + read/modify it
//		CDmrStringArrayConst var; <- Used to reference an existing array attribute + read it (no modify)
//-----------------------------------------------------------------------------
template< class BaseClass >
class CDmaStringArrayConstBase : public BaseClass
{
public:
	const char *operator[]( int i ) const;
	const char *Element( int i ) const;
	const char *Get( int i ) const;
	const CUtlVector< CUtlSymbolLarge > &Get() const;

	// Returns strlen of element i
	int	Length( int i ) const;
};

template< class B >
class CDmaStringArrayBase : public CDmaStringArrayConstBase< CDmaArrayBase< CUtlSymbolLarge, B > >
{
	typedef CDmaStringArrayConstBase< CDmaArrayBase< CUtlSymbolLarge, B > > BaseClass;

public:
	// Sets an element in the array
	void Set( int i, const char * pValue );

	// Adds an element, uses copy constructor
	int	AddToTail( const char *pValue );
	int	InsertBefore( int elem, const char *pValue );
};


//-----------------------------------------------------------------------------
// Specialization for elements
// NOTE: To use this class, don't use CDmaElementArrayBase directly. Instead, use
//		CDmaElementArray< element_type > var;	<- Instantiate an array attribute var as a member of a element class
//		CDmrElementArray< element_type >  var;	<- Used to reference an existing array attribute + read/modify it
//		CDmrElementArrayConst< element_type > var; <- Used to reference an existing array attribute + read it (no modify)
//-----------------------------------------------------------------------------
template< class E, class BaseClass >
class CDmaElementArrayConstBase : public BaseClass
{
public:
	// Returns the element type
	CUtlSymbolLarge GetElementType() const;

	// Array access
	E *operator[]( int i ) const;
	E *Element( int i ) const;
	E *Get( int i ) const;
	const DmElementHandle_t& GetHandle( int i ) const;
	const CUtlVector< DmElementHandle_t > &Get() const;

	// Search
	int	Find( const E *pValue ) const;
	int	Find( DmElementHandle_t h ) const;
};

template < class E, class B >
class CDmaElementArrayBase : public CDmaElementArrayConstBase< E, CDmaArrayBase< DmElementHandle_t, B > >
{
	typedef CDmaElementArrayConstBase< E, CDmaArrayBase< DmElementHandle_t, B > > BaseClass;

public:
	void SetHandle( int i, DmElementHandle_t h );
	void Set( int i, E *pElement );

	// Insertion
	int	AddToTail( );
	int	AddToTail( DmElementHandle_t src );
	int	AddToTail( E *pValue );
	int InsertBefore( int elem );
	int	InsertBefore( int elem, DmElementHandle_t src );
	int	InsertBefore( int elem, E *pValue );

	template< class C, HandleType_t HT > int AddToTail( const CDmeHandle<C,HT>& value )
	{
		return BaseClass::AddToTail( value.GetHandle() );
	}

	template< class C, HandleType_t HT > int InsertBefore( int elem, const CDmeHandle<C,HT>& value )
	{
		return BaseClass::InsertBefore( elem, value.GetHandle() );
	}
};


// NOTE: The next couple classes are implementation details used to create CDmrAray/CDmaArray

//-----------------------------------------------------------------------------
// Base classes that contain data or refer to it; used for array accessor classes
//-----------------------------------------------------------------------------
template< typename T >
class CDmaDataInternal
{
protected:
	typedef typename CDmAttributeInfo< T >::StorageType_t D;

	const T& Value() const { return m_Storage; }
	T& Value( ) { return m_Storage; }
	const D& Data() const { return m_Storage; }
	D& Data( ) { return m_Storage; }

private:
	D m_Storage;
};

template< typename T >
class CDmaDataExternal
{
protected:
	typedef typename CDmAttributeInfo< T >::StorageType_t D;

	CDmaDataExternal() : m_pStorage(0) {}
	void Attach( void *pData ) { m_pStorage = (D*)pData; }
	const T& Value() const { return *m_pStorage; }
	T& Value( ) { return *m_pStorage; }
	const D& Data() const { return *m_pStorage; }
	D& Data( ) { return *m_pStorage; }

private:
	D* m_pStorage;
};


//-----------------------------------------------------------------------------
// Versions for access, or for attribute vars
//-----------------------------------------------------------------------------
template< class T, class B >
class CDmaDecorator : public B
{
public:
	void Init( CDmElement *pOwner, const char *pAttributeName, int flags = 0 );
};


template< class T, class BaseClass >
class CDmrDecoratorConst : public BaseClass
{
public:
	void Init( const CDmAttribute* pAttribute );
	void Init( const CDmElement *pElement, const char *pAttributeName );

	bool IsValid() const;
};

template< class T, class BaseClass >
class CDmrDecorator : public BaseClass
{
public:
	void Init( CDmAttribute* pAttribute );
	void Init( CDmElement *pElement, const char *pAttributeName, bool bAddAttribute = false );

	bool IsValid() const;
};


#define DECLARE_ATTRIBUTE_ARRAY_VARIABLE( _className, _elementType )	\
	public:																\
		_className() {}

#define DECLARE_ATTRIBUTE_ARRAY_REFERENCE( _className, _elementType )	\
	public:																\
		_className() {}													\
		_className( CDmAttribute* pAttribute ) { this->Init( pAttribute ); }	\
		_className( CDmElement *pElement, const char *pAttributeName, bool bAddAttribute = false ) { this->Init( pElement, pAttributeName, bAddAttribute ); } \
		_className( CDmaArray<_elementType>& var ) { this->Init( var.GetAttribute() ); } \
		_className( CDmrArray<_elementType>& var ) { this->Init( var.GetAttribute() ); }

#define DECLARE_ATTRIBUTE_ARRAY_CONST_REFERENCE( _className, _elementType )	\
	public:																	\
		_className() {}														\
		_className( const CDmAttribute* pAttribute ) { this->Init( pAttribute ); } \
		_className( const CDmElement *pElement, const char *pAttributeName ) { this->Init( pElement, pAttributeName ); } \
		_className( const CDmaArray<_elementType>& var ) { this->Init( var.GetAttribute() ); } \
		_className( const CDmrArrayConst<_elementType>& var ) { this->Init( var.GetAttribute() ); } \
		_className( const CDmrArray<_elementType>& var ) { this->Init( var.GetAttribute() ); }

template<class T> class CDmrArray;
template<class T> class CDmrArrayConst;
template<class T> class CDmaArray;

//-----------------------------------------------------------------------------
// Versions for access, or for attribute vars
//-----------------------------------------------------------------------------
template<class T>
class CDmaArray : public CDmaDecorator< T, CDmaArrayBase< T, CDmaDataInternal< CUtlVector< T > > > >
{
	DECLARE_ATTRIBUTE_ARRAY_VARIABLE( CDmaArray, T );

public:
	const CDmaArray<T>& operator=( const CDmaArray<T> &val ) 
	{ 
		CopyArray( val.Base(), val.Count() ); 
		return *this; 
	}

	template< class C > const CDmaArray<T>& operator=( const C &val ) 
	{ 
		CopyArray( val.Base(), val.Count() ); 
		return *this; 
	}

private:
	CDmaArray( const CDmaArray& array ) {}
};


template<class T>
class CDmrArrayConst : public CDmrDecoratorConst< T, CDmaArrayConstBase< T, CDmaDataExternal< CUtlVector< T > > > >
{
	typedef CDmrDecoratorConst< T, CDmaArrayConstBase< T, CDmaDataExternal< CUtlVector< T > > > > BaseClass;
	DECLARE_ATTRIBUTE_ARRAY_CONST_REFERENCE( CDmrArrayConst, T );
};


template<class T>
class CDmrArray : public CDmrDecorator< T, CDmaArrayBase< T, CDmaDataExternal< CUtlVector< T > > > >
{
	typedef CDmrDecorator< T, CDmaArrayBase< T, CDmaDataExternal< CUtlVector< T > > > > BaseClass;
	DECLARE_ATTRIBUTE_ARRAY_REFERENCE( CDmrArray, T );

public:
	const CDmrArray<T>& operator=( const CDmrArray<T> &val ) 
	{ 
		CopyArray( val.Base(), val.Count() ); 
		return *this; 
	}

	template< class C > const CDmrArray<T>& operator=( const C &val ) 
	{ 
		CopyArray( val.Base(), val.Count() ); 
		return *this; 
	}
};

class CDmrStringArray;

class CDmaStringArray : public CDmaDecorator< CUtlSymbolLarge, CDmaStringArrayBase< CDmaDataInternal< CUtlVector< CUtlSymbolLarge > > > >
{
	DECLARE_ATTRIBUTE_ARRAY_VARIABLE( CDmaStringArray, CUtlSymbolLarge );

public:
	const CDmaStringArray& operator=( const CDmaStringArray &val ) 
	{ 
		CopyArray( val.Base(), val.Count() ); 
		return *this; 
	}

	template< class C > const CDmaStringArray& operator=( const C &val ) 
	{ 
		CopyArray( val.Base(), val.Count() ); 
		return *this; 
	}

private:
	CDmaStringArray( const CDmaStringArray& array ) {}
};

class CDmrStringArray : public CDmrDecorator< CUtlSymbolLarge, CDmaStringArrayBase< CDmaDataExternal< CUtlVector< CUtlSymbolLarge > > > >
{
	typedef CDmrDecorator< CUtlSymbolLarge, CDmaStringArrayBase< CDmaDataExternal< CUtlVector< CUtlSymbolLarge > > > > BaseClass;
	DECLARE_ATTRIBUTE_ARRAY_REFERENCE( CDmrStringArray, CUtlSymbolLarge );

public:
	CDmrStringArray( CDmaStringArray& var ) { this->Init( var.GetAttribute() ); }
	CDmrStringArray( CDmrStringArray& var ) { this->Init( var.GetAttribute() ); }

	const CDmrStringArray& operator=( const CDmrStringArray &val ) 
	{ 
		this->CopyArray( val.Base(), val.Count() ); 
		return *this; 
	}

	template< class C > const CDmrStringArray& operator=( const C &val ) 
	{ 
		CopyArray( val.Base(), val.Count() ); 
		return *this; 
	}
};

class CDmrStringArrayConst : public CDmrDecoratorConst< CUtlSymbolLarge, CDmaStringArrayConstBase< CDmaArrayConstBase< CUtlSymbolLarge, CDmaDataExternal< CUtlVector< CUtlSymbolLarge > > > > >
{
	typedef CDmrDecoratorConst< CUtlSymbolLarge, CDmaStringArrayConstBase< CDmaArrayConstBase< CUtlSymbolLarge, CDmaDataExternal< CUtlVector< CUtlSymbolLarge > > > > > BaseClass;
	DECLARE_ATTRIBUTE_ARRAY_CONST_REFERENCE( CDmrStringArrayConst, CUtlSymbolLarge );

public:
	CDmrStringArrayConst( const CDmaStringArray& var )		{ this->Init( var.GetAttribute() ); }
	CDmrStringArrayConst( const CDmrStringArray& var )		{ this->Init( var.GetAttribute() ); }
	CDmrStringArrayConst( const CDmrStringArrayConst& var )	{ this->Init( var.GetAttribute() ); }
};


//-----------------------------------------------------------------------------
// Prevent CDmaArray for DmElementHandle_t
//-----------------------------------------------------------------------------
template<> class CDmaArray<DmElementHandle_t> { private: CDmaArray(); };


template< class E > class CDmrElementArray;

template< class E = CDmElement >
class CDmaElementArray : public CDmaElementArrayBase< E, CDmaDataInternal< CUtlVector< DmElementHandle_t > > >
{
	DECLARE_ATTRIBUTE_ARRAY_VARIABLE( CDmaElementArray, DmElementHandle_t );

public:
	void Init( CDmElement *pOwner, const char *pAttributeName, int flags = 0 )
	{
		Assert( pOwner );
		this->m_pAttribute = pOwner->AddExternalAttribute( pAttributeName, AT_ELEMENT_ARRAY, &this->Value() );
		this->m_pAttribute->SetElementTypeSymbol( E::GetStaticTypeSymbol() );
		if ( flags )
		{
			this->m_pAttribute->AddFlag( flags );
		}
	}

	template< typename C > CDmaElementArray<E>& operator=( const C &val ) 
	{ 
		CopyArray( val.Base(), val.Count() ); 
		return *this; 
	}

	// NOTE: The copy operator= must be defined in addition to the generic one
	const CDmaElementArray<E>& operator=( const CDmaElementArray<E> &val ) 
	{ 
		CopyArray( val.Base(), val.Count() ); 
		return *this; 
	}

private:
	template< class C > CDmaElementArray( const CDmaElementArray<C>& var );
};

template< class E = CDmElement >
class CDmrElementArrayConst : public CDmaElementArrayConstBase< E, CDmaArrayConstBase< DmElementHandle_t, CDmaDataExternal< CUtlVector< DmElementHandle_t > > > >
{
public:
	CDmrElementArrayConst()
	{
		this->m_pAttribute = NULL;
	}

	CDmrElementArrayConst( const CDmAttribute* pAttribute )
	{
		this->Init( pAttribute );
	}

	CDmrElementArrayConst( const CDmElement *pElement, const char *pAttributeName )
	{
		this->Init( pElement, pAttributeName );
	}

	template< typename C > CDmrElementArrayConst( const CDmaElementArray<C>& var )
	{
		this->Init( var.GetAttribute() );
	}

	template< typename C > CDmrElementArrayConst( const CDmrElementArray<C>& var )
	{
		this->Init( var.GetAttribute() );
	}

	template< typename C > CDmrElementArrayConst( const CDmrElementArrayConst<C>& var )
	{
		this->Init( var.GetAttribute() );
	}

	void Init( const CDmAttribute* pAttribute )
	{
		if ( pAttribute && pAttribute->GetType() == AT_ELEMENT_ARRAY )
		{
			this->m_pAttribute = const_cast<CDmAttribute*>( pAttribute );
			this->Attach( this->m_pAttribute->GetAttributeData() );
		}
		else
		{
			this->m_pAttribute = NULL;
			this->Attach( NULL );
		}
	}

	void Init( const CDmElement *pElement, const char *pAttributeName )
	{
		const CDmAttribute *pAttribute = NULL;
		if ( pElement && pAttributeName && pAttributeName[0] )
		{
			pAttribute = pElement->GetAttribute( pAttributeName );
		}
		this->Init( pAttribute );
	}

	int Count() const
	{
		return this->IsValid() ? this->Value().Count() : 0;
	}

	bool IsValid() const
	{
		return this->m_pAttribute != NULL;
	}
};

template< class T = CDmElement >
class CDmrElementArray : public CDmaElementArrayBase< T, CDmaDataExternal< CUtlVector< DmElementHandle_t > > >
{
public:
	CDmrElementArray()
	{
		this->m_pAttribute = NULL;
	}

	CDmrElementArray( CDmAttribute* pAttribute )
	{
		this->Init( pAttribute );
	}

	CDmrElementArray( CDmElement *pElement, const char *pAttributeName, bool bAddAttribute = false )
	{
		this->Init( pElement, pAttributeName, bAddAttribute );
	}

	template< typename C > CDmrElementArray( CDmaElementArray<C>& var )
	{
		this->Init( var.GetAttribute() );
	}

	template< typename C > CDmrElementArray( CDmrElementArray<C>& var )
	{
		this->Init( var.GetAttribute() );
	}

	void Init( CDmAttribute* pAttribute )
	{
		if ( pAttribute && pAttribute->GetType() == AT_ELEMENT_ARRAY )
		{
			this->m_pAttribute = pAttribute;
			this->Attach( this->m_pAttribute->GetAttributeData() );
		}
		else
		{
			this->m_pAttribute = NULL;
			this->Attach( NULL );
		}
	}

	void Init( CDmElement *pElement, const char *pAttributeName, bool bAddAttribute = false )
	{
		CDmAttribute *pAttribute = NULL;
		if ( pElement && pAttributeName && pAttributeName[0] )
		{
			pAttribute = pElement->GetAttribute( pAttributeName );
			if ( bAddAttribute && !pAttribute )
			{
				pAttribute = pElement->CreateAttribute( pAttributeName, AT_ELEMENT_ARRAY );

				// FIXME: Should we do this?
				pAttribute->SetElementTypeSymbol( T::GetStaticTypeSymbol() );
			}
		}
		this->Init( pAttribute );
	}

	int Count() const
	{
		return IsValid() ? this->Value().Count() : 0;
	}

	bool IsValid() const
	{
		return this->m_pAttribute != NULL;
	}

	template< typename C > CDmrElementArray<T>& operator=( const C &val )
	{ 
		CopyArray( val.Base(), val.Count() ); 
		return *this; 
	}

	// NOTE: The copy operator= must be defined in addition to the generic one
	const CDmrElementArray<T>& operator=( const CDmrElementArray<T> &val )
	{ 
		CopyArray( val.Base(), val.Count() ); 
		return *this; 
	}
};


//-----------------------------------------------------------------------------
//
// Inline methods for CDmaVar
//
//-----------------------------------------------------------------------------
template< class T > inline CDmaVar<T>::CDmaVar( )
{
	m_pAttribute = NULL;
	CDmAttributeInfo<T>::SetDefaultValue( m_Storage );
}

template< class T > inline void CDmaVar<T>::Init( CDmElement *pOwner, const char *pAttributeName, int flags  )
{
	Assert( pOwner );
	m_pAttribute = pOwner->AddExternalAttribute( pAttributeName, CDmAttributeInfo<T>::AttributeType(), &m_Storage );
	Assert( m_pAttribute );
	if ( flags )
	{
		m_pAttribute->AddFlag( flags );
	}
}

template< class T > inline void CDmaVar<T>::InitAndSet( CDmElement *pOwner, const char *pAttributeName, const T &value, int flags )
{
	this->Init( pOwner, pAttributeName );
	this->Set( value );

	// this has to happen AFTER set so the set happens before FATTRIB_READONLY
	if ( flags )
	{
		m_pAttribute->AddFlag( flags );
	}
}

template< class T > inline const T& CDmaVar<T>::Set( const T &val )
{
	Assert( m_pAttribute );
	m_pAttribute->SetValue( val );
	return m_Storage;
}

template< class T > inline const T& CDmaVar<T>::operator=( const T &val ) 
{
	return this->Set( val );
}

template< class T > inline const CDmaVar<T>& CDmaVar<T>::operator=( const CDmaVar<T>& src )
{
	this->Set( src.Get() );
	return *this;
}

template< class T > inline const T& CDmaVar<T>::operator+=( const T &val ) 
{
	return this->Set( this->Value() + val );
}

template< class T > inline const T& CDmaVar<T>::operator-=( const T &val ) 
{
	return this->Set( this->Value() - val );
}

template< class T > inline const T& CDmaVar<T>::operator/=( const T &val ) 
{
	return this->Set( this->Value() / val );
}

template< class T > inline const T& CDmaVar<T>::operator*=( const T &val ) 
{
	return this->Set( this->Value() * val );
}

template< class T > inline const T& CDmaVar<T>::operator^=( const T &val ) 
{
	return this->Set( this->Value() ^ val );
}

template< class T > inline const T& CDmaVar<T>::operator|=( const T &val ) 
{
	return this->Set( this->Value() | val );
}

template< class T > inline const T& CDmaVar<T>::operator&=( const T &val ) 
{	
	return this->Set( this->Value() & val );
}

template< class T > inline T CDmaVar<T>::operator++()
{
	return this->Set( this->Value() + 1 );
}

template< class T > inline T CDmaVar<T>::operator--()
{
	return this->Set( this->Value() - 1 );
}

template< class T > inline T CDmaVar<T>::operator++( int ) // postfix version..
{
	T oldValue = this->Value();
	this->Set( this->Value() + 1 );
	return oldValue;
}

template< class T > inline T CDmaVar<T>::operator--( int ) // postfix version..
{
	T oldValue = this->Value();
	this->Set( this->Value() - 1 );
	return oldValue;
}

template< class T > inline CDmaVar<T>::operator const T&() const 
{
	return this->Value(); 
}

template< class T > inline const T& CDmaVar<T>::Get() const 
{
	return this->Value(); 
}

template< class T > inline const T* CDmaVar<T>::operator->() const 
{
	return &this->Value(); 
}

template< class T > inline CDmAttribute *CDmaVar<T>::GetAttribute()
{
	Assert( m_pAttribute );
	return m_pAttribute;
}

template< class T > inline const CDmAttribute *CDmaVar<T>::GetAttribute() const
{
	Assert( m_pAttribute );
	return m_pAttribute;
}

template< class T > inline bool CDmaVar<T>::IsDirty() const
{
	Assert( m_pAttribute );
	return m_pAttribute->IsFlagSet( FATTRIB_DIRTY );
}

template< class T > inline const T& CDmaVar<T>::Value() const 
{ 
	return m_Storage; 
}

template< class T > inline T& CDmaVar<T>::Value() 
{ 
	return m_Storage; 
}

template<> inline const DmElementHandle_t& CDmaVar< DmElementHandle_t >::Value() const 
{
	return m_Storage.m_Handle;
}

template<> inline DmElementHandle_t& CDmaVar< DmElementHandle_t >::Value() 
{
	return m_Storage.m_Handle;
}

template< class T > inline const typename CDmaVar<T>::D& CDmaVar<T>::Storage() const 
{ 
	return m_Storage; 
}

template< class T > inline typename CDmaVar<T>::D& CDmaVar<T>::Storage() 
{ 
	return m_Storage; 
}


//-----------------------------------------------------------------------------
//
// Inline methods for CDmaColor
//
//-----------------------------------------------------------------------------
inline void CDmaColor::SetColor( int r, int g, int b, int a )
{
	Color clr( r, g, b, a );
	m_pAttribute->SetValue( clr );
}

inline void CDmaColor::SetRed( int r )
{
	Color org = this->Value();
	org[ 0 ] = r;
	m_pAttribute->SetValue( org );
}

inline void CDmaColor::SetGreen( int g )
{
	Color org = this->Value();
	org[ 1 ] = g;
	m_pAttribute->SetValue( org );
}

inline void CDmaColor::SetBlue( int b )
{
	Color org = this->Value();
	org[ 2 ] = b;
	m_pAttribute->SetValue( org );
}

inline void CDmaColor::SetAlpha( int a )
{
	Color org = this->Value();
	org[ 3 ] = a;
	m_pAttribute->SetValue( org );
}

inline unsigned char CDmaColor::r() const
{
	return (unsigned char)this->Value().r();
}

inline unsigned char CDmaColor::g() const
{
	return (unsigned char)this->Value().g();
}

inline unsigned char CDmaColor::b() const
{
	return (unsigned char)this->Value().b();
}

inline unsigned char CDmaColor::a() const
{
	return (unsigned char)this->Value().a();
}

inline const unsigned char &CDmaColor::operator[](int index) const
{
	return this->Value()[index];
}

inline void CDmaColor::SetRawColor( int color )
{
	Color clr;
	clr.SetRawColor( color );
	m_pAttribute->SetValue( clr );
}


//-----------------------------------------------------------------------------
//
// Inline methods for CDmaTime
//
//-----------------------------------------------------------------------------
inline void CDmaTime::SetTenthsOfMS( int tms )
{
	Set( DmeTime_t( tms ) );
}

inline void CDmaTime::SetSeconds( float seconds )
{
	Set( DmeTime_t( seconds ) );
}

inline int CDmaTime::GetTenthsOfMS() const
{
	return this->Value().GetTenthsOfMS();
}

inline float CDmaTime::GetSeconds() const
{
	return this->Value().GetSeconds();
}


//-----------------------------------------------------------------------------
//
// Inline methods for CDmaString
//
//-----------------------------------------------------------------------------
inline const char *CDmaString::Get( ) const
{
	return this->Value().String();
}

inline CDmaString::operator const char*() const
{
	return this->Value().String();
}


inline void CDmaString::InitAndSet( CDmElement *pOwner, const char *pAttributeName, const char *pValue, int flags )
{
	CUtlSymbolLarge symbol = g_pDataModel->GetSymbol( pValue );
	CDmaVar< CUtlSymbolLarge >::InitAndSet( pOwner, pAttributeName, symbol, flags );
}


inline void CDmaString::Set( const char *pValue )
{
	CUtlSymbolLarge symbol = g_pDataModel->GetSymbol( pValue );
	m_pAttribute->SetValue( symbol );
}

// Returns strlen
inline int CDmaString::Length() const
{
	return V_strlen( this->Value().String() );
}

inline bool CDmaString::IsEmpty() const
{
	if ( this->Value() == UTL_INVAL_SYMBOL_LARGE )
		return true;

	const char *pString = this->Value().String();
	if ( pString == NULL )
		return true;

	if ( *pString == 0 )
		return true;

	return false;
}

inline CDmaString &CDmaString::operator=( const char *src )
{
	Set( src );
	return *this;
}

inline const CDmaString& CDmaString::operator=( const CDmaString& src )
{
	Set( src.Get() );
	return *this;
}


//-----------------------------------------------------------------------------
//
// Inline methods for CDmaBinaryBlock
//
//-----------------------------------------------------------------------------
inline void CDmaBinaryBlock::Get( void *pValue, int nMaxLen ) const
{
	this->Value().Get( pValue, nMaxLen );
}

inline void CDmaBinaryBlock::Set( const void *pValue, int nLen )
{
	CUtlBinaryBlock block( pValue, nLen );
	m_pAttribute->SetValue( block );
}

inline const void *CDmaBinaryBlock::Get() const
{
	return this->Value().Get();
}

inline const unsigned char& CDmaBinaryBlock::operator[]( int i ) const
{
	return this->Value()[i];
}

inline int CDmaBinaryBlock::Length() const
{
	return this->Value().Length();
}


//-----------------------------------------------------------------------------
//
// Inline methods for CDmaElement
//
//-----------------------------------------------------------------------------
template <class T>
inline void CDmaElement<T>::InitAndCreate( CDmElement *pOwner, const char *pAttributeName, const char *pElementName, int flags )
{
	Init( pOwner, pAttributeName );

	DmElementHandle_t hElement = DMELEMENT_HANDLE_INVALID;
	if ( !g_pDataModel->IsUnserializing() )
	{
		hElement = g_pDataModel->CreateElement( T::GetStaticTypeSymbol(), pElementName, pOwner->GetFileId() );
	}
	Assert( this->m_pAttribute );
	this->m_pAttribute->SetValue( hElement );

	// this has to happen AFTER set so the set happens before FATTRIB_READONLY
	this->m_pAttribute->AddFlag( flags | FATTRIB_MUSTCOPY );
}

template <class T>
inline void CDmaElement<T>::Init( CDmElement *pOwner, const char *pAttributeName, int flags )
{
	BaseClass::Init( pOwner, pAttributeName );

	Assert( this->m_pAttribute );
	this->m_pAttribute->SetElementTypeSymbol( T::GetStaticTypeSymbol() );
	if ( flags )
	{
		this->m_pAttribute->AddFlag( flags );
	}
}

template <class T>
inline CUtlSymbolLarge CDmaElement<T>::GetElementType() const
{
	return this->Data().m_ElementType;
}

template <class T>
inline T* CDmaElement<T>::GetElement() const
{
	CDmElement *pElement = g_pDataModel->GetElement( this->Value() );
	Assert( !pElement || pElement->IsA( T::GetStaticTypeSymbol() ) );
	return static_cast< T* >( pElement );
}

template <class T>
inline T* CDmaElement<T>::operator->() const
{
	return GetElement();
}

template <class T>
inline CDmaElement<T>::operator T*() const
{
	return GetElement();
}

template <class T>
inline void CDmaElement<T>::Set( T* pElement )
{
	Assert( m_pAttribute );
	m_pAttribute->SetValue( pElement ? pElement->GetHandle() : DMELEMENT_HANDLE_INVALID );
}

template <class T>
inline bool CDmaElement<T>::operator!() const
{
	return ( GetElement() == NULL );
}


//-----------------------------------------------------------------------------
//
// Inline methods for CDmaArrayBase
//
//-----------------------------------------------------------------------------
template< class T, class B >
inline const CUtlVector<T>& CDmaArrayConstBase<T,B>::Get() const
{
	return this->Value();
}

template< class T, class B >
inline const T *CDmaArrayConstBase<T,B>::Base() const
{
	return this->Value().Base();
}

template< class T, class B >
inline const T& CDmaArrayConstBase<T,B>::operator[]( int i ) const
{
	return this->Value()[ i ];
}

template< class T, class B >
const T& CDmaArrayConstBase<T,B>::Element( int i ) const
{
	return this->Value()[ i ];
}

template< class T, class B >
inline const T& CDmaArrayConstBase<T,B>::Get( int i ) const
{
	return this->Value()[ i ];
}

template< class T, class B >
const void* CDmaArrayConstBase<T,B>::GetUntyped( int i ) const
{
	return &( this->Value()[ i ] );
}

template< class T, class B >
inline int CDmaArrayConstBase<T,B>::Count() const
{
	return this->Value().Count();
}

template< class T, class B >
inline bool CDmaArrayConstBase<T,B>::IsValidIndex( int i ) const
{
	return this->Value().IsValidIndex( i );
}

template< class T, class B >
inline int CDmaArrayConstBase<T,B>::InvalidIndex( void ) const
{
	return this->Value().InvalidIndex();
}

template< class T, class B >
inline const CDmAttribute *CDmaArrayConstBase<T,B>::GetAttribute() const
{
	Assert( m_pAttribute );
	return m_pAttribute;
}

template< class T, class B >
inline CDmElement *CDmaArrayConstBase<T,B>::GetOwner()
{
	return m_pAttribute->GetOwner();
}

template< class T, class B >
inline bool CDmaArrayConstBase<T,B>::IsDirty() const
{
	return m_pAttribute->IsFlagSet( FATTRIB_DIRTY );
}


template< class T, class B >
inline CDmAttribute *CDmaArrayBase<T,B>::GetAttribute()
{
	Assert( this->m_pAttribute );
	return this->m_pAttribute;
}

template< class T, class B >
inline const CDmAttribute *CDmaArrayBase<T,B>::GetAttribute() const
{
	Assert( this->m_pAttribute );
	return this->m_pAttribute;
}


//-----------------------------------------------------------------------------
//
// Inline methods for CDmaStringArrayBase
//
//-----------------------------------------------------------------------------
template< class B >
inline const char *CDmaStringArrayConstBase<B>::operator[]( int i ) const
{
	return this->Value()[ i ].String();
}

template< class B >
inline const char *CDmaStringArrayConstBase<B>::Element( int i ) const
{
	return this->Value()[ i ].String();
}

template< class B >
inline const char *CDmaStringArrayConstBase<B>::Get( int i ) const
{
	return this->Value()[ i ].String();
}

template< class B >
inline const CUtlVector< CUtlSymbolLarge > &CDmaStringArrayConstBase<B>::Get() const
{
	return this->Value();
}

// Returns strlen of element i
template< class B >
inline int CDmaStringArrayConstBase<B>::Length( int i ) const
{
	return this->Value()[i].Length();
}

template< class B >
inline void CDmaStringArrayBase<B>::Set( int i, const char * pValue )
{
	CUtlSymbolLarge symbol = g_pDataModel->GetSymbol( pValue );
	BaseClass::Set( i, symbol );
}

// Adds an element, uses copy constructor
template< class B >
inline int CDmaStringArrayBase<B>::AddToTail( const char *pValue )
{
	CUtlSymbolLarge symbol = g_pDataModel->GetSymbol( pValue );
	return BaseClass::AddToTail( symbol );
}

template< class B >
inline int CDmaStringArrayBase<B>::InsertBefore( int elem, const char *pValue )
{
	CUtlSymbolLarge symbol = g_pDataModel->GetSymbol( pValue );
	return BaseClass::InsertBefore( elem, symbol );
}


//-----------------------------------------------------------------------------
//
// Inline methods for CDmaElementArrayBase
//
//-----------------------------------------------------------------------------
template< class E, class B > 
inline CUtlSymbolLarge CDmaElementArrayConstBase<E,B>::GetElementType() const
{
	return this->Data().m_ElementType;
}

template< class E, class B >
inline E *CDmaElementArrayConstBase<E,B>::operator[]( int i ) const
{
	return GetElement<E>( this->Value()[i] );
}

template< class E, class B >
inline E *CDmaElementArrayConstBase<E,B>::Element( int i ) const
{
	return GetElement<E>( this->Value()[i] );
}

template< class E, class B >
inline E *CDmaElementArrayConstBase<E,B>::Get( int i ) const
{
	return GetElement<E>( this->Value()[i] );
}

template< class E, class B >
inline const DmElementHandle_t& CDmaElementArrayConstBase<E,B>::GetHandle( int i ) const
{
	return this->Value()[i];
}

template< class E, class B >
inline const CUtlVector< DmElementHandle_t > &CDmaElementArrayConstBase<E,B>::Get() const
{
	return this->Value();
}

// Search
template< class E, class B >
inline int CDmaElementArrayConstBase<E,B>::Find( const E *pValue ) const
{
	if ( !pValue )
		return -1;
	return B::Find( pValue->GetHandle() );
}

template< class E, class B >
inline int CDmaElementArrayConstBase<E,B>::Find( DmElementHandle_t h ) const
{
	return B::Find( h );
}

template< class E, class B >
inline void CDmaElementArrayBase<E,B>::SetHandle( int i, DmElementHandle_t h )
{
	BaseClass::Set( i, h );
}

template< class E, class B >
inline void CDmaElementArrayBase<E,B>::Set( int i, E *pElement )
{
	BaseClass::Set( i, pElement ? pElement->GetHandle() : DMELEMENT_HANDLE_INVALID );
}

// Adds an element, uses copy constructor
template< class E, class B >
inline int CDmaElementArrayBase<E,B>::AddToTail( )
{
	return BaseClass::AddToTail( );
}

template< class E, class B >
inline int CDmaElementArrayBase<E,B>::AddToTail( E *pValue )
{
	return BaseClass::AddToTail( pValue ? pValue->GetHandle() : DMELEMENT_HANDLE_INVALID );
}

template< class E, class B >
inline int CDmaElementArrayBase<E,B>::AddToTail( DmElementHandle_t src )
{
	return BaseClass::AddToTail( src );
}

template< class E, class B >
inline int CDmaElementArrayBase<E,B>::InsertBefore( int elem )
{
	return BaseClass::InsertBefore( elem );
}

template< class E, class B >
inline int CDmaElementArrayBase<E,B>::InsertBefore( int elem, E *pValue )
{
	return BaseClass::InsertBefore( elem, pValue ? pValue->GetHandle() : DMELEMENT_HANDLE_INVALID );
}

template< class E, class B >
inline int CDmaElementArrayBase<E,B>::InsertBefore( int elem, DmElementHandle_t src )
{
	return BaseClass::InsertBefore( elem, src );
}



//-----------------------------------------------------------------------------
//
// Inline methods for CDmrGenericArray
//
//-----------------------------------------------------------------------------
inline const CDmAttribute *CDmrGenericArrayConst::GetAttribute() const
{
	Assert( m_pAttribute );
	return m_pAttribute;
}

inline bool CDmrGenericArrayConst::IsValid() const
{
	return m_pAttribute != NULL;
}

inline CDmAttribute *CDmrGenericArray::GetAttribute()
{
	Assert( m_pAttribute );
	return m_pAttribute;
}

inline const CDmAttribute *CDmrGenericArray::GetAttribute() const
{
	Assert( m_pAttribute );
	return m_pAttribute;
}


#endif // DMATTRIBUTEVAR_H
