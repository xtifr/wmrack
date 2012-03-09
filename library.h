/*
 * $Id: library.h,v 1.2 2003/10/01 22:44:19 xtifr Exp $
 *
 * part of wmrack
 *
 * handles the library path searchs
 *
 * Copyright (c) 1997 by Oliver Graf <ograf@fga.de>
 */
#ifndef WMRACK_LIBRARY_H_
#define WMRACK_LIBRARY_H_

#define LIB_CLOSED 0
#define LIB_READ   1
#define LIB_WRITE  2
#define LIB_APPEND 3

typedef struct
{
  FILE *f;
  char *name;
  int mode;
} LIBRARY;

char *lib_findfile(char *name, int here);
LIBRARY *lib_open(char *name, int mode);
int lib_close(LIBRARY *lib);
int lib_free(LIBRARY *lib);
int lib_reopen(LIBRARY *lib, int mode);
char *lib_gets(LIBRARY *lib, char *line, int len);
int lib_printf(LIBRARY *lib, char *format, ...);

#endif /* WMRACK_LIBRARY_H_ */
