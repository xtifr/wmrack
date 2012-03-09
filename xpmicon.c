/*
 * $Id: xpmicon.c,v 1.1.1.1 2001/02/12 22:26:11 xtifr Exp $
 *
 * part of WMRack
 *
 * handles the whole pixmap stuff (styles)
 *
 * Copyright (c) 1997 by Oliver Graf <ograf@fga.de>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/xpm.h>
#include <X11/extensions/shape.h>

#include "library.h"
#include "xpmicon.h"

/* include the default xpms */
#include "XPM/standart.style"

/*
 * first some requires variables
 */
XpmAttributes rackDefaultAttr;
XpmIcon rackXpm[]={{"cdnodisc",cdnodisc},
		   {"cdstopped",cdstopped},
		   {"cdplaying",cdplaying},
		   {"cdpaused",cdpaused},
		   {"mixer",mixer},
		   {"cdled",cdled},
		   {"mixled",mixled},
		   {"alphaled",alphaled}};

int curRack=RACK_NODISC;

XpmColorSymbol LedColorSymbols[4]={{"led_color_back", "#000000000000", 0},
				   {"led_color_high", "#0000FFFF0000", 0},
				   {"led_color_med",  "#00009CE60000", 0},
				   {"led_color_low",  "#000063180000", 0}};

char *ledAlphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";

/*
 * some internal functions
 */
/****************************************************************************/
Pixel xpm_getDimColor(Display *disp, Drawable draw, char *ColorName, float dim)
{
  XColor Color;
  XWindowAttributes Attributes;

  XGetWindowAttributes(disp,draw,&Attributes);
  Color.pixel = 0;

  if (!XParseColor (disp, Attributes.colormap, ColorName, &Color))
    fprintf(stderr,"xpm_getDimColor: can't parse %s\n", ColorName);
  else
    {
      Color.red/=dim;
      Color.green/=dim;
      Color.blue/=dim;
      Color.flags=DoRed|DoGreen|DoBlue;
      if (!XAllocColor (disp, Attributes.colormap, &Color))
	fprintf(stderr,"xpm_getDimColor: can't allocate %s\n", ColorName);
    }

  return Color.pixel;
}

int parseXpm(Display *disp, Drawable draw, char *buffer)
{
  int i, len;
  char *d1, *d2;

  /* check header */
  if (strncmp(buffer,"/* XPM */\nstatic char *",23)!=0)
    {
      fprintf(stderr,"parseXpm: invalid start of xpm\n");
      fprintf(stderr,buffer);
      return 1;
    }
  /* skip spaces */
  for (d1=buffer+23; *d1==32; d1++);
  /* find end */
  d2=strchr(d1,'[');
  if (d2==NULL)
    {
      fprintf(stderr,"parseXpm: can't parse xpm-name\n");
      return 1;
    }
  for (; *(d2-1)==32; d2--);
  len=d2-d1;
#ifdef DEBUG
  {
    char name[200];
    strncpy(name,d1,len);
    name[len]=0;
    fprintf(stderr,"parseXpm: found XPM %s\n",name);
  }
#endif
  /* find the right place */
  for (i=0; i<RACK_MAX; i++)
    {
      if (strncmp(rackXpm[i].name,d1,len)==0)
	{
	  if (rackXpm[i].loaded)
	    {
	      fprintf(stderr,"parseXpm: skipping double XPM for %s\n",d1);
	      break;
	    }
	  else
	    {
	      rackXpm[i].attributes=rackDefaultAttr;
	      if (XpmCreatePixmapFromBuffer(disp, draw, buffer,
					    &rackXpm[i].pixmap,
					    &rackXpm[i].mask,
					    &rackXpm[i].attributes)!=XpmSuccess)
		{
		  fprintf(stderr,"parseXpm: can't create pixmap from buffer\n");
		  return 1;
		}
	      rackXpm[i].loaded=1;
	      break;
	    }
	}
    }
  return 0;
}

/*
 * now the interface functions
 */

/*
 * xpm_setDefaultAttr(color)
 *
 * creates the attributes for xpm pixmap creation and sets
 * the led symbol colors to the specified color and 2 shades of it.
 */
int xpm_setDefaultAttr(Display *disp, Drawable draw, char *color, char *back)
{
  rackDefaultAttr.valuemask=XpmReturnPixels | XpmReturnExtensions | XpmColorSymbols;
  rackDefaultAttr.numsymbols=4;
  if (color!=NULL)
    {
      LedColorSymbols[1].value=NULL;
      LedColorSymbols[1].pixel=xpm_getDimColor(disp,draw,color,1);
      LedColorSymbols[2].value=NULL;
      LedColorSymbols[2].pixel=xpm_getDimColor(disp,draw,color,1.65);
      LedColorSymbols[3].value=NULL;
      LedColorSymbols[3].pixel=xpm_getDimColor(disp,draw,color,2.6);
    }
  if (back!=NULL)
    {
      LedColorSymbols[0].value=NULL;
      LedColorSymbols[0].pixel=xpm_getDimColor(disp,draw,back,1);
    }
  rackDefaultAttr.colorsymbols=LedColorSymbols;
  return 0;
}

/*
 * xpm_loadSet(display, drawable, filename)
 *
 * loads a style file
 */
int xpm_loadSet(Display *disp, Drawable draw, char *filename)
{
  FILE *f;
  char *buffer=NULL, line[4096], path[4096];
  int bufpos=0, bufalloc=0, i;

  buffer=lib_findfile(filename,1);
  if (buffer==NULL)
    {
      fprintf(stderr,"xpm_loadSet: can't find file\n");
      return 1;
    }
  strcpy(path,buffer);
  buffer=NULL;
#ifdef DEBUG
  fprintf(stderr,"xpm_loadSet: loading %s\n",path);
#endif

  f=fopen(path,"r");
  if (f==NULL)
    {
      perror("xpm_loadSet");
      return 1;
    }

  for (i=0; i<RACK_MAX; i++)
    rackXpm[i].loaded=0;

  while (fgets(line,4095,f)!=NULL)
    {
      if (strcmp(line,"/* XPM */\n")==0)
	{
	  if (buffer!=NULL)
	    {
	      if (parseXpm(disp,draw,buffer))
		{
		  free(buffer);
		  fclose(f);
		  return 1;
		}
	      free(buffer);
	      bufpos=bufalloc=0;
	    }
	  buffer=(char *)malloc(4096);
	  if (buffer==NULL)
	    {
	      fprintf(stderr,"xpm_loadSet: can' alloc enough mem\n");
	      fclose(f);
	      return 1;
	    }
	  bufalloc=4096;
	  strcpy(buffer+bufpos,line);
	  bufpos=strlen(buffer);
	  continue;
	}
      if (buffer!=NULL)
	{
	  if (bufpos+strlen(line)>=bufalloc)
	    {
	      char *newbuf;
	      newbuf=(char *)realloc(buffer,bufalloc+4096);
	      if (newbuf==NULL)
		{
		  fprintf(stderr,"xpm_loadSet: can' alloc enough mem\n");
		  fclose(f);
		  return 1;
		}
	      buffer=newbuf;
	      bufalloc+=4096;
	    }
	  strcpy(buffer+bufpos,line);
	  bufpos+=strlen(line);
	}
    }
  fclose(f);

  if (buffer!=NULL)
    {
      if (parseXpm(disp,draw,buffer))
	{
	  free(buffer);
	  return 1;
	}
      free(buffer);
    }

  /* check if all xpms are loaded */
  for (i=0; i<RACK_MAX; i++)
    if (!rackXpm[i].loaded)
      {
	fprintf(stderr,"xpm_loadSet: xpm for %s is missing, using default\n",
		rackXpm[i].name);
	if (xpm_setDefaultSet(disp,draw,i))
	  {
	    fprintf(stderr,"xpm_loadSet: can't set default pixmap\n");
	    return 1;
	  }
      }

  return 0;
}

/*
 * int xpm_getDefaultsSet(display, drawable, num)
 *
 * sets the specified pixmap buffer to the compiletime default
 * sets all pixmap buffers, if num is RACK_MAX
 *
 * returns 0 on success
 */
int xpm_setDefaultSet(Display *disp, Drawable draw, int num)
{
  int i, ret=0, x;

  if (num==RACK_MAX)
    for (i=0; i<RACK_MAX; i++)
      {
	if (rackXpm[i].standart==NULL)
	  continue;
	rackXpm[i].attributes=rackDefaultAttr;
	x=XpmCreatePixmapFromData(disp, draw, rackXpm[i].standart,
				  &rackXpm[i].pixmap,
				  &rackXpm[i].mask,
				  &rackXpm[i].attributes)!=XpmSuccess;
	ret|=x;
#ifdef DEBUG
	if (x)
	  fprintf(stderr,"xpm_setDefaultSet: failure to load %d (XpmErrNo %d)\n",i,x);
#endif
      }
  else
    {
      if (rackXpm[num].standart==NULL)
	return 0;
      rackXpm[num].attributes=rackDefaultAttr;
      ret|=XpmCreatePixmapFromData(disp, draw, rackXpm[num].standart,
				   &rackXpm[num].pixmap,
				   &rackXpm[num].mask,
				   &rackXpm[num].attributes)!=XpmSuccess;
#ifdef DEBUG
      if (ret)
	fprintf(stderr,"xpm_setDefaultSet: failure to load %d (XpmErrNo %d)\n",i,x);
#endif
    }
  return ret;
}

/*
 * xpm_freeSet(display)
 *
 * frees all pixmap data
 */
void xpm_freeSet(Display *disp)
{
  int i;

#ifdef DEBUG
  fprintf(stderr,"xpm_freeSet: freeing pixmaps\n");
#endif
  for (i=0; i<RACK_MAX; i++)
    {
      XFreePixmap(disp,rackXpm[i].pixmap);
      XFreePixmap(disp,rackXpm[i].mask);
    }
#ifdef DEBUG
  fprintf(stderr,"xpm_freeSet: done.\n");
#endif
}

