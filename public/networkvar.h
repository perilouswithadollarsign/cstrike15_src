//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef NETWORKVAR_H
#define NETWORKVAR_H
#ifdef _WIN32
#pragma once
#endif


#include "tier0/dbg.h"
#include "tier0/platform.h"
#include "convar.h"

#if defined( CLIENT_DLL ) || defined( GAME_DLL )
	#include "basehandle.h"
#endif


#pragma warning( disable : 4284 ) // warning C4284: return type for 'CNetworkVarT<int>::operator ->' is 'int *' (ie; not a UDT or reference to a UDT.  Will produce errors if applied using infix notation)

#define MyOffsetOf( type, var ) ( (int)(intp)&((type*)0)->var )

#ifdef _DEBUG
	#undef new
	extern bool g_bUseNetworkVars;
	#define CHECK_USENETWORKVARS if(g_bUseNetworkVars)

	// Use START_ and END_CHECK_USENETWORKVARS if your code is longer than one line.
	#define START_CHECK_USENETWORKVARS if(g_bUseNetworkVars) {
	#define END_CHECK_USENETWORKVARS }
#else
	#define CHECK_USENETWORKVARS // don't check for g_bUseNetworkVars
	#define START_CHECK_USENETWORKVARS
	#define END_CHECK_USENETWORKVARS
#endif



//
// Networkvar flags.
//
#define NETWORKVAR_IS_A_VECTOR				0x0001		// Is it any type of network vector?
#define NETWORKVAR_VECTOR_XYZ_FLAG			0x0002		// Is it a CNetworkVectorXYZ?
#define NETWORKVAR_VECTOR_XY_SEPARATEZ_FLAG	0x0004		// Is it a CNetworkVectorXY_SeparateZ?

#define NETWORKVAR_ALL_FLAGS ( NETWORKVAR_IS_A_VECTOR | NETWORKVAR_VECTOR_XYZ_FLAG | NETWORKVAR_VECTOR_XY_SEPARATEZ_FLAG )



// network vars use memcmp when fields are set.  To ensure proper behavior your
// object's memory should be initialized to zero.  This happens for entities automatically
// use this for other classes.
class CMemZeroOnNew
{
public:
	void *operator new( size_t nSize )
	{
		void *pMem = MemAlloc_Alloc( nSize );
		V_memset( pMem, 0, nSize );
		return pMem;
	}

	void* operator new( size_t nSize, int nBlockUse, const char *pFileName, int nLine )
	{
		void *pMem = MemAlloc_Alloc( nSize, pFileName, nLine );
		V_memset( pMem, 0, nSize );
		return pMem;
	}

	void operator delete(void *pData)
	{
		if ( pData )
		{
			g_pMemAlloc->Free(pData);
		}
	}

	void operator delete( void* pData, int nBlockUse, const char *pFileName, int nLine )
	{
		if ( pData )
		{
			g_pMemAlloc->Free(pData, pFileName, nLine );
		}
	}
};


inline int InternalCheckDeclareClass( const char *pClassName, const char *pClassNameMatch, void *pTestPtr, void *pBasePtr )
{
	// This makes sure that casting from ThisClass to BaseClass works right. You'll get a compiler error if it doesn't
	// work at all, and you'll get a runtime error if you use multiple inheritance.
	Assert( pTestPtr == pBasePtr );
	
	// This is triggered by IMPLEMENT_SERVER_CLASS. It does DLLClassName::CheckDeclareClass( #DLLClassName ).
	// If they didn't do a DECLARE_CLASS in DLLClassName, then it'll be calling its base class's version
	// and the class names won't match.
	Assert( (void*)pClassName == (void*)pClassNameMatch );
	return 0;
}


template <typename T> 
inline int CheckDeclareClass_Access( T *, const char *pShouldBe )
{
	return T::CheckDeclareClass( pShouldBe );
}

#ifndef _STATIC_LINKED
#ifdef _MSC_VER
#if defined(_DEBUG) && (_MSC_VER > 1200 )
	#define VALIDATE_DECLARE_CLASS 1
#endif
#endif
#endif

#ifdef  VALIDATE_DECLARE_CLASS

	#define DECLARE_CLASS( className, baseClassName ) \
		typedef baseClassName BaseClass; \
		typedef className ThisClass; \
		template <typename T> friend int CheckDeclareClass_Access(T *, const char *pShouldBe); \
		static int CheckDeclareClass( const char *pShouldBe ) \
		{ \
			InternalCheckDeclareClass( pShouldBe, #className, (ThisClass*)0xFFFFF, (BaseClass*)(ThisClass*)0xFFFFF ); \
			return CheckDeclareClass_Access( (BaseClass *)NULL, #baseClassName ); \
		}

	// Use this macro when you have a base class, but it's part of a library that doesn't use network vars
	// or any of the things that use ThisClass or BaseClass.
	#define DECLARE_CLASS_GAMEROOT( className, baseClassName ) \
		typedef baseClassName BaseClass; \
		typedef className ThisClass; \
		template <typename T> friend int CheckDeclareClass_Access(T *, const char *pShouldBe); \
		static int CheckDeclareClass( const char *pShouldBe ) \
		{ \
			return InternalCheckDeclareClass( pShouldBe, #className, (ThisClass*)0xFFFFF, (BaseClass*)(ThisClass*)0xFFFFF ); \
		}

	// Deprecated macro formerly used to work around VC++98 bug
	#define DECLARE_CLASS_NOFRIEND( className, baseClassName ) \
		DECLARE_CLASS( className, baseClassName )

	#define DECLARE_CLASS_NOBASE( className ) \
		typedef className ThisClass; \
		template <typename T> friend int CheckDeclareClass_Access(T *, const char *pShouldBe); \
		static int CheckDeclareClass( const char *pShouldBe ) \
		{ \
			return InternalCheckDeclareClass( pShouldBe, #className, 0, 0 ); \
		} 

#else
	#define DECLARE_CLASS( className, baseClassName ) \
		typedef baseClassName BaseClass; \
		typedef className ThisClass;

	#define DECLARE_CLASS_GAMEROOT( className, baseClassName )	DECLARE_CLASS( className, baseClassName )
	#define DECLARE_CLASS_NOFRIEND( className, baseClassName )	DECLARE_CLASS( className, baseClassName )

	#define DECLARE_CLASS_NOBASE( className )					typedef className ThisClass;
#endif




// All classes that contain CNetworkVars need a NetworkStateChanged() function. If the class is not an entity,
// it needs to forward the call to the entity it's in. These macros can help.
	
	// These macros setup an entity pointer in your class. Use IMPLEMENT_NETWORKVAR_CHAIN before you do
	// anything inside the class itself.
	class CBaseEntity;
	class CAutoInitEntPtr
	{
	public:
		CAutoInitEntPtr()
		{
			m_pEnt = NULL;
		}
		CBaseEntity *m_pEnt;
	};

	//TODO: Currently, these don't get the benefit of tracking changes to individual vars.
	// Would be nice if they did.
	#define DECLARE_NETWORKVAR_CHAIN() \
		CAutoInitEntPtr __m_pChainEntity; \
		void NetworkStateChanged() { CHECK_USENETWORKVARS __m_pChainEntity.m_pEnt->NetworkStateChanged(); } \
		void NetworkStateChanged( void *pVar ) { CHECK_USENETWORKVARS __m_pChainEntity.m_pEnt->NetworkStateChanged(); }

	#define IMPLEMENT_NETWORKVAR_CHAIN( varName ) \
		(varName)->__m_pChainEntity.m_pEnt = this;



// Use this macro when you want to embed a structure inside your entity and have CNetworkVars in it.
template< class T >
static inline void DispatchNetworkStateChanged( T *pObj )
{
	CHECK_USENETWORKVARS pObj->NetworkStateChanged();
}
template< class T >
static inline void DispatchNetworkStateChanged( T *pObj, void *pVar )
{
	CHECK_USENETWORKVARS pObj->NetworkStateChanged( pVar );
}

#define DECLARE_EMBEDDED_NETWORKVAR() \
	template <typename T> friend int ServerClassInit( T * ); \
	template <typename T> friend int ClientClassInit( T * ); \
	virtual void NetworkStateChanged() {} virtual void NetworkStateChanged( void *pProp ) {}

template < typename T, typename ContainingClass, typename GetOffset >
class NetworkVarEmbedded : public T
{
private:
	// NOTE: Assignment operator is disabled because it doesn't call copy constructors of scalar types within the aggregate, so they are not marked changed
	template< class U > NetworkVarEmbedded& operator=( U &&val ) = delete;

	T& Data() { return *this; }
	const T& Data() const { return *this; }

public:
	operator const T& () const { return Data(); }
	
	// $$$REI TODO: Why doesn't this implementation work as an assignment operator?
	template< typename U >
	void CopyFrom( U&& src ) {
		Data() = Forward<U>( src );
		NetworkStateChanged();
	}

	const T* Get( void ) const { return &Data(); }
	T* Get( void ) { return &Data(); }

	ContainingClass* GetEmbeddedVarOuterPtr() { return ( ContainingClass* )( ( ( char* )this ) - GetOffset::Get() ); }
	virtual void NetworkStateChanged() 
	{
		ContainingClass *pOuter = GetEmbeddedVarOuterPtr();
		DispatchNetworkStateChanged( pOuter );
	}

	virtual void NetworkStateChanged( void *pVar )
	{
		ContainingClass *pOuter = GetEmbeddedVarOuterPtr();
		DispatchNetworkStateChanged( pOuter, pVar );
	}
};

#define CNetworkVarEmbedded( type, name ) \
    struct GetOffset_##name{ static FORCEINLINE int Get() { return MyOffsetOf( ThisClass, name ); } }; \
	typedef NetworkVarEmbedded< type, ThisClass, GetOffset_##name > NetworkVar_##name; \
	friend class NetworkVarEmbedded< type, ThisClass, GetOffset_##name >; \
	NetworkVar_##name name;

// Zero the object -- necessary for CNetworkVar and possibly other cases.
template<typename T>
FORCEINLINE void EnsureValidValue( T &x ) { x = T(0); }
// For types that cannot compile the line above (such as QAngle, Vector, etc.)
// define a EnsureValidValue overload in the appropriate header files.

class CNetworkVarFlagsBase
{
public:
	// A combination of NETWORKVAR_ flags. SendProps store these for additional information
	// about what kind of CNetworkVar the SendProp is linked to.
	static FORCEINLINE int GetNetworkVarFlags() { return 0; }
};


// when we box ints into floats, the comparison of floats doesn't work

template <typename T>
inline bool NetworkParanoidUnequal( const T &a, const T &b )
{
	return a != b;
}

template <>
inline bool NetworkParanoidUnequal( const float &a, const float &b )
{
	//return 0 != memcmp( &a, &b, sizeof( a ) );
	union
	{
		float f32;
		uint32 u32;
	} p, q;
	p.f32 = a;
	q.f32 = b;
	return p.u32 != q.u32;
}

template <>
inline bool NetworkParanoidUnequal( const double &a, const double &b )
{
	// return 0 != memcmp( &a, &b, sizeof( a ) );
	union
	{
		double f64;
		uint64 u64;
	} p, q;
	p.f64 = a;
	q.f64 = b;
	return p.u64 != q.u64;
}

template< class Type, class Changer >
class CNetworkVarBase : public CNetworkVarFlagsBase
{
public:
	CNetworkVarBase()
	{
		// Always initialize m_Value. Not doing this means that the initial behavior
		// is random. For float types this random behavior can persist arbitrarily
		// long because of our non-IEEE comparisons which may make NaN be equal to
		// everything.
		EnsureValidValue( m_Value );
	}

	FORCEINLINE explicit CNetworkVarBase( Type val )
	: m_Value( val )
	{
		NetworkStateChanged();
	}

	FORCEINLINE const Type& SetDirect( const Type &val )
	{
		NetworkStateChanged();
		m_Value = val;
		return m_Value;
	}

	FORCEINLINE const Type& Set( const Type &val )
	{
		if ( NetworkParanoidUnequal( m_Value, val ) )
		{
			NetworkStateChanged();
			m_Value = val;
		}
		return m_Value;
	}
	
	template< class C >
	FORCEINLINE const Type& operator=( const C &val ) 
	{ 
		return Set( ( const Type )val ); 
	}

	template< class C >
	FORCEINLINE const Type& operator=( const CNetworkVarBase< C, Changer > &val ) 
	{ 
		return Set( ( const Type )val.m_Value ); 
	}

	FORCEINLINE Type& GetForModify()
	{
		NetworkStateChanged();
		return m_Value;
	}

	template< class C >
	FORCEINLINE const Type& operator+=( const C &val ) 
	{
		return Set( m_Value + ( const Type )val ); 
	}

	template< class C >
	FORCEINLINE const Type& operator-=( const C &val ) 
	{
		return Set( m_Value - ( const Type )val ); 
	}
	
	template< class C >
	FORCEINLINE const Type& operator/=( const C &val ) 
	{
		return Set( m_Value / ( const Type )val ); 
	}
	
	template< class C >
	FORCEINLINE const Type& operator*=( const C &val ) 
	{
		return Set( m_Value * ( const Type )val ); 
	}
	
	template< class C >
	FORCEINLINE const Type& operator^=( const C &val ) 
	{
		return Set( m_Value ^ ( const Type )val ); 
	}

	template< class C >
	FORCEINLINE const Type& operator|=( const C &val ) 
	{
		return Set( m_Value | ( const Type )val ); 
	}

	FORCEINLINE const Type& operator++()
	{
		return (*this += 1);
	}

	FORCEINLINE Type operator--()
	{
		return (*this -= 1);
	}
	
	FORCEINLINE Type operator++( int ) // postfix version..
	{
		Type val = m_Value;
		(*this += 1);
		return val;
	}

	FORCEINLINE Type operator--( int ) // postfix version..
	{
		Type val = m_Value;
		(*this -= 1);
		return val;
	}
	
	// For some reason the compiler only generates type conversion warnings for this operator when used like 
	// CNetworkVarBase<unsigned char> = 0x1
	// (it warns about converting from an int to an unsigned char).
	template< class C >
	FORCEINLINE const Type& operator&=( const C &val ) 
	{	
		return Set( m_Value & ( const Type )val ); 
	}

	FORCEINLINE operator const Type&() const 
	{
		return m_Value; 
	}
	
	FORCEINLINE const Type& Get() const 
	{
		return m_Value; 
	}
	
	FORCEINLINE const Type* operator->() const 
	{
		return &m_Value; 
	}

	Type m_Value;

protected:
	FORCEINLINE void NetworkStateChanged()
	{
		Changer::NetworkStateChanged( this );
	}
	
	FORCEINLINE void NetworkStateChanged( void *pVar )
	{
		Changer::NetworkStateChanged( this, pVar );
	}
};



#include "networkvar_vector.h"



template< class Type, class Changer >
class CNetworkColor32Base : public CNetworkVarBase< Type, Changer >
{
	typedef CNetworkVarBase< Type, Changer > base;
public:
	inline void Init( byte rVal, byte gVal, byte bVal )
	{
		SetR( rVal );
		SetG( gVal );
		SetB( bVal );
	}
	inline void Init( byte rVal, byte gVal, byte bVal, byte aVal )
	{
		SetR( rVal );
		SetG( gVal );
		SetB( bVal );
		SetA( aVal );
	}

	const Type& operator=( const Type &val ) 
	{ 
		return this->Set( val );
	}

	const Type& operator=( const CNetworkColor32Base<Type,Changer> &val ) 
	{ 
		return base::Set( val.m_Value );
	}
	
	inline byte GetR() const { return this->m_Value.r; }
	inline byte GetG() const { return this->m_Value.g; }
	inline byte GetB() const { return this->m_Value.b; }
	inline byte GetA() const { return this->m_Value.a; }
	inline void SetR( byte val ) { SetVal( this->m_Value.r, val ); }
	inline void SetG( byte val ) { SetVal( this->m_Value.g, val ); }
	inline void SetB( byte val ) { SetVal( this->m_Value.b, val ); }
	inline void SetA( byte val ) { SetVal( this->m_Value.a, val ); }

protected:
	inline void SetVal( byte &out, const byte &in )
	{
		if ( out != in )
		{
			this->NetworkStateChanged();
			out = in;
		}
	}
};


// Network quaternion wrapper.
template< class Type, class Changer >
class CNetworkQuaternionBase : public CNetworkVarBase< Type, Changer >
{
	typedef CNetworkVarBase< Type, Changer > base;
public:
	inline void Init( float ix=0, float iy=0, float iz=0, float iw = 0 ) 
	{
		base::Set( Quaternion( ix, iy, iz, iw ) );
	}
	
	const Type& operator=( const Type &val ) 
	{ 
		return Set( val ); 
	}

	const Type& operator=( const CNetworkQuaternionBase<Type,Changer> &val ) 
	{ 
		return Set( val.m_Value );
	}

	inline float GetX() const { return this->m_Value.x; }
	inline float GetY() const { return this->m_Value.y; }
	inline float GetZ() const { return this->m_Value.z; }
	inline float GetW() const { return this->m_Value.w; }
	inline float operator[]( int i ) const { return this->m_Value[i]; }

	inline void SetX( float val ) { DetectChange( this->m_Value.x, val ); }
	inline void SetY( float val ) { DetectChange( this->m_Value.y, val ); }
	inline void SetZ( float val ) { DetectChange( this->m_Value.z, val ); }
	inline void SetW( float val ) { DetectChange( this->m_Value.w, val ); }
	inline void Set( int i, float val ) { DetectChange( this->m_Value[i], val ); }

	bool operator==( const Type &val ) const 
	{ 
		return !NetworkParanoidUnequal( this->m_Value, ( Type )val );
	}

	bool operator!=( const Type &val ) const 
	{
		return NetworkParanoidUnequal( this->m_Value, val );
	}

	const Type operator+( const Type &val ) const 
	{
		return this->m_Value + val; 
	}

	const Type operator-( const Type &val ) const
	{ 
		return this->m_Value - val; 
	}

	const Type operator*( const Type &val ) const
	{
		return this->m_Value * val; 
	}

	const Type& operator*=( float val )
	{
		return Set( this->m_Value * val );
	}

	const Type operator*( float val ) const
	{
		return this->m_Value * val; 
	}

	const Type operator/( const Type &val ) const
	{
		return this->m_Value / val; 
	}

private:
	inline void DetectChange( float &out, float in ) 
	{
		if ( NetworkParanoidUnequal( out, in ) )
		{
			this->NetworkStateChanged();
			out = in;
		}
	}
};


// Network ehandle wrapper.
#if defined( CLIENT_DLL ) || defined( GAME_DLL )
	template< class Type, class Changer >
	class CNetworkHandleBase : public CNetworkVarBase< CBaseHandle, Changer >
	{
		typedef CNetworkVarBase< CBaseHandle, Changer > base;
	public:
		const Type* operator=( const Type *val ) 
		{ 
			return Set( val ); 
		}
			
		const Type& operator=( const CNetworkHandleBase<Type,Changer> &val ) 
		{ 
			const CBaseHandle &handle = CNetworkVarBase<CBaseHandle,Changer>::Set( val.m_Value );
			return *(const Type*)handle.Get();
		}

		bool operator !() const 
		{ 
			return !this->m_Value.Get(); 
		}
		
		operator Type*() const 
		{ 
			return static_cast< Type* >( this->m_Value.Get() );
		}

		const Type* Set( const Type *val )
		{
			if ( CNetworkHandleBase<Type,Changer>::m_Value != val )
			{
				this->NetworkStateChanged();
				CNetworkHandleBase<Type,Changer>::m_Value = val;
			}
			return val;
		}
		
		Type* Get() const 
		{ 
			return static_cast< Type* >( CNetworkHandleBase<Type,Changer>::m_Value.Get() );
		}

		Type* operator->() const 
		{ 
			return static_cast< Type* >( CNetworkHandleBase<Type,Changer>::m_Value.Get() );
		}

		bool operator==( const Type *val ) const 
		{
			return CNetworkHandleBase<Type,Changer>::m_Value == val; 
		}

		bool operator!=( const Type *val ) const 
		{
			return CNetworkHandleBase<Type,Changer>::m_Value != val;
		}
	};



	#define CNetworkHandle( type, name ) CNetworkHandleInternal( type, name, NetworkStateChanged )

	#define CNetworkHandleInternal( type, name, stateChangedFn ) \
		NETWORK_VAR_START( type, name ) \
		NETWORK_VAR_END( type, name, CNetworkHandleBase, stateChangedFn )
#endif


// Use this macro to define a network variable.
#define CNetworkVar( type, name ) \
	NETWORK_VAR_START( type, name ) \
	NETWORK_VAR_END( type, name, CNetworkVarBase, NetworkStateChanged )


// Use this macro when you have a base class with a variable, and it doesn't have that variable in a SendTable,
// but a derived class does. Then, the entity is only flagged as changed when the variable is changed in
// an entity that wants to transmit the variable.
	#define CNetworkVarForDerived( type, name ) \
		virtual void NetworkStateChanged_##name() {} \
		virtual void NetworkStateChanged_##name( void *pVar ) {} \
		NETWORK_VAR_START( type, name ) \
		NETWORK_VAR_END( type, name, CNetworkVarBase, NetworkStateChanged_##name )

	#define CNetworkHandleForDerived( type, name ) \
		virtual void NetworkStateChanged_##name() {} \
		virtual void NetworkStateChanged_##name( void *pVar ) {} \
		CNetworkHandleInternal( type, name, NetworkStateChanged_##name )
		
	#define CNetworkArrayForDerived( type, name, count ) \
		virtual void NetworkStateChanged_##name() {} \
		virtual void NetworkStateChanged_##name( void *pVar ) {} \
		CNetworkArrayInternal( type, name, count, NetworkStateChanged_##name )

	#define IMPLEMENT_NETWORK_VAR_FOR_DERIVED( name ) \
		virtual void NetworkStateChanged_##name() { CHECK_USENETWORKVARS NetworkStateChanged(); } \
		virtual void NetworkStateChanged_##name( void *pVar ) { CHECK_USENETWORKVARS NetworkStateChanged( pVar ); }


// This virtualizes the change detection on the variable, but it is ON by default.
// Use this when you have a base class in which MOST of its derived classes use this variable
// in their SendTables, but there are a couple that don't (and they
// can use DISABLE_NETWORK_VAR_FOR_DERIVED).
	#define CNetworkVarForDerived_OnByDefault( type, name ) \
		virtual void NetworkStateChanged_##name() { CHECK_USENETWORKVARS NetworkStateChanged(); } \
		virtual void NetworkStateChanged_##name( void *pVar ) { CHECK_USENETWORKVARS NetworkStateChanged( pVar ); } \
		NETWORK_VAR_START( type, name ) \
		NETWORK_VAR_END( type, name, CNetworkVarBase, NetworkStateChanged_##name )

	#define DISABLE_NETWORK_VAR_FOR_DERIVED( name ) \
		virtual void NetworkStateChanged_##name() {} \
		virtual void NetworkStateChanged_##name( void *pVar ) {}



#define CNetworkQuaternion( name ) \
	NETWORK_VAR_START( Quaternion, name ) \
	NETWORK_VAR_END( Quaternion, name, CNetworkQuaternionBase, NetworkStateChanged )

// Helper for color32's. Contains GetR(), SetR(), etc.. functions.
#define CNetworkColor32( name ) \
	NETWORK_VAR_START( color32, name ) \
	NETWORK_VAR_END( color32, name, CNetworkColor32Base, NetworkStateChanged )


#define CNetworkString( name, length ) \
	class NetworkVar_##name; \
	friend class NetworkVar_##name; \
	typedef ThisClass MakeANetworkVar_##name; \
	class NetworkVar_##name : public CNetworkVarFlagsBase\
	{ \
	public: \
		operator const char*() const { return m_Value; } \
		const char* Get() const { return m_Value; } \
		char* GetForModify() \
		{ \
			NetworkStateChanged(); \
			return m_Value; \
		} \
	protected: \
		inline void NetworkStateChanged() \
		{ \
			START_CHECK_USENETWORKVARS \
			ThisClass *pThis = ((ThisClass*)(((char*)this) - MyOffsetOf(ThisClass,name))); \
			pThis->NetworkStateChanged(); \
			END_CHECK_USENETWORKVARS \
		} \
	private: \
		char m_Value[length]; \
	}; \
	typedef NetworkVar_##name NetworkVarType_##name; \
	NetworkVar_##name name;




// Use this to define networked arrays.
// You can access elements for reading with operator[], and you can set elements with the Set() function.
#define CNetworkArrayInternal( type, name, count, stateChangedFn ) \
	class NetworkVar_##name; \
	friend class NetworkVar_##name; \
	typedef ThisClass MakeANetworkVar_##name; \
	class NetworkVar_##name \
	{ \
	public: \
		template <typename T> friend int ServerClassInit(T *);	\
		const type& operator[]( int i ) const \
		{ \
			return Get( i ); \
		} \
		\
		const type& Get( int i ) const \
		{ \
			Assert( i >= 0 && i < count ); \
			return m_Value[i]; \
		} \
		\
		type& GetForModify( int i ) \
		{ \
			Assert( i >= 0 && i < count ); \
			NetworkStateChanged( i ); \
			return m_Value[i]; \
		} \
		\
		void Set( int i, const type &val ) \
		{ \
			Assert( i >= 0 && i < count ); \
			if( memcmp( &m_Value[i], &val, sizeof(type) ) ) \
			{ \
				NetworkStateChanged( i ); \
			       	m_Value[i] = val; \
			} \
		} \
		const type* Base() const { return m_Value; } \
		int Count() const { return count; } \
		type m_Value[count]; \
	protected: \
		inline void NetworkStateChanged( int index ) \
		{ \
			START_CHECK_USENETWORKVARS \
			ThisClass *pThis = ((ThisClass*)(((char*)this) - MyOffsetOf(ThisClass,name))); \
			pThis->stateChangedFn( &m_Value[index] ); \
			END_CHECK_USENETWORKVARS \
		} \
	}; \
	NetworkVar_##name name;


#define CNetworkArray( type, name, count )  CNetworkArrayInternal( type, name, count, NetworkStateChanged )


// Internal macros used in definitions of network vars.
#define NETWORK_VAR_START( networkVarType, networkVarName ) \
	class NetworkVar_##networkVarName; \
	friend class NetworkVar_##networkVarName; \
	typedef ThisClass MakeANetworkVar_##networkVarName; \
	class NetworkVar_##networkVarName \
	{ \
	public: \
		template <typename T> friend int ServerClassInit(T *);


// pChangedPtr could be the Y or Z component of a vector, so possibly != to pNetworkVar.
#define NETWORK_VAR_END( networkVarType, networkVarName, baseClassName, stateChangedFn ) \
	public: \
		static inline void NetworkStateChanged( void *pNetworkVar, void *pChangedPtr ) \
		{ \
			START_CHECK_USENETWORKVARS \
			ThisClass *pThis = ( (ThisClass*)(((char*)pNetworkVar) - MyOffsetOf(ThisClass,networkVarName)) ); \
			pThis->stateChangedFn( pChangedPtr ); \
			END_CHECK_USENETWORKVARS \
		} \
		static inline void NetworkStateChanged( void *pNetworkVar ) \
		{ \
			NetworkStateChanged( pNetworkVar, pNetworkVar ); \
		} \
	}; \
	typedef baseClassName< networkVarType, NetworkVar_##networkVarName > NetworkVarType_##networkVarName; \
	baseClassName< networkVarType, NetworkVar_##networkVarName > networkVarName;



#endif // NETWORKVAR_H
