/*
 * $Id: mixer.c,v 1.1.1.1 2001/02/12 22:25:53 xtifr Exp $
 *
 * mixer utility functions for WMRack
 *
 * Copyright (c) 1997 by Oliver Graf <ograf@fga.de>
 *
 * this is very linux specific !!!
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/soundcard.h>

#include "mixer.h"

char *mixer_labels[]=SOUND_DEVICE_LABELS;
char *mixer_names[]=SOUND_DEVICE_NAMES;
char *mixer_shortnames[]={"VO", "BA", "TR", "SY", "PC", "SP", "LI", "MI",
			  "CD", "MX", "P2", "RE", "IG", "OG", "L1", "L2", "L3"};
int mixer_devices=SOUND_MIXER_NRDEVICES;

/*
 * mixer_getinfo(MIXER)
 *
 * reads the supported devices and their capabilities
 *
 * returns 0 on success
 */
int mixer_getinfo(MIXER *mix)
{
  if (!mix || mix->fd==0)
    return -1;

  if (ioctl(mix->fd,SOUND_MIXER_INFO,&mix->id)) {
    fprintf(stderr,"mixer_getinfo[info]: %s\n",strerror(errno));
    memset(&mix->id,0,48);
  }
#ifdef DEBUG
  else
    fprintf(stderr,"mixer_getinfo[info] - successful\n");
#endif
  if (ioctl(mix->fd,SOUND_MIXER_READ_DEVMASK,&mix->mask)) {
    fprintf(stderr,"mixer_getinfo[devmask]: %s\n",strerror(errno));
    mix->mask=0;
  }
#ifdef DEBUG
  else
    fprintf(stderr,"mixer_getinfo[devmask] - successful\n");
#endif
  if (ioctl(mix->fd,SOUND_MIXER_READ_STEREODEVS,&mix->stereo)) {
    fprintf(stderr,"mixer_getinfo[stereodevs]: %s\n",strerror(errno));
    mix->stereo=0;
  }
#ifdef DEBUG
  else
    fprintf(stderr,"mixer_getinfo[stereodevs] - successful\n");
#endif
  if (ioctl(mix->fd,SOUND_MIXER_READ_CAPS,&mix->caps)) {
    fprintf(stderr,"mixer_getinfo[caps]: %s\n",strerror(errno));
    mix->caps=0;
  }
#ifdef DEBUG
  else
    fprintf(stderr,"mixer_getinfo[caps] - successful\n");
#endif
  if (ioctl(mix->fd,SOUND_MIXER_READ_RECSRC,&mix->recsrc)) {
    fprintf(stderr,"mixer_getinfo[recsrc]: %s\n",strerror(errno));
    mix->recsrc=0;
  }
#ifdef DEBUG
  else
    fprintf(stderr,"mixer_getinfo[recsrc] - successful\n");
#endif
  if (ioctl(mix->fd,SOUND_MIXER_READ_RECMASK,&mix->recmask)) {
    fprintf(stderr,"mixer_getinfo[recmask]: %s\n",strerror(errno));
    mix->recmask=0;
  }
#ifdef DEBUG
  else
    fprintf(stderr,"mixer_getinfo[recmask] - successful\n");
#endif
  return 0;
}

/*
 * mixer_readvol(mixer,dev)
 *
 * reads the current volume setting of the specified device
 *
 * returns 0 if volume is equal to last read, 1 if volume has changed
 * and a negative value on failure
 */
int mixer_readvol(MIXER *mix, int dev)
{
  int i;

  if (!mix || mix->fd==0)
    return -1;

  mix->old_vol[dev]=mix->cur_vol[dev];

  if (mixer_isdevice(mix,dev))
    {
      if (ioctl(mix->fd,MIXER_READ(dev),&mix->cur_vol[dev]))
	{
	  fprintf(stderr,"mixer_readvol[%s]: %s\n",mixer_names[dev],strerror(errno));
	  mix->cur_vol[dev]=-1;
	}
      else
	if (mix->old_vol[dev]!=mix->cur_vol[dev])
	  return 1;
    }
  else
    mix->cur_vol[i]=-1;

  return 0;
}

/*
 * mixer_readvols(MIXER)
 *
 * reads the current volume settings of all supported devices
 *
 * returns 0 if volume is equal to last read, 1 if volume has changed
 * and a negative value on failure
 */
int mixer_readvols(MIXER *mix)
{
  int i, change=0;

  if (!mix || mix->fd==0)
    return -1;

  memcpy(mix->old_vol,mix->cur_vol,sizeof(int)*32);

  for (i=0; i<mixer_devices; i++)
    {
      if (mixer_isdevice(mix,i))
	{
	  if (ioctl(mix->fd,MIXER_READ(i),&mix->cur_vol[i]))
	    {
	      fprintf(stderr,"mixer_readvol[%s]: %s\n",mixer_names[i],strerror(errno));
	      mix->cur_vol[i]=-1;
	      /* mix->mask&=~(1<<i); */
	    }
	  else
	    change|=mix->old_vol[i]!=mix->cur_vol[i];
	}
      else
	mix->cur_vol[i]=-1;
    }

  return change;
}

/*
 * mixer_open(char)
 *
 * opens the specified device as mixer and call getinfo and readvols
 *
 * returns a pointer to a MIXER struct on success or NULL on failure
 */
MIXER *mixer_open(char *device)
{
  MIXER *mix=NULL;
  mix=malloc(sizeof(MIXER));
  if (mix==NULL)
    {
      perror("mixer_open[malloc]");
      return NULL;
    }
  mix->fd=open(device,0);
  if (mix->fd<0) {
    fprintf(stderr,"mixer_open: %s\n",strerror(errno));
    free(mix);
    return NULL;
  }
  mix->device=strdup(device);
  mixer_getinfo(mix);
  mixer_readvols(mix);
  return mix;
}

/*
 * mixer_close(MIXER)
 *
 * closes the mixer and frees all data
 */
void mixer_close(MIXER *mix)
{
  if (mix) {
    close(mix->fd);
    if (mix->device)
      free(mix->device);
    free(mix);
  }
}

/*
 * mixer_setvol(mix,dev,vol)
 *
 * sets the specified device to a mono volume
 *
 * returns 0 on success
 */
int mixer_setvol(MIXER *mix, int dev, int vol)
{
  if (!mix || mix->fd==0)
    return -1;

  if (!mixer_isdevice(mix,dev))
    {
#ifdef DEBUG
      fprintf(stderr,"mixer_setvol: unsupported device\n");
#endif
      return 1;
    }

  if (vol<0) vol=0;
  if (vol>100) vol=100;
  
  if (mixer_isstereo(mix,dev))
    mixer_setvols(mix,dev,vol,vol);
  else
    {
      vol=mixer_makevol(vol,vol);
      if (ioctl(mix->fd,MIXER_WRITE(dev),&vol))
	perror("mixer_setvol[write]");
#ifdef DEBUG
      else
	fprintf(stderr,"mixer_setvol[set]: vol set to %02d\n",vol>>8);
#endif
    }

  return 0;
}

/*
 * mixer_setvols(mix,dev,left,right)
 *
 * sets the specified device to a stereo volume
 *
 * returns 0 on success
 */
int mixer_setvols(MIXER *mix, int dev, int left, int right)
{
  int vol;
  
  if (!mix || mix->fd==0)
    return -1;

  if (!mixer_isdevice(mix,dev))
    {
#ifdef DEBUG
      fprintf(stderr,"mixer_setvols: unsupported device\n");
#endif
      return 1;
    }

  if (left<0) left=0;
  if (left>100) left=100;
  if (right<0) right=0;
  if (right>100) right=100;
  vol=mixer_makevol(left,right);

  if (mixer_ismono(mix,dev))
    mixer_setvol(mix,dev,(left+right)/2);
  else
    {
      if (ioctl(mix->fd,MIXER_WRITE(dev),&vol))
	perror("mixer_setvols[write]");
#ifdef DEBUG
      else
	fprintf(stderr,"mixer_setvols[set]: vol set to %02d:%02d\n",
		vol&0xff,(vol>>8)&0xff);
#endif
    }

  return 0;
}

/*
 * mixer_setrecsrc(mix,dev,state)
 *
 * sets or clears the RECSRC state of the specified device
 *
 * returns 0 on success
 */
int mixer_setrecsrc(MIXER *mix, int dev, int state, int exclusive)
{
  int new;

  if (!mix || mix->fd==0)
    return -1;

  if (mixer_isdevice(mix,dev) && mixer_isrecdev(mix,dev))
    {
      mix->old_recsrc=mix->recsrc;
      if (state)
	{
	  if (mix->caps&SOUND_CAP_EXCL_INPUT || exclusive)
	    new=(1<<dev);
	  else
	    new=mix->recsrc|(1<<dev);
	}
      else
	new=mix->recsrc&(~(1<<dev));
#ifdef DEBUG
      if (new==0)
	fprintf(stderr,"mixer_setrecsrc[write]: warning, no recsrc may not be possible");
#endif
      if (ioctl(mix->fd,SOUND_MIXER_WRITE_RECSRC,&new))
	perror("mixer_serrecsrc[write]");
      else
	{
	  if (ioctl(mix->fd,SOUND_MIXER_READ_RECSRC,&mix->recsrc))
	    {
	      perror("mixer_setrecsrc[read]");
	      mix->recsrc=0;
	    }
#ifdef DEBUG
	  else
	    fprintf(stderr,"mixer_setrecsrc[read]: %s is %s\n",
		    mixer_names[dev],
		    mixer_isrecsrc(mix,dev)?"RECSRC":"not RECSRC");
#endif
	}
    }
  else
    {
#ifdef DEBUG
      fprintf(stderr,"mixer_setrecsrc: unsupported device\n");
#endif
      return 1;
    }
  return 0;  
}

int mixer_changevol(MIXER *mix, int dev, int delta)
{
  int left, right;

  if (!mix || mix->fd==0)
    return -1;

  if (!mixer_isdevice(mix,dev))
    {
#ifdef DEBUG
      fprintf(stderr,"mixer_changevol: unsupported device\n");
#endif
      return 1;
    }

  if (mixer_isstereo(mix,dev))
    {
      left=mixer_volleft(mix,dev)+delta;
      if (left<0) left=0;
      if (left>100) left=100;
      right=mixer_volright(mix,dev)+delta;
      if (right<0) right=0;
      if (right>100) right=100;
      left=mixer_makevol(left,right);
    }
  else
    {
      left=mixer_volmono(mix,dev)+delta;
      if (left<0) left=0;
      if (left>100) left=100;
      left=mixer_makevol(left,left);
    }
  if (ioctl(mix->fd,MIXER_WRITE(dev),&left))
    perror("mixer_changevol[write]");
#ifdef DEBUG
  else
    fprintf(stderr,"mixer_changevol[set]: vol set to %02d:%02d\n",left&0xff,left>>8);
#endif

  return 0;
}

int mixer_changeleft(MIXER *mix, int dev, int delta)
{
  int vol;

  if (!mix || mix->fd==0)
    return -1;

  if (!mixer_isdevice(mix,dev))
    {
#ifdef DEBUG
      fprintf(stderr,"mixer_changeleft: unsupported device\n");
#endif
      return 1;
    }

  if (mixer_ismono(mix,dev))
    mixer_changevol(mix,dev,delta);
  else
    {
      vol=mixer_volleft(mix,dev)+delta;
      if (vol<0) vol=0;
      if (vol>100) vol=100;
      vol=mixer_makevol(vol,mixer_volright(mix,dev));
      if (ioctl(mix->fd,MIXER_WRITE(dev),&vol))
	perror("mixer_changeleft[write]");
#ifdef DEBUG
      else
	fprintf(stderr,"mixer_changeleft[set]: vol set to %02d:%02d\n",
		vol&0xff,(vol>>8)&0xff);
#endif
    }

  return 0;
}

int mixer_changeright(MIXER *mix, int dev, int delta)
{
  int vol;

  if (!mix || mix->fd==0)
    return -1;

  if (!mixer_isdevice(mix,dev))
    {
#ifdef DEBUG
      fprintf(stderr,"mixer_changeright: unsupported device\n");
#endif
      return 1;
    }

  if (mixer_ismono(mix,dev))
    mixer_changevol(mix,dev,delta);
  else
    {
      vol=mixer_volright(mix,dev)+delta;
      if (vol<0) vol=0;
      if (vol>100) vol=100;
      vol=mixer_makevol(mixer_volleft(mix,dev),vol);
      if (ioctl(mix->fd,MIXER_WRITE(dev),&vol))
	perror("mixer_changeright[write]");
#ifdef DEBUG
      else
	fprintf(stderr,"mixer_changeright[set]: vol set to %02d:%02d\n",
		vol&0xff,(vol>>8)&0xff);
#endif
    }

  return 0;
}

int mixer_changebal(MIXER *mix, int dev, int delta)
{
  int left, right;

  if (!mix || mix->fd==0)
    return -1;

  if (!mixer_isdevice(mix,dev))
    {
#ifdef DEBUG
      fprintf(stderr,"mixer_changebal: unsupported device\n");
#endif
      return 1;
    }

  if (mixer_ismono(mix,dev))
    mixer_changevol(mix,dev,delta);
  else
    {
      left=mixer_volleft(mix,dev)-delta;
      right=mixer_volright(mix,dev)+delta;
      if (left<0)
	{
	  right+=left;
	  left=0;
	}
      if (right<0)
	{
	  left+=right;
	  right=0;
	}
      if (left>100)
	{
	  right+=(left-100);
	  left=100;
	}
      if (right>100)
	{
	  left+=(right-100);
	  right=100;
	}
      left=mixer_makevol(left,right);
      if (ioctl(mix->fd,MIXER_WRITE(dev),&left))
	perror("mixer_changebal[write]");
#ifdef DEBUG
      else
	fprintf(stderr,"mixer_changebal[set]: vol set to %02d:%02d\n",
		left&0xff,(left>>8)&0xff);
#endif
    }

  return 0;
}
