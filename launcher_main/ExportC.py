# Export from maya directly into simple vertex / index buffer for direct rendering 
# Usage: just run the script below on a selected mesh object. it'll print vertex and index buffers


import sys
import math
import os
import StringIO
import maya.OpenMaya as OpenMaya
import maya.OpenMayaMPx as OpenMayaMPx

selList = OpenMaya.MSelectionList()
OpenMaya.MGlobal.getActiveSelectionList(selList)
selListIter = OpenMaya.MItSelectionList(selList)

listVertices = []
listFaces = []

while not selListIter.isDone():
	dagPath = OpenMaya.MDagPath()
	component = OpenMaya.MObject()
	selListIter.getDagPath(dagPath, component)
	
	rot = OpenMaya.MQuaternion()
	
	dagTransform = dagPath.transform()
	
	if( dagTransform.apiType() == OpenMaya.MFn.kTransform ):
		xformFn = OpenMaya.MFnTransform(dagTransform)
		xform = xformFn.transformation().asRotateMatrix()
		xformFn.getRotation( rot )
	else:
		xform = OpenMaya.MMatrix()
		xform.setToIdentity()
	
	dagPath.extendToShape()
	if (dagPath.apiType() == OpenMaya.MFn.kMesh):
		mesh = dagPath.node()
		meshFn = OpenMaya.MFnMesh(mesh)

		vertIter = OpenMaya.MItMeshVertex( dagPath, component )
		print "// ", vertIter.count(), " verts in ", dagPath.partialPathName()
		print "ErrorRenderLoop::Vertex_t g_verts_%s[%d] = {" % (dagPath.partialPathName(), vertIter.count())
		while not vertIter.isDone():
			vertPos = OpenMaya.MVector(vertIter.position()).rotateBy(rot)
			vertColor = OpenMaya.MColor(1,1,1,1)
			vertNormal = OpenMaya.MVector()
			vertIter.getNormal( vertNormal )
			if( vertIter.hasColor() ):
				vertIter.getColor( vertColor )
			vertNormal = vertNormal.rotateBy(rot)
			print "{%.3f,%.3f,%.3f,  %.3f,%.3f,%.3f, 0x%02X%02X%02X%02X }," % (vertPos.x,vertPos.y,vertPos.z, vertNormal.x, vertNormal.y, vertNormal.z,  vertColor.r*255, vertColor.g*255,vertColor.b*255, vertColor.a*255 )
			vertIter.next()
		print "};"
		
		faceIter = OpenMaya.MItMeshPolygon(dagPath,component)
		print "// ", faceIter.count(), " triangles"
		print "uint16_t g_tris_%s[%d][3] = {" % ( dagPath.partialPathName(), faceIter.count() )
		faceCount = 0
		while not faceIter.isDone():
			faceVerts = OpenMaya.MIntArray()
			faceIter.getVertices( faceVerts )
			print ("" if faceCount == 0 else ", "),  "{ ", ", ".join( str(x) for x in faceVerts ), " }"
			faceCount = faceCount + 1
			faceIter.next()
		print "};"
		
	selListIter.next()
