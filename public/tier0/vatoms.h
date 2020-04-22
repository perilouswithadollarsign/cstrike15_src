// This is the interface to Valve atoms, a simple global table of pointers
// that does not change when we reload a game system. Its purpose is to facilitate
// on-the-fly unloading and reloading of AppSystems. The intended use is this:
// Appsystem allocates its interfaces on global heap (managed by tier0) and 
// places pointers to those interfaces into the atom table. Then, when 
// somebody unloads and reloads this system, the system will see that the atom
// is not NULL and reconnect so that the old pointers remain valid.
////////////////////////////////////////////////////////////////////////////////

#ifndef TIER0_ATOMS_HDR
#define TIER0_ATOMS_HDR


enum VAtomEnum
{
	VATOM_VJOBS, // vjobs.prx system
	VATOM_COUNT
};

// Do NOT cache these pointers to pointers, they may change
// the *GetVAtom() doesn't change, though
PLATFORM_INTERFACE void** GetVAtom( int nAtomIndex );


#endif