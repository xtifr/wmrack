/*
 * $Id: cdrom.c,v 1.4 2007/11/25 23:54:46 xtifr Exp $
 *
 * cdrom utility functions for WMRack
 *
 * Copyright (c) 1997 by Oliver Graf <ograf@fga.de>
 *
 * some hints taken from WorkBone
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>

#ifdef linux
#  include <sys/vfs.h>
#  include <linux/cdrom.h>
#elif defined(__sun) && defined(__SVR4) /* Solaris */
#  include <sys/cdio.h>
#  include <sys/statvfs.h>
#  define statfs statvfs
#elif defined(__FreeBSD__)
#  include <sys/mount.h>
#  include <sys/cdio.h>

#  define cdrom_tocentry ioc_read_toc_single_entry
#  define cdte_track track
#  define cdte_format address_format
#  define cdte_ctrl entry.control
#  define cdte_addr entry.addr
#  define CDROMREADTOCENTRY CDIOREADTOCENTRY
#  define CDROM_LEADOUT 0xAA
#  define CDROM_MSF CD_MSF_FORMAT
#  define CDROM_DATA_TRACK 4

#  define cdrom_tochdr ioc_toc_header
#  define cdth_trk0 starting_track
#  define cdth_trk1 ending_track
#  define CDROMREADTOCHDR CDIOREADTOCHEADER

#  define cdrom_msf ioc_play_msf
#  define cdmsf_min0 start_m
#  define cdmsf_sec0 start_s
#  define cdmsf_frame0 start_f
#  define cdmsf_min1 end_m
#  define cdmsf_sec1 end_s
#  define cdmsf_frame1 end_f

#  define cdrom_subchnl ioc_read_subchannel
#  define cdsc_format address_format
#  define cdsc_audiostatus data->header.audio_status
#  define cdsc_trk data->what.position.track_number
#  define cdsc_absaddr data->what.position.absaddr
#  define cdsc_reladdr data->what.position.reladdr
#  define CDROMSUBCHNL CDIOCREADSUBCHANNEL
#  define CDROM_AUDIO_INVALID CD_AS_AUDIO_INVALID
#  define CDROM_AUDIO_PLAY CD_AS_PLAY_IN_PROGRESS
#  define CDROM_AUDIO_PAUSED CD_AS_PLAY_PAUSED
#  define CDROM_AUDIO_COMPLETED CD_AS_PLAY_COMPLETED
#  define CDROM_AUDIO_NO_STATUS CD_AS_NO_STATUS

#  define CDROMPLAYMSF CDIOCPLAYMSF
#  define CDROMPAUSE CDIOCPAUSE
#  define CDROMRESUME CDIOCRESUME
#  define CDROMSTART CDIOCSTART
#  define CDROMSTOP CDIOCSTOP
#  define CDROMEJECT CDIOCEJECT

#else
#  include <sundev/srreg.h>
#endif /* linux */

#include "cdrom.h"

/* workaround -- used ints in MSF, linux uses u_char, what use other systems? */
#define msftoMSF(d,s) d.minute=s.minute; d.second=s.second; d.frame=s.frame;
#define MSFtocdmsf(d,s,e) d.cdmsf_min0=s.minute; d.cdmsf_sec0=s.second; d.cdmsf_frame0=s.frame; d.cdmsf_min1=e.minute; d.cdmsf_sec1=e.second; d.cdmsf_frame1=e.frame;

/*
 * cddb_sum(num)
 *
 * utility function to cddb_discid
 */
int cddb_sum(int n)
{
  char buf[12], *p;
  int  ret = 0;

  /* For backward compatibility this algorithm must not change */
  sprintf(buf,"%u",n);
  for (p=buf; *p!='\0'; p++)
    ret+=(*p-'0');
  return (ret);
}

/*
 * cddb_discid(cdinfo)
 *
 * calculates the id for a given cdinfo structure
 */
unsigned long cddb_discid(CDInfo *cdinfo)
{
  int i, t=0, n=0;

  /* For backward compatibility this algorithm must not change */
  for (i=0; i<cdinfo->tracks; i++) {
    n+=cddb_sum((cdinfo->track[i].start.minute*60) + cdinfo->track[i].start.second);
    t+=((cdinfo->track[i+1].start.minute*60) + cdinfo->track[i+1].start.second) -
      ((cdinfo->track[i].start.minute*60) + cdinfo->track[i].start.second);
  }
  cdinfo->discid=((n % 0xff) << 24 | t << 8 | cdinfo->tracks);
  return cdinfo->discid;
}

/*
 * cd_readTOC(CD)
 *
 * Read the table of contents from the CD.
 * returns 0 on success
 */
int cd_readTOC(CD *cd)
{
  struct cdrom_tochdr   hdr;
  struct cdrom_tocentry entry;
  int                   i, j;
  MSF                   tmp;

  if (cd->fd<0)
    return 1;

  if (ioctl(cd->fd, CDROMREADTOCHDR, &hdr))
    {
      perror("cd_readTOC[readtochdr]");
      return 1;
    }

  cd->info.start=hdr.cdth_trk0;
  cd->info.end=hdr.cdth_trk1;
  cd->info.tracks=hdr.cdth_trk1-hdr.cdth_trk0+1;
#ifdef DEBUG
  fprintf(stderr,"cd_readTOC[DEBUG]: read header with %d tracks\n",
	  cd->info.tracks);
#endif

  cd->info.track=malloc(cd->info.tracks*sizeof(TrackInfo));
  if (cd->info.track==NULL)
    {
      perror("cd_readTOC[malloc]");
      return 1;
    }
  for (i=0; i<=cd->info.tracks; i++)
    {
      if (i==cd->info.tracks)
	entry.cdte_track=CDROM_LEADOUT;
      else
	entry.cdte_track=cd->info.start+i;
      entry.cdte_format=CDROM_MSF;
      if (ioctl(cd->fd, CDROMREADTOCENTRY, &entry))
	{
	  perror("cd_readTOC[tocentry read]");
	  free(cd->info.track);
	  return 1;
	}

      if (entry.cdte_track!=CDROM_LEADOUT)
	{
	  cd->info.track[i].num=i+1;
	  msftoMSF(cd->info.track[i].start,entry.cdte_addr.msf);
	  cd->info.track[i].end.minute=-1;
	  cd->info.track[i].end.second=-1;
	  cd->info.track[i].end.frame=-1;
	  cd->info.track[i].data=entry.cdte_ctrl&CDROM_DATA_TRACK?1:0;
	  if (i>0)
	    {
	      cd->info.track[i-1].end=cd->info.track[i].start;
	      cd->info.track[i-1].length=subMSF(cd->info.track[i-1].end,
						cd->info.track[i-1].start);
	    }
	}
      else
	{
	  msftoMSF(cd->info.track[i-1].end,entry.cdte_addr.msf);
	  cd->info.track[i-1].length=subMSF(cd->info.track[i-1].end,
					    cd->info.track[i-1].start);
	}
    }

  cddb_discid(&cd->info);

  return 0;
}

/*
 * cd_playMSF(cd,start,end)
 *
 * sends the actual play to the cdrom.
 */
int cd_playMSF(CD *cd, MSF start, MSF end)
{
  struct cdrom_msf msf;

  if (cd==NULL || cd->fd<0)
    return 1;

  MSFtocdmsf(msf,start,end);

  if (cd->info.current.mode==CDM_STOP)
    if (ioctl(cd->fd, CDROMSTART))
      {
	perror("cd_playMSF[CDROMSTART]");
	return 1;
      }

  if (ioctl(cd->fd, CDROMPLAYMSF, &msf))
    {
      printf("cd_playMSF[playmsf]\n");
      printf("  msf = %02d:%02d:%02d %02d:%02d:%02d\n",
	     msf.cdmsf_min0, msf.cdmsf_sec0, msf.cdmsf_frame0,
	     msf.cdmsf_min1, msf.cdmsf_sec1, msf.cdmsf_frame1);
      perror("cd_playMSF[CDROMPLAYMSF]");
      return 1;
    }
  return 0;
}

/*
 * cd_freeinfo(cd)
 *
 * frees a allocate CDInfo
 */
void cd_freeinfo(CD *cd)
{
  if (cd->info.track)
    free(cd->info.track);
  if (cd->info.list.track)
    free(cd->info.list.track);
  memset(&cd->info,0x0,sizeof(CDInfo));
  memset(&cd->info.current,0xff,sizeof(CDPosition));
  /* paranoia */
  cd->info.track=NULL;
  cd->info.list.track=NULL;
}

/*
 * cd_close(cd)
 *
 * closes the cd device and frees all data structures inside
 */
void cd_close(CD *cd)
{
  if (cd==NULL)
    return;

  cd_freeinfo(cd);
  free(cd->device);
  close(cd->fd);
  free(cd);
}

/*
 * cd_open(device)
 *
 * sets the device for further cd-access
 * set force to TRUE for immidiate change
 */
CD *cd_open(char *device, int noopen)
{
  struct stat st;
  CD *cd;

  if (stat(device,&st))
    {
      perror("cd_open[stat]");
      return NULL;
    }

  cd=(CD *)malloc(sizeof(CD));
  if (cd==NULL)
    {
      perror("cd_open[malloc]");
      return NULL;
    }

  cd->device=strdup(device);
  cd->fd=-1;
  cd->status=-1;
  memset(&cd->info,0x0,sizeof(CDInfo));
  memset(&cd->info.current,0xff,sizeof(CDPosition));

  if (!noopen)
    cd_reopen(cd);

  return cd;
}

/*
 * cd_suspend(cd)
 *
 * closes the cd device, but leaves all structures intact
 */
int cd_suspend(CD *cd)
{
  if (cd==NULL)
    return -1;

  close(cd->fd);
  cd->fd=-1;
  cd->status=-1;
  cd_freeinfo(cd);

  return 0;
}

/*
 * cd_reopen(cd)
 *
 * reopens a suspended cd device, and reads the new TOC if cd is changed
 */
int cd_reopen(CD *cd)
{
  int start, end, track;
  
  if (cd==NULL)
    return 1;

  if ((cd->fd=open(cd->device,O_RDONLY|O_NONBLOCK))<0)
    {
      perror("cd_reopen[open]");
      return 1;
    }

  cd_freeinfo(cd);

  if (!cd_readTOC(cd))
    {
      cd_resetpl(cd);
      cd->status=cd_getStatus(cd,0,1);
      switch (cd->info.current.mode)
	{
	case CDM_PLAY:
	  cd->info.play.last_action=CDA_PLAY;
	  break;
	case CDM_PAUSE:
	  cd->info.play.last_action=CDA_PAUSE;
	  break;
	case CDM_STOP:
	  cd->info.play.last_action=CDA_STOP;
	  break;
	case CDM_COMP:
	case CDM_EJECT:
	  cd->info.play.last_action=CDA_NONE;
	  break;
	}
      start=cd->info.play.cur_track=cd_findtrack(cd,cd->info.current.track);

      /* do play list optimization:
       * find the longest possible track sequence without jumps
       */
      for (track=cd->info.list.track[start].num, end=start+1;
	   end<cd->info.list.tracks && track==cd->info.list.track[end].num-1;
	   track=cd->info.list.track[end].num, end++);
      end--;

      cd->info.play.cur_end=end;
      cd->info.play.repeat_mode=CDR_NONE;
      cd->info.play.play_type=CDP_NORMAL;
    }
  else
    {
      cd_freeinfo(cd);
      cd->status=-1;
      close(cd->fd);
      cd->fd=-1;
    }

  return 0;
}

static struct timeval last_upd={0,0};

/*
 * cd_getStatus(cd,reopen,force)
 *
 * Return values:
 *  -1 error
 *   0 No CD in drive.
 *   1 CD in drive.
 *   2 CD has just been inserted (TOC has been read)
 *
 * Updates the CDPosition struct of the cd.
 * If reopen is not 0, the device is automatically reopened if needed.
 * This will query the cdrom only one time within 1/2 second to reduce
 * overhead. Force will overide this behaviour.
 */
int cd_getStatus(CD *cd, int reopen, int force)
{
  struct cdrom_subchnl sc;
  int                  ret=1, newcd=0, im_stop=0;
  CDPosition           *cur;
  struct timeval       now;
#ifdef __FreeBSD__
  struct cd_sub_channel_info data;
  sc.data = &data;
  sc.data_len = sizeof(data);
  sc.data_format = CD_CURRENT_POSITION;
#endif

  if (cd==NULL)
    return -1;

  if (cd->fd<0)
    {
      if (!reopen)
	{
	  cd->status=0;
	  return 0;
	}
      if (cd_reopen(cd))
	return -1;
      newcd=1;
    }

  gettimeofday(&now,NULL);
  if (force
      || ((now.tv_sec-last_upd.tv_sec)*1000000L+(now.tv_usec-last_upd.tv_usec))>500000L)
    {
      cur=&cd->info.current;
      
      sc.cdsc_format=CDROM_MSF;
      
      if (ioctl(cd->fd, CDROMSUBCHNL, &sc))
	{
	  memset(cur,0xff,sizeof(CDPosition));
	  cur->mode=CDM_EJECT;
	  cd->status=0;
	  return 0;
	}

      /* only set update time if ioctl was successful */
      last_upd=now;

      if (newcd)
	{
	  memset(cur,0xff,sizeof(CDPosition));
	  cur->mode=CDM_STOP;
	  ret=2;
	}
      
      switch (sc.cdsc_audiostatus)
	{
	case CDROM_AUDIO_PLAY:
	  cur->mode=CDM_PLAY;
	  
	setpos:
	  cur->track=cd_findtrack(cd,sc.cdsc_trk);
	  
	  msftoMSF(cur->relmsf,sc.cdsc_reladdr.msf);
	  msftoMSF(cur->absmsf,sc.cdsc_absaddr.msf);
	  
	  break;
	  
	case CDROM_AUDIO_PAUSED:
	  if (cd->info.play.last_action!=CDA_STOP)
	    {
	      cur->mode=CDM_PAUSE;
	      goto setpos;
	    }
	  memset(cur,0xff,sizeof(CDPosition));
	  cur->mode=CDM_STOP;
	  break;
	  
	case CDROM_AUDIO_COMPLETED:
	  cur->mode=CDM_COMP;
	  break;
	  
	case CDROM_AUDIO_NO_STATUS:
	case CDROM_AUDIO_INVALID: /* my TOSHIBA CD-ROM XM-5602B wants this */
	  memset(cur,0xff,sizeof(CDPosition));
	  cur->mode=CDM_STOP;
	  break;
	}
      cd->status=ret;
      
      /*
	if (ret>0 && cur->track>cd->info.tracks && cur->mode!=CDM_EJECT)
	{
	cd_doStop(cd);
	im_stop=1;
	}
      */
      
      switch (cd->info.play.repeat_mode)
	{
	case CDR_NONE:
	  if ((cd->info.play.last_action==CDA_PLAY && cur->mode!=CDM_PLAY)
	      /* this means: the user wants the cdrom to play,
	       * but the cdrom does not play */
	      || cur->mode==CDM_COMP || im_stop)
	    {
#ifdef DEBUG
	      fprintf(stderr,"cd_getStatus[DEBUG]: switching to next track\n");
#endif
	      cd->info.play.cur_track=cd->info.play.cur_end+1;
	      if (cd->info.play.cur_track<cd->info.list.tracks)
		cd_doPlay(cd,cd->info.play.cur_track);
	      else
		cd_doStop(cd);
	    }
	  break;
	case CDR_ALL:
	  if ((cd->info.play.last_action==CDA_PLAY && cur->mode!=CDM_PLAY)
	      || cur->mode==CDM_COMP || im_stop)
	    {
	      cd->info.play.cur_track++;
	      if (cd->info.play.cur_track>=cd->info.list.tracks)
		{
#ifdef DEBUG
		  fprintf(stderr,"cd_getStatus[DEBUG]: repeating list\n");
#endif
		  if (cd->info.play.play_type==CDP_RANDOM)
		    cd_randomize(cd);
		  cd_doPlay(cd,0);
		}
	      else
		{
#ifdef DEBUG
		  fprintf(stderr,"cd_getStatus[DEBUG]: switching to next track\n");
#endif
		  cd_doPlay(cd,cd->info.play.cur_track);
		}
	    }
	  break;
	case CDR_ONE:
	  if ((cd->info.play.last_action==CDA_PLAY && cur->mode!=CDM_PLAY)
	      || cur->mode==CDM_COMP || im_stop)
	    {
#ifdef DEBUG
	      fprintf(stderr,"cd_getStatus[DEBUG]: repeating track\n");
#endif
	      cd_doPlay(cd,cd->info.play.cur_track);
	    }
	  break;
	}
    }

  return ret;
}

/*
 * cd_doPlay(cd,start);
 *
 * start playing the cd. cd must be opened. play starts at track start goes
 * until end of track end.
 * 0 is the first track of the play list.
 */
int cd_doPlay(CD *cd, int start)
{
  int end, track;

  if (cd==NULL || cd->fd<0)
    return 1;
  if (start<0)
    start=0;
  if (start>=cd->info.list.tracks)
    start=cd->info.list.tracks-1;

  /* do play list optimization:
   * find the longest possible track sequence without jumps
   */
  for (track=cd->info.list.track[start].num, end=start+1;
       end<cd->info.list.tracks && track==cd->info.list.track[end].num-1;
       track=cd->info.list.track[end].num, end++);
  end--;

#ifdef DEBUG
  fprintf(stderr,"cd_doPlay[DEBUG]: play from [%d]%02d:%02d:%02d to end of [%d]%02d:%02d:%02d\n",
	  start,
	  cd->info.list.track[start].start.minute,
	  cd->info.list.track[start].start.second,
	  cd->info.list.track[start].start.frame,
	  end,
	  cd->info.list.track[end].end.minute,
	  cd->info.list.track[end].end.second,
	  cd->info.list.track[end].end.frame);
#endif
  cd->info.play.last_action=CDA_PLAY;
  cd->info.play.cur_track=start;
  cd->info.play.cur_end=end;
  return cd_playMSF(cd,
		    cd->info.list.track[start].start,
		    cd->info.list.track[end].end);
}

/*
 * cd_doPause(cd)
 *
 * Pause the CD, if it's in play mode.
 * Resume the CD, If it's already paused.
 */
int cd_doPause(CD *cd)
{
  if (cd==NULL || cd->fd<0)
    return 1;

  switch (cd->info.current.mode) {
  case CDM_PLAY:
    cd->info.current.mode=CDM_PAUSE;
#ifdef DEBUG
    fprintf(stderr,"cd_doPause[DEBUG]: pausing cdrom\n");
#endif
    ioctl(cd->fd, CDROMPAUSE);
    cd->info.play.last_action=CDA_PAUSE;
    break;
  case CDM_PAUSE:
    cd->info.current.mode=CDM_PLAY;
#ifdef DEBUG
    fprintf(stderr,"cd_doPause[DEBUG]: resuming cdrom\n");
#endif
    ioctl(cd->fd, CDROMRESUME);
    cd->info.play.last_action=CDA_PLAY;
    break;
#ifdef DEBUG
  default:
    fprintf(stderr,"cd_doPause[DEBUG]: not playing or pausing\n");
#endif
  }
  return 0;
}

/*
 * cd_doStop(cd)
 *
 * Stop the CD if it's not already stopped.
 */
int cd_doStop(CD *cd)
{
  if (cd==NULL || cd->fd<0)
    return 1;

  if (cd->info.current.mode!=CDM_STOP)
    {
      /* olis cdrom needs this */
      if (cd->info.current.mode==CDM_PLAY)
	cd_doPause(cd);
#ifdef DEBUG
      fprintf(stderr,"cd_doStop[DEBUG]: stopping cdrom\n");
#endif
      if (ioctl(cd->fd, CDROMSTOP))
	{
	  perror("cd_doStop[CDROMSTOP]");
	  return 1;
	}
      cd->info.play.last_action=CDA_STOP;
      if (cd->info.current.track>=cd->info.list.tracks)
	cd->info.play.cur_track=0;
      else
	cd->info.play.cur_track=cd->info.current.track;
      memset(&cd->info.current,0xff,sizeof(CDPosition));
      cd->info.current.mode=CDM_STOP;
    }
  return 0;
}

/*
 * cd_doEject(CD *cd)
 *
 * Eject the current CD, if there is one.
 * Returns 0 on success, 1 if the CD couldn't be ejected, or 2 if the
 * CD contains a mounted filesystem.
 */
int cd_doEject(CD *cd)
{
  struct statfs ust;

  if (cd==NULL)
    return 0;

  if (cd->fd<0) /* reopen a closed device */ 
    {
      if ((cd->fd=open(cd->device,0))<0)
      {
	perror("cd_doEject[(re)open]");
	return 1;
      }
    }

  if (statfs(cd->device, &ust))
    {
      perror("cd_doEject[is_mounted]");
      cd_suspend(cd);
      return 2;
    }

#ifdef DEBUG
  fprintf(stderr,"cd_doEject[DEBUG]: stopping cdrom\n");
#endif
  ioctl(cd->fd, CDROMSTOP);
  cd->info.play.last_action=CDA_STOP;
#ifdef DEBUG
  fprintf(stderr,"cd_doEject[DEBUG]: ejecting cdrom\n");
#endif
  if (ioctl(cd->fd, CDROMEJECT))
    {
      perror("cd_doEject[CDROMEJECT]");
      cd_suspend(cd);
      return 1;
    }

  cd_suspend(cd);

  return 0;
}

/*
 * cd_doSkip(cd,seconds)
 *
 * Skip some seconds from current position
 */
int cd_doSkip(CD *cd, int secs)
{
  MSF start, end;

  if (cd==NULL || cd->fd<0)
    return 1;

  end=cd->info.list.track[cd->info.play.cur_end].end;
  start=cd->info.current.absmsf;
  start.second+=secs;
  start=normMSF(start);
#ifdef DEBUG
  fprintf(stderr,"cd_doSkip[DEBUG]: skipping %d seconds\n",secs);
#endif
  if (cmpMSF(start,cd->info.list.track[0].start)<0)
    {
      start=cd->info.list.track[0].start;
#ifdef DEBUG
      fprintf(stderr,"cd_doSkip[DEBUG]: can't skip before first track\n");
#endif
    }

  if (cmpMSF(start,end)>0)
    {
#ifdef DEBUG
      fprintf(stderr,"cd_doSkip[DEBUG]: at end of track ... waiting\n");
      return 0;
#endif
    }

#ifdef DEBUG
  fprintf(stderr,"cd_doSkip[DEBUG]: play from %02d:%02d:%02d to %02d:%02d:%02d\n",
	  start.minute, start.second, start.frame,
	  end.minute, end.second, end.frame);
#endif
  return cd_playMSF(cd,start,end);
}

/*
 * cmpMSF(a,b)
 *
 * compares two MSF structs
 */
int cmpMSF(MSF a, MSF b)
{
  int fa=msfFrames(a), fb=msfFrames(b);
  if (fa<fb)
    return -1;
  else if (fa>fb)
    return 1;
  return 0;
}

/*
 * subMSF(a,b)
 *
 * subtract b from a
 */
MSF subMSF(MSF a, MSF b)
{
  MSF c;
  c.minute=a.minute-b.minute;
  c.second=a.second-b.second;
  c.frame =a.frame -b.frame;
  return normMSF(c);
}

/*
 * addMSF(a,b)
 *
 * add a to b
 */
MSF addMSF(MSF a, MSF b)
{
  MSF c;
  c.minute=a.minute+b.minute;
  c.second=a.second+b.second;
  c.frame =a.frame +b.frame;
  return normMSF(c);
}

/*
 * normMSF(msf)
 *
 * normalize msf to limits
 */
MSF normMSF(MSF msf)
{
  while (msf.frame<0) {msf.frame+=FRAMES; msf.second--;}
  while (msf.second<0) {msf.second+=SECONDS; msf.minute--;}
  if (msf.minute<0) msf.minute=0;
  while (msf.frame>FRAMES) {msf.frame-=FRAMES; msf.second++;}
  while (msf.second>SECONDS) {msf.second-=SECONDS; msf.minute++;}
  return msf;
}

/*
 * cd_resetpl(cd,all)
 *
 * reset the play list to all non-data tracks (or all tracks if all is true)
 */
int cd_resetpl(CD *cd)
{
  int i;

  if (cd==NULL)
    return 1;

  if (cd->info.list.track)
    free(cd->info.list.track);

  cd->info.list.tracks=0;
  MSFzero(cd->info.list.length);
  
  if (cd->info.tracks==0)
    return 0;

  cd->info.list.track=(TrackInfo *)malloc(cd->info.tracks*sizeof(TrackInfo));
  if (cd->info.list.track==NULL)
    {
      perror("cd_resetpl[malloc]");
      return 1;
    }

  cd->info.list.allocated=cd->info.tracks;
  for (i=0; i<cd->info.tracks; i++)
    if (!cd->info.track[i].data)
      {
	cd->info.list.track[cd->info.list.tracks++]=cd->info.track[i];
	cd->info.list.length=addMSF(cd->info.list.length,
				    cd->info.list.track[cd->info.list.tracks-1].length);
      }

  cd->info.play.play_type=CDP_NORMAL;

#ifdef DEBUG
  fprintf(stderr,"cd_resetpl[DEBUG]: cd playlist reseted\n");
#endif

  return 0;
}

/*
 * cd_findtrack(cd,number)
 *
 * find the track with number in the play list and return it's index
 * returns -1 if not found
 */
int cd_findtrack(CD *cd, int num)
{
  int i;
  
  if (cd==NULL || cd->info.track==NULL)
    return -1;

  for (i=0; i<cd->info.list.tracks; i++)
    if (cd->info.list.track[i].num==num)
      return i;

  return -1;
}

/*
 * cd_randomize(cd)
 *
 * create a random, non-repeating play list from all non-data tracks
 */
int cd_randomize(CD *cd)
{
  int i, *wech, r, j;

  if (cd==NULL)
    return 1;

  if (cd->info.list.track)
    free(cd->info.list.track);

  cd->info.list.tracks=0;
  MSFzero(cd->info.list.length);
  
  if (cd->info.tracks==0)
    return 0;

  cd->info.list.track=(TrackInfo *)malloc(cd->info.tracks*sizeof(TrackInfo));
  if (cd->info.list.track==NULL)
    {
      perror("cd_randomize[malloc]");
      return 1;
    }

  wech=(int *)malloc(sizeof(int)*cd->info.tracks);
  if (wech==NULL)
    {
      perror("cd_randomize[tmp-malloc]");
      free(wech);
      return 1;
    }

  r=cd->info.tracks;
  for (i=0; i<cd->info.tracks; i++)
    {
      if (!cd->info.track[i].data)
	wech[i]=0;
      else
	{
	  wech[i]=1;
	  r--;
	}
    }
  

  while (r)
    {
      i=random()%r;
      for (j=0; j<cd->info.tracks; j++)
	{
	  if (i)
	    i--;
	  else if (!i && !wech[j])
	    {
	      cd->info.list.track[cd->info.list.tracks++]=cd->info.track[j];
	      cd->info.list.length=addMSF(cd->info.list.length,
					  cd->info.list.track[cd->info.list.tracks-1].length);
	      wech[j]++;
	      r--;
	      break;
	    }
	}
    }

  cd->info.play.play_type=CDP_RANDOM;

#ifdef DEBUG
  fprintf(stderr,"cd_randomize[DEBUG]: cd playlist randomized\n");
#endif

  return 0;
}

/*
 * cd_setpl(cd,list)
 *
 * set the playlist of cd to list
 */
int cd_setpl(CD *cd, CDPlayList *list)
{
  if (cd==NULL || list==NULL)
    return 1;

  if (cd->info.list.track!=NULL)
    free(cd->info.list.track);

  cd->info.list.track=(TrackInfo *)malloc(list->allocated*sizeof(TrackInfo));
  if (cd->info.list.track==NULL)
    {
      perror("cd_setpl[malloc]");
      return 2;
    }

  cd->info.list.length=list->length;
  cd->info.list.allocated=list->allocated;
  cd->info.list.tracks=list->tracks;
  memcpy(cd->info.list.track,list->track,list->allocated*sizeof(TrackInfo));

#ifdef DEBUG
  fprintf(stderr,"cd_setpl[DEBUG]: cd playlist set to playlist 0x%08x\n",list);
#endif

  return 0;
}

/*
 * cdpl_new()
 *
 * create a new empty playlist structure
 */
CDPlayList *cdpl_new()
{
  CDPlayList *new;

  new=(CDPlayList *)malloc(sizeof(CDPlayList));
  if (new==NULL)
    {
      perror("cdpl_new[malloc]");
      return NULL;
    }
  memset(new,0,sizeof(CDPlayList));

#ifdef DEBUG
  fprintf(stderr,"cdpl_new[DEBUG]: new playlist created (0x%08x)\n",new);
#endif

  return new;
}

/*
 * cdpl_free(list)
 *
 * free the memory occupied by a playlist
 */
int cdpl_free(CDPlayList *list)
{
  if (list)
    {
#ifdef DEBUG
      fprintf(stderr,"cdpl_free[DEBUG]: free playlist 0x%08x\n",list);
#endif
      if (list->track)
	free(list->track);
      free(list);
    }
  return 0;
}

/*
 * cdpl_add(cd,track)
 *
 * add track to list
 */
int cdpl_add(CDPlayList *list, CD *cd, int track)
{
  if (list==NULL || track>=cd->info.tracks || track<0)
    return 1;

  if (list->track==NULL)
    {
      list->track=(TrackInfo *)malloc(10*sizeof(TrackInfo));
      list->allocated=10;
      list->tracks=0;
    }
  else if (list->allocated==list->tracks)
    {
      TrackInfo *new;
      list->allocated+=10;
      new=realloc(list->track,list->allocated*sizeof(TrackInfo));
      if (new==NULL)
	{
	  perror("cdpl_add[realloc]");
	  return 2;
	}
      list->track=new;
    }

  list->track[list->tracks++]=cd->info.track[track];
  list->length=addMSF(list->length,cd->info.track[track].length);

#ifdef DEBUG
  fprintf(stderr,"cdpl_add[DEBUG]: track %d added to playlist 0x%08x (size %d)\n",
	  cd->info.track[track].num,list,list->tracks);
#endif

  return 0;
}

/*
 * cdpl_reset(list)
 *
 * set the playlist to zero tracks
 */
int cdpl_reset(CDPlayList *list)
{
  if (list==NULL)
    return 1;

  if (list->track)
    free(list->track);

  memset(list,0,sizeof(CDPlayList));

#ifdef DEBUG
  fprintf(stderr,"cdpl_reset[DEBUG]: playlist 0x%08x reseted\n",list);
#endif

  return 0;
}
