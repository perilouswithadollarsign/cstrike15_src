/// @file response_system.cpp
/// This file defines the editor's version 
/// of a response-system. 

#ifndef CS_RESPONSE_SYSTEM_H
#define CS_RESPONSE_SYSTEM_H

#include "UtlSortVector.h"

class ResponseSystemImplementationCLI : public ResponseRules::CResponseSystem
{
public:

#pragma region Overrides on CResponseSystem
	/// From ResponseRules::CResponseSystem.
	/// There, it returns the filename to load; here
	/// it is NULL, since the file comes from the editor
	/// dialog.
	virtual const char *GetScriptFile( void ) ;

	virtual void PrecacheResponses( bool bEnable );
	virtual void Release();

	inline void LoadFromFile( const char *filename );
#pragma endregion

	int CountRules() ;

	/// USed to return a sorted list of all rules matching the criteria in order of score (not just the best)
	struct RuleAndScorePair_t
	{
		ResponseRules::ResponseRulePartition::tIndex ruleidx;
		float score;

		RuleAndScorePair_t( const ResponseRules::ResponseRulePartition::tIndex &_idx, float _score ) : ruleidx(_idx), score(_score) {};
		RuleAndScorePair_t( ) : ruleidx(ResponseRules::ResponseRulePartition::InvalidIdx()) {};

		struct LessFunc
		{
			// actually "more" since sort from best to worst score
			bool Less( const RuleAndScorePair_t & lhs, const RuleAndScorePair_t & rhs, void *pContext )
			{
				if ( lhs.score == rhs.score )
				{
					return lhs.ruleidx < rhs.ruleidx;
				}
				else
				{
					return lhs.score > rhs.score;
				}
			}
		};
	};
	typedef CUtlSortVector<RuleAndScorePair_t, RuleAndScorePair_t::LessFunc> FindAllRulesRetval_t;
	void FindAllRulesMatchingCriteria(  FindAllRulesRetval_t* RESTRICT outputList, 
		const ResponseRules::CriteriaSet& set, ResponseRules::IResponseFilter *pFilter = NULL );
	
};


inline void ResponseSystemImplementationCLI::LoadFromFile( const char *filename )
{
	Clear();
	return LoadRuleSet( filename );
}


#endif