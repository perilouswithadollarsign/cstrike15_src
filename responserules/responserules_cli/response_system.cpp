/// @file response_system.cpp
/// This file contains the unmanaged code implementing the editor's version 
/// of a response-system.  

#include "stdafx.h"
using namespace ResponseRules;

const char *ResponseSystemImplementationCLI::GetScriptFile( void ) 
{
	return NULL;
}

#pragma managed(push, off)

void ResponseSystemImplementationCLI::PrecacheResponses( bool bEnable )
{
	// precaching is meaningless in the editor
	Assert(false);
}

void ResponseSystemImplementationCLI::Release( )
{
	// precaching is meaningless in the editor
	Assert(false);
}

int ResponseSystemImplementationCLI::CountRules() 
{
	return m_RulePartitions.Count();
}

/// Resets the output vector and overwrites it entirely.
/// <remarks>
/// Meant to be the same algorithm as CResponseSystem::FindBestMatchingRule().
/// </remarks>
void ResponseSystemImplementationCLI::FindAllRulesMatchingCriteria( CUtlSortVector<RuleAndScorePair_t, RuleAndScorePair_t::LessFunc> * RESTRICT outputList, const CriteriaSet& set, IResponseFilter *pFilter /*= NULL */ )
{
	outputList->RemoveAll();
	outputList->EnsureCapacity(16);

	ResponseRulePartition::tRuleDict &rules = m_RulePartitions.GetDictForCriteria( set );
	int c = rules.Count();
	int i;
	for ( i = 0; i < c; i++ )
	{
		float score = ScoreCriteriaAgainstRule( set, rules, i, false );
		outputList->Insert( RuleAndScorePair_t( m_RulePartitions.IndexFromDictElem( &rules, i ), score ));
	}

}

#pragma managed(pop)
