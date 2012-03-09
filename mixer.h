/*
 * $Id: mixer.h,v 1.2 2003/10/01 22:44:19 xtifr Exp $
 *
 * Copyright (c) 1997 by Oliver Graf <ograf@fga.de>
 */

#ifndef WMRACK_MIXER_H_
#define WMRACK_MIXER_H_

extern char *mixer_labels[];
extern char *mixer_names[];
extern char *mixer_shortnames[];
extern int mixer_devices;

typedef struct {
  char *device;
  int fd;
  char id[16];
  char name[32];
  int mask;
  int recmask;
  int recsrc;
  int old_recsrc;
  int stereo;
  int caps;
  int cur_vol[32];
  int old_vol[32];
} MIXER;

#define mixer_isdevice(m,d) ((d<mixer_devices)?(m->mask&(1<<d)):0)
#define mixer_isstereo(m,d) ((d<mixer_devices)?(m->stereo&(1<<d)):0)
#define mixer_ismono(m,d)   (!mixer_isstereo(m,d))
#define mixer_isrecsrc(m,d) ((d<mixer_devices)?(m->recsrc&(1<<d)):0)
#define mixer_isrecdev(m,d) ((d<mixer_devices)?(m->recmask&(1<<d)):0)

#define mixer_makevol(l,r) ((r<<8)+l)
#define mixer_volmono(m,d) ((d<mixer_devices)?((m->cur_vol[d]>>8)&0xff):-1)
#define mixer_volleft(m,d) (mixer_isstereo(m,d)?((d<mixer_devices)?(m->cur_vol[d]&0xff):-1):mixer_volmono(m,d))
#define mixer_volright(m,d) mixer_volmono(m,d)

#define mixer_volchanged(m,d) (mixer_isdevice(m,d)?(m->old_vol[d]!=m->cur_vol[d]):0)
#define mixer_srcchanged(m,d) (mixer_isrecdev(m,d) \
			       ? ((m->old_recsrc&(1<<d))!=(m->recsrc&(1<<d))) \
			       : 0)

MIXER *mixer_open(char *device);
void mixer_close(MIXER *mix);
int mixer_readvol(MIXER *mix, int dev);
int mixer_readvols(MIXER *mix);
int mixer_setvol(MIXER *mix, int dev, int vol);
int mixer_setvols(MIXER *mix, int dev, int left, int right);
int mixer_setrecsrc(MIXER *mix, int dev, int state, int exclusive);

int mixer_changevol(MIXER *mix, int dev, int delta);
int mixer_changeleft(MIXER *mix, int dev, int delta);
int mixer_changeright(MIXER *mix, int dev, int delta);
int mixer_changebal(MIXER *mix, int dev, int delta);

#endif /* WMRACK_MIXER_H */
