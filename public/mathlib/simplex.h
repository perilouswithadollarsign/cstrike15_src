//========= Copyright c 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef VPHYSICS_PHYSX_SIMPLEX_H
#define VPHYSICS_PHYSX_SIMPLEX_H

#include "tier0/dbg.h"

//////////////////////////////////////////////////////////////////////////
// Direct simplex LP solver using tableau and Gauss pivoting rule;
// see http://www.teachers.ash.org.au/mikemath/mathsc/linearprogramming/simplex.PDF
// After constructing an instance of this class with the appropriate number of vars and constraints,
// fill in constraints using SetConstraintFactor and SetConstraintConst
//
// Here's the problem in its canonical form:
//  Maximize objective = c'x : x[i] >= 0, A.x <= b; and make that c' positive! negative will automatically mean 0 is your best answer
//  Vector x is the vector of unknowns (variables) and has dimentionality of numVariables
//  Vector c is dotted with the x, so has the same dimentionality; you set it with SetObjectiveFactor()
//  every component of x must be positive in feasible solution
//  A is constraint matrix and has dims: m_numConstraints by m_numVariables; you set it with SetConstraintFactor();
//  b has dims: m_numConstraints, you set it with SetConstraintConst()
//
// This is solved with the simplest possible simplex method (I have no good reason to implement pivot rules now) 
// The simplex tableau (m_pTableau) starts like this:
// | A  | b |
// | c' | 0 |
//
class CSimplex
{
public:
	int m_numConstraints, m_numVariables;
	float *m_pTableau;
	float *m_pInitialTableau;
	float *m_pSolution;
	int *m_pBasis; // indices of basis variables, corresponding to each row in the tableau; >= numVars if the slack var corresponds to that row
	int *m_pNonBasis; // indices of non-basis primal variables (labels on the top of the classic Tucker(?) tableau)
	enum StateEnum{kInfeasible, kUnbound, kOptimal, kUnknown, kCannotPivot};
	StateEnum m_state;
	//CVarBitVec m_isBasis;
public:
	CSimplex();
	CSimplex(int numVariables, int numConstraints);
	~CSimplex();

	void Init(int numVariables, int numConstraints);
	void InitTableau(const float *pTableau);
	void SetObjectiveFactors(int numFactors, const float *pFactors);

	void SetConstraintFactor(int nConstraint, int nConstant, float fFactor);
	void SetConstraintConst(int nConstraint, float fConst);
	void SetObjectiveFactor(int nConstant, float fFactor);

	StateEnum Solve(float flThreshold = 1e-5f, int maxStallIterations = 128);
	StateEnum SolvePhase1(float flThreshold = 1e-5f, int maxStallIterations = 128);
	StateEnum SolvePhase2(float flThreshold = 1e-5f, int maxStallIterations = 128);
	float GetSolution(int nVariable)const;
	float GetSlack(int nConstraint)const;
	float GetObjective()const;
	void PrintTableau()const;

protected:
	void Destruct();
	float *operator [] (int row) {Assert(row >= 0 && row < NumRows());return m_pTableau + row * NumColumns();}
	float &Tableau(int row, int col){Assert(row >= 0 && row < NumRows());return m_pTableau[row * NumColumns()+col];}
	float Tableau(int row, int col)const{Assert(row >= 0 && row < NumRows());return m_pTableau[row * NumColumns()+col];}
	float GetInitialTableau(int row, int col)const{return m_pInitialTableau[row * NumColumns()+col];}
	bool IteratePhase1();
	bool IteratePhase2();
	int NumRows()const {return m_numConstraints + 1;}
	int NumColumns()const{return m_numVariables + 1;}
	void Validate();
	void PrepareTableau();
	void GatherSolution();
	bool Pivot(int nPivotRow, int nPivotColumn);

	void MultiplyRow(int nRow, float fFactor);
	void AddRowMulFactor(int nTargetRow, int nPivotRow, float fFactor);

	int FindPivotColumn();
	int FindPivotRow(int nColumn);

	int FindLastNegConstrRow();
	int ChooseNegativeElementInRow(int nRow);
};


#endif