/*
 * $Id: cdctrl.c,v 1.2 2003/10/01 22:44:19 xtifr Exp $
 *
 * Part of WMRack
 *
 * command line tool to control the cdrom
 * created to test the cdrom control stuff
 *
 * Copyright (c) 1997 by Oliver Graf <ograf@fga.de>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "cdrom.h"

char *commands[]={"info", "status", "play", "pause", "skip", "stop", "eject", "playlist"};
#define NUM_CMD 8

void usage()
{
  fprintf(stderr,"Usage: cdctrl [-d DEVICE] [info|status|play|pause|skip|stop|eject|playlist] PARAMS\n");
}

int main(int argc, char **argv)
{
  CD *cd;
  int fa=1, i, s=0, e=0xaa, status;
  char *device="/dev/cdrom";
  struct timeval tm;
  
  gettimeofday(&tm,NULL);
  srandom(tm.tv_usec^tm.tv_sec);

  if (argc<2)
    {
      usage();
      exit(EXIT_FAILURE);
    }

  if (strcmp(argv[1],"-d")==0)
    {
      device=argv[2];
      fa=3;
      argc-=2;
    }

  if (argc<2)
    {
      usage();
      exit(EXIT_FAILURE);
    }

  for (i=0; i<NUM_CMD; i++)
    if (strcmp(argv[fa],commands[i])==0)
      break;
  if (i==NUM_CMD)
    {
      usage();
      exit(EXIT_FAILURE);
    }

  cd=cd_open(device,0);
  if (cd->status==-1)
    printf("No CDROM in drive\n");
  else
    status=cd_getStatus(cd,0,1);

  switch (i)
    {
    case 0:
      printf("discid: %08lX\n",cd->info.discid);
      printf("Tracks: %d\n",cd->info.tracks);
      for (i=0; i<cd->info.tracks; i++)
	{
	  printf("%2d[%2d]: start(%02d:%02d:%02d) end(%02d:%02d:%02d) len(%02d:%02d:%02d) %s\n",i,
		 cd->info.track[i].num,
		 cd->info.track[i].start.minute,
		 cd->info.track[i].start.second,
		 cd->info.track[i].start.frame,
		 cd->info.track[i].end.minute,
		 cd->info.track[i].end.second,
		 cd->info.track[i].end.frame,
		 cd->info.track[i].length.minute,
		 cd->info.track[i].length.second,
		 cd->info.track[i].length.frame,
		 cd->info.track[i].data?"DATA":"");
	}
    showlist:
      printf("PlayList: %d length(%02d:%02d:%02d)\n",
	     cd->info.list.tracks,
	     cd->info.list.length.minute,
	     cd->info.list.length.second,
	     cd->info.list.length.frame);
      for (i=0; i<cd->info.list.tracks; i++)
	{
	  printf("%2d[%2d]: start(%02d:%02d:%02d) end(%02d:%02d:%02d) len(%02d:%02d:%02d) %s\n",i,
		 cd->info.list.track[i].num,
		 cd->info.list.track[i].start.minute,
		 cd->info.list.track[i].start.second,
		 cd->info.list.track[i].start.frame,
		 cd->info.list.track[i].end.minute,
		 cd->info.list.track[i].end.second,
		 cd->info.list.track[i].end.frame,
		 cd->info.list.track[i].length.minute,
		 cd->info.list.track[i].length.second,
		 cd->info.list.track[i].length.frame,
		 cd->info.list.track[i].data?"DATA":"");
	}
      break;
    case 1:
      printf("status is %d\n",status);
      printf("mode %d track %d\n",
	     cd->info.current.mode,cdinfo.current.track);
      if (cd->info.current.track!=-1)
	printf("%d[%d]: %02d:%02d:%02d->%02d:%02d:%02d len(%02d:%02d:%02d) d%d\n",
	       cd->info.current.track,
	       cd->info.list.track[cd->info.current.track].num,
	       cd->info.list.track[cd->info.current.track].start.minute,
	       cd->info.list.track[cd->info.current.track].start.second,
	       cd->info.list.track[cd->info.current.track].start.frame,
	       cd->info.list.track[cd->info.current.track].end.minute,
	       cd->info.list.track[cd->info.current.track].end.second,
	       cd->info.list.track[cd->info.current.track].end.frame,
	       cd->info.list.track[cd->info.current.track].length.minute,
	       cd->info.list.track[cd->info.current.track].length.second,
	       cd->info.list.track[cd->info.current.track].length.frame,
	       cd->info.list.track[cd->info.current.track].data);
      else
	printf("no current track\n");
      printf("rel %02d:%02d:%02d abs %02d:%02d:%02d\n",
	     cd->info.current.relmsf.minute,
	     cd->info.current.relmsf.second,
	     cd->info.current.relmsf.frame,
	     cd->info.current.absmsf.minute,
	     cd->info.current.absmsf.second,
	     cd->info.current.absmsf.frame);
      break;
    case 2:
      if (argc>2)
	s=atoi(argv[fa+1]);
      cd_doPlay(cd,s);
      break;
    case 3:
      cd_doPause(cd);
      break;
    case 4:
      if (argc!=3)
	{
	  fprintf(stderr,"skip needs only one arg (seconds)\n");
	  exit(EXIT_FAILURE);
	}
      s=atoi(argv[fa+1]);
      cd_doSkip(cd,s);
      break;
    case 5:
      cd_doStop(cd);
      break;
    case 6:
      cd_doEject(cd);
      break;
    case 7:
      if (argc==2)
	cd_randomize(cd);
      else
	{
	  CDPlayList *list=cdpl_new();
	  int t;
	  for (i=fa+1; i<argc+fa-1; i++)
	    {
	      t=cd_findtrack(cd,atoi(argv[i]));
	      if (t>=0)
		cdpl_add(list,cd,t);
	    }
	  cd_setpl(cd,list);
	}
      cd_doPlay(cd,0);
      fprintf(stderr,"\nWARNING: cdctrl CAN ONLY SET PLAYLIST, BUT PLAYING IS LINEAR\n\n");
      goto showlist;
    default:
      usage();
      exit(EXIT_FAILURE);
    }
  cd_close(cd);
  return EXIT_SUCCESS;
}
