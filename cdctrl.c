/*
 * $Id: cdctrl.c,v 1.1.1.1 2001/02/12 22:25:33 xtifr Exp $
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
      printf("discid: %08lX\n",cd_info(cd,discid));
      printf("Tracks: %d\n",cd_info(cd,tracks));
      for (i=0; i<cd_info(cd,tracks); i++)
	{
	  printf("%2d[%2d]: start(%02d:%02d:%02d) end(%02d:%02d:%02d) len(%02d:%02d:%02d) %s\n",i,
		 cd_info(cd,track)[i].num,
		 cd_info(cd,track)[i].start.minute,
		 cd_info(cd,track)[i].start.second,
		 cd_info(cd,track)[i].start.frame,
		 cd_info(cd,track)[i].end.minute,
		 cd_info(cd,track)[i].end.second,
		 cd_info(cd,track)[i].end.frame,
		 cd_info(cd,track)[i].length.minute,
		 cd_info(cd,track)[i].length.second,
		 cd_info(cd,track)[i].length.frame,
		 cd_info(cd,track)[i].data?"DATA":"");
	}
    showlist:
      printf("PlayList: %d length(%02d:%02d:%02d)\n",
	     cd_list(cd,tracks),
	     cd_list(cd,length).minute,
	     cd_list(cd,length).second,
	     cd_list(cd,length).frame);
      for (i=0; i<cd_list(cd,tracks); i++)
	{
	  printf("%2d[%2d]: start(%02d:%02d:%02d) end(%02d:%02d:%02d) len(%02d:%02d:%02d) %s\n",i,
		 cd_list(cd,track)[i].num,
		 cd_list(cd,track)[i].start.minute,
		 cd_list(cd,track)[i].start.second,
		 cd_list(cd,track)[i].start.frame,
		 cd_list(cd,track)[i].end.minute,
		 cd_list(cd,track)[i].end.second,
		 cd_list(cd,track)[i].end.frame,
		 cd_list(cd,track)[i].length.minute,
		 cd_list(cd,track)[i].length.second,
		 cd_list(cd,track)[i].length.frame,
		 cd_list(cd,track)[i].data?"DATA":"");
	}
      break;
    case 1:
      printf("status is %d\n",status);
      printf("mode %d track %d\n",
	     cd_cur(cd,mode),cd_cur(cd,track));
      if (cd_cur(cd,track)!=-1)
	printf("%d[%d]: %02d:%02d:%02d->%02d:%02d:%02d len(%02d:%02d:%02d) d%d\n",
	       cd_cur(cd,track),
	       cd_list(cd,track)[cd_cur(cd,track)].num,
	       cd_list(cd,track)[cd_cur(cd,track)].start.minute,
	       cd_list(cd,track)[cd_cur(cd,track)].start.second,
	       cd_list(cd,track)[cd_cur(cd,track)].start.frame,
	       cd_list(cd,track)[cd_cur(cd,track)].end.minute,
	       cd_list(cd,track)[cd_cur(cd,track)].end.second,
	       cd_list(cd,track)[cd_cur(cd,track)].end.frame,
	       cd_list(cd,track)[cd_cur(cd,track)].length.minute,
	       cd_list(cd,track)[cd_cur(cd,track)].length.second,
	       cd_list(cd,track)[cd_cur(cd,track)].length.frame,
	       cd_list(cd,track)[cd_cur(cd,track)].data);
      else
	printf("no current track\n");
      printf("rel %02d:%02d:%02d abs %02d:%02d:%02d\n",
	     cd_cur(cd,relmsf.minute),
	     cd_cur(cd,relmsf.second),
	     cd_cur(cd,relmsf.frame),
	     cd_cur(cd,absmsf.minute),
	     cd_cur(cd,absmsf.second),
	     cd_cur(cd,absmsf.frame));
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
