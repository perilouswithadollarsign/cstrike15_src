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

#ifndef RESPONSE_TYPES_MARSHAL_H
#define RESPONSE_TYPES_MARSHAL_H
#pragma once

using namespace System;


// forward-declare some classes we wrap
namespace ResponseRules
{
	struct ResponseParams;
	struct ParserResponse;
	class CRR_Response;
	
};

#include "response_types_marshal_wrappers.h"

namespace ResponseRulesCLI
{
	ref class ResponseSystemCLI;
	// try to maintain a singleton pointer to the response system
	public value class SingletonResponseSystem_t
	{
	public:
		property ResponseSystemCLI ^RS
		{
			static ResponseSystemCLI ^get() { return g_RS; }
			static void set(ResponseSystemCLI ^ r ) { g_RS=r; }
		}

	private:
		static ResponseSystemCLI ^g_RS;
	} SingletonResponseSystem;


	//-----------------------------------------------------------------------------
	// "internal" types
	//-----------------------------------------------------------------------------

	/// Stub for now
	public ref class ResponseFollowup {};


    /// <summary>
    /// Encapsulates the comparison of a value against a criterion.
    /// </summary>
    /// <remarks>
    /// The Matcher performs the comparison between the value specified in 
    /// the criterion and the value specified in a context. Eg, a criterion
    /// with key "foo", value 5 and matcher ">" means that only queries with
    /// context "foo" greater than 5 will match this criterion.
    /// 
    /// This is a representation only since the actual matching code is all in the 
    /// C++ side.
    /// </remarks>
    public ref class Matcher : public NativeTypeCopyWrapper<ResponseRules::Matcher>
	{
		typedef NativeTypeCopyWrapper<ResponseRules::Matcher> BaseClass;
	public:
		Matcher( ) {};
		Matcher( ResponseRules::Matcher *ptr ) : BaseClass(ptr, false) {};
		property String^ Token
		{
			String ^get();
		}

		property String^ RawSource
		{
			String ^get();
		}

		property String^ Description
		{
			String ^get();
		}

		// flag accessors -- from the comparison mechanism
		// inherited from Yahn's original system of changelist
		// 59153.
		property  bool FlagValid 
		{
			inline bool get() { return m_pNative->valid; }
		}
		property  bool FlagNumeric 
		{
			inline bool get() { return m_pNative->isnumeric; }
		}
		property  bool FlagNotEqual 
		{
			inline bool get() { return m_pNative->notequal; }
		}
		property  bool FlagUseMin 
		{
			inline bool get() { return m_pNative->usemin; }
		}
		property  bool FlagMinEquals 
		{
			inline bool get() { return m_pNative->minequals; }
		}
		property  bool FlagUseMax 
		{
			inline bool get() { return m_pNative->usemax; }
		}
		property  bool FlagMaxEquals
		{
			inline bool get() { return m_pNative->maxequals; }
		}


    };

	/// <summary>
    /// Represents a single AI_Criterion inside a rule. 
    /// </summary>
    /// <remarks>
    /// A response rule contains a list of criteria, each of which consists of a key,
    /// a matcher (comparator), and a value. A query into the response system contains a 
    /// list of key:value pairs. Each of these is tested against all the criteria in a rule.
    /// Each matching criterion increases that rule's score. The best scoring rule is selected.
    /// If a criterion is marked as Required, then its failure rejects the rule.
    /// Otherwise it just doesn't contribute to the score.
    /// </remarks>
    public ref class Criterion : public NativeTypeCopyWrapper<ResponseRules::Criteria>
    {
		typedef NativeTypeCopyWrapper<ResponseRules::Criteria> BaseClass;
	public:
		Criterion() {};
		Criterion( ResponseRules::Criteria *ptr, int _index ) : BaseClass( ptr, false ), index(_index)  
		{
			comparison = gcnew Matcher( &ptr->matcher );
		};
        property String^ Key
        {
            String^ get();
        }

        property String^ Value
        {
            String^ get();
        }

        property bool Required
        {
            bool get();
        }

        property Matcher^ Comparison
        {
            Matcher^ get();
        }

        property float Weight
        {
            float get();
		}

		property int Idx
		{
			int get() { return index; }
		}


		/*
        /// dummy criteria data for testing
        public static Criterion g_DummyCriteria[] = 
        { 
            new Criterion( "foo",       "1",    1, false, new Matcher(">") ),
            new Criterion( "bar",       "soup", 1, false, new Matcher("")  ),
            new Criterion( "Concept",   "Talk", 1, true,  new Matcher("")  )
        };
		*/

	private:
        float weight;
        bool required;
        Matcher^ comparison;
		int index; ///< my ID in the large dictionary of all criteria.
    };

	
	public value struct responseparams_interval_t
	{
		float start;
		float range;

		responseparams_interval_t( const ResponseRules::responseparams_interval_t &from );
		virtual  String ^ToString() override;
		/*
		interval_t &ToInterval( interval_t &dest ) const	{ dest.start = start; dest.range = range; return dest; }
		void FromInterval( const interval_t &from )			{ start = from.start; range = from.range; }
		float Random() const								{ interval_t temp = { start, range }; return RandomInterval( temp ); }
		*/
	};

	/// <summary>
    /// Parameters for a ParserResponse.
    /// </summary>
    /// <remarks>
    /// </remarks>
	public ref struct ResponseParams : public NativeTypeCopyWrapper<ResponseRules::ResponseParams>
	{
	private:
		typedef NativeTypeCopyWrapper<ResponseRules::ResponseParams> BaseClass;
	public:
		// manually match the native version. 
		// there is no way to automatically wrap
		// a native enum to a managed enum.
		[Flags]
		enum class ParamFlags_t
		{
			None =	(0),
			RG_DELAYAFTERSPEAK =	(1<<0),
			RG_SPEAKONCE =			(1<<1),
			RG_ODDS =				(1<<2),
			RG_RESPEAKDELAY =		(1<<3),
			RG_SOUNDLEVEL =			(1<<4),
			RG_DONT_USE_SCENE =		(1<<5),
			RG_STOP_ON_NONIDLE =	(1<<6),
			RG_WEAPONDELAY =		(1<<7),
			RG_DELAYBEFORESPEAK =	(1<<8),
		};

		/*
		ResponseParams()
		{
			flags = 0;
			odds = 100;
			delay.start = 0;f
			delay.range = 0;
			respeakdelay.start = 0;
			respeakdelay.range = 0;
			weapondelay.start = 0;
			weapondelay.range = 0;
			soundlevel = 0;
			predelay.start = 0;
			predelay.range = 0;
		}
		*/

		ResponseParams(){};

		ResponseParams( ResponseRules::ResponseParams *pSourceNativeParams ) :
		BaseClass( pSourceNativeParams, false ) {};


		property responseparams_interval_t delay
		{
			responseparams_interval_t get();
		}
		property responseparams_interval_t respeakdelay	
		{
			responseparams_interval_t get();
		}
		property responseparams_interval_t weapondelay	
		{
			responseparams_interval_t get();
		}
		property responseparams_interval_t predelay		
		{
			responseparams_interval_t get();
		}
		
		property short			odds	
		{
			short get();
		}
		property unsigned short	flags	
		{
			unsigned short get();
		}
		property bool IsSpeakOnce
		{
			bool get() { return ( ((ParamFlags_t)(flags)) & ParamFlags_t::RG_RESPEAKDELAY ) != ParamFlags_t::None ; }
		}
		property byte			soundlevel
		{
			byte get();
		}

		ResponseFollowup^ m_pFollowup;

	protected:
		// ResponseRules::ResponseParams *m_pNative;
	};
	


	/// <summary>
    /// Represents a Response as read from the script file. 
    /// </summary>
    /// <remarks>
    /// The action that ensues as a result of a query into the RR system
	/// is a Response. It may be a .vcd, a sound, or many other things.
	/// This Response class represents the entry in the source data file,
	/// not the result of a query into the system. Its analogue in C++
	/// is the ParserResponse.
    /// </remarks>
	public ref class Response : public NativeTypeCopyWrapper<ResponseRules::ParserResponse>
	{
		typedef NativeTypeCopyWrapper<ResponseRules::ParserResponse> BaseClass ;
	public:
		Response(){};
		Response( ResponseRules::ParserResponse *x , int index) : BaseClass(x, false), 
			m_index(index)
		{
			m_params =  gcnew ResponseParams(&x->params);
		};

		/// The various types of response available.
		enum class ResponseType
		{
			RESPONSE_NONE = 0,
			RESPONSE_SPEAK,
			RESPONSE_SENTENCE,
			RESPONSE_SCENE,
			RESPONSE_RESPONSE, // A reference to another response by name
			RESPONSE_PRINT,

			NUM_RESPONSES,
		};

		property ResponseParams ^params
		{
			ResponseParams ^get() { return m_params; }
		}

		/// A string containing the filename of a .vcd, the name of a sound script, etc.
		property String^ value
		{
			String^ get();
		}

		/// This response's relative weight in a rule.
		property float weight
		{
			float get();
		}

		/// Used to track how many times a response can get said
		property byte depletioncount
		{
			byte get();
		}
	
		/// What kind of response this is
		property ResponseType type
		{
			ResponseType get();
		}

		/// Special flags that can be specified in the response
		property bool DisplayFirst
		{
			bool get();
		}
		property bool DisplayLast
		{
			bool get();
		}

		// property ResponseFollowup^ followup;
		

	protected:
		/// in my owning responsegroup
		int m_index;
		ResponseParams ^m_params;

	};


	/// <summary>
	/// Represents a group of Responses, eg all the ones available 
	/// when a rule is matched.
	/// </summary>
	/// <remarks>
	/// The ordering of responses in this group isn't innately important.
	/// However some responses may be marked "first" or "last" in which 
	/// case they are played either at beginning or end for the rule.
	/// Implements IList, representing a list of the responses it contains.
	/// </remarks>
	public ref class ResponseGroup : public NativeTypeCopyWrapper<ResponseRules::ResponseGroup>
	{
		typedef public NativeTypeCopyWrapper<ResponseRules::ResponseGroup> BaseClass;
	public:
		ResponseGroup() {};
		ResponseGroup( ResponseRules::ResponseGroup *ptr, int index );
		property bool Sequential
		{
			bool get();
		}

		property bool NoRepeat
		{
			bool get();
		}

		property bool Enabled
		{
			bool get();
		}

		property int CurrentIndexInsideGroup
		{
			int get();
		}

		property int Count
		{
			int get();
		}

		/// The index of this response group inside the global system's dict
		property int Index
		{
			int get() { return m_index; }
		}

		/// For WPF views, get the responses as a list.
		property System::Collections::IList^ ResponsesList
		{
			System::Collections::IList^ get() { return m_shadowResponseArray; }
		}


		/// indexer goes into the cutlvector inside the group
		property Response ^ default[int]
		{
			Response ^get( int idx );
			void set( int idx, Response^ from );
		}

	protected:
		/// in the global system dictionary
		int m_index;

	private:
		/// we'll preallocate the Response wrapper objects inside here
		/// to speed up the access in wpf views.
		array<Response ^>^ m_shadowResponseArray; 
	};


	/// <summary>
	/// Represents a Response Rule
	/// </summary>
	/// <remarks>
	/// A rule contains a bunch of criteria and a group of responses. 
	/// A query into the RR system means iterating through the database of
	/// Rules with a set of contexts; the contexts are matched against the 
	/// criteria and the best match is returned.
	/// </remarks>
	public ref class Rule : public NativeTypeCopyWrapper<ResponseRules::Rule>
	{
		typedef NativeTypeCopyWrapper<ResponseRules::Rule> BaseClass ;
	public:
		typedef IndexPropertyToIListReadOnly<ResponseGroup ^> ResGroupAsIList_t;
		typedef IndexPropertyToIListReadOnly<Criterion ^> CriteriaAsIList_t;

	public:
		Rule(){};
		Rule( ResponseRules::Rule *x, ResponseRules::ResponseRulePartition::tIndex idx, const char *name ) :
			BaseClass(x, false), m_index(idx), m_name(gcnew String(name)),
			m_ResponseGroupsAsList( nullptr ), m_CriteriaAsList( nullptr )
			{
				m_ResponseGroupsAsList = gcnew ResGroupAsIList_t(
					gcnew ResGroupAsIList_t::lGetter(this, &ResponseGroups::get),
					gcnew ResGroupAsIList_t::lCounter(this, &NumResponses::get) ) ;

				m_CriteriaAsList = gcnew CriteriaAsIList_t(
					gcnew CriteriaAsIList_t::lGetter(this, &Criteria::get),
					gcnew CriteriaAsIList_t::lCounter(this, &NumCriteria::get) ) ;
			};

		property String^ Context
		{
			String^ get();
			void    set(String ^s);
		}

		property bool Enabled
		{
			bool get();
		}

		property bool MatchOnce
		{
			bool get();
		}

		property bool IsApplyContextToWorld
		{
			bool get();
		}

		// String^ GetValueForRuleCriterionByName( ResponseSystemCLI^ responsesystem, String^ criterionname );

		/// indexes into the response system
		property unsigned short ResponseIndices[int]
		{
			unsigned short get( int idx );
		}
		property int NumResponses
		{
			int get( );
		}
		property ResponseGroup ^ResponseGroups[int]
		{
			ResponseGroup ^get( int idx );
		}
		property ResGroupAsIList_t ^ResponseGroupsAsIList
		{
			ResGroupAsIList_t ^get() { return m_ResponseGroupsAsList; }
		}


		property unsigned short CriteriaIndices[int]
		{
			unsigned short get( int idx );
		}
		property int NumCriteria
		{
			int get( );
		}
		property Criterion ^Criteria[int]
		{
			Criterion ^get( int idx );
		}
		property CriteriaAsIList_t ^CriteriaAsIList
		{
			CriteriaAsIList_t ^get() { return m_CriteriaAsList; }
		}

		property ResponseRules::ResponseRulePartition::tIndex IndexInResponseSystem
		{
			ResponseRules::ResponseRulePartition::tIndex get() { return m_index; }
		}

		property String^ Name
		{
			String ^get() { return m_name; }
		}

	private:
		///  index inside responsesystem
		ResponseRules::ResponseRulePartition::tIndex m_index; 
		String ^m_name;
		ResGroupAsIList_t ^m_ResponseGroupsAsList;
		CriteriaAsIList_t ^m_CriteriaAsList;
	};



	//----------------------------------------------------------------------------
	// "public" types
	//-----------------------------------------------------------------------------

#if 0
	/// Takes ownership of the response object and DELETES IT when finalized.
	public ref class ResponseQueryResult
	{
	public:
		ResponseQueryResult( ) : m_pNative(NULL) {};
		ResponseQueryResult( ResponseRules::CRR_Response * source ) : m_pNative(source) {};
		~ResponseQueryResult() { this->!ResponseQueryResult(); }
		!ResponseQueryResult() 
		{ 
			delete m_pNative; 
			m_pNative = NULL; 
		}

		property String^ RuleName
		{
			String^ get();
		}

		property String^ ResponseName
		{
			String^ get();
		}

	protected:
		ResponseRules::CRR_Response *m_pNative;
	};
#endif


	// henceforth contexts shall be called "facts".
};


#endif