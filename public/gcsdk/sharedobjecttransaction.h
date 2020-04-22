//====== Copyright ©, Valve Corporation, All rights reserved. =======
//
// Purpose: Base class for transactions that modify a CGCSharedObjectCache and the database
//
//=============================================================================

#ifndef SHAREDOBJECTTRANSACTION_H
#define SHAREDOBJECTTRANSACTION_H
#ifdef _WIN32
#pragma once
#endif

namespace GCSDK
{

class CSharedObjectTransaction
{
public:

	/**
	 * Constructor that will begin a transaction
	 * @param sqlAccess
	 * @param pName
	 */
	CSharedObjectTransaction( CSQLAccess &sqlAccess, const char *pName );

	/**
	 * Destructor
	 */
	~CSharedObjectTransaction();

	/**
	 * Adds an object that exists in the given CGCSharedObjectCache to be managed in this transaction.
	 * Call this before making any modifications to the object
	 * @param pSOCache the owner CGCSharedObjectCache
	 * @param pObject the object that will be modified
	 */
	bool AddManagedObject( CGCSharedObjectCache *pSOCache, CSharedObject *pObject );

	/**
	 * Adds a brand new object to the given CGCSharedObjectCache
	 * @param pSOCache the owner CGCSharedObjectCache
	 * @param pObject the newly created object
	 * @return true if successful, false otherwise
	 */
	bool BAddNewObject( CGCSharedObjectCache *pSOCache, CSharedObject *pObject, bool bAddToDatabase = true );

	/**
	 * Removes an existing object from the CGCSharedObjectCache
	 * @param pSOCache the owner CGCSharedObjectCache
	 * @param pObject the object to be removed from the CGCSharedObjectCache
	 * @param bRemoveFromDatabase whether to remove the item from the database or not
	 * @return true if successful, false otherwise
	 */
	bool BRemoveObject( CGCSharedObjectCache *pSOCache, CSharedObject *pObject );

	/**
	 * Marks in the transaction that the object was modified.  The object must have been previously added via
	 * the AddManagedObject() call in order for the object to be marked dirty.  If the object is new to the
	 * CGCSharedObjectCache, then calling this will return false (which is not necessarily an error)
	 *
	 * @param pObject the object that will be modified
	 * @param unFieldIdx the field that was changed
	 * @return true if the field was marked dirty, false otherwise.
	 */
	bool BModifiedObject( CSharedObject *pObject, uint32 unFieldIdx );

	/**
	 * @param pSOCache
	 * @param soIndex
	 * @return the CSharedObject that matches either in the CGCSharedObjectCache or to be added
	 */
	CSharedObject *FindSharedObject( CGCSharedObjectCache *pSOCache, const CSharedObject &soIndex );

	/**
	 * Rolls back any changes made to the objects in-memory and in the database
	 *
	 * This function should not be made virtual -- it's called from within the destructor.
	 */
	void Rollback();

	/**
	 * Commits any changes to the database and also to memory
	 * @return true if successful, false otherwise
	 */
	bool BYieldingCommit( bool bAllowEmpty = false );

	/**
	 * @return GCSDK::CSQLAccess associated with this transaction
	 */
	CSQLAccess &GetSQLAccess() { return m_sqlAccess; }

protected:

	/**
	 * Reverts all in-memory modifications and deletes all newly created objects.
	 *
	 * This function should not be made virtual -- it's called from within the destructor.
	 */
	void Undo();

	struct undoinfo_t
	{
		CSharedObject *pObject;
		CGCSharedObjectCache *pSOCache;
		CSharedObject *pOriginalCopy;
		bool bAddToDatabase;
	};

	// variables
	CUtlVector< undoinfo_t > m_vecObjects_Added;
	CUtlVector< undoinfo_t > m_vecObjects_Removed;
	CUtlVector< undoinfo_t > m_vecObjects_Modified;
	CSQLAccess &m_sqlAccess;
}; // class CSharedObjectTransaction

}; // namespace GCSDK

#endif // SHAREDOBJECTTRANSACTION_H
