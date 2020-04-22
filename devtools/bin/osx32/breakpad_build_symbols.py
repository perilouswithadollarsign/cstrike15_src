#!/usr/bin/env python

import datetime
import getopt
import glob
import os
import pickle
import shutil
import sys
import zipfile
import re
import subprocess as sp

import pdb

def ShowUsage():
	print "breakpad_build_symbols.py [options] bindirectory symboldirectory]"
	print "-f/--force              Force rebuild of .sym files."

# 
# Set program defaults.
#
g_bForce = False

def main():
	global g_bForce
	
	try:
		opts, args = getopt.getopt( sys.argv[1:], "f", [ "force" ] )
	
	except getopt.GetoptError, e:
		print ""
		print "Argument error: ", e
		print ""
		ShowUsage()
		sys.exit(1)

	for o, a in opts:
		if o in ( "-f", "--force" ):
			g_bForce = True

	# now look for all files in the specified path
	print "building symbols for %s to %s" % ( args[ 0 ], args[ 1 ] )
	
	dump_syms = os.getcwd() + "/dump_syms"

        rebuildcount = 0
        visitcount = 0
	for root, dirs, files in os.walk(args[ 0 ]):
                for name in dirs:
			dsymdirname = os.path.join(root, name)
			#print "checking %s" % dsymdirname
			if dsymdirname[-5:] == '.dSYM':
                                visitcount += 1
				dylibfiletime = os.path.getmtime( dsymdirname )
				# get the first line
				command = dump_syms + " -g " + dsymdirname
				p = sp.Popen( command, stdout=sp.PIPE, stderr=sp.PIPE, shell=True )
				firstline = p.communicate()[ 0 ];
                                #line syntax
                                # MODULE mac x86 59C9A56A5EB38C85A185BA60877C89610 engine.dylib
                                tokens = firstline.split()
                                if ( len( tokens ) != 5 ):
                                        continue
                                rawlibname = tokens[ 4 ]
                                # print "shortname %s\n" % rawlibname
                                symdir = args[1] + "/" + tokens[ 4 ] + "/" + tokens[ 3 ]
                                if not os.path.isdir( symdir ):
                                        os.makedirs( symdir )
                                symfile = symdir + "/" + rawlibname + ".sym"
				if ( os.path.exists( symfile ) ):
                                   # check time stamp
                                   symfilefiletime = os.path.getmtime( symfile )
				   symfilesize = os.path.getsize( symfile )
                                   # print " %s %d %d" % (symfile, dylibfiletime, symfilefiletime)
                                   if ( symfilefiletime >= dylibfiletime and not g_bForce and symfilesize > 0 ):
                                      continue

                                # do full processing
				command = dump_syms + " " + dsymdirname
				p = sp.Popen( command, stdout=sp.PIPE, stderr=sp.PIPE, shell=True )
				symbols = p.communicate()[ 0 ]
                                print "  :%s" % symfile
				# print "     %s" % symbols
				fd = open( symfile, 'wb' )
				fd.write( symbols )
				fd.close()
				# force time stamp
                                os.utime( symfile, ( dylibfiletime, dylibfiletime ) )
                                rebuildcount += 1
        
	print " rebuilt %d out of %d symbol files" % ( rebuildcount, visitcount )

if __name__ == '__main__':
	main()
 
