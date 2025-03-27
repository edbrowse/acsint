#ifndef _LINUX_ACSINT_H
#define _LINUX_ACSINT_H

/* Prototypes and definitions for the acsint device driver. */

#include <linux/input.h>
#include <linux/kd.h>
#include <linux/keyboard.h>

/* Commands that Acsint sends or receives */
enum acs_command {
	ACS_NULL,
/* configuration commands */
	ACS_BUFSIZE,		/* size of userland tty buffer */
	ACS_CLEAR_KEYS,
	ACS_SET_KEY,
	ACS_UNSET_KEY,
	ACS_ISMETA,
	ACS_PUSH_TTY,
/* Configure which sounds are made automatically */
	ACS_SOUNDS,		/* on or off */
	ACS_SOUNDS_TTY,
	ACS_SOUNDS_KMSG,
/* ask the driver to make specific sounds for you */
	ACS_CLICK,
	ACS_CR,
	ACS_SWOOP,
	ACS_NOTES,		/* series of notes */
	ACS_STEPS,		/* like a scale going up or down */
/* Request to bring the tty log up to date */
	ACS_REFRESH,
/* should keys go to the adapter or console or both */
	ACS_BYPASS,		/* send next char through */
	ACS_MONITOR,		/* to monitor keystrokes as you type */
	ACS_DIVERT,		/* adapter grabs a string of text */
/* Timing break between successive sections of output */
	ACS_OBREAK,
/* events coming back */
	ACS_KEYSTROKE,
	ACS_TTY_NEWCHARS,	/* the ones you haven't seen yet */
	ACS_TTY_MORECHARS,	/* there are more chars pending */
	ACS_FGC,		/* foreground console */
	ACS_PRINTK,
};

/* Here is a bound; you can't capture keys at or beyond this point. */
#define ACS_NUM_KEYS 128

/* Symbolic constants for the keys are in input.h */

/* Symbolic constants for the led states are in kd.h */

/* Symbolic constants for the shift states are derived from keyboard.h,
 * but I turn them into bits for you.
 * This makes it easier to describe modified key chords such as alt control X.
 * Just or things together. */
#define ACS_SS_SHIFT (1<<KG_SHIFT)
#define ACS_SS_RALT (1<<KG_ALTGR)
#define ACS_SS_CTRL (1<<KG_CTRL)
#define ACS_SS_LALT (1<<KG_ALT)
#define ACS_SS_ALT (ACS_SS_LALT|ACS_SS_RALT)

#define ACS_KEY_T 0x20

#endif
