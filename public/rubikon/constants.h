//========= Copyright © 1996-2012, Valve Corporation, All rights reserved. ============//
#ifndef CONSTANTS_HDR
#define CONSTANTS_HDR

#include "tier0/basetypes.h"
#include "resourcefile/resourcestream.h"

//! Global tuning constants based on meter-kilogram-seconds (MKS) units. Here adjusted 
//! to inches for Source2  (1 inch ~ 2.5 cm <=> 1 cm ~ 0.4 inch <=> 1 m ~ 40 inch)


//---------------------------------------------------------------------------------------
// General 
//---------------------------------------------------------------------------------------
//! A small distance used as collision detection and dynamics tolerance. It is chosen 
//! to be numerically significant, but visually insignificant.
//! (This is 1/32 of an inch)
#define LINEAR_SLOP					0.03125f

//! A small angle used a collision detection and dynamics tolerance. It is chosen 
//! to be numerically significant, but visually insignificant.
#define ANGULAR_SLOP				(2.0f / 180.0f * M_PI)


//---------------------------------------------------------------------------------------
// Convex hulls 
//---------------------------------------------------------------------------------------
//! The *desired* maximum number of vertices on a convex hull. The number of vertices is 
//! chosen such that quadric shapes (e.g. cylinder and cones) can be approximated smoothly. 
//! Do not change this value!
#define DESIRED_HULL_VERTICES		 32

//! Maximum number of vertices supported at runtime on a convex hull.
//! Do not change this value!
#define MAX_HULL_VERTICES			256

//! Maximum number of half-edges supported at runtime on a convex hull.
//! Do not change this value!
#define MAX_HULL_HALFEDGES			256

//! Maximum number of faces supported at runtime on a convex hull.
//! Do not change this value!
#define MAX_HULL_FACES				256

//! The relative weld tolerance when pre-processing hull vertices.
//! This value is dimensionless!
#define RELATIVE_WELD_TOLERANCE		0.01f


//---------------------------------------------------------------------------------------
// Broadphase 
//---------------------------------------------------------------------------------------
//! The maximum stack depth for iterative tree traversals.
#define STACK_SIZE					64

//! Constant to indicate an invalid proxy identifier
#define NULL_PROXY					-1

//! The initial node capacity of the dynamic tree
#define INITIAL_NODE_CAPACITY		32

//! This is used to fatten AABBs in the dynamic tree. This allows proxies
//! to move by a small amount without triggering a tree adjustment.
//! (This is 0.1 [m] in MKS)
#define BOUNDS_EXTENSION			4.0f		

//! This is used to fatten AABBs in the dynamic tree. This is used to predict
//! the future position based on the current displacement.
//! (This is a dimensionless multiplier)
#define BOUNDS_MULTIPLIER			2.0f


//---------------------------------------------------------------------------------------
// Collision 
//---------------------------------------------------------------------------------------
//! The maximum number of contact points between two touching shapes. Do not change this value!
#define MAX_CONTACT_POINTS			4

//! The number of buckets used in the mesh SAH builder
#define SAH_BUILDER_BUCKET_COUNT	32

//! The desired number of triangles in a leaf node of the mesh BVH
#define DESIRED_TRIANGLES_PER_LEAF	4

//! The maximum number of triangles in a leaf node of the mesh BVH
#define MAXIMUM_TRIANGLES_PER_LEAF	8

//! The maximum number of iteration of the TOI root finder
#define MAX_ROOT_FINDER_ITERATIONS	64

//! The radius of the hull/mesh shape skin. This should not be modified. Making
//! this smaller means polytopes will have an insufficient buffer for continuous 
//! collision. Making it larger may create artifacts for vertex collision.
#define CONVEX_RADIUS			  (2.0f * LINEAR_SLOP)

//! The maximum number of sub-steps per contact in a continuous physics simulation.
#define MAX_SUBSTEPS				4


//---------------------------------------------------------------------------------------
// Dynamics 
//---------------------------------------------------------------------------------------
//! Define default solver position iterations. 
#define DEFAULT_POSITION_ITERATIONS	2

//! Define default solver velocity iterations.  
#define DEFAULT_VELOCITY_ITERATIONS	8

//! Define default gravity. 
//! (This is 9 [m/s^2] in MKS)
#define DEFAULT_GRAVITY				360.0f

//! Default air density, taken from CDragController 
#define DEFAULT_AIR_DENSITY			2.0f;

//! The maximum linear velocity of a body. This limit is very large and is used
//! to prevent numerical problems. You shouldn't need to adjust this.
#define MAX_TRANSLATION				 80.0f

//! The maximum angular velocity of a body. This limit is very large and is used
//! to prevent numerical problems. You shouldn't need to adjust this. 
#define MAX_ROTATION				(0.25f * M_PI)

//! A velocity threshold for elastic collisions. Any collision with a relative linear
//! velocity below this threshold will be treated as inelastic.
//! (This is 1 [m/s] in MKS)
#define VELOCITY_THRESHOLD			40.0f

//! The maximum linear position correction used when solving constraints. This helps to
//! prevent overshoot.
//! (This is 0.2 [m] in MKS)
#define MAX_LINEAR_CORRECTION		8.0f

//! The maximum angular position correction used when solving constraints. This helps to
//! prevent overshoot.
#define MAX_ANGULAR_CORRECTION		(8.0f / 180.0f * M_PI)

//! A Baumgarte like factor when using projection for constraint stabilization.	This only
//! effects contacts and limits. We want to avoid overshooting so this value should be in
//! range 0.1 - 0.2. Overshooting can result in loosing contacts and warm-starting information
//! which has a bunch of negative side effects (e.g. jitter and bad friction).
//! (This is a dimensionless multiplier)
#define TOI_BAUMGARTE				0.75f
#define POSITION_BAUMGARTE			0.2f

//! Factor when using Baumgarte stabilization to stabilize velocity constraints. This will
//! effect all joints and add momentum when enabled.
//! (This is a dimensionless multiplier)
#define VELOCITY_BAUMGARTE			0.1f


//---------------------------------------------------------------------------------------
// Sleeping
//---------------------------------------------------------------------------------------
//! The time in seconds that a body must be still before it will go to sleep.
#define TIME_TO_SLEEP				0.5f

//! A body cannot sleep if its linear velocity is above this tolerance.
//! (This is 0.01 [m/s] in MKS)
#define LINEAR_SLEEP_TOLERANCE		0.4f

//! A body cannot sleep if its angular velocity is above this tolerance.
#define ANGULAR_SLEEP_TOLERANCE	  (2.0f / 180.0f * M_PI)


//---------------------------------------------------------------------------------------
// Default joint parameter
//---------------------------------------------------------------------------------------
#define DEFAULT_JOINT_CFM	float( 0.0f )
#define DEFAULT_JOINT_ERP	float( 0.1f )


// dummy schema data: workaround for a build dependency issue
schema struct RnDummy_t  
{
	int32 m_nDummy;
};


#endif