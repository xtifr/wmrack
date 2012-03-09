/*
 * $Id: library.c,v 1.1.1.1 2001/02/12 22:25:47 xtifr Exp $
 *
 * part of wmrack
 *
 * handles the library path searchs
 *
 * Copyright (c) 1997 by Oliver Graf <ograf@fga.de>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include "config.h"
#include "library.h"

static char *lib_mode[]={"", "r", "w", "a"};

#ifdef HAVE_GNUSTEP
static char *lib_global_path[]={"/usr/local/GNUstep/Library/WMRack/",
				"/usr/GNUstep/Library/WMRack/",
				NULL};

static char *lib_personal_path[]={"/GNUstep/Library/WMRack/",
				  NULL};
#else
static char *lib_global_path[]={"/usr/X11R6/lib/X11/WMRack/",
				NULL};

static char *lib_personal_path[]={"/.wmrack/",
				  NULL};
#endif /* HAVE_GNUSTEP */

static char lib_stat_path[4096]="";

char *lib_findfile(char *name, int here)
{
  char path[4096]="";
  int i;
  struct stat st;

  if (here)
    {
      strcpy(path,name);
      if (stat(path,&st)!=0)
	*path=0;
    }

  if (*path==0)
    for (i=0; lib_personal_path[i]; i++)
      {
	strcpy(path,getenv("HOME"));
	strcat(path,lib_personal_path[i]);
	if (here)
	  {
	    strcat(path,name);
	    if (stat(path,&st)==0)
	      break;
	  }
	else
	  if (stat(path,&st)==0)
	    {
	      strcat(path,name);
	      break;
	    }
	*path=0;
      }

  if (*path==0)
    {
      for (i=0; lib_global_path[i]; i++)
	{
	  strcpy(path,lib_global_path[i]);
	  strcat(path,name);
	  if (stat(path,&st)==0)
	    break;
	  *path=0;
	}
      if (*path==0)
	{
	  strcpy(path,getenv("HOME"));
	  strcat(path,lib_personal_path[0]);
	  if (mkdir(path,0755))
	    return NULL;
	  strcat(path,name);
	}
    }

  strcpy(lib_stat_path,path);

  return lib_stat_path;
}

LIBRARY *lib_open(char *name, int mode)
{
  char *p=NULL;
  LIBRARY *lib=NULL;
  FILE *f=NULL;

  p=lib_findfile(name,0);
  if (p==NULL)
    return NULL;

  f=fopen(p,lib_mode[mode]);
  if (f!=NULL)
    {
      lib=(LIBRARY *)malloc(sizeof(LIBRARY));
      lib->name=strdup(p);
      lib->f=fopen(lib->name,lib_mode[mode]);
      lib->mode=mode;
    }
  return lib;
}

int lib_close(LIBRARY *lib)
{
  fclose(lib->f);
  lib->f=NULL;
  lib->mode=LIB_CLOSED;
  return 0;
}

int lib_free(LIBRARY *lib)
{
  lib_close(lib);
  free(lib->name);
  free(lib);
  return 0;
}

int lib_reopen(LIBRARY *lib, int mode)
{
  lib_close(lib);
  lib->f=fopen(lib->name,lib_mode[mode]);
  if (lib->f==NULL)
    return -1;
  return 0;
}

char *lib_gets(LIBRARY *lib, char *line, int len)
{
  if (lib->mode==LIB_READ)
    return fgets(line,len,lib->f);
  return NULL;
}

int lib_printf(LIBRARY *lib, char *format, ...)
{
  va_list args;
  int s;

  if (lib->mode==LIB_READ)
    return 0;
  va_start(args,format);
  s=vfprintf(lib->f,format,args);
  va_end(args);
  return s;
}
