/*
 * $Id: cdrom.h,v 1.1.1.1 2001/02/12 22:25:42 xtifr Exp $
 *
 * Copyright (c) 1997 by Oliver Graf <ograf@fga.de>
 */

#ifndef _MY_CDROM_H
#define _MY_CDROM_H

/* cd status modes */
#define CDM_CLOSED -1
#define CDM_COMP    0
#define CDM_PLAY    1
#define CDM_PAUSE   2
#define CDM_STOP    3
#define CDM_EJECT   CDM_CLOSED

/* cd actions */
#define CDA_NONE    0
#define CDA_STOP    1
#define CDA_PLAY    2
#define CDA_PAUSE   3

/* repeatmode */
#define CDR_NONE    0
#define CDR_ONE     1
#define CDR_ALL     2

/* playtype */
#define CDP_NORMAL  0
#define CDP_RANDOM  1

typedef struct {
  int minute;
  int second;
  int frame;
} MSF;

#define FRAMES 75
#define SECONDS 60

#define fSecs(f) (int)(f/FRAMES)
#define fMins(f) (int)(f/FRAMES/SECONDS)

#define msfFrames(msf) (((msf.minute*SECONDS)+msf.second)*FRAMES+msf.frame)
#define msfSecs(msf)   (((msf.minute*SECONDS)+msf.second)

#define MSFnone(msf)   msf.minute=msf.second=msf.frame=-1
#define MSFzero(msf)   msf.minute=msf.second=msf.frame=0

typedef struct {
  int num;
  MSF start;  /* real start */
  MSF end;    /* start of the next track == end */
  MSF length; /* length of the track in MSF */
  int data;   /* datatrack? */
} TrackInfo;

typedef struct {
  int mode;
  int track;
  MSF relmsf;
  MSF absmsf;
} CDPosition;

typedef struct {    /* stores information about the last play action */
  int last_action;  /* last action, needed for auto-play-next-in-list */
  int cur_track;    /* this is the automatical up-counted track within the list */
  int cur_end;      /* this is the calculated end of an uninterupted sequence */
  int repeat_mode;  /* repeat some or all or none */
  int play_type;    /* normal or random */
} CDPlayInfo;

typedef struct {    /* stores a array of tracks to play */
  MSF length;       /* length of the play list */
  int allocated;    /* tracks allacated in track entry */
  int tracks;       /* number of tracks in the play list */
  TrackInfo *track; /* the tracks */
} CDPlayList;

typedef struct {
  CDPosition    current;
  CDPlayInfo    play;
  CDPlayList    list;    /* is initialized with all non-data tracks */
  unsigned long discid;
  int           start;   /* first track of the cd */
  int           end;     /* last track of the cd */
  int           tracks;  /* number of tracks (end-start)+1 */
  TrackInfo     *track;
} CDInfo;

typedef struct {
  int    fd;
  char   *device;
  int    status;   /* this keeps the return value of cd_getStatus() */
  CDInfo info;     /* only one cd per device, anything else is stupid */
} CD;

CD *cd_open(char *device, int noopen);
int cd_reopen(CD *cd);
int cd_suspend(CD *cd);
void cd_close(CD *cd);

void cd_freeinfo(CD *cd);

int cd_getStatus(CD *cd, int reopen, int force);

/*
 * all cd_do* commands assume that prior to their call a cd_getStatus
 * is done to update the CDPosition struct of the cd
 */
int cd_playMSF(CD *cd, MSF start, MSF end);
int cd_doPlay(CD *cd, int start);
int cd_doSkip(CD *cd, int secs);
int cd_doPause(CD *cd);
int cd_doStop(CD *cd);
int cd_doEject(CD *cd);

int cmpMSF(MSF a, MSF b);
MSF subMSF(MSF a, MSF b);
MSF addMSF(MSF a, MSF b);
MSF normMSF(MSF msf);

/*
 * play list functions
 *
 * first the CD internal stuff
 */
int cd_resetpl(CD *cd);
int cd_findtrack(CD *cd, int num);
int cd_randomize(CD *cd);
int cd_setpl(CD *cd, CDPlayList *list);

/*
 * now extra play lists
 */
CDPlayList *cdpl_new();
int cdpl_free(CDPlayList *list);
int cdpl_add(CDPlayList *list, CD *cd, int track);
int cdpl_reset(CDPlayList *list);

#define cd_info(c,w) (c->info.##w)
#define cd_cur(c,w)  (c->info.current.##w)
#define cd_play(c,w) (c->info.play.##w)
#define cd_list(c,w) (c->info.list.##w)

#endif
