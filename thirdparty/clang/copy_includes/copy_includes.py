import glob, sys, os, stat, shutil

g_nFilesCopied = 0
g_nFilesAdded = 0
g_nFilesSkipped = 0

def IsSame( a, b ):
	return False
	if a.st_size == b.st_size:
		if a.st_mtime == b.st_mtime:
			return True #perhaps we need to actually compare the files??
		if abs( a.st_mtime - b.st_mtime ) < 1: # may not be runnable on all platforms, but runs fine on windows python 2.6+
			return True
	else:
		return False

def Copy(fromDir, toDir, extensions):
	global g_nFilesCopied
	global g_nFilesAdded
	global g_nFilesSkipped
	if( toDir == "" ):
		toDir = os.getcwd()
	toDir = os.path.abspath( toDir )
	fromDir = os.path.abspath( fromDir )
	for root, subFolders, files in os.walk(fromDir):
		for file in files:
			if  os.path.splitext(file)[1][1:].strip() in extensions and root[:len(fromDir)]==fromDir:
				fromFile = os.path.join(root, file)
				fromFileStat = os.stat( fromFile )
				toFile = os.path.join(toDir,root[len(fromDir)+1:],file)
				p4AddNeeded = True
				if( os.path.isfile( toFile ) ): #file exists already, if it's the same file, there's no need to copy anything
					p4AddNeeded = False
					toFileStat = os.stat( toFile )

					if( IsSame( toFileStat, fromFileStat ) ):
						g_nFilesSkipped += 1
						continue
					if not( toFileStat.st_mode & stat.S_IWRITE ):
						os.system( "p4 edit " + toFile )

				if os.path.exists( os.path.dirname( toFile ) ):
					if not os.path.isdir( os.path.dirname( toFile ) ):
						print "This is not a dir. Expected a dir: " + os.path.dirname( toFile )
						sys.exit(-1)
				else:
					os.makedirs( os.path.dirname( toFile ) )

				#print fromFile + " -> " + toFile
				shutil.copyfile( fromFile, toFile )
				os.utime( toFile, ( fromFileStat.st_atime, fromFileStat.st_mtime ) )
				if p4AddNeeded:
					g_nFilesAdded += 1
					os.system( "p4 add " + toFile )
				g_nFilesCopied += 1

extInc = ["h","inc","inl","gen","def"]
extLib = ["lib","pdb","def"]

Copy( "f:/L/llvm.build64/tools/clang/include", "include/win64", extInc )
Copy( "f:/L/llvm.build64/include", "include/win64", extInc )
Copy( "f:/L/llvm.build/tools/clang/include", "include/win32", extInc )
Copy( "f:/L/llvm.build/include", "include/win32", extInc )
Copy( "f:/L/llvm/include", "include", extInc )
Copy( "f:/L/llvm/tools/clang/include", "include", extInc )
Copy( "f:/L/llvm.build64/lib/Debug", "lib/win64/Debug", extLib )
Copy( "f:/L/llvm.build64/lib/RelWithDebInfo", "lib/win64/Release", extLib )
Copy( "f:/L/llvm.build/lib/Debug", "lib/win32/Debug", extLib )
Copy( "f:/L/llvm.build/lib/RelWithDebInfo", "lib/win32/Release", extLib )


print "Files copied: %d, added: %d, skipped: %d" % ( g_nFilesCopied, g_nFilesAdded, g_nFilesSkipped )
