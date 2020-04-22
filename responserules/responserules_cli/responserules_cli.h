// responserules_cli.h

#pragma once

using namespace System;


#pragma unmanaged
namespace ResponseRules
{
	/// forward declare unmanaged implementation
	class CriteriaSet;
}
#pragma managed


#include "response_types_marshal.h"

namespace ResponseRulesCLI {

	typedef CCLIUtlDictEnumerable< ResponseRules::Criteria, short, ResponseRulesCLI::Criterion > CriterionDictWrapper_t;
	typedef CCLIUtlDictEnumerable< ResponseRules::ResponseGroup, short, ResponseRulesCLI::ResponseGroup > ResponseGroupDictWrapper_t;
	// enumerations aren't used right now it seems.
	// typedef CCLIUtlDictEnumerable< ResponseRules::CResponseSystem::Enumeration, short, System::Single  > EnumerationDictWrapper_t;
	


	/// Encapsulates an entire response system based on a file,
	/// containing responses, rules, criteria, and other data.
	public ref class ResponseSystemCLI
	{
	public:
		// Allocate the native object on the C++ Heap via a constructor
		ResponseSystemCLI();

		// Deallocate the native object on a destructor
		~ResponseSystemCLI() ;

		/// Load this system from a talker file.
		virtual void LoadFromFile( String^ filename );
		int CountRules();

		property ResponseGroupDictWrapper_t ^ResponseGroupsDict
		{
			ResponseGroupDictWrapper_t ^get() { return m_ResponseGroupsDict; }
		}

		property CriterionDictWrapper_t ^CriteriaDict
		{
			CriterionDictWrapper_t ^get() { return m_CriteriaDict; }
		}

		/*
		property EnumerationDictWrapper_t ^EnumerationsDict
		{
			EnumerationDictWrapper_t ^get() { return m_EnumerationsDict; }
		}
		*/

		/// interface to enumerate rules.
		ref class RulesAsList : public System::Collections::IEnumerable, System::Collections::Specialized::INotifyCollectionChanged
		{
		public:

			ref struct RulesIterator : System::Collections::IEnumerator
			{
			public:
				RulesIterator( ResponseSystemCLI^ enumerable ) : 
				  m_System(enumerable), m_bIsBefore(false), m_idx(ResponseRules::ResponseRulePartition::InvalidIdx()) {Reset();};
				  virtual void Reset();
				  virtual bool MoveNext(); // return false when falling off the end

				  property Object^ Current
				  {
					  virtual Object^ get();
				  };

			protected:
				bool m_bIsBefore; // is "one before" everything in the array, required by iterator interface
				bool IsValid() ;
				ResponseSystemCLI ^m_System; ///< the system I iterate
				ResponseRules::ResponseRulePartition::tIndex m_idx;
			};

			RulesAsList( ResponseSystemCLI^ owner ) : m_owner(owner) {};

			property int Count
			{
				int get() { return m_owner->GetNativePtr()->CountRules(); }
			}

			virtual System::Collections::IEnumerator^ GetEnumerator() 
			{
				return gcnew RulesIterator(m_owner);
			}

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

		private:
			ResponseSystemCLI^ m_owner;
		};

		property RulesAsList^ Rules
		{
			RulesAsList^ get() { return m_RulesContainer; };
		}

#pragma region "Queries Into Response System"

		/// Given a dictionary of key:value pairs (the contexts and their values),
		/// find the best matching rule.
		Rule ^FindBestMatchingRule( System::Collections::IDictionary ^facts );

		/// Find *all* rules that match the given criteria, in sorted order of best to worst.
		array< System::Collections::Generic::KeyValuePair<Rule ^,float> >^ FindAllRulesMatchingCriteria( System::Collections::IDictionary ^facts );

#pragma endregion

		/// please be careful
		ResponseSystemImplementationCLI *GetNativePtr();

	protected:
		// Deallocate the native object on the finalizer just in case no destructor is called
		!ResponseSystemCLI() ;
		ResponseSystemImplementationCLI *m_pImpl;

		void TurnIDictIntoCriteriaSet( System::Collections::IDictionary ^facts, ResponseRules::CriteriaSet *critset );
		Rule ^RuleFromIdx( ResponseRules::ResponseRulePartition::tIndex idx );

	private:
		CriterionDictWrapper_t ^m_CriteriaDict;
		ResponseGroupDictWrapper_t ^m_ResponseGroupsDict;
		// EnumerationDictWrapper_t ^m_EnumerationsDict;
		RulesAsList ^m_RulesContainer;
	};
}