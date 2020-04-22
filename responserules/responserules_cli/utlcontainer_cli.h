//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Wrappers to turn various Source Utl* containers into CLR 
// enumerables.
//
// $NoKeywords: $
//=============================================================================//

#ifndef UTLCONTAINER_CLI_H
#define UTLCONTAINER_CLI_H

#if defined( _WIN32 )
#pragma once
#endif

/// Handy wrapper to turn any indexing property into 
/// an IList (assuming it is numbered 0..count). You 
/// need only initialize it with a delegate that yields
/// int->type and another that gets the count.
generic< typename T >
public ref class IndexPropertyToIListReadOnly : public System::Collections::Generic::IList<T>
{
public:
	delegate T lGetter( int idx ); // will wrap the indexer
	delegate int lCounter( ); // will wrap the counter

	IndexPropertyToIListReadOnly( lGetter ^getter, lCounter ^counter  )
	{
		m_getterFunc = getter;
		m_counterFunc = counter;
	}

	/*
	virtual ~IndexPropertyToIListReadOnly( ) { !IndexPropertyToIListReadOnly( ); }
	virtual !IndexPropertyToIListReadOnly( ) {}
	*/

	property int Count
	{
		virtual int get() { return m_counterFunc(); }
	}

#pragma region IList<T> Members

	virtual int IndexOf(T item)
	{
		int count = Count::get();
		for ( int i = 0 ; i < count ; ++i )
		{
			if ( item->Equals( m_getterFunc(i) ) )
				return i;
		}
		return -1;
	}

	virtual void Insert(int index, T item)
	{
		throw gcnew NotSupportedException( "Read-only." );
	}

	virtual void RemoveAt(int index)
	{
		throw gcnew NotSupportedException("Read-only.");
	}

	property T default[int]
	{
		virtual T get(int index)
		{
			if ( index < 0 || index > Count::get() )
			{
				throw gcnew ArgumentOutOfRangeException();
			}
			else
			{
				return m_getterFunc(index);
			}

		}
		virtual void set(int index, T to)
		{
			throw gcnew NotSupportedException("Read-only.");
		}
	}

#pragma endregion


#pragma region ICollection<T> Members

	virtual void Add(T item)
	{
		throw gcnew NotSupportedException("Read-only.");
	}

	virtual void Clear()
	{
		throw gcnew NotSupportedException("Read-only.");
	}

	virtual bool Contains(T item)
	{
		return IndexOf(item) != -1;
	}

	virtual void CopyTo( cli::array<T,1> ^arr, int start)
	{
		int stop = Count::get(); 
		for (int i = 0 ; i < stop ; ++i )
		{
			arr->SetValue((*this)[i],start+i);
		}
		// throw gcnew NotImplementedException();
	}

	property bool IsReadOnly
	{
		virtual bool get() { return true; }
	}

	virtual bool Remove(T item)
	{
		throw gcnew NotSupportedException("Read-only.");
	}

#pragma endregion

#pragma region Enumerator
	ref class LinearEnumerator : System::Collections::Generic::IEnumerator<T>
	{
		// Enumerators are positioned before the first element
		// until the first MoveNext() call.
		int position;
	public:
		LinearEnumerator(IndexPropertyToIListReadOnly<T> ^owner)
		{
			m_owner = owner;
			position = -1;
		}
		~LinearEnumerator(){};
		!LinearEnumerator(){};

		virtual bool MoveNext()
		{
			position++;
			return ( position < m_owner->Count );
		}

		virtual void Reset()
		{
			position = -1;
		}

		virtual property T Current
		{
			virtual T get() = System::Collections::Generic::IEnumerator<T>::Current::get
			{
				if ( position >= 0 &&  position < m_owner->Count )
				{
					return m_owner[position];
				}
				else
				{
					throw gcnew InvalidOperationException();
				}
			}
		}

		virtual property System::Object^ CurrentAgainBecauseCPP_CLISucks
		{
			virtual System::Object^ get() = System::Collections::IEnumerator::Current::get
			{
				if ( position >= 0 &&  position < m_owner->Count )
				{
					return m_owner[position];
				}
				else
				{
					throw gcnew InvalidOperationException();
				}
			}
		}

		IndexPropertyToIListReadOnly<T> ^m_owner;
	};
#pragma endregion

#pragma region IEnumerable<T> Members

	virtual System::Collections::Generic::IEnumerator<T> ^ GetEnumerator()
	{
		return gcnew LinearEnumerator(this);
	}

	virtual System::Collections::IEnumerator^ GetEnumerator2() = System::Collections::IEnumerable::GetEnumerator
	{
		return gcnew LinearEnumerator(this);
	}


#pragma endregion


protected:
	lGetter ^m_getterFunc;
	lCounter ^m_counterFunc;
};

#if 0
/// <summary>
/// Tiny class that wraps an indexing property in a class with an IList interface
/// so that the WPF databinding can access it. 
/// </summary>
/// <remarks>
/// Assumes that all indexes are from 0..count.
/// </remarks>
/// <typeparam name="U"> The type of the class whose property we wrap </typeparam>
/// <typeparam name="T"> The type of the value returned from the class property </typeparam>
public class BindingWrapper<T> : IList<T>, INotifyCollectionChanged
{
	// lambda types. You'll pass in one of each of these with the constructor.
	/// <summary>
	/// Given an int, return the i-th element of wrapper property in owning class.
	/// </summary>
	public delegate T lGetter( int idx );

	/// <summary>
	/// Given an int, return the i-th element of wrapper property in owning class.
	/// </summary>
	public delegate int lCounter( );

	public BindingWrapper( /*U owner,*/ lGetter getter, lCounter counter )
	{
		// m_owner  = owner;
		m_getterFunc = getter;
		m_counterFunc = counter;
	}

#region IList<T> Members

	public int IndexOf(T item)
	{
		throw new NotImplementedException();
		/*
		// hang onto this number
		int count = Count;
		for (int i = 0 ; i < count ; ++i  )
		{
		if (this[i] == item)
		return i;
		}
		return -1;
		*/
	}

	public void Insert(int index, T item)
	{
		throw new NotSupportedException( "Read-only." );
	}

	public void RemoveAt(int index)
	{
		throw new NotSupportedException("Read-only.");
	}

	public T this[int index]
	{
		get
		{
			if (index < 0 || index > Count)
			{
				throw new ArgumentOutOfRangeException();
			}
			else
			{
				return m_getterFunc(index);
			}

		}
		set
		{
			throw new NotSupportedException("Read-only.");
		}
	}

#endregion

#region ICollection<T> Members

	public void Add(T item)
	{
		throw new NotSupportedException("Read-only.");
	}

	public void Clear()
	{
		throw new NotSupportedException("Read-only.");
	}

	public bool Contains(T item)
	{
		throw new NotImplementedException();
	}

	public void CopyTo(T[] array, int arrayIndex)
	{
		throw new NotSupportedException("Noncopyable.");
	}

	public int Count
	{
		get { return m_counterFunc(); }
	}

	public bool IsReadOnly
	{
		get { return true; }
	}

	public bool Remove(T item)
	{
		throw new NotSupportedException("Read-only.");
	}

#endregion


#region Enumerator
	public class LinearEnumerator : System.Collections.IEnumerator
	{
		// Enumerators are positioned before the first element
		// until the first MoveNext() call.
		int position = -1;

		public LinearEnumerator(BindingWrapper<T> owner)
		{
			m_owner = owner;
		}

		public bool MoveNext()
		{
			position++;
			return ( position < m_owner.Count );
		}

		public void Reset()
		{
			position = -1;
		}

		public Object Current
		{
			get
			{
				try
				{
					return m_owner[position];
				}
				catch (IndexOutOfRangeException)
				{
					throw new InvalidOperationException();
				}
			}
		}

		BindingWrapper<T> m_owner;
	}
#endregion

#region IEnumerable<T> Members

	public IEnumerator<T> GetEnumerator()
	{
		throw new NotImplementedException(); // return new LinearEnumerator(this);
	}

#endregion


#region IEnumerable Members

	System.Collections.IEnumerator System.Collections.IEnumerable.GetEnumerator()
	{
		return new LinearEnumerator(this);
	}

#endregion

#region INotifyCollectionChanged
	public event NotifyCollectionChangedEventHandler CollectionChanged;
	public virtual void OnCollectionChanged(NotifyCollectionChangedEventArgs e)
	{
		if (CollectionChanged != null)
		{
			CollectionChanged(this, e);
		}
	}
#endregion

#region Private data
	// U m_owner;
	lGetter m_getterFunc;
	lCounter m_counterFunc;
#endregion
}
#endif


/// Common code for classes that wrap native RR
/// types. You have the option to make a COPY 
/// rather than a pointer to the native class:
/// in that case, the constructor here new's a copy
/// of the given type on the heap, and the finalizer
/// deletes it.
/// It may be necessary to specialize the constructor
/// in certain cases where a managed function is not
/// allowed to new the given type.
/// Subclass this with a CLI version of the wrapped
/// class and create whatever accessor properties you 
/// would like to expose to the managed side.
template< typename T >
public ref class NativeTypeCopyWrapper
{
public:
	NativeTypeCopyWrapper(  ) : m_pNative(NULL), m_bIsCopy(true) {};
	NativeTypeCopyWrapper( T* from ) // this overload assumes no copy
	{
		m_bIsCopy = false;
		if ( false )
		{
			m_pNative = new T( *from );
		}
		else
		{
			m_pNative = from;
		}
	}
	NativeTypeCopyWrapper( T* from, bool bCopy )
	{
		m_bIsCopy = bCopy;
		if ( bCopy )
		{
			m_pNative = new T( *from );
		}
		else
		{
			m_pNative = from;
		}
	}
	~NativeTypeCopyWrapper() { this->!NativeTypeCopyWrapper(); }
	!NativeTypeCopyWrapper() 
	{ 
		if ( m_bIsCopy )
			delete m_pNative; 

		m_pNative = NULL; 
	}
	// copy constructor
	NativeTypeCopyWrapper(NativeTypeCopyWrapper% r) 
	{
		m_bIsCopy = r->m_bIsCopy;
		if (m_bIsCopy)
			m_pNative = new T( *r->GetNativePtr() );
		else
			m_pNative = r->GetNativePtr();
	}

	inline bool IsValid( ) { return m_pNative != NULL; }

	/// use the assignment operator on the internal native type, copying
	/// over the one in the given FROM class, and  make me point to it.
	/// So named to be explicit about what's happening, and because I'm
	/// not sure what happens when you = on a ref.
	void Assign( NativeTypeCopyWrapper^ from )
	{
		*m_pNative = *from.m_pNative;
	}

	/// please be careful
	T* GetNativePtr() { return m_pNative; }

protected:
	T* m_pNative;
	bool m_bIsCopy;
};



// CUtlDict as enumerable.
#include "utldict.h"

/*
template <class T, class I> 
class CUtlDict
*/

namespace Tier1CLI
{
	namespace Containers
	{
		public interface class INotifiableList 
			:	public System::Collections::IList ,
				public System::Collections::Specialized::INotifyCollectionChanged
		{
			virtual void OnCollectionChanged(System::Collections::Specialized::NotifyCollectionChangedEventArgs ^e) = 0;
		};
	}
};

/// <summary>
/// A class that wraps a tier1 CUtlDict in a way that can be iterated over from C#.
/// </summary>
/// <remarks>
/// The CONVERTOR template parameter below shall be the class
/// that wraps the native C++ * type into a CLR reference type.
/// It should have a constructor that takes a parameter of type
/// T* , which will be called like:
/// return gcnew CONVERTOR(T *foo);
/// </remarks>
template <class T, typename I, class CONVERTOR = NativeTypeCopyWrapper<T> >
public ref class CCLIUtlDictEnumerable :  // public System::Collections::IEnumerable, 
	public Tier1CLI::Containers::INotifiableList
{
public:
	typedef CUtlDict< T,I > dict_t;
	typedef CONVERTOR value_t;
	typedef const char * key_t;

#pragma region Enumeration interface
	ref struct tuple_t
	{
		key_t key;
		value_t ^val;
		I index;

		inline tuple_t( const key_t &k, T &v, I i ) :  key(k), index(i)
		{
			val = gcnew CONVERTOR( &v, i );
		};

		// conversions to CLR types
		property System::String^ Name
		{
			System::String^ get()
			{
				return gcnew String(key);
			}
		}
		property CONVERTOR^ Val
		{
			CONVERTOR^ get()
			{
				return val;
			}
		}
		property I Index
		{
			I get()
			{
				return index;
			}
		}
	};

	/*
	// convenience functions for WPF databindings, which may hand back a 
	// tuple_t object to C#, which can't get at the typedefs above.
	// this can cause trouble with getting at the properties; rather than
	// use reflection, you can just use these.
	System::String^ NameFor( tuple_t ^ tup )
	{
		return tup->Name;
	}
	value_t^ ValFor( tuple_t ^ tup )
	{
		return tup->Val;
	}
	*/

	CONVERTOR ^GetValue( I idx )
	{
		return gcnew CONVERTOR(m_pInnerDict->Element(idx));
	}

	CONVERTOR ^GetKey( I idx )
	{
		return gcnew String(m_pInnerDict->GetElementName(idx));
	}

	property int Count
	{
		virtual int get() { return m_pInnerDict->Count(); }
	}

	/// Iterator type. Walks over the UtlDict in the same order as its
	/// internal iterator. Returns a tuple of <key,value>, where key is
	/// always a string type (as in the utldict).
	/// TODO: can I make this a value struct so it doesn't need to be
	/// boxed/unboxed?
	ref struct Enumerator : public System::Collections::IEnumerator
	{
		typedef CCLIUtlDictEnumerable<T,I> owner_t;

		Enumerator( dict_t *pDict ) : m_pEnumeratedDict(pDict), m_bIsBefore(true) 
		{
			m_index = dict_t::InvalidIndex();
		}

		inline bool IsValid()
		{
			return m_pEnumeratedDict->IsValidIndex(m_index);
		}

		// IEnumerator interface
		virtual void Reset()
		{
			m_bIsBefore = true;
			m_index = m_pEnumeratedDict->First();
		}

		// return false when falling off the end
		virtual bool MoveNext()
		{
			if (m_bIsBefore)
			{
				m_bIsBefore = false;
				m_index = m_pEnumeratedDict->First();
				return IsValid();
			}
			else
			{
				if ( !IsValid() )
				{
					return false;
				}
				else
				{
					m_index = m_pEnumeratedDict->Next( m_index );
					return IsValid();
				}
			}
		}

		property System::Object^ Current
		{
			virtual System::Object^ get()
			{
				if ( IsValid() )
				{
					return gcnew tuple_t( m_pEnumeratedDict->GetElementName(m_index), 
						m_pEnumeratedDict->Element(m_index),
						m_index);
				}
				else
				{
					throw gcnew System::InvalidOperationException();
				}	
			}
		};

		// data:
	protected:
		I m_index;
		dict_t *m_pEnumeratedDict;
		bool m_bIsBefore;
	};

	virtual System::Collections::IEnumerator^ GetEnumerator() 
	{
		return gcnew Enumerator(m_pInnerDict);
	}
#pragma endregion

	bool IsValidIndex( I idx ) { return m_pInnerDict->IsValidIndex(idx); }
	tuple_t ^GetElement( I i ) 
	{
		return gcnew tuple_t( m_pInnerDict->GetElementName(i), 
			m_pInnerDict->Element(i),
			i);
	}

#pragma region ILIST interface

	virtual int IndexOf( System::Object ^obj )
	{
		tuple_t ^t = dynamic_cast< tuple_t ^>(obj);
		if (t)
		{
			return t->index;
		}
		else
		{
			throw gcnew System::ArrayTypeMismatchException();
		}
	}

	virtual bool Contains( System::Object ^obj )
	{
		tuple_t ^t = dynamic_cast< tuple_t ^>(obj);
		if (t)
		{
			return IsValidIndex(t->index);
		}
		else
		{
			throw gcnew System::ArrayTypeMismatchException();
		}
	}

	virtual void Insert(int index, System::Object ^ item)
	{
		throw gcnew NotSupportedException( "Read-only." );
	}

	virtual void Remove(System::Object ^)
	{
		throw gcnew NotSupportedException("Read-only.");
	}

	virtual void RemoveAt(int index)
	{
		throw gcnew NotSupportedException("Read-only.");
	}

	virtual int Add(System::Object ^o)
	{
		throw gcnew NotSupportedException("Read-only.");
	}

	virtual void Clear()
	{
		throw gcnew NotSupportedException("Read-only.");
	}

	virtual property Object ^ SyncRoot
	{
		Object ^ get() { return this; }
	}

	virtual void CopyTo(Array ^arr, int start)
	{
		int stop = Count::get(); 
		for (int i = 0 ; i < stop ; ++i )
		{
			arr->SetValue((*this)[i],start+i);
		}
		// throw gcnew NotImplementedException();
	}

	property System::Object ^ default[int]
	{
		virtual System::Object ^get( int index )
		{
			if (index < 0 || index > Count)
			{
				throw gcnew ArgumentOutOfRangeException();
			}
			else
			{
				return GetElement(index);
			}

		}
		virtual void set(int idx, System::Object ^s)
		{
			throw gcnew NotSupportedException("Read-only.");
		}
	}

	property bool IsReadOnly
	{
		virtual bool get() { return true; }
	}

	property bool IsFixedSize
	{
		virtual bool get() { return true; }
	}

	property bool IsSynchronized
	{
		virtual bool get() { return true; }
	}

#pragma endregion


#pragma region INotifyCollectionChanged interface

	System::Collections::Specialized::NotifyCollectionChangedEventHandler ^m_CollectionChanged;
	virtual event System::Collections::Specialized::NotifyCollectionChangedEventHandler ^CollectionChanged
	{
		void add(System::Collections::Specialized::NotifyCollectionChangedEventHandler ^ d) {
			m_CollectionChanged += d;
		}
		void remove(System::Collections::Specialized::NotifyCollectionChangedEventHandler ^ d) {
			m_CollectionChanged -= d;
		}
	}
	virtual void OnCollectionChanged(System::Collections::Specialized::NotifyCollectionChangedEventArgs ^e)
	{
		if (m_CollectionChanged != nullptr)
		{
			m_CollectionChanged(this, e);
		}
	}
#pragma endregion


	/// construct with a pointer at a UtlDict. Does not 
	/// own the pointer; simply stores it internally.
	CCLIUtlDictEnumerable( dict_t *pInnerDict ) 
		//: m_CollectionChanged(gcnew System::Collections::Specialized::NotifyCollectionChangedEventHandler )  
	{ 
		Init(pInnerDict); 
	};
	CCLIUtlDictEnumerable( ) :m_pInnerDict(NULL)// 
		//,m_CollectionChanged(gcnew System::Collections::Specialized::NotifyCollectionChangedEventHandler )  
	{ };

	void Init( dict_t *pInnerDict )
	{
		m_pInnerDict = pInnerDict;
	}

protected:
	dict_t *m_pInnerDict;
};




#endif