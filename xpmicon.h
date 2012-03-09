/*
 * $Id: xpmicon.h,v 1.1.1.1 2001/02/12 22:26:11 xtifr Exp $
 *
 * part of wmrack
 *
 * handles the whole pixmap stuff
 *
 * Copyright (c) 1997 by Oliver Graf <ograf@fga.de>
 */
#ifndef _XPMICON_H
#define _XPMICON_H

/* Xpm struct */
typedef struct {
  char *name;
  char **standart;
  int loaded;
  XpmAttributes attributes;
  Pixmap pixmap;
  Pixmap mask;
} XpmIcon;

/* the XpmIcon definitions and variables */
#define RACK_NODISC     0
#define RACK_STOP       1
#define RACK_PLAY       2
#define RACK_PAUSE      3
#define RACK_MIXER      4
#define RACK_LED_PLAYER 5
#define RACK_LED_MIXER  6
#define RACK_LED_ALPHA  7
#define RACK_MAX        8

extern XpmIcon rackXpm[];
extern int curRack;
extern char *ledAlphabet;

#define currentXpm(w) rackXpm[curRack].##w
#define ledXpm(x,w) rackXpm[x].##w

int xpm_setDefaultAttr(Display *disp, Drawable draw, char *color, char *back);
int xpm_loadSet(Display *disp, Drawable draw, char *filename);
int xpm_setDefaultSet(Display *disp, Drawable draw, int num);
void xpm_freeSet(Display *disp);

#endif
