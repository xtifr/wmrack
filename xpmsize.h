/*
 * $Id: xpmsize.h,v 1.2 2003/10/02 03:46:30 xtifr Exp $
 *
 * defines to match xpm details -- must be edited if xpms change.
 *
 * copyright (c) Chris Waters 2001, 2003
 */

#ifndef WMRACK_XPMSIZE_H_
#define WMRACK_XPMSIZE_H_

/*
 * Special switch widget
 * (This should be the same on all views)
 */

#define RACK_SWITCH_TOP 17
#define RACK_SWITCH_BOTTOM 29
#define RACK_SWITCH_LEFT 17
#define RACK_SWITCH_RIGHT 30

#define IN_RACK_SWITCH_BUTTON(ev) ((ev).xbutton.x >= RACK_SWITCH_LEFT \
				   && (ev).xbutton.x <= RACK_SWITCH_RIGHT \
				   && (ev).xbutton.y >= RACK_SWITCH_TOP \
				   && (ev).xbutton.y <= RACK_SWITCH_BOTTOM)

/*
 * CD Widgets
 */

/* time display */
#define CD_TIME_WIDGET_BOTTOM 14  /* top and sides are max */
#define IN_CD_TIME_WIDGET(ev) ((ev).xbutton.y <= CD_TIME_WIDGET_BOTTOM)

/* play/stop buttons */
#define CD_PLAY_BUTTON_TOP 17
#define CD_PLAY_BUTTON_BOTTOM 29 
#define CD_PLAY_BUTTON_LEFT 0
#define CD_PLAY_BUTTON_RIGHT 12

#define CD_STOP_BUTTON_TOP CD_PLAY_BUTTON_TOP
#define CD_STOP_BUTTON_BOTTOM CD_PLAY_BUTTON_BOTTOM
#define CD_STOP_BUTTON_LEFT 35
#define CD_STOP_BUTTON_RIGHT 47

#define IN_CD_PLAY_BUTTON(ev) ((ev).xbutton.x <= CD_PLAY_BUTTON_RIGHT \
			       && (ev).xbutton.y >= CD_PLAY_BUTTON_TOP \
			       && (ev).xbutton.y <= CD_PLAY_BUTTON_BOTTOM)
#define IN_CD_STOP_BUTTON(ev) ((ev).xbutton.x >= CD_STOP_BUTTON_LEFT \
			       && (ev).xbutton.y >= CD_STOP_BUTTON_TOP \
			       && (ev).xbutton.y <= CD_STOP_BUTTON_BOTTOM)

/* next/prev buttons */
#define CD_PREV_BUTTON_TOP 34
#define CD_PREV_BUTTON_BOTTOM 47
#define CD_PREV_BUTTON_LEFT 0
#define CD_PREV_BUTTON_RIGHT 12

#define CD_NEXT_BUTTON_TOP CD_PREV_BUTTON_TOP
#define CD_NEXT_BUTTON_BOTTOM CD_PREV_BUTTON_BOTTOM
#define CD_NEXT_BUTTON_LEFT 35
#define CD_NEXT_BUTTON_RIGHT 47

#define IN_CD_PREV_BUTTON(ev) ((ev).xbutton.x <= CD_PREV_BUTTON_RIGHT \
			       && (ev).xbutton.y >= CD_PREV_BUTTON_TOP)
#define IN_CD_NEXT_BUTTON(ev) ((ev).xbutton.x >= CD_NEXT_BUTTON_LEFT \
			       && (ev).xbutton.y >= CD_NEXT_BUTTON_TOP)

/* track display widget */
#define CD_TRACK_WIDGET_TOP 33
#define CD_TRACK_WIDGET_BOTTOM 47
#define CD_TRACK_WIDGET_LEFT 14
#define CD_TRACK_WIDGET_RIGHT 33

#define IN_CD_TRACK_WIDGET(ev) ((ev).xbutton.x >= CD_TRACK_WIDGET_LEFT \
				&& (ev).xbutton.x <= CD_TRACK_WIDGET_RIGHT \
				&& (ev).xbutton.y >= CD_TRACK_WIDGET_TOP)

/*
 * Mixer widgets
 */

/* volume sliders */
#define MX_LEFT_SLIDER_EDGE 10
#define MX_RIGHT_SLIDER_EDGE 37

#define IN_MX_LEFT_SLIDER(ev) ((ev).xbutton.x <= MX_LEFT_SLIDER_EDGE)
#define IN_MX_RIGHT_SLIDER(ev) ((ev).xbutton.x >= MX_RIGHT_SLIDER_EDGE)
#define IN_MX_SLIDER(ev) (IN_MX_LEFT_SLIDER(ev) || IN_MX_RIGHT_SLIDER(ev))

/* channel selector */
#define MX_CHAN_WIDGET_BOTTOM 14
#define MX_CHAN_WIDGET_LEFT 14
#define MX_CHAN_WIDGET_RIGHT 33

#define IN_MX_CHAN_WIDGET(ev) ((ev).xbutton.x >= MX_CHAN_WIDGET_LEFT \
			       && (ev).xbutton.x <= MX_CHAN_WIDGET_RIGHT \
			       && (ev).xbutton.y <= MX_CHAN_WIDGET_BOTTOM)

/* mute button */
#define MX_MUTE_BUTTON_TOP 34
#define MX_MUTE_BUTTON_LEFT 17
#define MX_MUTE_BUTTON_RIGHT 30

#define IN_MX_MUTE_BUTTON(ev) ((ev).xbutton.x >= MX_MUTE_BUTTON_LEFT \
			       && (ev).xbutton.x <= MX_MUTE_BUTTON_RIGHT \
			       && (ev).xbutton.y >= MX_MUTE_BUTTON_TOP)

/* calculate volume from mouse coordinate */
#define CLICK_VOLUME(ev) (((47 - (ev).xbutton.y) / 4) * 10)

#endif /* WMRACK_XPMSIZE_H_ */
