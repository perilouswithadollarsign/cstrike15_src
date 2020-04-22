import vs
import vs.dmeutils
from vs import dmeutils

animUtils = dmeutils.CDmAnimUtils
DagList = dmeutils.DagList


def AreVectorsEqual( vectorA, vectorB ):
    if ( vectorA.x != vectorB.y ):
        return False
    if ( vectorA.y != vectorB.y ):
        return False
    if ( vectorA.z != vectorB.z ):
        return False
    return True

root = vs.g_pDataModel.RestoreFromFile( "./test.dmx" )
print root

# Get the cube and sphere dag nodes from the scene
cubeDag = root.model.children[ 0 ]
print cubeDag
sphereDag = root.model.children[ 1 ]
print sphereDag
        
# Test getting the position and rotatation of a dag
cubePos = animUtils.GetDagPosition( DagList( cubeDag ), vs.TS_WORLD_SPACE )
cubeRot = animUtils.GetDagRotation( DagList( cubeDag ), vs.TS_WORLD_SPACE )
print cubePos
print cubeRot

# Create a test dag
testDag = animUtils.CreateDag( "testDag", vs.vec3_origin, vs.quat_identity )
print testDag

# Test creating a dag list with multiple dag nodes
dagList = vs.DagList( cubeDag, sphereDag, testDag )

# Move the test dag
animUtils.TransformDagNodes( vs.Vector( 5, 5, 5 ), vs.quat_identity, DagList( testDag ), False, vs.dmeutils.TS_WORLD_SPACE )
testPos = animUtils.GetDagPosition( DagList( testDag ), vs.TS_WORLD_SPACE )
print testPos 

# Constrain the test dag, this should overwrite the position of the test dag
testConstraint = animUtils.CreatePointConstraint( "testConstraint", testDag, DagList( cubeDag ), False, 1.0 )
print testConstraint

testPos = animUtils.GetDagPosition( DagList( testDag ), vs.TS_WORLD_SPACE )
print testPos 

# Generate log samples for the test dag evaluating the constraint
animUtils.GenerateLogSamples( testDag, None, True, True )


# Make the sphere a cild of the cube, but keep its world space 
# position, so the position should not change with this operation
print "Re-parent with maintain world space, following position should be the same."
spherePos = vs.CDmAnimUtils_GetDagPosition( vs.DagList( sphereDag ), vs.TS_WORLD_SPACE )
print spherePos

vs.dmeutils.CDmAnimUtils_ReParentDagNode( sphereDag, cubeDag, True, vs.REPARENT_LOGS_MAINTAIN_WORLD )
spherePos = vs.CDmAnimUtils_GetDagPosition( vs.DagList( sphereDag ), vs.TS_WORLD_SPACE )
print spherePos

# Now move the cube, this should change the position of both the cube 
# and the sphere since the sphere should now be a child of the cube.
print "Now the world space position of the sphere should have changed since its parent position changed"
vs.dmeutils.CDmAnimUtils_MoveDagNodes( vs.Vector( 0, 0, 10 ), vs.DagList( cubeDag ), True, vs.dmeutils.TS_WORLD_SPACE )
spherePos = vs.CDmAnimUtils_GetDagPosition( vs.DagList( sphereDag ), vs.TS_WORLD_SPACE )
print spherePos











