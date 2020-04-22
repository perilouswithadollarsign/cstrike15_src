//======= Copyright © 1996-2006, Valve Corporation, All rights reserved. ======
//
// Maya Undo helper, use it like this
//
//    class CMyCmd : MPxCommand
//    {
//    	// ... regular stuff ... //
//    
//    	ValveMaya::CUndo m_undo;
//    };
//    
//    MStatus CMyCmd::doIt( const MArgList &mArgList )
//    {
//    	m_undo.SetArgList( Syntax(), mArgList );
//    }
//    
//    MStatus CMyCmd::redoIt( const MArgList &mArgList )
//    {
//    	// Get at command line args
//    	m_undo.ArgDatabase().isFlagSet( ... );
//    
//    	// Do operations
//    	m_undo.SaveCurrentSelection();
//    	m_undo.DagModifier().createNode( ... );
//    	m_undo.DagModifierDoIt();
//    	m_undo.SetAttr( mPlug, value );
//    	
//    	/// etc ...
//    }
//    
//    MStatus CMyCmd::undoIt( const MArgList &mArgList )
//    {
//    	m_undo.Undo();
//    }
//    
//    bool CMyCmd::isUndoable()
//    {
//    	return m_undo.IsUndoable();
//    }
//    
// If there's a need to get fancy, any of the CUndoOp* classes can be
// constructed via 'new' and a boost::shared_ptr< CUndoOp > constructed
// with that pointed can be passed to CUndo::Push().  Note that means
// that the pointer will be managed by boost::shared_ptr so if the
// lifetime of the data needs to be controlled by the caller (it shouldn't)
// then a shared_ptr should be kept
//
// Setting the ArgList and using ArgDatabase doesn't affect the undo/redo
// ability but it's a convnient place to keep that data
//
//=============================================================================

#ifndef VALVEMAYA_UNDO_H
#define VALVEMAYA_UNDO_H
#if defined( _WIN32 )
#pragma once
#endif


// Standard includes


// Maya includes
#include <maya/MArgDatabase.h>
#include <maya/MArgList.h>
#include <maya/MDagModifier.h>
#include <maya/MDagPath.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MSyntax.h>
#include <maya/MTransformationMatrix.h>


// Valve includes
#include "tier1/utlstack.h"


namespace ValveMaya
{

// Forward declarations
class CUndoOp;


//=============================================================================
//
//
// CUndo: Undo stack manager class
//
//
//=============================================================================

class CUndo
{
public:
	CUndo();

	~CUndo();

	void Clear();

	MStatus SetArgList(
		const MSyntax &mSyntax,
		const MArgList &mArgList );

	const MArgDatabase &ArgDatabase();

	void SaveCurrentSelection();

	MDagModifier &DagModifier();

	MStatus DagModifierDoIt();

	MStatus Connect(
		const MPlug &srcP,
		const MPlug &dstP,
		bool force = false );

	bool SetAttr(
		MPlug &mPlug,
		MObject &val );

	bool SetAttr(
		MPlug &mPlug,
		double val );

	bool Lock(
		MPlug &mPlug,
		bool lock );

	void NodeCreated( MObject &nodeObject );

	void Push(
		CUndoOp *pUndoOp );

	bool IsUndoable() const;

	MStatus Undo();

protected:
	MArgDatabase *m_pArgDatabase;
	CUtlStack< CUndoOp * > m_undoStack;
};


//=============================================================================
//
//
// CUndoOp: Undo stack member abstract base class
//
//
//=============================================================================
class CUndoOp
{
public:
	virtual ~CUndoOp()
	{
	}

	virtual void Undo() = 0;
};


//=============================================================================
//
//
// CUndoOpDagModifier: Undo stack member Dag Modifier class
//
//
//=============================================================================
class CUndoOpDagModifier : public CUndoOp
{
public:
	virtual ~CUndoOpDagModifier()
	{
	}

	virtual void Undo()
	{
		m_mDagModifier.undoIt();
	}

protected:
	friend class CUndo;

	MDagModifier m_mDagModifier;
};


//=============================================================================
//
//
// CUndoOpSetAttr: Undo stack member for setting attributes
//
//
//=============================================================================
class CUndoOpSetAttr : public CUndoOp
{
public:
	CUndoOpSetAttr(
		MPlug &mPlug,
		MObject &mObjectVal );

	CUndoOpSetAttr(
		MPlug &mPlug,
		double numericVal );

	virtual ~CUndoOpSetAttr()
	{
	}

	virtual void Undo();

protected:
	MPlug m_mPlug;
	MObject m_mObjectVal;
	double m_numericVal;
	const bool m_numeric;

private:
	// Visual C++ is retarded - tell it there's no assignment operator
	CUndoOpSetAttr &operator=( const CUndoOpSetAttr &rhs );

};


//=============================================================================
//
//
// CUndoOpSelection: Undo stack member for changing selection
//
//
//=============================================================================
class CUndoOpSelection : public CUndoOp
{
public:
	CUndoOpSelection();

	virtual ~CUndoOpSelection()
	{
	}

	virtual void Undo();

protected:
	MSelectionList m_mSelectionList;

};


//=============================================================================
//
//
// CUndoOpSelection: Undo stack member for locking and unlocking attributes
//
//
//=============================================================================
class CUndoOpLock : public CUndoOp
{
public:

	CUndoOpLock(
		MPlug &mPlug,
		bool lock );

	virtual ~CUndoOpLock()
	{
	}

	virtual void Undo();

protected:
	MPlug m_mPlug;
	const bool m_locked;

private:
	// Visual C++ is retarded - tell it there's no assignment operator
	CUndoOpLock &operator=( const CUndoOpLock &rhs );
};


//=============================================================================
//
//=============================================================================
class CUndoOpResetRestPosition : public CUndoOp
{
public:
	CUndoOpResetRestPosition(
		const MDagPath &mDagPath );

	virtual ~CUndoOpResetRestPosition()
	{
	}

	virtual void Undo();

protected:
	const MDagPath m_mDagPath;
	MTransformationMatrix m_matrix;
};


//=============================================================================
// For node creation via something like MFnMesh::create, etc..
//=============================================================================
class CUndoOpNodeCreated : public CUndoOp
{
public:
	CUndoOpNodeCreated(
		MObject &mObject );

	virtual ~CUndoOpNodeCreated()
	{
	}

	virtual void Undo();

protected:
	MObject m_nodeObject;
};


}
#endif // VALVEMAYA_UNDO_H