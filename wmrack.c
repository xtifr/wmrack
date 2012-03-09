/*
 * $Id: wmrack.c,v 1.6 2007/11/25 23:24:15 xtifr Exp $
 *
 * WMRack - WindowMaker Sound Control Panel
 *
 * Copyright (c) 1997 by Oliver Graf  <ograf@fga.de>
 * copyright 2003-2006 by Chris Waters <xtifr@users.sourceforge.net>
 *
 * ascd originally by Rob Malda <malda@cs.hope.edu>
 *   http://www.cs.hope.edu/~malda/
 *
 * This is an 'WindowMaker Look & Feel' Dock applet that can be
 * used to control the cdrom and the mixer.
 *
 * Should also work swallowed in any fvwm compatible button bar.
 */

#define WMR_VERSION "1.4"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/xpm.h>
#include <X11/extensions/shape.h>

#include "xpmicon.h"
#include "xpmsize.h"
#include "cdrom.h"
#include "mixer.h"
#include "library.h"

#define MAXRCLINE 1024
#define POLLFREQ 50000L		/* microseconds between checking events */

/* Functions *****************************************************************/
void usage ();
void parseCmdLine (int argc, char *argv[]);
void initHandler ();
void createWindow (int, char **);
void shutDown (int);
void mainLoop ();
int flushExpose (Window w);
void redrawWindow ();
void redrawDisplay (int force_win, int force_disp);
Pixel getColor (char *name);
Time getTime ();
int loadMixerRC ();
int saveMixerRC ();
void rack_popup (char *msg);

/* Global stuff **************************************************************/
Display *Disp;
Window Root;
Window Iconwin;
Window Win;
char *Geometry = 0;
GC WinGC;

/* varibles for the options */
char *StyleXpmFile = NULL;
char *LedColor = NULL;
char *BackColor = NULL;
char CdDevice[1024] = "/dev/cdrom";
char MixerDevice[1024] = "/dev/mixer";
int withdraw = 0;
int noprobe = 0;

MSF last_time = { -1, -1, -1 };
int last_track = -1;
int last_cdmode = -1;		/* to sense a change */
int displaymode = 0;		/* bit 1 is run/remain, bit 2 is track/total */
int newdisc = 0;
int curMixer = 0;		/* this is the device number of the currently shown mixer scale */
int lastMixer = 0;
char *popup_display = NULL;
time_t popup_time = 0;
int popup_done = 0;

/* Mode of WMRack */
#define MODE_CDPLAYER 0
#define MODE_MIXER    1
int WMRack_Mode = MODE_CDPLAYER;

/* our cd device */
CD *cd = NULL;
CDPlayList *playlist = NULL;
int start_track = 0;

/* and the mixer */
MIXER *mixer = NULL;
int mixer_order[32], mixer_max = 0;
LIBRARY *mixer_lib = NULL;

/*
 * start the whole stuff and enter the MainLoop
 */
int
main (int argc, char *argv[])
{
    struct timeval tm;

    parseCmdLine (argc, argv);
    cd = cd_open (CdDevice, noprobe);
    if (cd_getStatus (cd, 0, 1) > 0)
    {
	if (cd->info.list.tracks == 0)
	{
	    rack_popup ("DATA");
	    cd_suspend (cd);
	}
	else
	    newdisc = 1;
    }
    mixer = mixer_open (MixerDevice);
    loadMixerRC ();
#ifdef DEBUG
    fprintf (stderr, "wmrack: Mixer RC loaded\n");
#endif
    initHandler ();
    gettimeofday (&tm, NULL);
    srandom (tm.tv_usec ^ tm.tv_sec);
    createWindow (argc, argv);
    mainLoop ();
    return 0;
}

/*
 * usage()
 *
 * print out usage text and exit
 */
void
usage ()
{
    fprintf (stderr, "wmrack - Version " WMR_VERSION "\n");
    fprintf (stderr, "usage: wmrack [OPTIONS] \n");
    fprintf (stderr, "\n");
    fprintf (stderr, "OPTION                  DEFAULT        DESCRIPTION\n");
    fprintf (stderr,
	     "-b|--background COLSPEC black          color of the led background\n");
    fprintf (stderr,
	     "-d|--device DEV         /dev/cdrom     device of the Drive\n");
    fprintf (stderr, "-h|--help               none           display help\n");
    fprintf (stderr,
	     "-l|--ledcolor COLSPEC   green          set the color of the led\n");
    fprintf (stderr,
	     "-m|--mixer DEV          /dev/mixer     device of the Mixer\n");
    fprintf (stderr,
	     "-p|--noprobe            off            disable the startup probe\n");
    fprintf (stderr,
	     "-s|--style STYLEFILE    compile-time   load an alternate set of xpm\n");
    fprintf (stderr,
	     "-w|--withdrawn          off            start withdrawn or not\n");
    fprintf (stderr,
	     "-M|--mode [cd|mixer]    cd             start in which mode\n");
    fprintf (stderr, "\n");
    exit (1);
}

/*
 * parseCmdLine(argc,argv)
 *
 * parse the command line args
 */
void
parseCmdLine (int argc, char *argv[])
{
    int i, j;
    char opt;
    struct
    {
	char *name;
	char option;
    } Options[] = { { "background", 'b'},
		    { "device", 'd'},
		    { "withdrawn", 'w'},
		    { "help", 'h'},
		    { "ledcolor", 'l'},
		    { "mixer", 'm'},
		    { "style", 's'},
		    { "noprobe", 'p'},
		    { "mode", 'M'},
		    { NULL, 0} };

    for (i = 1; i < argc; i++)
    {
	if (argv[i][0] == '-')
	{
	    if (argv[i][1] == '-')
	    {
		for (j = 0; Options[j].name != NULL; j++)
		    if (strcmp (Options[j].name, argv[i] + 2) == 0)
		    {
			opt = Options[j].option;
			break;
		    }
		if (Options[j].name == NULL)
		    usage ();
	    }
	    else
	    {
		if (argv[i][2] != 0)
		    usage ();
		opt = argv[i][1];
	    }
	    switch (opt)
	    {
	    case 'b':		/* Led Color */
		if (++i >= argc)
		    usage ();
		BackColor = strdup (argv[i]);
		continue;
	    case 'd':		/* Device */
		if (++i >= argc)
		    usage ();
		strcpy (CdDevice, argv[i]);
		continue;
	    case 'w':		/* start Withdrawn */
		withdraw = 1;
		continue;
	    case 'h':		/* usage */
		usage ();
		break;
	    case 'l':		/* Led Color */
		if (++i >= argc)
		    usage ();
		LedColor = strdup (argv[i]);
		continue;
	    case 'm':		/* Device */
		if (++i >= argc)
		    usage ();
		strcpy (MixerDevice, argv[i]);
		continue;
	    case 's':
		if (++i >= argc)
		    usage ();
		StyleXpmFile = strdup (argv[i]);
		continue;
	    case 'p':
		noprobe = 1;
		continue;
	    case 'M':
		if (++i >= argc)
		    usage ();
		if (strcmp (argv[i], "cd") == 0)
		{
		    WMRack_Mode = MODE_CDPLAYER;
		    curRack = RACK_NODISC;
		}
		else if (strcmp (argv[i], "mixer") == 0)
		{
		    WMRack_Mode = MODE_MIXER;
		    curRack = RACK_MIXER;
		}
		continue;
	    default:
		usage ();
	    }
	}
	else
	    usage ();
    }

}

/*
 * initHandler()
 *
 * inits the signal handlers
 * note: this function is not currently recursive (shouldn't be an issue)
 */
void
initHandler ()
{
    static struct sigaction sa;

    sa.sa_handler = shutDown;
    sigfillset (&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction (SIGTERM, &sa, NULL);
    sigaction (SIGINT, &sa, NULL);
    sigaction (SIGHUP, &sa, NULL);
    sigaction (SIGQUIT, &sa, NULL);
    sigaction (SIGPIPE, &sa, NULL);
    sa.sa_handler = SIG_IGN;
    sigaction (SIGUSR1, &sa, NULL);
    sigaction (SIGUSR2, &sa, NULL);
}

/*
 * createWindow(argc,argv)
 *
 * create the basic shaped window and set all the required stuff
 */
void
createWindow (int argc, char **argv)
{
    int i;
    unsigned int borderwidth;
    char *display_name = NULL;
    char *wname = "wmrack";
    XGCValues gcv;
    unsigned long gcm;
    XTextProperty name;
    Pixel back_pix, fore_pix;
    int screen;
    int x_fd;
    int d_depth;
    int ScreenWidth, ScreenHeight;
    XSizeHints SizeHints;
    XWMHints WmHints;
    XClassHint classHint;

    /* Open display */
    if (!(Disp = XOpenDisplay (display_name)))
    {
	fprintf (stderr, "wmrack: can't open display %s\n",
		 XDisplayName (display_name));
	exit (1);
    }

    screen = DefaultScreen (Disp);
    Root = RootWindow (Disp, screen);
    d_depth = DefaultDepth (Disp, screen);
    x_fd = XConnectionNumber (Disp);
    ScreenHeight = DisplayHeight (Disp, screen);
    ScreenWidth = DisplayWidth (Disp, screen);

    xpm_setDefaultAttr (Disp, Root, LedColor, BackColor);
    if (StyleXpmFile)
    {
	if (xpm_loadSet (Disp, Root, StyleXpmFile))
	{
	    fprintf (stderr, "wmrack: can't load and create pixmaps\n");
	    XCloseDisplay (Disp);
	    exit (1);
	}
    }
    else if (xpm_setDefaultSet (Disp, Root, RACK_MAX))
    {
	fprintf (stderr, "wmrack: can't create pixmaps\n");
	XCloseDisplay (Disp);
	exit (1);
    }

    SizeHints.flags = USSize | USPosition;
    SizeHints.x = 0;
    SizeHints.y = 0;
    back_pix = getColor ("white");
    fore_pix = getColor ("black");

    XWMGeometry (Disp, screen, Geometry, NULL, (borderwidth = 1), &SizeHints,
		 &SizeHints.x, &SizeHints.y, &SizeHints.width,
		 &SizeHints.height, &i);

    SizeHints.width = rackXpm[curRack].attributes.width;
    SizeHints.height = rackXpm[curRack].attributes.height;
    Win = XCreateSimpleWindow (Disp, Root, SizeHints.x, SizeHints.y,
			       SizeHints.width, SizeHints.height,
			       borderwidth, fore_pix, back_pix);
    Iconwin = XCreateSimpleWindow (Disp, Win, SizeHints.x, SizeHints.y,
				   SizeHints.width, SizeHints.height,
				   borderwidth, fore_pix, back_pix);
    XSetWMNormalHints (Disp, Win, &SizeHints);

    classHint.res_name = "wmrack";
    classHint.res_class = "WMRack";
    XSetClassHint (Disp, Win, &classHint);

    XSelectInput (Disp, Win,
		  (ExposureMask | ButtonPressMask | ButtonReleaseMask |
		   StructureNotifyMask | ButtonMotionMask));
    XSelectInput (Disp, Iconwin,
		  (ExposureMask | ButtonPressMask | ButtonReleaseMask |
		   StructureNotifyMask | ButtonMotionMask));

    if (XStringListToTextProperty (&wname, 1, &name) == 0)
    {
	fprintf (stderr, "wmrack: can't allocate window name\n");
	exit (-1);
    }
    XSetWMName (Disp, Win, &name);

    /* Create WinGC */
    gcm = GCForeground | GCBackground | GCGraphicsExposures;
    gcv.foreground = fore_pix;
    gcv.background = back_pix;
    gcv.graphics_exposures = False;
    WinGC = XCreateGC (Disp, Root, gcm, &gcv);

    XShapeCombineMask (Disp, Win, ShapeBounding, 0, 0,
		       rackXpm[curRack].mask, ShapeSet);
    XShapeCombineMask (Disp, Iconwin, ShapeBounding, 0, 0,
		       rackXpm[curRack].mask, ShapeSet);

    WmHints.initial_state = withdraw ? WithdrawnState : NormalState;
    WmHints.icon_window = Iconwin;
    WmHints.window_group = Win;
    WmHints.icon_x = SizeHints.x;
    WmHints.icon_y = SizeHints.y;
    WmHints.flags =
	StateHint | IconWindowHint | IconPositionHint | WindowGroupHint;
    XSetCommand (Disp, Win, argv, argc);
    XSetWMHints (Disp, Win, &WmHints);
    XMapWindow (Disp, Win);
    redrawDisplay (1, 1);
}

/*
 * shutDown(int sig)
 *
 * handler for signal and close-down function
 */
void
shutDown (int sig)
{
#ifdef DEBUG
    if (sig)
	fprintf (stderr, "wmrack: got signal %s\n", strsignal (sig));
    else
	fprintf (stderr, "wmrack: manual shutdown\n");
    fprintf (stderr, "wmrack: Shutting down\n");
#endif
    saveMixerRC ();
#ifdef DEBUG
    fprintf (stderr, "wmrack: mixer RC written\n");
#endif
    cd_close (cd);
#ifdef DEBUG
    fprintf (stderr, "wmrack: cd closed\n");
#endif
    mixer_close (mixer);
#ifdef DEBUG
    fprintf (stderr, "wmrack: mixer closed\n");
#endif
/* cause it's no good, this is commented out (CloseDisplay woes)
  xpm_freeSet(Disp);
#  ifdef DEBUG
  fprintf(stderr,"wmrack: XPMs freed\n");
#  endif
  XFreeGC(Disp, WinGC);
#  ifdef DEBUG
  fprintf(stderr,"wmrack: GC freed\n");
#  endif
  XDestroyWindow(Disp, Win);
#  ifdef DEBUG
  fprintf(stderr,"wmrack: Win destroyed\n");
#  endif
  XDestroyWindow(Disp, Iconwin);
#  ifdef DEBUG
  fprintf(stderr,"wmrack: Iconwin destroyed\n");
#  endif
*/
    XCloseDisplay (Disp);
#ifdef DEBUG
    fprintf (stderr, "wmrack: Display closed\n");
#endif
    exit (0);
}

/*
 * mainLoop()
 *
 * the main event loop
 */
void
mainLoop ()
{
    XEvent Event;
    Time when;
    Time press_time = -1;
    int skip_count, skip_amount, skip_delay;
    int force_win = 0, force_disp = 0;
    int change_volume = 0, vol_y, vol_side;

    while (1)
    {
	/* Check events */
	while (XPending (Disp))
	{
	    XNextEvent (Disp, &Event);
	    switch (Event.type)
	    {
	    case Expose:
		if (Event.xexpose.count == 0)
		    last_time.minute = last_time.second = last_time.frame =
			-1;
		force_win = 1;
		break;
	    case ButtonPress:
		switch (WMRack_Mode)
		{
		case MODE_CDPLAYER:
		    newdisc = 0;
		    cd_getStatus (cd, 0, 0);

		    if (IN_CD_TIME_WIDGET (Event))
		    {
			switch (Event.xbutton.button)
			{
			case 1:
			    if (cd->info.current.mode == CDM_PLAY)
				displaymode ^= 1;
			    break;
			case 2:
			    if (cd->info.current.mode == CDM_PLAY)
				displaymode ^= 2;
			    break;
			case 3:
			    switch (cd->info.play.repeat_mode)
			    {
			    case CDR_NONE:
				cd->info.play.repeat_mode = CDR_ALL;
				break;
			    case CDR_ALL:
				cd->info.play.repeat_mode = CDR_ONE;
				break;
			    default:
				cd->info.play.repeat_mode = CDR_NONE;
				break;
			    }
			    if (cd->info.play.repeat_mode == 2
				&& cd->info.current.mode == CDM_PLAY)
				start_track = cd->info.current.track;
			    break;
			}
			force_disp = 1;
		    }
		    else if (IN_CD_PLAY_BUTTON (Event))
		    {
			if (cd->info.current.mode == CDM_PLAY
			    || cd->info.current.mode == CDM_PAUSE)
			    cd_doPause (cd);
			else if (cd->info.current.mode == CDM_EJECT)
			{
			    if (cd_getStatus (cd, 1, 0))
			    {
				start_track = 0;
				if (cd->info.list.tracks == 0)
				{
				    rack_popup ("DATA");
				    cd_suspend (cd);
				}
				else
				{
				    newdisc = 1;
				    cd_doPlay (cd, start_track);
				}
			    }
			}
			else
			{
			    if (playlist != NULL)
			    {
				cd_setpl (cd, playlist);
				cdpl_free (playlist);
				playlist = NULL;
				cd_doPlay (cd, 0);
			    }
			    else
				cd_doPlay (cd, start_track);
			}
		    }
		    else if (IN_CD_STOP_BUTTON (Event))
		    {
			if (cd->info.current.mode == CDM_PLAY
			    || cd->info.current.mode == CDM_PAUSE)
			{
			    cd_doStop (cd);
			    start_track = cd->info.play.cur_track;
			}
			else if (cd->info.current.mode == CDM_EJECT)
			{
			    if (Event.xbutton.button == 3)
				cd_doEject (cd);
			    else
			    {
				if (cd_getStatus (cd, 1, 1))
				{
				    start_track = 0;
				    if (cd->info.list.tracks == 0)
				    {
					rack_popup ("DATA");
					cd_suspend (cd);
				    }
				    else
					newdisc = 1;
				}
			    }
			}
			else
			    cd_doEject (cd);
		    }
		    else if (IN_CD_PREV_BUTTON(Event))
		    {
			press_time = Event.xbutton.time;
			skip_count = 0;
			skip_delay = 8;
			skip_amount = -1;
		    }
		    else if (IN_CD_NEXT_BUTTON(Event))
		    {
			press_time = Event.xbutton.time;
			skip_count = 0;
			skip_delay = 8;
			skip_amount = 1;
		    }
		    else if (IN_CD_TRACK_WIDGET(Event))
		    {
			if (cd->info.current.mode != CDM_STOP)
			    break;
			if (Event.xbutton.state & ControlMask)
			{
			    switch (Event.xbutton.button)
			    {
			    case 1:
				if (playlist != NULL)
				{
				    cdpl_free (playlist);
				    playlist = NULL;
				    force_disp = 1;
				}
				else
				{
				    playlist = cdpl_new ();
				    cd_resetpl (cd);
				    rack_popup ("PROG");
				}
				break;
			    case 3:
				if (playlist != NULL)
				    cdpl_free (playlist);
				playlist = NULL;
				cd_randomize (cd);
				rack_popup ("RAND");
				break;
			    }
			}
			else if (Event.xbutton.state & Mod1Mask
				 && playlist != NULL)
			{
			    char num[20];

			    /* add current track to playlist */
			    cdpl_add (playlist, cd, start_track);
			    sprintf (num, "%02d%02d", playlist->tracks,
				     playlist->track[playlist->tracks - 1]);
			    rack_popup (num);
			}
		    }
		    else if (IN_RACK_SWITCH_BUTTON(Event))
		    {
			if (Event.xbutton.button == 3
			    && (Event.xbutton.state & ControlMask))
			    shutDown (0);
			WMRack_Mode = MODE_MIXER;
			force_win = 1;
		    }
#ifdef DEBUG
		    else
			fprintf (stderr,"WMRack: click outside widgets\n");
#endif
		    break;

		case MODE_MIXER:
		    if (IN_MX_SLIDER(Event))
		    {
			int change;

			/* change volume */
			change_volume = Event.xbutton.button;
			vol_y = Event.xbutton.y_root;
			vol_side = IN_MX_LEFT_SLIDER (Event);
			switch (change_volume)
			{
			case 1:
			    if (vol_side)
				mixer_setvols (mixer, mixer_order[curMixer],
					       CLICK_VOLUME (Event),
					       mixer_volright (mixer,
							       mixer_order
							       [curMixer]));
			    else
				mixer_setvols (mixer, mixer_order[curMixer],
					       mixer_volleft (mixer,
							      mixer_order
							      [curMixer]),
					       CLICK_VOLUME (Event));
			    break;
			case 2:
			    mixer_setvol (mixer, mixer_order[curMixer],
					  CLICK_VOLUME (Event));
			    break;
			case 3:
			    change = (CLICK_VOLUME (Event) -
				      (vol_side
				       ? mixer_volleft (mixer,
							mixer_order[curMixer])
				       : mixer_volright (mixer,
							 mixer_order
							 [curMixer])));
			    if (vol_side)
				mixer_changebal (mixer, mixer_order[curMixer],
						 -change);
			    else
				mixer_changebal (mixer, mixer_order[curMixer],
						 change);
			    break;
			}
		    }
		    else if (IN_MX_CHAN_WIDGET (Event))
		    {
			if (Event.xbutton.state & ControlMask)
			{
			    int i, j, c;

			    switch (Event.xbutton.button)
			    {
			    case 1:	/* show all mixer devices */
				c = mixer_order[curMixer];
				for (j = i = 0; i < mixer_devices; i++)
				    if (mixer_isdevice (mixer, i))
				    {
					if (i == c)
					    curMixer = j;
					mixer_order[j++] = i;
				    }
				mixer_max = j;
				force_disp = 1;
				break;
			    case 3:	/* delete this device */
				if (mixer_max > 1)
				{
				    if (curMixer == mixer_max - 1)
					curMixer--;
				    else
					memmove (&mixer_order[curMixer],
						 &mixer_order[curMixer + 1],
						 sizeof (int) *
						 (mixer_max - curMixer));
				    mixer_max--;
				    force_disp = 1;
				}
				break;
			    }
			}
			else
			{
			    switch (Event.xbutton.button)
			    {
			    case 1:
				do {
				    curMixer++;
				    if (curMixer == mixer_max)
					curMixer = 0;
				} while (!mixer_isdevice (mixer,
							  mixer_order
							  [curMixer]));
				break;
			    case 2:
				curMixer = 0;
				break;
			    case 3:
				do {
				    if (curMixer == 0)
					curMixer = mixer_max;
				    curMixer--;
				} while (!mixer_isdevice (mixer,
							  mixer_order
							  [curMixer]));
				break;
			    }
			}
		    }
		    else if (IN_MX_MUTE_BUTTON (Event))
		    {
			if (!mixer_isrecdev (mixer, mixer_order[curMixer]))
			    break;
			switch (Event.xbutton.button)
			{
			case 1:
			    mixer_setrecsrc (mixer, mixer_order[curMixer],
					     mixer_isrecsrc (mixer,
							     mixer_order
							     [curMixer])
					     ? 0 : 1, 0);
			    break;
			case 2:
			    mixer_setrecsrc (mixer, mixer_order[curMixer],
					     1, 1);
			    break;
			}
		    }
		    else if (IN_RACK_SWITCH_BUTTON (Event))
		    {
			if (Event.xbutton.button == 3
			    && (Event.xbutton.state & ControlMask))
			    shutDown (0);
			WMRack_Mode = MODE_CDPLAYER;
			force_win = 1;
		    }
#ifdef DEBUG
		    else
			fprintf (stderr,"WMRack: click outside widgets\n");
#endif
		    break;
		}
		break;

	    case MotionNotify:
		switch (WMRack_Mode)
		{
		case MODE_MIXER:
		    if (change_volume)
		    {
			if ((Event.xmotion.y_root - vol_y) / 8)
			{
			    int change =
				((vol_y - Event.xmotion.y_root) / 8) * 10;
			    switch (change_volume)
			    {
			    case 1:
				if (vol_side)
				    mixer_changeleft (mixer,
						      mixer_order[curMixer],
						      change);
				else
				    mixer_changeright (mixer,
						       mixer_order[curMixer],
						       change);
				break;
			    case 2:
				mixer_changevol (mixer,
						 mixer_order[curMixer],
						 change);
				break;
			    case 3:
				if (vol_side)
				    mixer_changebal (mixer,
						     mixer_order[curMixer],
						     -change);
				else
				    mixer_changebal (mixer,
						     mixer_order[curMixer],
						     change);
			    }
			    vol_y = Event.xmotion.y_root;
			}
		    }
		}
		break;

	    case ButtonRelease:
		switch (WMRack_Mode)
		{
		case MODE_CDPLAYER:
		    if (press_time == -1)
			break;

		    if (IN_CD_PREV_BUTTON (Event)
			&& Event.xbutton.time - press_time < 200)
		    {
			if (cd->info.current.mode == CDM_PLAY
			    || cd->info.current.mode == CDM_PAUSE)
			{
			    if (cd->info.current.relmsf.minute
				|| cd->info.current.relmsf.second > 2)
				cd_doPlay (cd, cd->info.current.track);
			    else
				cd_doPlay (cd, cd->info.current.track - 1);
			}
			else if (cd->info.current.mode == CDM_STOP)
			{
			    if (start_track > 0)
				start_track--;
			    else
				start_track = cd->info.list.tracks - 1;
			}
		    }
		    else if (IN_CD_NEXT_BUTTON (Event)
			     && Event.xbutton.time - press_time < 200)
		    {
			if (cd->info.current.mode == CDM_PLAY
			    || cd->info.current.mode == CDM_PAUSE)
			{
			    if (cd->info.current.track < 
				cd->info.list.tracks - 1)
				cd_doPlay (cd, cd->info.current.track + 1);

			}
			else if (cd->info.current.mode == CDM_STOP)
			{
			    if (start_track < cd->info.list.tracks - 1)
				start_track++;
			    else
				start_track = 0;
			}
		    }
		    break;
		case MODE_MIXER:
		    if (change_volume)
		    {
			change_volume = 0;
		    }
		    break;
		}
		press_time = -1;
		change_volume = 0;
		break;
	    case DestroyNotify:
		shutDown (SIGTERM);
		break;
	    }
	    XFlush (Disp);
	}
	/* now check for a pressed button */
	if (press_time != -1)
	{
	    if (cd->info.current.mode == CDM_PLAY)
	    {
		when = getTime ();
		if (when - press_time > 500)
		{
		    /* this is needed because of the faster pace */
		    cd_getStatus (cd, 0, 1);
		    skip_count++;
		    if (skip_count % skip_delay == 0)
		    {
			if (IN_CD_PREV_BUTTON (Event))
			    cd_doSkip (cd, skip_amount);
			else if (IN_CD_NEXT_BUTTON (Event))
			    cd_doSkip (cd, skip_amount);
		    }
		    switch (skip_count)
		    {
		    case 5:
		    case 10:
		    case 20:
			skip_delay >>= 1;
			break;
		    }
		}
	    }
	}
	/* do a redraw of the LED display */
	redrawDisplay (force_win, force_disp);
	usleep (POLLFREQ);
	force_win = force_disp = 0;
    }
}

/*
 * flushExpose(window)
 *
 * remove any Expose events from the current EventQueue
 */
int
flushExpose (Window w)
{
    XEvent dummy;
    int i = 0;

    while (XCheckTypedWindowEvent (Disp, w, Expose, &dummy))
	i++;
    return i;
}

/*
 * redrawWindow()
 *
 * combine mask and draw pixmap
 */
void
redrawWindow ()
{
    flushExpose (Win);
    flushExpose (Iconwin);

    XShapeCombineMask (Disp, Win, ShapeBounding, 0, 0,
		       rackXpm[curRack].mask, ShapeSet);
    XShapeCombineMask (Disp, Iconwin, ShapeBounding, 0, 0,
		       rackXpm[curRack].mask, ShapeSet);

    XCopyArea (Disp, rackXpm[curRack].pixmap, Win, WinGC,
	       0, 0, rackXpm[curRack].attributes.width,
	       rackXpm[curRack].attributes.height, 0, 0);
    XCopyArea (Disp, rackXpm[curRack].pixmap, Iconwin, WinGC, 0, 0,
	       rackXpm[curRack].attributes.width, 
	       rackXpm[curRack].attributes.height, 0, 0);
}

/*
 * paint_cd_led(flash,track,cdtime)
 *
 * draws the digital numbers to the pixmaps
 */
void
paint_cd_led (int flash, int track[], int cdtime[])
{
    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Win, WinGC,
	       (track[0] ? 8 * track[0] : 80), 0, 8, 11, 16, 35);
    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Iconwin, WinGC,
	       (track[0] ? 8 * track[0] : 80), 0, 8, 11, 16, 35);
    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Win, WinGC,
	       8 * track[1], 0, 8, 11, 24, 35);
    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Iconwin, WinGC,
	       8 * track[1], 0, 8, 11, 24, 35);

    if (flash || cd->info.current.mode != CDM_PAUSE)
    {
	if (cd->info.current.mode == CDM_PLAY || 
	    cd->info.current.mode == CDM_PAUSE)
	{
	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Win, WinGC,
		       ((displaymode & 2) ? 94 : 98), 0, 4, 5, 3, 2);
	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Iconwin, WinGC,
		       ((displaymode & 2) ? 94 : 98), 0, 4, 5, 3, 2);
	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Win, WinGC,
		       ((displaymode & 1) ? 94 : 98), 5, 4, 1, 3, 7);
	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Iconwin, WinGC,
		       ((displaymode & 1) ? 94 : 98), 5, 4, 1, 3, 7);
	}
	else
	{
	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Win, WinGC,
		       98, 0, 4, 6, 3, 2);
	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Iconwin, WinGC,
		       98, 0, 4, 6, 3, 2);
	}

	XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Win, WinGC,
		   ((playlist != NULL) ? 94 : 98), 6, 4, 5, 3, 8);
	XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Iconwin, WinGC,
		   ((playlist != NULL) ? 94 : 98), 6, 4, 5, 3, 8);

	if (popup_display == NULL)
	{
	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Win, WinGC,
		       (cdtime[0] ? 8 * cdtime[0] : 80), 0, 8, 11, 7, 2);
	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Iconwin, WinGC,
		       (cdtime[0] ? 8 * cdtime[0] : 80), 0, 8, 11, 7, 2);

	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Win, WinGC,
		       8 * cdtime[1], 0, 8, 11, 15, 2);
	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Iconwin, WinGC,
		       8 * cdtime[1], 0, 8, 11, 15, 2);

	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Win, WinGC,
		       88, 0, 3, 11, 23, 2);
	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Iconwin, WinGC,
		       88, 0, 3, 11, 23, 2);

	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Win, WinGC,
		       8 * cdtime[2], 0, 8, 11, 26, 2);
	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Iconwin, WinGC,
		       8 * cdtime[2], 0, 8, 11, 26, 2);

	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Win, WinGC,
		       8 * cdtime[3], 0, 8, 11, 34, 2);
	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Iconwin, WinGC,
		       8 * cdtime[3], 0, 8, 11, 34, 2);

	}

	XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Win, WinGC,
		   (cd->info.play.repeat_mode != CDR_NONE ? 102 : 106), 0, 4,
		   5, 42, 2);
	XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Iconwin, WinGC,
		   (cd->info.play.repeat_mode != CDR_NONE ? 102 : 106), 0, 4,
		   5, 42, 2);
	XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Win, WinGC,
		   (cd->info.play.repeat_mode == CDR_ONE ? 102 : 106), 6, 4,
		   5, 42, 8);
	XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Iconwin, WinGC,
		   (cd->info.play.repeat_mode == CDR_ONE ? 102 : 106), 6, 4,
		   5, 42, 8);
    }
    else
    {
	XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Win, WinGC,
		   98, 0, 4, 11, 3, 2);
	XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Iconwin, WinGC,
		   98, 0, 4, 11, 3, 2);

	if (popup_display == NULL)
	{
	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Win, WinGC,
		       80, 0, 8, 11, 7, 2);
	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Iconwin, WinGC,
		       80, 0, 8, 11, 7, 2);

	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Win, WinGC,
		       80, 0, 8, 11, 15, 2);
	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Iconwin, WinGC,
		       80, 0, 8, 11, 15, 2);

	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Win, WinGC,
		       91, 0, 3, 11, 23, 2);
	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Iconwin, WinGC,
		       91, 0, 3, 11, 23, 2);

	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Win, WinGC,
		       80, 0, 8, 11, 26, 2);
	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Iconwin, WinGC,
		       80, 0, 8, 11, 26, 2);

	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Win, WinGC,
		       80, 0, 8, 11, 34, 2);
	    XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Iconwin, WinGC,
		       80, 0, 8, 11, 34, 2);
	}

	XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Win, WinGC,
		   106, 0, 4, 11, 42, 2);
	XCopyArea (Disp, rackXpm[RACK_LED_PLAYER].pixmap, Iconwin, WinGC,
		   106, 0, 4, 11, 42, 2);
    }

    if (popup_display != NULL)
    {
	int disp_pos[4] = { 7, 15, 26, 34 };
	char *d;
	int i, j;

#if 0
	if (!popup_done)
	{
	    XFillRectangle(Disp, Win, WinGC, 7, 2, 35, 11);
	    XFillRectangle(Disp, Iconwin, WinGC, 7, 2, 35, 11);
	    popup_done=1;
	}
#endif

	for (j = 0, d = popup_display; *d; d++, j++)
	{
	    for (i = 0; ledAlphabet[i]; i++)
		if (toupper (*d) == ledAlphabet[i])
		{
		    XCopyArea (Disp, rackXpm[RACK_LED_ALPHA].pixmap, Win,
			       WinGC, i * 8, 0, 8, 11, disp_pos[j], 2);
		    XCopyArea (Disp, rackXpm[RACK_LED_ALPHA].pixmap, Iconwin,
			       WinGC, i * 8, 0, 8, 11, disp_pos[j], 2);
		    break;
		}
	}
    }

}

/*
 * paint_mixer_led()
 *
 * draws the digital scales and signs to the pixmaps
 */
void
paint_mixer_led ()
{
    int i;

    /* the device name */
    for (i = 0; ledAlphabet[i]; i++)
	if (toupper (mixer_shortnames[mixer_order[curMixer]][0]) ==
	    ledAlphabet[i])
	{
	    XCopyArea (Disp, rackXpm[RACK_LED_ALPHA].pixmap, Win, WinGC,
		       i * 8, 0, 8, 11, 16, 2);
	    XCopyArea (Disp, rackXpm[RACK_LED_ALPHA].pixmap, Iconwin, WinGC,
		       i * 8, 0, 8, 11, 16, 2);
	    break;
	}
    for (i = 0; ledAlphabet[i]; i++)
	if (toupper (mixer_shortnames[mixer_order[curMixer]][1]) ==
	    ledAlphabet[i])
	{
	    XCopyArea (Disp, rackXpm[RACK_LED_ALPHA].pixmap, Win, WinGC,
		       i * 8, 0, 8, 11, 24, 2);
	    XCopyArea (Disp, rackXpm[RACK_LED_ALPHA].pixmap, Iconwin, WinGC,
		       i * 8, 0, 8, 11, 24, 2);
	    break;
	}

    /* the recsrc button */
    if (mixer_isrecdev (mixer, mixer_order[curMixer]))
    {
	if (mixer_isrecsrc (mixer, mixer_order[curMixer]))
	    i = 13;
	else
	    i = 0;
    }
    else
	i = 26;
    XCopyArea (Disp, rackXpm[RACK_LED_MIXER].pixmap, Win, WinGC,
	       44, i, 14, 13, 17, 34);
    XCopyArea (Disp, rackXpm[RACK_LED_MIXER].pixmap, Iconwin, WinGC,
	       44, i, 14, 13, 17, 34);

    /* the volume displays */
    /* left */
    i = (mixer_volleft (mixer, mixer_order[curMixer]) / 10);
    if (i < 0)
	i = 0;
    if (i > 10)
	i = 10;
    i *= 4;
    XCopyArea (Disp, rackXpm[RACK_LED_MIXER].pixmap, Win, WinGC,
	       i, 0, 3, 39, 4, 4);
    XCopyArea (Disp, rackXpm[RACK_LED_MIXER].pixmap, Iconwin, WinGC,
	       i, 0, 3, 39, 4, 4);
    /* right */
    i = (mixer_volright (mixer, mixer_order[curMixer]) / 10);
    if (i < 0)
	i = 0;
    if (i > 10)
	i = 10;
    i *= 4;
    XCopyArea (Disp, rackXpm[RACK_LED_MIXER].pixmap, Win, WinGC,
	       i, 0, 3, 39, 41, 4);
    XCopyArea (Disp, rackXpm[RACK_LED_MIXER].pixmap, Iconwin, WinGC,
	       i, 0, 3, 39, 41, 4);

}

/*
 * split this function into the real redraw and a pure 
 * display time/track function.
 * redraw wants a complete redraw (covering, movement, etc.)
 * but the display of the time/track does not need this overhead (SHAPE)
 */
void
redrawDisplay (int force_win, int force_disp)
{
    int track[2] = { 0, 0 };
    int cdtime[4] = { 0, 0, 0, 0 };
    static time_t last_flash_time;
    static flash = 0;
    int st = 0, newRack = RACK_NODISC, im_stop = 0;
    MSF pos;

    st = cd_getStatus (cd, 0, 0);

    if (!force_win && !force_disp && popup_display == NULL)
    {
	/* test if something has changed */
	switch (WMRack_Mode)
	{
	case MODE_CDPLAYER:
	    if (cd->info.current.mode != CDM_PAUSE
		&& last_cdmode == cd->info.current.mode
		&& (st < 1
		    || (last_time.minute == cd->info.current.relmsf.minute
			&& last_time.second == cd->info.current.relmsf.second
			&& last_track == start_track)))
		return;
	    break;
	case MODE_MIXER:
	    mixer_readvol (mixer, mixer_order[curMixer]);
	    if (curMixer == lastMixer
		&& !mixer_volchanged (mixer, mixer_order[curMixer])
		&& !mixer_srcchanged (mixer, mixer_order[curMixer]))
		return;
	    break;
	}
    }

#ifdef DEBUG
    if (last_cdmode != cd->info.current.mode)
    {
	fprintf (stderr, "wmrack: cur_cdmode %d\n", cd->info.current.mode);
    }
#endif

    if (cd->info.current.mode == CDM_STOP
	&& cd->info.play.last_action == CDA_PLAY)
	start_track = 0;

    lastMixer = curMixer;

    last_cdmode = cd->info.current.mode;
    if (st > 0)
    {
	last_time = cd->info.current.relmsf;
	last_track = start_track;
    }
    else
    {
	MSFnone (last_time);
	last_track = -1;
    }

    if (cd->info.current.mode == CDM_PAUSE)
    {
	time_t flash_time = time (NULL);

	if (flash_time == last_flash_time && !force_win)
	    return;
	last_flash_time = flash_time;
	flash = !flash;
    }
    else
    {
	last_flash_time = 0;
	flash = 1;
    }

    if (popup_display != NULL)
    {
	if (popup_time == 0)
	    popup_time = time (NULL);
	else
	{
	    time_t now = time (NULL);

	    if (now > popup_time + 1)
	    {
		free (popup_display);
		popup_display = NULL;
		popup_time = 0;
		popup_done = 0;
	    }
	}
    }

    newRack = RACK_PLAY;

    if (WMRack_Mode == MODE_MIXER)
	newRack = RACK_MIXER;
    else
    {
	switch (cd->info.current.mode)
	{
	case CDM_PAUSE:
	    newRack = RACK_PAUSE;
	case CDM_PLAY:
	    track[0] = cd->info.list.track[cd->info.current.track].num / 10;
	    track[1] = cd->info.list.track[cd->info.current.track].num % 10;
	    switch (displaymode)
	    {
	    case 0:
		pos = cd->info.current.relmsf;
		break;
	    case 1:
		pos = subMSF (cd->info.list.track[cd->info.current.track].length,
			      cd->info.current.relmsf);
		break;
	    case 2:
		pos = subMSF (cd->info.current.absmsf, 
			      cd->info.track[0].start);
		break;
	    case 3:
		pos = subMSF (cd->info.track[cd->info.tracks - 1].end,
			      cd->info.current.absmsf);
		break;
	    }
	    cdtime[0] = pos.minute / 10;
	    cdtime[1] = pos.minute % 10;
	    cdtime[2] = pos.second / 10;
	    cdtime[3] = pos.second % 10;
	    break;
	case CDM_STOP:
	    newRack = RACK_STOP;
	    if (newdisc)
	    {
		track[0] = cd->info.list.tracks / 10;
		track[1] = cd->info.list.tracks % 10;
	    }
	    else
	    {
		track[0] = cd->info.list.track[start_track].num / 10;
		track[1] = cd->info.list.track[start_track].num % 10;
	    }
	    if (playlist == NULL)
	    {
		cdtime[0] = cd->info.list.length.minute / 10;
		cdtime[1] = cd->info.list.length.minute % 10;
		cdtime[2] = cd->info.list.length.second / 10;
		cdtime[3] = cd->info.list.length.second % 10;
	    }
	    else
	    {
		cdtime[0] = playlist->length.minute / 10;
		cdtime[1] = playlist->length.minute % 10;
		cdtime[2] = playlist->length.second / 10;
		cdtime[3] = playlist->length.second % 10;
	    }
	    break;
	case CDM_COMP:
	    newRack = RACK_STOP;
	    goto set_null;
	case CDM_EJECT:
	    newRack = RACK_NODISC;
	default:
	  set_null:
	    track[0] = 0;
	    track[1] = 0;
	    cdtime[0] = 0;
	    cdtime[1] = 0;
	    cdtime[2] = 0;
	    cdtime[3] = 0;
	    break;
	}
    }

    if (newRack != curRack || force_win)
    {
	/* Mode has changed, paint new mask and pixmap */
	curRack = newRack;
	redrawWindow ();
    }

    switch (curRack)
    {
    case RACK_MIXER:
	paint_mixer_led ();
	break;
    default:
	paint_cd_led (flash, track, cdtime);
	break;
    }

}

/*
 * getColor(colorname)
 *
 * save way to get a color from X
 */
Pixel
getColor (char *ColorName)
{
    XColor Color;
    XWindowAttributes Attributes;

    XGetWindowAttributes (Disp, Root, &Attributes);
    Color.pixel = 0;

    if (!XParseColor (Disp, Attributes.colormap, ColorName, &Color))
	fprintf (stderr, "wmrack: can't parse %s\n", ColorName);
    else if (!XAllocColor (Disp, Attributes.colormap, &Color))
	fprintf (stderr, "wmrack: can't allocate %s\n", ColorName);

    return Color.pixel;
}

/*
 * getTime()
 *
 * returns the time in milliseconds
 */
Time
getTime ()
{
    struct timeval tv;

    gettimeofday (&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

/*
 * loadMixerRC()
 *
 * loads the mixer defaults
 */
int
loadMixerRC ()
{
    char line[MAXRCLINE], dev[MAXRCLINE], src[MAXRCLINE], *d;
    int i, j, err = 0;
    int nfields, left, right;

    mixer_max = 0;
    mixer_lib = lib_open ("mixer", LIB_READ);
    if (mixer_lib == NULL)
    {
	fprintf (stderr, "wmrack: can't read mixer file\n");
	err = -1;
	goto endload;
    }

    while ((lib_gets (mixer_lib, line, MAXRCLINE)) != NULL)
    {
	dev[0] = src[0] = '\0';

	nfields = sscanf (line, "%s %d:%d %s", &dev, &left, &right, &src);

	if (nfields >= 3)	/* got at least the left & right info */
	{
	    for (i = 0; i < mixer_devices; i++)
	    {
		if (strcmp (mixer_names[i], dev) == 0)
		{
		    mixer_setvols (mixer, i, left, right);
		    if (strcmp (src, "src") == 0)
		        mixer_setrecsrc (mixer, i, 1, 0);
		    break;
		}
	    }
#ifdef DEBUG
	    if (i == mixer_devices)
		fprintf (stderr, "wmrack: unsupported device '%s'\n", dev);
#endif
	}
	else   /* the sscanf failed, check for an ORDER line */
	{
	    d = strtok (line, " \t\n\r");
	    if (strcmp (d, "ORDER") == 0)
	    {
		while ((d = strtok (NULL, " \t\n\r")) != NULL)
		{
		    for (i = 0; i < mixer_devices; i++)
			if (strcmp (d, mixer_names[i]) == 0)
			    break;
		    if (i < mixer_devices && mixer_isdevice (mixer, i))
		    {
			mixer_order[mixer_max++] = i;
#ifdef DEBUG
			fprintf (stderr, "wmrack: mixer_order %d=%s\n",
				 mixer_max, mixer_names[i]);
#endif
		    }
#ifdef DEBUG
		    else
		      fprintf (stderr, "wmrack: unsupported device '%s'\n", d);
#endif
		}
	    }
#if DEBUG
	    else
		fprintf (stderr, "wmrack: invalid setting '%s'\n", line);
#endif
	}
    }

    lib_free (mixer_lib);

  endload:
    if (mixer_max == 0)
    {
#ifdef DEBUG
	fprintf (stderr, "wmrack: setting default mixer_order\n");
#endif
	for (j = i = 0; i < mixer_devices; i++)
	    if (mixer_isdevice (mixer, i))
		mixer_order[j++] = i;
	mixer_max = j;
    }

    mixer_readvols (mixer);

    return err;
}

/*
 * saveMixerRC()
 *
 * writes the mixer defaults
 */
int
saveMixerRC ()
{
    int i;

    mixer_lib = lib_open ("mixer", LIB_WRITE);
    if (mixer_lib == NULL)
    {
	fprintf (stderr, "wmrack: can't write mixer file\n");
	return -1;
    }

    for (i = 0; i < mixer_devices; i++)
    {
	if (mixer_isdevice (mixer, i))
	{
	    if (mixer_isstereo (mixer, i))
		lib_printf (mixer_lib, "%s %d:%d%s\n",
			    mixer_names[i],
			    mixer_volleft (mixer, i),
			    mixer_volright (mixer, i),
			    mixer_isrecsrc (mixer, i) ? " src" : "");
	    else
		lib_printf (mixer_lib, "%s %d:%d%s\n",
			    mixer_names[i],
			    mixer_volmono (mixer, i),
			    mixer_volmono (mixer, i),
			    mixer_isrecsrc (mixer, i) ? " src" : "");
	}
    }
    if (mixer_max > 0)
    {
	lib_printf (mixer_lib, "ORDER");
	for (i = 0; i < mixer_max; i++)
	{
	    lib_printf (mixer_lib, " %s", mixer_names[mixer_order[i]]);
	}
	lib_printf (mixer_lib, "\n");
    }
    lib_close (mixer_lib);

    return 0;
}

void
rack_popup (char *msg)
{
    if (popup_display != NULL)
	free (popup_display);
    popup_display = strdup (msg);
    popup_done = 0;
}
