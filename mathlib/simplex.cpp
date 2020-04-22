#include <basetypes.h>
#include <float.h>
#include "simplex.h"

// a nice tutorial on simplex method: http://math.uww.edu/~mcfarlat/ism.htm
CSimplex::CSimplex():
	m_numVariables(0),m_numConstraints(0),m_pTableau(0),m_pInitialTableau(0), m_pSolution(0), m_pBasis(0)
{
}

CSimplex::CSimplex(int numVariables, int numConstraints):
	m_numVariables(0),m_numConstraints(0),m_pTableau(0),m_pInitialTableau(0), m_pSolution(0), m_pBasis(0)
{
	Init(numVariables, numConstraints);
}

void CSimplex::Init(int numVariables, int numConstraints)
{
	Destruct();
	m_numVariables = numVariables; m_numConstraints = numConstraints;
	m_pTableau = new float[(NumRows()+1) * NumColumns()];
	m_pInitialTableau = new float[(NumRows()+1) * NumColumns()];
	m_pSolution = m_pTableau + NumRows() * NumColumns();
	// allocating basis and non-basis indices in one call
	m_pBasis = new int[m_numConstraints + m_numVariables];
	m_pNonBasis = m_pBasis + m_numConstraints;
	m_state = kUnknown;
}


void CSimplex::PrintTableau()const
{
	Msg("problem.Init(%d,%d);\nfloat test[%d]={", m_numVariables, m_numConstraints, (m_numVariables+1)*(m_numConstraints+1));
	for(int i = 0; i < NumRows(); ++i)
	{
		for(int j = 0;j < NumColumns(); ++j)
		{
			Msg(" %g,",Tableau(i,j));
		}
		Msg("\n");
	}
	Msg("}");
}


void CSimplex::InitTableau(const float *pTableau)
{
	const float *p = pTableau;
	for(int nRow = 0; nRow  <= m_numConstraints; ++nRow)
	{
		for(int nColumn = 0; nColumn < m_numVariables; ++nColumn)
		{
			Tableau(nRow, nColumn) = *(p++);
		}
		Tableau(nRow, NumColumns()-1) = *(p++);
	}
}

CSimplex::~CSimplex()
{
	Destruct();
}

void CSimplex::Destruct()
{
	delete[]m_pInitialTableau;
	m_pInitialTableau = NULL;
	delete[]m_pTableau;
	m_pTableau = NULL;
	delete[]m_pBasis;
	m_pBasis = NULL;
}


CSimplex::StateEnum CSimplex::Solve(float flThreshold, int maxStallIterations)
{
	m_state = kUnknown;
	PrepareTableau();
	if(SolvePhase1(flThreshold, maxStallIterations) == kUnknown)
		SolvePhase2(flThreshold, maxStallIterations);
	GatherSolution();
	return m_state;
}



///////////////////////////////////////////////////////////////////////////
// bring constraints to b>=0 form for phase-2 full solution
CSimplex::StateEnum CSimplex::SolvePhase1(float flThreshold, int maxStallIterations)
{
	for(int nPotentiallyInfiniteLoop = 0; nPotentiallyInfiniteLoop < maxStallIterations; ++nPotentiallyInfiniteLoop)
	{
		if(!IteratePhase1())
			break;
	}
	return m_state;
}

//////////////////////////////////////////////////////////////////////////
// Solve the linear problem ; 
//   \param flThreshold         - this is how much we need to improve objective every step that's not considered lost
//   \param maxStallIterations  - this is how many "lost" (see flThreshold) steps we may take before we bail
//
CSimplex::StateEnum CSimplex::SolvePhase2(float flThreshold, int maxStallIterations)
{
	for(int nPotentiallyInfiniteLoop = 0; nPotentiallyInfiniteLoop < maxStallIterations; ++nPotentiallyInfiniteLoop)
	{
		if(!IteratePhase2())
			break;
	}
	Validate();
	return m_state;
}

// fill out m-pSolution array (primal solution)
void CSimplex::GatherSolution()
{
	// Notes:
	// PRIMAL SOLUTION is indicated by the rightmost column of the tableau;
	// there are at most m_numConstraint basic variables that participate in the solution.
	// The original problem PRIMAL unknowns are numbered 0..m_numVariables; the rest (m_numVariables+1..m_numVariables+m_numConstraints) are the PRIMAL SLACK variables
	// DUAL SOLUTION is in the row [m_numConstraints], and it's basic variables are indicated by m_pNonBasic array and are reversed:
	// first the DUAL SLACK variables are numbered 0..m_numVariables; the rest (m_numVariables+1..m_numVariables+m_numConstraints) are the DUAL variables

	memset(m_pSolution, 0, sizeof(*m_pSolution) * NumColumns()); // initial value of all X's are 0's
	for(int nRow = 0; nRow < m_numConstraints; ++nRow)
	{
		int nBasisVariable = m_pBasis[nRow];
		m_pSolution[nBasisVariable] = Tableau(nRow, NumColumns()-1);
	}
	m_pSolution[m_numVariables+m_numConstraints] = Tableau(m_numConstraints, NumColumns()-1);
}


///////////////////////////////////////////////////////////////////////////
// Find and pivot a row with negative constraint const (right side)
// return false - if can't find such constraint or can't pivot
//
bool CSimplex::IteratePhase1()
{
	int nFixRow = FindLastNegConstrRow();
	if(nFixRow < 0)
		return false; // phase 1 complete: no rows to fix
	int nPivotColumn = ChooseNegativeElementInRow(nFixRow);
	if(nPivotColumn < 0)
	{
		m_state = kInfeasible;
		return false;
	}

	int nPivotRow = nFixRow;
	float flMinimizer = Tableau (nPivotRow, NumColumns()-1)/Tableau(nPivotRow, nPivotColumn); // minimize this

	// UNTESTED! What's the rule to choose pivot in phase1?
	for(int nCandidatePivotRow = nPivotRow + 1; nCandidatePivotRow < m_numConstraints; ++nCandidatePivotRow)
	{
		float flCandidateConst = Tableau (nCandidatePivotRow,NumColumns()-1), flCandidatePivot = Tableau (nCandidatePivotRow, nPivotColumn);
		if ( flCandidateConst < 0 && flCandidatePivot > 1e-6f )
		{
			float flCandidateMinimizer = flCandidateConst / flCandidatePivot;
			if(flCandidateMinimizer < flMinimizer)
			{
				flCandidateMinimizer = flMinimizer;
				nPivotRow = nCandidatePivotRow;      // UNTESTED! 
			}
		}
	}

	return Pivot(nPivotRow, nPivotColumn);
}

//////////////////////////////////////////////////////////////////////////
// Return the index of the last row with negative Constraint Const (b[i] in A.x<=b formulation)
int CSimplex::FindLastNegConstrRow()
{
	int nFixRow = -1;
	for(int nRow = 0;  nRow < m_numConstraints; ++nRow)
	{
		if(Tableau(nRow, NumColumns()-1) < 0)
		{
			nFixRow = nRow;
		}
	}
	return nFixRow;
}

///////////////////////////////////////////////////////////////////////////
// Choose some (e.g. the most negative) negative number in the row
int CSimplex::ChooseNegativeElementInRow(int nFixRow)
{
	int indexNegElement = -1;
	float flMinElement = 0;
	for(int nColumn = 0; nColumn < m_numVariables; ++nColumn)
	{
		float flElement = Tableau(nFixRow, nColumn);
		if(flElement < flMinElement)
		{
			indexNegElement = nColumn;
			flMinElement = flElement;
		}
	}
	return indexNegElement;
}

bool CSimplex::IteratePhase2()
{
	int nPivotColumn = FindPivotColumn();
	if(nPivotColumn < 0)
	{
		m_state = kOptimal;
		return false;
	}
	int nPivotRow = FindPivotRow(nPivotColumn);
	if(nPivotRow < 0)
	{
		m_state = kUnbound;
		return false;
	}

	bool ok = Pivot(nPivotRow, nPivotColumn);

	// since we replaced the basis variable, we have to replace its corresponding column

	return ok;
}


//////////////////////////////////////////////////////////////////////////
// Self-explanatory, isn't it?
bool CSimplex::Pivot(int nPivotRow, int nPivotColumn)
{
	if(fabs(Tableau(nPivotRow, nPivotColumn)) < 1e-8f)
	{
		m_state = kCannotPivot;
		return false; // Can NOT pivot on zero :( choose another (ie. fancier) pivot rule
	}

	/// get the 1/Tij, then replace the multiplied element with it
	float flFactor = 1.0f / Tableau(nPivotRow, nPivotColumn);
	MultiplyRow(nPivotRow, flFactor);

	for(int i = 0; i <= m_numConstraints; ++i)
	{
		if(i != nPivotRow)
		{
			float flFactorOther = -Tableau(i,nPivotColumn);
			AddRowMulFactor(i, nPivotRow, flFactorOther);
			Tableau(i,nPivotColumn) = flFactorOther * flFactor; // replace the column with original column / -pivot
		}
	}
	Tableau(nPivotRow, nPivotColumn) = flFactor;

	int nEnteringVariable = m_pNonBasis[nPivotColumn];
	int nExitingVariable = m_pBasis[nPivotRow];

	// remember the index of the entering new basis var
	m_pBasis[nPivotRow]        = nEnteringVariable;
	m_pNonBasis[nPivotColumn]  = nExitingVariable;

	Validate();
	return true;
}


//////////////////////////////////////////////////////////////////////////
// find the column with the most negative number in the last (objective) row
int CSimplex::FindPivotColumn()
{
	int nBest = -1;
	float flBest = 0;
	for(int i = 0; i < m_numVariables; ++i)
	{
		float flElement = Tableau(m_numConstraints, i);
		if(flElement > flBest)
		{
			flBest = flElement;
			nBest = i;
		}
	}
	if(nBest < 0)
	{
		m_state = kOptimal;
		return -1;
	}
	else
		return nBest;
};


int CSimplex::FindPivotRow(int nColumn)
{
	float flBest = FLT_MAX;
	int nBest = -1;
	for(int nRow = 0; nRow < m_numConstraints; ++nRow)
	{		    
		float flPivotCandidate = Tableau(nRow, nColumn);
		if(flPivotCandidate > 1e-6f)
		{
			// don't perform any tests unless flTest is finite
			float flTest = Tableau(nRow, NumColumns()-1) / flPivotCandidate;
			if(flTest < flBest)
			{
				// flBest is either Infinity or is worse; it's worse in any case, so replace it
				flBest = flTest;
				nBest = nRow;
			}
		}
	}

	return nBest;
}


void CSimplex::MultiplyRow(int nRow, float flFactor)
{
	for(int nColumn = 0; nColumn < NumColumns(); ++nColumn)
	{
		Tableau(nRow, nColumn) *= flFactor;
	}
}


void CSimplex::AddRowMulFactor(int nTargetRow, int nPivotRow, float fFactor)
{
	for(int nColumn = 0; nColumn < NumColumns(); ++nColumn)
	{
		Tableau(nTargetRow, nColumn) += Tableau(nPivotRow, nColumn) * fFactor;
	}
}


// set the I matrix in the slack columns of the tableau
void CSimplex::PrepareTableau()
{
/*
	for(int nRow = 0; nRow < m_numConstraints + 1; ++nRow)
	{
		for(int nColumn = 0; nColumn < m_numConstraints; ++nColumn)
			Tableau(nRow, nColumn + m_numVariables) = 0;
	}
*/
	for(int nonBasis = 0; nonBasis < m_numVariables; ++nonBasis)
	{
		m_pNonBasis[nonBasis] = nonBasis;
	}
	for(int nConstraint = 0; nConstraint < m_numConstraints; ++nConstraint)
	{
		m_pBasis[nConstraint] = m_numVariables + nConstraint;	// slack variables
		//Tableau(nConstraint, nConstraint + m_numVariables) = 1.0f;
	} 
	//m_pSolution[m_numVariables+m_numConstraints] = 
	Tableau(m_numConstraints, NumColumns()-1) = 0.0f; // starting with "0" objective, and all "0" variables
	memcpy(m_pInitialTableau,m_pTableau,(NumRows()+1) * NumColumns() * sizeof(float));
}



void CSimplex::SetConstraintConst(int nConstraint, float fConst)
{
	m_pSolution[m_numVariables + nConstraint] = Tableau(nConstraint, NumColumns()-1) = fConst;
}


void CSimplex::SetConstraintFactor(int nConstraint, int nConstant, float fFactor)
{
	Tableau(nConstraint, nConstant) = fFactor;
}

void CSimplex::SetObjectiveFactor(int nConstant, float fFactor)
{
	// the objective factor is negated because for the objective P = cx , we write it as -c x + P -> max
	Tableau(m_numConstraints, nConstant) = fFactor;
}

void CSimplex::SetObjectiveFactors(int numFactors, const float *pFactors)
{
	Assert(numFactors == m_numVariables);
	for(int i =0; i < m_numVariables && i < numFactors; ++i)
		SetObjectiveFactor(i,pFactors[i]);
}



float CSimplex::GetSolution(int nVariable)const
{
	Assert(nVariable < m_numVariables);
	return m_pSolution[nVariable];
}


float CSimplex::GetSlack(int nConstraint)const
{
	Assert(nConstraint < m_numConstraints);
	return m_pSolution[m_numVariables + nConstraint];
}


float CSimplex::GetObjective()const
{
/*
	float flResult = 0;
	for(int i = 0; i < m_numVariables + m_numConstraints;  ++i)
		flResult -= m_pSolution[i] * Tableau(m_numConstraints,i);
	return flResult;
*/
	return Tableau(m_numConstraints, NumColumns()-1);
}


void CSimplex::Validate()
{
#if defined(_DEBUG) && 0
	GatherSolution();
	for(int i = 0; i <= m_numConstraints; ++i)
	{
		float flRes = 0;
		for(int j = 0; j < m_numVariables; ++j)
			flRes += GetInitialTableau(i,j) * m_pSolution[j];
		if(i == m_numConstraints)
		{
			Msg("Objective = %g; basis:",flRes);
			for (int j = 0; j < m_numVariables; ++j)
				Msg(" %g", m_pSolution[j]);
			Msg(" |slacks:");
			for(int j = 0; j < m_numConstraints; ++j)
				Msg(" %g", m_pSolution[j+m_numVariables]);
			Msg("\n");
		}
		else
			Msg("%g\t<= %g\n", flRes, GetInitialTableau(i,NumColumns()-1));
	}
#endif
}



class CSimplexTestUnit
{
public:
	CSimplexTestUnit()
	{
		CSimplex test(3,2);
		test.SetObjectiveFactor(0, 12);
		test.SetObjectiveFactor(1, 8);
		test.SetObjectiveFactor(2, 24);
		test.SetConstraintFactor(0, 0, 6);
		test.SetConstraintFactor(0, 1, 2);
		test.SetConstraintFactor(0, 2, 4);
		test.SetConstraintConst(0, 200);
		test.SetConstraintFactor(1, 0, 2);
		test.SetConstraintFactor(1, 1, 2);
		test.SetConstraintFactor(1, 2, 12);
		test.SetConstraintConst(1, 160);

		test.Solve();

		test.Init(2,2);
		float test2[] = {2,1,3,  3,1,4,  17,5,0};
		test.InitTableau(test2);
		test.Solve();
		// m_pSolution (test.m_pSolution) should be : 30 40 | 0 0 | 4100

		//////////////////////////////////////////////////////////////////////////
		// unbound-solution problem: x1-x2<=1 && x2-x1<=1, maximize x1+x2; if x1==x2, we can go unbound x1==x2 -> +inf
		// the dual formulation is infeasible in this case: v2-v1 >= 1 && v1-v2 >= 1, which are self-contradictory
		test.Init(2,2);
		float testUnsolvable[] = {-1,1,1, 1,-1,1, 1,1,0};
		test.InitTableau(testUnsolvable);
		test.Solve();


		//////////////////////////////////////////////////////////////////////////
		// General Simplex problem: equality constraint
		test.Init(2, 3);
		float testGenSimplex[] = {1,1,20, 1,2,30, -1,-2,-30, 2,1,0};
		test.InitTableau(testGenSimplex);
		test.Solve();

		test.Init(7,6);
		float testA[56]={ -1, 1, 0, -0, -0, 0, 1, 13.0048,
			1, -1, 0, -0, -0, 0, 1, 13.0048,
			0, -0, -1, 1, -0, 0, 1, 13.0048,
			0, -0, 1, -1, -0, 0, 1, 13.0048,
			0, -0, 0, -0, 1, -1, 1, 0.00100005,
			0, -0, 0, -0, -1, 1, 1, 0.405401,
			0, 0, 0, 0, 0, 0, 1, 0
		};
		test.InitTableau(testA);
		test.Solve();
	}
};


// this is for debugging and unit-testing
//static CSimplexTestUnit s_test;
