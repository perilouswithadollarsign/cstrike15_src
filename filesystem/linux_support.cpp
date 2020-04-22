//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $  
//=============================================================================//

#include <dirent.h>

#include "tier1/strtools.h"
#include "tier0/memdbgoff.h"
#include "linux_support.h"

#ifdef OSX
#include <AvailabilityMacros.h>
#endif

char selectBuf[PATH_MAX];

#if defined(OSX) && !defined(MAC_OS_X_VERSION_10_9)
int FileSelect(struct dirent *ent)
#else
int FileSelect(const struct dirent *ent)
#endif
{
	const char *mask=selectBuf;
	const char *name=ent->d_name;
	
	//printf("Test:%s %s\n",mask,name);
	
	if(!strcmp(name,".") || !strcmp(name,"..") ) return 0;
	
	if(!strcmp(selectBuf,"*.*")) return 1;
	
	while( *mask && *name )
	{
		if(*mask=='*')
		{
			mask++; // move to the next char in the mask
			if(!*mask) // if this is the end of the mask its a match 
			{
				return 1;
			}
			while(*name && toupper(*name)!=toupper(*mask)) 
			{ // while the two don't meet up again
				name++;
			}
			if(!*name) 
			{ // end of the name
				break; 
			}
		}
		else if (*mask!='?')
		{
			if( toupper(*mask) != toupper(*name) )
			{	// mismatched!
				return 0;
			}
			else
			{	
				mask++;
				name++;
				if( !*mask && !*name) 
				{ // if its at the end of the buffer
					return 1;
				}
				
			}
			
		}
		else /* mask is "?", we don't care*/
		{
			mask++;
			name++;
		}
	}	
	
	return( !*mask && !*name ); // both of the strings are at the end
}

int FillDataStruct(FIND_DATA *dat)
{
	struct stat fileStat;
	
	if(dat->numMatches<0)
		return -1;
	
	char szFullPath[MAX_PATH];
	Q_snprintf( szFullPath, sizeof(szFullPath), "%s/%s", dat->cBaseDir, dat->namelist[dat->numMatches]->d_name );  
	
	if(!stat(szFullPath,&fileStat))
	{
		dat->dwFileAttributes=fileStat.st_mode;           
	}
	else
	{
		dat->dwFileAttributes=0;
	}	
	
	// now just put the filename in the output data
	Q_snprintf( dat->cFileName, sizeof(dat->cFileName), "%s", dat->namelist[dat->numMatches]->d_name );  
	
	//printf("%s\n", dat->namelist[dat->numMatches]->d_name);
	free(dat->namelist[dat->numMatches]);
	
  	dat->numMatches--;
	return 1;
}


HANDLE FindFirstFile( const char *fileName, FIND_DATA *dat)
{
	char nameStore[PATH_MAX];
	char *dir=NULL;
	int n,iret=-1;
	
	Q_strncpy(nameStore,fileName, sizeof( nameStore ) );
	
	if(strrchr(nameStore,'/') )
	{
		dir=nameStore;
		while(strrchr(dir,'/') )
		{
			struct stat dirChk;
			
			// zero this with the dir name
			dir=strrchr(nameStore,'/');
			*dir='\0';
			
			dir=nameStore;
			stat(dir,&dirChk);
			
			if( S_ISDIR( dirChk.st_mode ) )
			{
				break;	
			}
		}
	}
	else
	{
		// couldn't find a dir seperator...
		return (HANDLE)-1;
	}
	
	if( strlen(dir)>0 )
	{
		Q_strncpy(selectBuf,fileName+strlen(dir)+1, sizeof( selectBuf ) );
		Q_strncpy(dat->cBaseDir,dir, sizeof( dat->cBaseDir ) );
		dat->namelist = NULL;
		n = scandir(dir, &dat->namelist, FileSelect, alphasort);
		if (n < 0)
		{
			// silently return, nothing interesting
			dat->namelist = NULL;
		}
		else 
		{
			dat->numMatches=n-1; // n is the number of matches
			iret=FillDataStruct(dat);
			if ( ( iret<0 ) && dat->namelist )
			{
				free(dat->namelist);
				dat->namelist = NULL;
			}
			
		}
	}
	
	//	printf("Returning: %i \n",iret);
	return (HANDLE)(intp)iret;
}

bool FindNextFile(HANDLE handle, FIND_DATA *dat)
{
	if(dat->numMatches<0)
	{	
		if ( dat->namelist != NULL )
		{
			free( dat->namelist );
			dat->namelist = NULL;
		}
		return false; // no matches left
	}	
	
	FillDataStruct(dat);
	return true;
}

bool FindClose(HANDLE handle)
{
	return true;
}



static char fileName[MAX_PATH];

#if defined(OSX) && !defined(MAC_OS_X_VERSION_10_9)
int CheckName(struct dirent *dir)
#else
int CheckName(const struct dirent *dir)
#endif
{
	return !strcasecmp( dir->d_name, fileName );
}


const char *findFileInDirCaseInsensitive(const char *file, char *pFileNameOut)
{

	const char *dirSep = strrchr(file,'/');
	if( !dirSep )
	{
		dirSep=strrchr(file,'\\');
		if( !dirSep ) 
		{
			return NULL;
		}
	}

	char *dirName = static_cast<char *>( alloca( ( dirSep - file ) +1 ) ); 
	if( !dirName )
		return NULL;

	strncpy( dirName , file, dirSep - file );
	dirName[ dirSep - file ] = '\0';

	struct dirent **namelist = NULL;

	strncpy( fileName, dirSep + 1, MAX_PATH );


	int n = scandir( dirName , &namelist, CheckName, alphasort );

	// Free all entries beyond the first one, we don't care about them
	while ( n > 1 )
	{
		-- n;
		free( namelist[n] );
	}
	
	if ( n > 0 )
	{
		Q_snprintf( pFileNameOut, sizeof( fileName ), "%s/%s", dirName, namelist[0]->d_name );
		free( namelist[0] );
		n = 0;
	}
	else
	{
		Q_strncpy( pFileNameOut, file, MAX_PATH );
		Q_strlower( pFileNameOut );
	}

	if ( ( n >= 0 ) && namelist )
	{
		free( namelist );
	}

	return pFileNameOut;
}

