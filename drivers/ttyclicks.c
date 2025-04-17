/* ttyclicks.c: generate clicks as output is sent to the screen.
 * Please see Documentation/accessibility/ttyclicks.txt for more details.
 *
 * Copyright (C) Karl Dahlke, 2004-2014.
 * This software may be freely distributed under the GPL,
 * general public license, as articulated by the Free Software Foundation.
 */

/* None of this is implemented on computers without the inbuilt speaker. */
/* Or set NOCLICKS at the command line if you wish. */
#include <linux/kconfig.h>
#include <linux/module.h>
#include <linux/version.h>
#ifndef CONFIG_I8253_LOCK
#define NOCLICKS 1
#warning "NOCLICKS set because toggle speaker seems not to be present"
#endif

#ifndef NOCLICKS
#include <linux/notifier.h>
#include <linux/keyboard.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <linux/vt_kern.h>	/* for fg_console and speaker sounds */
#include <linux/ctype.h>
#include <linux/console.h>
#include <linux/io.h>		/* for inb() outb() */
#include <linux/delay.h>
#include <linux/i8253.h>
#endif

#include "ttyclicks.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Karl Dahlke - eklhad@gmail.com");
MODULE_DESCRIPTION
	("Console output generates clicks, resembling a mechanical teletype.");

static int enabled = 1;
module_param(enabled, int, 0);
MODULE_PARM_DESC(enabled,
		 "sounds are enabled as soon as this module is loaded, default = 1 (yes)");

static int fgtty = 1;
module_param(fgtty, int, 0);
MODULE_PARM_DESC(fgtty,
		 "foreground tty generates clicks and chirps, default = 1 (yes)");

static int cursormotion;
module_param(cursormotion, int, 0);
MODULE_PARM_DESC(cursormotion,
		 "generate clicks for output characters that move the cursor or set screen attributes; default is 0 (no)");

static int kmsg = 1;
module_param(kmsg, int, 0);
MODULE_PARM_DESC(kmsg,
		 "kernel warning/error message generates a sequence of tones to get your attension, default = 1 (yes)");

static int sleep;
module_param(sleep, int, 0);
MODULE_PARM_DESC(sleep, "sleep between the clicks of the output characters - not yet implemented.");

/*
 * Here are some symbols that we export to other modules
 * so they can turn clicks on and off.
 * They correspond (roughly) to the module parameters given above.
 */

bool ttyclicks_on = true;
EXPORT_SYMBOL_GPL(ttyclicks_on);
bool ttyclicks_tty = true;
EXPORT_SYMBOL_GPL(ttyclicks_tty);
bool ttyclicks_kmsg = true;
EXPORT_SYMBOL_GPL(ttyclicks_kmsg);

#ifndef NOCLICKS

/*
 * Don't click the ansi escape sequences that move the cursor on the screen.
 * However, esc [ ... H moves the cursor to a new line, and might be worth
 * a cr sound. Not sure about that one.
 * escState holds the state of the escape sequence.
 * 0 regular output, 1 escape received, 2 escape [ received.
 * A letter ends the escape sequence and goes from state 1 or 2 back to state 0.
 */

static char escState;

/*
 * Kernel alert messages don't go through tty, and you won't hear any clicks.
 * You don't want clicks anyways; you want something more.
 * You want an unusual noise that alerts you -
 * there is a kernel message that requires your attention!
 * When a newline comes through printk,
 * I make a sound with up and down tones.
 */

static const short printk_sound[] = {
	730, 7, 760, 7, 790, 7, 760, 7, 730, 7, -1, 7, 0, 0
};

static void my_printk(struct console *cons, const char *msg, unsigned int len)
{
	char c;
	while (len--) {
		c = *msg++;
		if (c == '\n' && ttyclicks_on & ttyclicks_kmsg)
			ttyclicks_notes(printk_sound);
	}
}				/* my_printk */

static struct console clickconsole = {
	.name = "tty clicks",
	.write = my_printk,
	.flags = CON_ENABLED,
};

/*
 * Now for something subtle.
 * I often hit caps lock by accident.  Don't we all?
 * But I can't tell the difference, until I've typed an entire paragraph
 * in upper case.
 * So it is helpful to hear a high beep every time a capital letter is echoed.
 * It's easy to test for upper case (at least in English).
 * but when does an output character represent an echo of an input character?
 * Don't count on tty cooked mode to tell you;
 * lots of editors put the tty in raw mode,
 * and then there's ssh (when you're working remotely), and so on.
 * No - we need a more general approach.
 * I watch the keystrokes as they come in, and give them time stamps.
 * If an output letter matches an input letter, and not too much time
 * has gone by, I call it an echo character.
 * It's not perfect, but nearly so.
 * That is the only reason to monitor keystrokes.
 * If I didn't want echo capital letters to sound different,
 * I wouldn't need a keyboard notifier at all.
 * Retain up to 16 keystrokes with time stamps in jiffies
 * in a circular buffer with head and tail pointers.
 * kb notifier puts keys on at the head,
 * and vt notifier takes keys off at the tail,
 * matching them against vt output and checking for echo.
 */

#define ECHOEXPIRE 3		/* in seconds */
#define ECHOMAX 16

static struct {
	char inkey;
	unsigned long intime;
} echochars[ECHOMAX];
static int echead, ectail;

/* char is displayed on screen; is it echo? */
static int charIsEcho(char c)
{
	int j = ectail;
	char d;

	while (j != echead) {
		if ((long)jiffies - (long)echochars[j].intime > HZ * ECHOEXPIRE) {
/* this keystroke too old, discard it */
			if (++j == ECHOMAX)
				j = 0;
			ectail = j;
			continue;
		}
/* This one has to match, or other output is getting in the way. */
		d = echochars[j].inkey;
		if (++j == ECHOMAX)
			j = 0;
		if (d == c) {
			ectail = j;
			return 1;
		}
	}

	ectail = echead;
	return 0;
}				/* charIsEcho */

static int
keystroke(struct notifier_block *this_nb, unsigned long type, void *data)
{
	struct keyboard_notifier_param *param = data;
	char key = param->value;
	int downflag = param->down;
	int shiftstate = param->shift;
	int j;

	/* if no sounds, then no need to do anything. */
	/* Also, no raw, no control keys, and only down events. */
	if (!(ttyclicks_on & ttyclicks_tty) ||
	type != KBD_KEYSYM ||
	param->vc->vc_mode == KD_GRAPHICS ||
	downflag == 0 ||
	key == 0 ||
	shiftstate & 0xe)
		return NOTIFY_DONE;

	j = echead;
	if (++j == ECHOMAX)
		j = 0;
/* typing ahead, is there room for this key in the echo buffer? */
	if (j != ectail) {
		echochars[echead].inkey = key;
		echochars[echead].intime = jiffies;
		echead = j;
	}

	return NOTIFY_DONE;
}				/* keystroke */

/*
 * Here's the deal about notifier priority.
 * All this module does is make clicks as characters appear on screen.
 * Any other module is probably going to be more important.
 * In fact other modules may want to eat events before this one
 * makes its noises. So give this module a low priority.
 * 0 is default, so select a negative number.
 */

static struct notifier_block nb_key = {
	.notifier_call = keystroke,
	.priority = -70
};

/* Here are some routines to click the speaker, or make tones, etc. */
/* In each case ttyclicks_on is the master switch. */

/* intervals measured in microseconds */
#define TICKS_CLICK 600
#define TICKS_CHARWAIT 4000

/* Use the global PIT lock ! */

/* Toggle the speaker, but not if a tone is sounding */
static void speaker_toggle(void)
{
	char c;
	unsigned long flags;

	raw_spin_lock_irqsave(&i8253_lock, flags);
	c = inb_p(0x61);
	if ((c & 3) != 3) {
		c &= 0xfe;
		c ^= 2;		/* toggle */
		outb(c, 0x61);
	}
	raw_spin_unlock_irqrestore(&i8253_lock, flags);
}

static void speaker_sing(unsigned int freq)
{
	unsigned long flags;
	raw_spin_lock_irqsave(&i8253_lock, flags);
	if (freq) {
		unsigned int count = PIT_TICK_RATE / freq;
		/* set command for counter 2, 2 byte write */
		outb_p(0xB6, 0x43);
		/* select desired HZ */
		outb_p(count & 0xff, 0x42);
		outb((count >> 8) & 0xff, 0x42);
		/* enable counter 2 */
		outb_p(inb_p(0x61) | 3, 0x61);
	} else {
		/* disable counter 2 */
		outb(inb_p(0x61) & 0xFC, 0x61);
	}
	raw_spin_unlock_irqrestore(&i8253_lock, flags);
}

static void my_mksteps(int f1, int f2, int step, int duration);

#endif

/* the sound of a character click */
void ttyclicks_click(void)
{
#ifndef NOCLICKS
	if (!ttyclicks_on)
		return;
	speaker_toggle();
	udelay(TICKS_CLICK);
	speaker_toggle();
#endif
}				/* ttyclicks_click */
EXPORT_SYMBOL_GPL(ttyclicks_click);

void ttyclicks_cr(void)
{
#ifndef NOCLICKS
	if (!ttyclicks_on)
		return;
	my_mksteps(2900, 3600, 10, 10);
#endif
}				/* ttyclicks_cr */
EXPORT_SYMBOL_GPL(ttyclicks_cr);

#ifndef NOCLICKS

/*
 * Push notes onto a sound fifo and play them via an asynchronous thread.
 * This is particularly helpful when the adapter is not working,
 * for whatever reason.  These functions are central to the kernel,
 * and do not depend on sound cards, loadable modules, etc.
 * These notes can also alert a system administrator to conditions
 * that warrant immediate attention.
 * Each note is specified by 2 shorts.  The first is the frequency in hurtz,
 * and the second is the duration in hundredths of a second.
 * A frequency of -1 is a rest.
 * A frequency of 0 ends the list of notes.
 */

#define SF_LEN 64		/* length of sound fifo */
static short sf_fifo[SF_LEN];
static int sf_head, sf_tail;
static DEFINE_RAW_SPINLOCK(soundfifo_lock);

/* Pop the next sound out of the sound fifo. */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,15,1)
typedef unsigned long TimerCallbackArg;
#else
typedef struct timer_list * TimerCallbackArg;
#endif
static void pop_soundfifo(TimerCallbackArg);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,15,1)
static DEFINE_TIMER(kd_mknotes_timer, pop_soundfifo, 0, 0);
#else
static DEFINE_TIMER(kd_mknotes_timer, pop_soundfifo);
#endif

static void pop_soundfifo(TimerCallbackArg notUsed)
{
	unsigned long flags;
	int freq, duration;
	int i;
	long jifpause;

	raw_spin_lock_irqsave(&soundfifo_lock, flags);

	i = sf_tail;
	if (i == sf_head) {
		freq = 0;
		duration = 0;
	} else {
		freq = sf_fifo[i];
		duration = sf_fifo[i + 1];
		i += 2;
		if (i == SF_LEN)
			i = 0;
		sf_tail = i;
	}

	raw_spin_unlock_irqrestore(&soundfifo_lock, flags);

	if (freq == 0) {
		/* turn off singing speaker */
		speaker_sing(0);
		del_timer(&kd_mknotes_timer);
		return;
	}

	jifpause = msecs_to_jiffies(duration);
	/* not sure of the rounding, if duration < HZ */
	if (jifpause == 0)
		jifpause = 1;
	mod_timer(&kd_mknotes_timer, jiffies + jifpause);

	if (freq < 0) {
		/* This is a rest between notes */
		speaker_sing(0);
	} else {
		speaker_sing(freq);
	}
}

/* Push a string of notes into the sound fifo. */
static void my_mknotes(const short *p)
{
	int i;
	bool wake = false;
	unsigned long flags;

	if (*p == 0)
		return;		/* empty list */

	raw_spin_lock_irqsave(&soundfifo_lock, flags);

	i = sf_head;
	if (i == sf_tail)
		wake = true;

	/* Copy shorts into the fifo, until the terminating zero. */
	while (*p) {
		sf_fifo[i++] = *p++;
		sf_fifo[i++] = (*p++) * 10;
		if (i == SF_LEN)
			i = 0;	/* wrap around */
		if (i == sf_tail) {
			/* fifo is full */
			goto done;
		}
		sf_head = i;
	}

	/* try to add on a rest, to carry the last note through */
	sf_fifo[i++] = -1;
	sf_fifo[i++] = 10;
	if (i == SF_LEN)
		i = 0;		/* wrap around */
	if (i != sf_tail)
		sf_head = i;

done:
	raw_spin_unlock_irqrestore(&soundfifo_lock, flags);

	/* first sound,  get things started. */
	if (wake)
		pop_soundfifo(0);
}

/* Push an ascending or descending sequence of notes into the sound fifo.
 * Step is a geometric factor on frequency, increase by x percent.
 * 100% goes up by octaves, -50% goes down by octaves.
 * 12% is a wholetone scale, while 6% is a chromatic scale.
 * Duration is in milliseconds, for very fast frequency sweeps.  But this
 * is based on jiffies timing, so is subject to the resolution of HZ. */
static void my_mksteps(int f1, int f2, int step, int duration)
{
	int i;
	bool wake = false;
	unsigned long flags;

	/* are the parameters in range? */
	if (step != (char)step)
		return;
	if (duration <= 0 || duration > 2000)
		return;
	if (f1 < 50 || f1 > 8000)
		return;
	if (f2 < 50 || f2 > 8000)
		return;

	/* avoid infinite loops */
	if (step == 0 || (f1 < f2 && step < 0) || (f1 > f2 && step > 0))
		return;

	raw_spin_lock_irqsave(&soundfifo_lock, flags);

	i = sf_head;
	if (i == sf_tail)
		wake = true;

	/* Copy shorts into the fifo, until start reaches end */
	while ((step > 0 && f1 < f2) || (step < 0 && f1 > f2)) {
		sf_fifo[i++] = f1;
		sf_fifo[i++] = duration;
		if (i == SF_LEN)
			i = 0;	/* wrap around */
		if (i == sf_tail) {
			/* fifo is full */
			goto done;
		}
		sf_head = i;
		f1 = f1 * (100 + step) / 100;
		if (f1 < 50 || f1 > 8000)
			break;
	}

	/* try to add on a rest, to carry the last note through */
	sf_fifo[i++] = -1;
	sf_fifo[i++] = 10;
	if (i == SF_LEN)
		i = 0;		/* wrap around */
	if (i != sf_tail)
		sf_head = i;

done:
	raw_spin_unlock_irqrestore(&soundfifo_lock, flags);

	/* first sound,  get things started. */
	if (wake)
		pop_soundfifo(0);
}

#endif

/* Put a string of notes into the sound fifo. */
void ttyclicks_notes(const short *p)
{
#ifndef NOCLICKS
	if (!ttyclicks_on)
		return;
	my_mknotes(p);
#endif
}				/* ttyclicks_notes */
EXPORT_SYMBOL_GPL(ttyclicks_notes);

void ttyclicks_steps(int f1, int f2, int step, int duration)
{
#ifndef NOCLICKS
	if (!ttyclicks_on)
		return;
	my_mksteps(f1, f2, step, duration);
#endif
}				/* ttyclicks_steps */
EXPORT_SYMBOL_GPL(ttyclicks_steps);

void ttyclicks_bell(void)
{
#ifndef NOCLICKS
	static const short notes[] = {
		1800, 10, 0, 0
	};
	ttyclicks_notes(notes);
#endif
}				/* ttyclicks_bell */
EXPORT_SYMBOL_GPL(ttyclicks_bell);

#ifndef NOCLICKS

/* Returns a time in microseconds to wait. */
static int soundFromChar(char c, int minor)
{
	static const short capnotes[] = {
		3000, 3, 0, 0
	};

/* are sounds disabled? */
	if (!ttyclicks_on)
		return 0;

	if (c == '\07') {
		ttyclicks_bell();
		return 0;
	}

	if (!ttyclicks_tty)
		return 0;

/* Don't click for background screens */
	if (minor != fg_console + 1)
		return 0;

	if (c == '\n') {
		ttyclicks_cr();
		return 0;
	}

	if (charIsEcho(c) && c >= 'A' && c <= 'Z') {
		ttyclicks_notes(capnotes);
		return TICKS_CHARWAIT;
	}

/* Treat a nonprintable characterlike a space; just pause. */
	if (c >= 0 && c <= ' ')
		return TICKS_CHARWAIT;

/* regular printable character */
	ttyclicks_click();
	return TICKS_CHARWAIT - TICKS_CLICK;
}				/* soundFromChar */

/* Get char from the console, and make the sound. */
static int
vt_out(struct notifier_block *this_nb, unsigned long type, void *data)
{
	struct vt_notifier_param *param = data;
	struct vc_data *vc = param->vc;
	int minor = vc->vc_num + 1;
	int msecs = 0, usecs = 0;
	long jifpause;
	int unicode = param->c;
	char c = param->c;

	if (type != VT_PREWRITE)
		goto done;

	if (param->vc->vc_mode == KD_GRAPHICS)
		goto done;
/* Sorry, n-tilde doesn't click, for now. */
	if (unicode >= 128) {
		escState = 0;
		goto done;
	}

	if (escState == 2) {
		if (isalpha(c)) {
			/* a letter indicates end of escape sequence. */
			escState = 0;
			if(c == 'H')
				ttyclicks_cr();
		}
		goto done;
	}

	if (escState == 1) {
		if (c == '[') {
			escState = 2;
			goto done;
		}
/* Anything else closes the sequence, that's wrong, but will do for now. */
		if(c != '\33')
			escState = 0;
		goto done;
	} else if (c == '\33' && !cursormotion) {
		escState = 1;
		goto done;
	}

	if (!isdigit(c) && c != '?' && c != '#' && c != ';')
		escState = 0;

	usecs = soundFromChar(c, minor);

/*
 * If it's the bell, I make the beep, not the console.
 * This is the only char that this module eats.
 */

	if (c == 7)
		return NOTIFY_STOP;

	if (!usecs)
		goto done;

/*
Tell the process to sleep.
Can't suspend in the middle of a notifier.
Best approach is to return NOTIFY_SLEEP, but that isn't implemented yet.
*/

	msecs = (usecs + 800) / 1000;
	jifpause = msecs_to_jiffies(msecs);
/* jifpause should always be positive, but let's make sure */
	if (sleep && jifpause > 0) {
		/*
		 * This magical line of code only works if you edit vt.c
		 * and recompile the kernel.
		 * A patch is available from the acsint project.
		 * Again, NOTIFY_SLEEP is a better solution.
		 */
		param->c = 0xac97;
	} else {
		udelay(usecs);
	}

done:
	return NOTIFY_DONE;
}				/* vt_out */

static struct notifier_block nb_vt = {
	.notifier_call = vt_out,
	.priority = -70
};

#endif

/* Load and unload the module. */

static int __init click_init(void)
{
	int rc = 0;

	ttyclicks_on = enabled;
	ttyclicks_tty = fgtty;
	ttyclicks_kmsg = kmsg;

#ifndef NOCLICKS
	rc = register_vt_notifier(&nb_vt);
	if (rc)
		return rc;

	rc = register_keyboard_notifier(&nb_key);
	if (rc) {
		unregister_vt_notifier(&nb_vt);
		return rc;
	}

	register_console(&clickconsole);
#endif

	return rc;
}				/* click_init */

static void __exit click_exit(void)
{
	ttyclicks_on = 0;

#ifndef NOCLICKS
	unregister_console(&clickconsole);
	unregister_keyboard_notifier(&nb_key);
	unregister_vt_notifier(&nb_vt);

/* possible race conditions here with timers hanging around */
	sf_head = sf_tail = 0;
	pop_soundfifo(0);
#endif
}				/* click_exit */

module_init(click_init);
module_exit(click_exit);
