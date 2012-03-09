/*
 * $Id: mixctrl.c,v 1.1.1.1 2001/02/12 22:25:47 xtifr Exp $
 *
 * Part of WMRack
 *
 * command line tool to control the mixer
 * created to test the mixer control stuff
 *
 * Copyright (c) 1997 by Oliver Graf <ograf@fga.de>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mixer.h"

void usage()
{
  fprintf(stderr,"Usage: mixctrl [-d DEVICE] [CHANNELNAME|CHANNELNUM] [0-100|0-100:0-100|src|nosrc|exsrc]\n");
}

int main(int argc, char **argv)
{
  MIXER *mix;
  int fa=1, i, volid, left=0, right=0, command=0;
  char *device="/dev/mixer", *d1;

  if (argc<1)
    {
      usage();
      exit(EXIT_FAILURE);
    }

  if (argc>2 && strcmp(argv[1],"-d")==0)
    {
      device=argv[2];
      argc-=2;
      fa=3;
    }

  if (argc>2)
    {
      volid=strtol(argv[fa],&d1,10);
      if (*d1!=0)
	{
	  for (volid=0; volid<mixer_devices; volid++)
	    if (strcmp(argv[fa],mixer_names[volid])==0)
	      break;
	}
      d1=strchr(argv[fa+1],':');
      if (d1)
	{
	  *d1++=0;
	  left=atoi(argv[fa+1]);
	  right=atoi(d1);
	  command=1;
	}
      else
	{
	  if (strcmp(argv[fa+1],"src")==0)
	    command=2;
	  else if (strcmp(argv[fa+1],"nosrc")==0)
	    command=3;
	  else if (strcmp(argv[fa+1],"exsrc")==0)
	    command=4;
	  else
	    {
	      left=atoi(argv[fa+1]);
	      command=0;
	    }
	}
    }

  mix=mixer_open(device);
  if (mix==NULL)
    printf("Can't open mixer\n");
  else
    {
      printf("Mixer initialized\n");
      printf("Mixer id %s, Mixer name %s\n",mix->id,mix->name);
      printf("Supported devices:\n");
      for (i=0; i<mixer_devices; i++)
	if (mixer_isdevice(mix,i))
	  {
	    if (mixer_isstereo(mix,i))
	      printf("  %2d - %-8s (%3d:%3d) %s%s\n",i,mixer_names[i],
		     mixer_volleft(mix,i),mixer_volright(mix,i),
		     (mixer_isrecdev(mix,i)?"REC":""),
		     (mixer_isrecsrc(mix,i)?"SRC":""));
	    else
	      printf("  %2d - %-8s (  %3d  ) %s%s\n",i,mixer_names[i],
		     mixer_volmono(mix,i),
		     (mixer_isrecdev(mix,i)?"REC":""),
		     (mixer_isrecsrc(mix,i)?"SRC":""));
	  }
    }

  if (volid<mixer_devices)
    {
      switch (command)
	{
	case 0:
	  printf("Setting %s to volume %d\n",mixer_names[volid],left);
	  mixer_setvol(mix,volid,left);
	  break;
	case 1:
	  printf("Setting %s to volume %d:%d\n",mixer_names[volid],left,right);
	  mixer_setvols(mix,volid,left,right);
	  break;
	case 2:
	  printf("Setting %s as recsrc\n",mixer_names[volid]);
	  mixer_setrecsrc(mix,volid,1,0);
	  break;
	case 3:
	  printf("Clearing recsrc of %s\n",mixer_names[volid]);
	  mixer_setrecsrc(mix,volid,0,0);
	  break;
	case 4:
	  printf("Setting %s as exclusive recsrc\n",mixer_names[volid]);
	  mixer_setrecsrc(mix,volid,1,1);
	  break;
	}
	if (mixer_isdevice(mix,volid))
	  {
	    if (mixer_isstereo(mix,volid))
	      printf("  %2d - %-8s (%3d:%3d) %s%s\n",volid,mixer_names[volid],
		     mixer_volleft(mix,volid),mixer_volright(mix,volid),
		     (mixer_isrecdev(mix,volid)?"REC":""),
		     (mixer_isrecsrc(mix,volid)?"SRC":""));
	    else
	      printf("  %2d - %-8s (  %3d  ) %s%s\n",volid,mixer_names[volid],
		     mixer_volmono(mix,volid),
		     (mixer_isrecdev(mix,volid)?"REC":""),
		     (mixer_isrecsrc(mix,volid)?"SRC":""));
	  }
    }

  mixer_close(mix);
  printf("Mixer closed\n");
  return EXIT_SUCCESS;
}
