//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Marhsalled types for native response-rules data.
//			Rather than have the CLR try to access the response rules natively,
//			we copy the data out from the native types to these garbage-collected
//			CLI types. 
//			This is manually kept in sync with the native types in response_types.h
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "response_types_marshal.h"

#pragma unmanaged
#include "../../responserules/runtime/response_types_internal.h"
#pragma managed

using namespace ResponseRulesCLI;



// ============================================================= //
//    Matcher
// ============================================================= //


String^ Matcher::Token::get()
{
	return gcnew String(m_pNative->GetToken());
}

String^ Matcher::RawSource::get()
{
	return gcnew String(m_pNative->GetRaw());
}

// Duped from
// ResponseRules::Matcher::Describe

String^ Matcher::Description::get()
{
	if ( !m_pNative->valid )
	{
		return gcnew String("Invalid!");
	}
	System::Text::StringBuilder sb("", 128);

	int minmaxcount = 0;
	if ( m_pNative->usemin )
	{
		sb.AppendFormat( ">{0}{1:f3}", m_pNative->minequals ? "=" : "", m_pNative->minval );
		minmaxcount++;
	}
	if ( m_pNative->usemax )
	{
		sb.AppendFormat( "{0}<{1}{2:f3}", 
			minmaxcount > 0 ? " and " : "",
			m_pNative->maxequals ? "=" : "",
			m_pNative->maxval );

		minmaxcount++;
	}

	if ( minmaxcount >= 1 )
	{
		return sb.ToString();
	}
	else if ( m_pNative->notequal )
	{
		return (gcnew String("!="))->Concat(Token);  
	}
	else
	{
		return (gcnew String("=="))->Concat(Token);  
	}
}

// ============================================================= //
//    ResponseParams
// ============================================================= //


String ^ ResponseRulesCLI::responseparams_interval_t::ToString()
{
	if ( range > 0 )
	{
		System::Text::StringBuilder sb("", 16);
		sb.AppendFormat("[{0:f1}+{1:f1}]", start, range );
		return sb.ToString();
	}
	else if ( start > 0 )
	{
		System::Text::StringBuilder sb("", 16);
		sb.AppendFormat("{0:f1}", start );
		return sb.ToString();
	}
	else
	{
		return gcnew String("0");
	}
}


// unmanaged thunk for below
#pragma unmanaged
ResponseRules::ResponseParams *CopyResponseParams( ResponseRules::ResponseParams *pSourceNativeParams  )
{
	return new ResponseRules::ResponseParams( *pSourceNativeParams ); 
}
#pragma managed


responseparams_interval_t::responseparams_interval_t( const ResponseRules::responseparams_interval_t &from )
{
	start = from.start;
	range = from.range;
}

responseparams_interval_t ResponseParams::delay::get()
{
	return responseparams_interval_t( m_pNative->delay );
}

responseparams_interval_t ResponseParams::respeakdelay::get()
{
	return responseparams_interval_t( m_pNative->respeakdelay );
}

responseparams_interval_t ResponseParams::weapondelay::get()
{
	return responseparams_interval_t( m_pNative->weapondelay );
}

responseparams_interval_t ResponseParams::predelay::get()
{
	return responseparams_interval_t( m_pNative->predelay );
}


short	ResponseParams::odds::get()	
{
	return m_pNative->odds;
}

unsigned short	ResponseParams::flags::get()	
{
	return m_pNative->flags;
}

byte ResponseParams::soundlevel::get()
{
	return m_pNative->soundlevel;
}



// ============================================================= //
//    Response
// ============================================================= //

/// A string containing the filename of a .vcd, the name of a sound script, etc.
String^ Response::value::get()
{
	return gcnew String( m_pNative->value );
}

/// This response's relative weight in a rule.
float Response::weight::get()
{
	return m_pNative->weight.GetFloat();
}

/// Used to track how many times a response can get said
byte Response::depletioncount::get()
{
	return m_pNative->depletioncount;
}

/// What kind of response this is
Response::ResponseType Response::type::get()
{
	return ResponseType(m_pNative->type);
}

/// Special flags that can be specified in the response
bool Response::DisplayFirst::get()
{
	return m_pNative->first;
}

bool Response::DisplayLast::get()
{
	return m_pNative->last;
}


// ============================================================= //
//    ResponseGroup
// ============================================================= //


// indexer goes into the cutlvector inside the group
Response ^ ResponseGroup::default::get( int idx )
{
	// Response ^frotz = gcnew Response(& m_pNative->group[idx] );
	// return gcnew Response(&m_pNative->group[idx]);
	return m_shadowResponseArray[idx];
}

#pragma unmanaged
static void AssignParserResponse( ResponseRules::ParserResponse *to, const ResponseRules::ParserResponse *from )
{
	*to=*from;
}
#pragma managed

void ResponseGroup::default::set( int idx, Response^ from )
{
	AssignParserResponse( &m_pNative->group[idx], from->GetNativePtr() );
}

bool ResponseGroup::Sequential::get()
{
	return m_pNative->IsSequential();
}

bool ResponseGroup::NoRepeat::get()
{
	return m_pNative->IsNoRepeat();
}

bool ResponseGroup::Enabled::get()
{
	return m_pNative->IsEnabled();
}

int ResponseGroup::CurrentIndexInsideGroup::get()
{
	return m_pNative->GetCurrentIndex();
}

int ResponseGroup::Count::get()
{
	return m_pNative->group.Count();
}


ResponseGroup::ResponseGroup( ResponseRules::ResponseGroup * RESTRICT ptr, int index ) 
	: BaseClass(ptr, false), m_index(index)
{
	int numOfResponses = ptr->group.Count();
	m_shadowResponseArray = gcnew array<Response ^>(numOfResponses);

	for ( int i = 0 ; i < numOfResponses ; ++i )
	{
		m_shadowResponseArray[i] = gcnew Response( &ptr->group[i], i );
	}
}

// ============================================================= //
//    Criterion
// ============================================================= //


String^ Criterion::Key::get()
{
	return gcnew String(m_pNative->name);
}

String^ Criterion::Value::get()
{
	return gcnew String(m_pNative->value);
}

bool Criterion::Required::get()
{
	return m_pNative->required;
}

Matcher^ Criterion::Comparison::get()
{
	return comparison;
}

float Criterion::Weight::get()
{
	return m_pNative->weight.GetFloat();
}


// ============================================================= //
//    Rule
// ============================================================= //


String^ Rule::Context::get()
{
	return gcnew String(m_pNative->GetContext());
}

void Rule::Context::set(String ^s)
{
	m_pNative->SetContext( StrToAnsi(s) );
}

bool Rule::Enabled::get()
{
	return m_pNative->IsEnabled();
}

bool Rule::MatchOnce::get()
{
	return m_pNative->IsMatchOnce();
}

bool Rule::IsApplyContextToWorld::get()
{
	return m_pNative->IsApplyContextToWorld();
}

// String^ GetValueForRuleCriterionByName( ResponseSystemCLI^ responsesystem, String^ criterionname );

unsigned short Rule::ResponseIndices::get( int idx )
{
	return m_pNative->m_Responses[idx] ;
}
int Rule::NumResponses::get( )
{
	return m_pNative->m_Responses.Count();
}

ResponseGroup ^Rule::ResponseGroups::get( int idx )
{
	return safe_cast<ResponseGroup ^>(SingletonResponseSystem_t::RS::get()->ResponseGroupsDict[idx]);
}

unsigned short Rule::CriteriaIndices::get( int idx )
{
	return  m_pNative->m_Criteria[idx]; 
}

int Rule::NumCriteria::get( )
{
	return m_pNative->m_Criteria.Count();
}

Criterion ^Rule::Criteria::get( int idx )
{
	return safe_cast<Criterion ^>(SingletonResponseSystem_t::RS::get()->CriteriaDict[idx]);
}
