// This is the main DLL file.

#include "stdafx.h"

extern int TestRandomNumberGeneration( int bottom, int top ) ;
extern const char *TestFileSystemHook( ) ;

using namespace ResponseRulesCLI;

 namespace MikeCTest
 {
       public ref class Person
	   {
       private:
            String^ _name;
       public:
            Person( String^ name )
            {
                 _name = name;
            }
            String^ Description()
            {
				// this is the big test!
				int rndNum = TestRandomNumberGeneration( 0, 10 );
				String^ foo = gcnew String(TestFileSystemHook());
				
                return ( "My name is " + _name + 
					     "  filesystem is " + foo
					     // "   my ID is " + rndNum.ToString() 
						 );
            }

			static bool HopefullyDontCrash()
		   {
				TestFileSystemHook( );
				return true;
		   }
       };

 }


#include "response_system.h"



StrToAnsi::StrToAnsi( String ^unicodestr )
{
	m_pStr = (char*)(void*)System::Runtime::InteropServices::Marshal::StringToHGlobalAnsi(unicodestr);
}

StrToAnsi::~StrToAnsi( )
{
	System::Runtime::InteropServices::Marshal::FreeHGlobal((System::IntPtr)(void*)m_pStr);
}

StrToAnsi::operator TCHAR *() const
{
	return m_pStr;
}

/*
using namespace System::Runtime::InteropServices; // for class Marshal

void PrintMessage(System::String^ str)
{
	const char* str2 = (char*)(void*)Marshal::StringToHGlobalAnsi(str);
	printf(str2);
	Marshal::FreeHGlobal((System::IntPtr)(void*)str2);
}

PrintMessage("Method 2");
*/


using namespace ResponseRulesCLI;

// Allocate the native object on the C++ Heap via a constructor
ResponseSystemCLI::ResponseSystemCLI() 
{
	m_pImpl = new ResponseSystemImplementationCLI();
	m_CriteriaDict = gcnew CriterionDictWrapper_t(&m_pImpl->m_Criteria);
	m_ResponseGroupsDict = gcnew ResponseGroupDictWrapper_t(&m_pImpl->m_Responses);
	m_RulesContainer = gcnew RulesAsList(this);
}

// Deallocate the native object on a destructor
ResponseSystemCLI::~ResponseSystemCLI() 
{
	this->!ResponseSystemCLI();
}

// Deallocate the native object on the finalizer just in case no destructor is called
ResponseSystemCLI::!ResponseSystemCLI() 
{
	delete m_pImpl;
}


void ResponseSystemCLI::LoadFromFile( String^ filename )
{
	return m_pImpl->LoadFromFile( StrToAnsi(filename) );
}


int ResponseSystemCLI::CountRules()
{
	return m_pImpl->CountRules();
}

ResponseSystemImplementationCLI *ResponseSystemCLI::GetNativePtr()
{
	return m_pImpl;
}

void ResponseSystemCLI::RulesAsList::RulesIterator::Reset()
{
	m_bIsBefore = true;
	m_idx = m_System->GetNativePtr()->m_RulePartitions.First();
}

bool ResponseSystemCLI::RulesAsList::RulesIterator::MoveNext()
{
	if (m_bIsBefore)
	{
		m_bIsBefore = false;
		m_idx = m_System->GetNativePtr()->m_RulePartitions.First();
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
			// stick the index on the stack temporarily so we can pass a reference to it
			ResponseRules::ResponseRulePartition::tIndex tmp = m_idx;
			m_idx = m_System->GetNativePtr()->m_RulePartitions.Next(tmp);
			return IsValid();
		}
	}
}

bool ResponseSystemCLI::RulesAsList::RulesIterator::IsValid()
{
	// stick the index on the stack temporarily so we can pass a reference to it
	ResponseRules::ResponseRulePartition::tIndex tmp = m_idx;
	return m_System->GetNativePtr()->m_RulePartitions.IsValid( tmp );
}

Object^ ResponseSystemCLI::RulesAsList::RulesIterator::Current::get()
{
	if (IsValid())
	{
		ResponseRules::ResponseRulePartition::tIndex i = m_idx;
		return gcnew Rule(&m_System->GetNativePtr()->m_RulePartitions[i],
			i,
			m_System->GetNativePtr()->m_RulePartitions.GetElementName(i)  );
	}
	else
	{
		throw gcnew System::InvalidOperationException();
	}
}

/*
/// access to the dictionary of index->criteria
Criterion ^ ResponseSystemCLI::Criteria::get( short key )
{
	return gcnew Criterion(&m_pImpl->m_Criteria[key], key);
}
unsigned int ResponseSystemCLI::CriteriaCount::get() 
{
	return m_pImpl->m_Criteria.Count();
}
*/

Rule ^ ResponseSystemCLI::FindBestMatchingRule( System::Collections::IDictionary ^facts )
{
	ResponseRules::CriteriaSet criteria;
	TurnIDictIntoCriteriaSet( facts, &criteria );
	float bestscore; // of matching rule
	ResponseRules::ResponseRulePartition::tIndex bestRuleIndex = m_pImpl->FindBestMatchingRule( criteria, false, bestscore );

	if ( m_pImpl->m_RulePartitions.IsValid( bestRuleIndex ) )
	{
		return RuleFromIdx(bestRuleIndex);
	}
	else
	{
		return nullptr;
	}
}

Rule ^ResponseSystemCLI::RuleFromIdx( ResponseRules::ResponseRulePartition::tIndex idx )
{
	return gcnew Rule( &m_pImpl->m_RulePartitions[idx], 
		idx, 
		m_pImpl->m_RulePartitions.GetElementName(idx) );
}

void ResponseSystemCLI::TurnIDictIntoCriteriaSet( System::Collections::IDictionary ^facts, ResponseRules::CriteriaSet *critset )
{
	// for each key and value in the dictionary, add to set.
	for each ( System::Collections::DictionaryEntry pair in facts )
	{
		critset->AppendCriteria( StrToAnsi(pair.Key->ToString()), StrToAnsi(pair.Value->ToString())  );
	}
}

array< System::Collections::Generic::KeyValuePair<Rule ^,float> >^ ResponseSystemCLI::FindAllRulesMatchingCriteria( System::Collections::IDictionary ^facts )
{
	ResponseRules::CriteriaSet crits;
	TurnIDictIntoCriteriaSet( facts, &crits );
	ResponseSystemImplementationCLI::FindAllRulesRetval_t ruleresults;
	m_pImpl->FindAllRulesMatchingCriteria( &ruleresults, crits );
	if ( ruleresults.Count() < 1 )
	{
		// return empty array.
		return gcnew array< System::Collections::Generic::KeyValuePair<Rule ^,float> >(0);
	}
	else
	{
		const int count = ruleresults.Count();
		array< System::Collections::Generic::KeyValuePair<Rule ^,float> >^ retval = gcnew array< System::Collections::Generic::KeyValuePair<Rule ^,float> >(count);
		for (int i = 0 ; i < count ; ++i )
		{
			const ResponseSystemImplementationCLI::RuleAndScorePair_t &pair = ruleresults[i];
			retval[i] = System::Collections::Generic::KeyValuePair<Rule ^,float>(RuleFromIdx(pair.ruleidx),pair.score);

			/*
			retval[i].Key = RuleFromIdx(pair.ruleidx);
			retval[i].Value = pair.score;
			*/
		}
		return retval;
	}
}


/*
#pragma unmanaged
#include "../../responserules/runtime/response_types_internal.h"
#pragma managed
*/
