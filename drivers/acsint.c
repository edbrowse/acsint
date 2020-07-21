/* Acsint - Accessibility intercepter.
 * Originally written by Saqib Shaikh in 2005.
 * Modified by Karl Dahlke in 2011 to use notifiers.
 */

#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/kbd_kern.h>
#include <linux/ctype.h>
#include <linux/slab.h>		/* malloc and free */
#include <linux/vt.h>
#include <linux/vt_kern.h>	/* for fg_console */
#include <linux/console.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/tty_flip.h>
#include <linux/miscdevice.h>
#include <linux/version.h>
#include <linux/poll.h>

#include "ttyclicks.h"
#include "acsint.h"

#define ACS_DEVICE "/dev/acsint"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Karl Dahlke - eklhad@gmail.com");
MODULE_DESCRIPTION
    ("Accessibility intercepter - pass keystroke and tty events to user space");

static int major;
module_param(major, int, 0);
MODULE_PARM_DESC(major,
		 "major number for /dev/acsint, default is dynamic allocation through misc_register");

/* For various critical sections of code. */
static DEFINE_RAW_SPINLOCK(acslock);

/* circular buffer of output characters received from the tty */
struct cbuf {
	unsigned int area[65536];
	unsigned int *start, *end;
	unsigned int *head, *tail;
/* mark the place where we last copied data to user space */
	unsigned int *mark;
/* Mark the point where we last saw an echo character */
	unsigned int *echopoint;
};

/* These are allocated, one per console, as needed. */
static struct cbuf *cbuf_tty[MAX_NR_CONSOLES];

/* in case we can't malloc a buffer */
static const char cb_nomem_message[] =
    "Kernel cannot allocate space for this console";

/* set to 1 if you have sent the above nomem message down to the user */
static unsigned char cb_nomem_refresh[MAX_NR_CONSOLES];

/* set to 1 if you have tried to allocate */
static unsigned char cb_nomem_alloc[MAX_NR_CONSOLES];

/* Staging area to copy tty data down to user space */
/* This is a snapshot of the circular buffer. */
static unsigned int cb_staging[65536];

/* size of userland buffer; characters will copy from staging to this buffer */
static int user_bufsize = 256;

/* jiffies value for the last output character. */
/* This is reset if the last output character is echo. */
static unsigned long last_oj;
/* How many tenths of a second separate one burst of output from the next? */
static int outputbreak = 5;

/* Initialize / reset the variables in the circular buffer. */
static void cb_reset(struct cbuf *cb)
{
	if (!cb)
		return;		/* never allocated */
	cb->start = cb->area;
	cb->end = cb->area + 65536;
	cb->head = cb->start;
	cb->tail = cb->start;
	cb->mark = cb->start;
	cb->echopoint = 0;
}

/* check to see if the circular buffer was allocated. */
/* If never attempted, try to allocate it. */
/* mino is minor-1, a 0 based index into arrays, similar to fg_console. */
static void checkAlloc(int mino, bool from_vt)
{
	struct cbuf *cb = cbuf_tty[mino];
	if (cb)
		return;		/* already allocated */
	if (cb_nomem_alloc[mino])
		return;		/* already tried to allocate */
	cb_nomem_alloc[mino] = 1;
	cb = kmalloc(sizeof(*cb), (from_vt ? GFP_ATOMIC : GFP_KERNEL));
	if (!cb) {
		printk(KERN_ERR "Failed to allocate memory for console %d.\n",
		       mino + 1);
		return;
	}
	cb_reset(cb);
	cbuf_tty[mino] = cb;
}

/* Put a character on the end of the circular buffer.
 * Drop the oldest character if the buffer is full.
 * This is called under a spinlock, so we don't have to worry about the reader
 * draining characters while this routine adds characters on. */
static void cb_append(struct cbuf *cb, unsigned int c)
{
	if (!cb)
		return;		/* should never happen */
	*cb->head = c;
	++cb->head;
	if (cb->head == cb->end)
		cb->head = cb->start;
	if (cb->head == cb->tail) {
		/* buffer full, drop the last character */
		if (cb->tail == cb->mark)
			cb->mark = 0;
		if (cb->tail == cb->echopoint)
			cb->echopoint = 0;
		++cb->tail;
		if (cb->tail == cb->end)
			cb->tail = cb->start;
	}
}

/* Indicate which keys, by key code, are meta.  For example,
 * shift, alt, numlock, etc.  These are the state changing keys.
 * Also flag the simulated shift states, on or off, for shift,
 * ralt, control, lalt.
 * I don't do the bookkeeping for the standar keys, the kernel does that.
 * I keep track for any other keys you might equate with shift or alt etc. */

#define ACS_SS_KERNEL 0x20 /* states handled by the kernel */

static unsigned char ismeta[ACS_NUM_KEYS];
static unsigned char metaflag[4];

static void
reset_meta(void)
{
	int j;

	for (j = 0; j < ACS_NUM_KEYS; ++j)
		ismeta[j] = 0;
/* These all have to be less than ACS_NUM_KEYS */
	ismeta[KEY_LEFTCTRL] = ACS_SS_KERNEL;
	ismeta[KEY_RIGHTCTRL] = ACS_SS_KERNEL;
	ismeta[KEY_LEFTSHIFT] = ACS_SS_KERNEL;
	ismeta[KEY_RIGHTSHIFT] = ACS_SS_KERNEL;
	ismeta[KEY_LEFTALT] = ACS_SS_KERNEL;
	ismeta[KEY_RIGHTALT] = ACS_SS_KERNEL;
	ismeta[KEY_CAPSLOCK] = ACS_SS_KERNEL;
	ismeta[KEY_NUMLOCK] = ACS_SS_KERNEL;
	ismeta[KEY_SCROLLLOCK] = ACS_SS_KERNEL;

	for(j=0; j<4; ++j)
		metaflag[j] = 0;
} /* reset_meta */


/* Indicate which keys should be captured by your running adapter.
 * This is an unsigned short, with a bit for each shift alt control combination.
 * This includes the first bit, which corresponds to a shift state of 0,
 * or the plain key.  You want that for function keys etc,
 * but probably not for letters on the main keyboard.
 * Those should always pass through to the console.
 * But I don't place any restrictions on what is intercepted,
 * so do whatever you like. */

static unsigned short capture[ACS_NUM_KEYS];

/* If a key is captured, it can still be passed through to the console. */

static unsigned short passt[ACS_NUM_KEYS];

static void clear_keys(void)
{
	int i;
	for (i = 0; i < ACS_NUM_KEYS; i++)
		capture[i] = passt[i] = 0;
}

/* divert all keys to user space, to grab the next key or build a string. */
static bool key_divert;

/* Monitor keystrokes that are passed to the console, by sending
 * them to user space as well. This is like /usr/bin/tee. */
static bool key_monitor;

/* pass the next key through to the console. */
static bool key_bypass;

/* The array "rbuf" is used for passing key/tty events to user space.
 * A reading buffer of sorts.  See device_read() below.
 * Despite the names head and tail, it's not a true circular buffer.
 * The process has to read the data before rbuf_head reaches rbuf_end,
 * or data is lost.
 * That's not a problem, because you just can't type faster
 * than the daemon can gather up those keystrokes.
 * Every event is 4 bytes, except echo, which is 8.
 * Thus everything stays 4 byte aligned.
 * This is necessary to pass down unicodes.
 */

#define RBUF_LEN 400
static char rbuf[RBUF_LEN];
static const char *rbuf_end = rbuf + RBUF_LEN;
static char *rbuf_tail, *rbuf_head;

/* Wait until this driver has some data to read. */
DECLARE_WAIT_QUEUE_HEAD(wq);

static bool in_use;		/* only one process opens this device at a time */
static int last_fgc;		/* last fg_console */

/* Push characters onto the input queue of the foreground tty.
 * This is for macros, or cut&paste. */
static void tty_pushstring(const char *cp, int len)
{
	struct vc_data *d = vc_cons[fg_console].d;

	if (!d)
		return;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 11)
	tty_insert_flip_string(d->port.tty, cp, len);
	tty_flip_buffer_push(d->port.tty);
#else
	tty_insert_flip_string(&d->port, cp, len);
	tty_flip_buffer_push(&d->port);
#endif
}				/* tty_pushstring */

/* File operations for /dev/acsint. */

static int device_open(struct inode *inode, struct file *file)
{
	int j;

/* A theoretical race condition here; too unlikely for me to worry about. */
	if (in_use)
		return -EBUSY;

	for (j = 0; j < MAX_NR_CONSOLES; ++j) {
		cb_reset(cbuf_tty[j]);
		cb_nomem_refresh[j] = 0;
		cb_nomem_alloc[j] = 0;
	}

	reset_meta();
	clear_keys();
	key_divert = false;
	key_monitor = false;
	key_bypass = false;

/* At startup we tell the process which virtual console it is on.
 * Place this directive in rbuf to be read. */
	rbuf[0] = ACS_FGC;
	rbuf[1] = fg_console + 1;	/* minor number */
	rbuf_tail = rbuf;
	rbuf_head = rbuf + 4;
	last_fgc = fg_console;
	checkAlloc(fg_console, false);

	in_use = true;
	return 0;
}

static int device_close(struct inode *inode, struct file *file)
{
	in_use = false;
	rbuf_head = rbuf_tail = rbuf;
	return 0;
}

static ssize_t device_read(struct file *file, char *buf, size_t len,
			   loff_t * offset)
{
	int bytes_read = 0;
	struct cbuf *cb;
	bool catchup;
	bool catchup_head, catchup_echo;
/* catch up length - how many characters to copy down to user space */
	int culen = 0;
	unsigned int *cup = 0;	/* the catchup poin */
	char *temp_head, *temp_tail, *t;
	int j, j2;
	int retval;
	unsigned long irqflags;

	if (!in_use)
		return 0;	/* should never happen */

	retval = wait_event_interruptible(wq, (rbuf_head > rbuf_tail));
	if (retval)
		return retval;

/* you can only read on behalf of the foreground console */
	cb = cbuf_tty[fg_console];

/* Use temp pointers, more keystrokes could be appended while
 * we're doing this; that's ok. */
	temp_head = rbuf_head;
	temp_tail = rbuf_tail;

/* Skip ahead to the last FGC event if present. */
	for (t = temp_tail; t < temp_head; t += 4) {
		if (*t == ACS_FGC)
			temp_tail = t;
		if (*t == ACS_TTY_MORECHARS)
			t += 4;
	}

	raw_spin_lock_irqsave(&acslock, irqflags);

	catchup = false;
	catchup_head = false;
	catchup_echo = false;

	if ((!cb && !cb_nomem_refresh[fg_console]) ||
	    (cb && cb->head != cb->mark)) {
		/* MORECHARS echo 0 doesn't force us to catch up,
		 * but anything else does.
		 * echo forces a catch up to the echopoint.
		 * Other commands force catch up to the head. */
		for (t = temp_tail; t < temp_head; t += 4) {
			if (*t == ACS_TTY_MORECHARS) {
				t += 4;
				if (t[-3])
					catchup_echo = true;
				continue;
			}
			catchup_head = true;
			break;
		}
	}

	if (catchup_echo && cb->echopoint)
		catchup = true, cup = cb->echopoint;

	if (catchup_head)
		catchup = true, cup = cb->head;

	if (catchup) {
		if (cb) {
			if (cb->mark == 0)
				cb->mark = cb->tail;
			if (cup >= cb->mark)
				culen = cup - cb->mark;
			else
				culen =
				    (cb->end - cb->mark) + (cup - cb->start);
		} else {
			culen = sizeof(cb_nomem_message) - 1;
		}

		if (cb) {
			/* One chunk or two. */
			if (cup >= cb->mark) {
				if (culen)
					memcpy(cb_staging, cb->mark, culen * 4);
			} else {
				j = cb->end - cb->mark;
				memcpy(cb_staging, cb->mark, j * 4);
				j2 = cup - cb->start;
				if (j2)
					memcpy(cb_staging + j, cb->start,
					       j2 * 4);
			}
			cb->mark = cup;
			cb->echopoint = 0;
		} else {
			for (j = 0; j < culen; ++j)
				cb_staging[j] = cb_nomem_message[j];
			cb_nomem_refresh[fg_console] = 1;
		}
	}

	raw_spin_unlock_irqrestore(&acslock, irqflags);

/* Now pass down the events. */
/* First fgc, then catch up, then the rest. */
	if (*temp_tail == ACS_FGC && len >= 4) {
		if (copy_to_user(buf, temp_tail, 4))
			return -EFAULT;
		temp_tail += 4;
		bytes_read += 4;
		buf += 4;
		len -= 4;
	}

	if (catchup) {
		cup = cb_staging;
/* ratchet culen down to the size of the userland buffer */
		if (culen > user_bufsize) {
			j = culen - user_bufsize;
			cup += j, culen -= j;
		}
	}

	if (catchup && len >= (culen + 1) * 4) {
		char cu_cmd[4];	/* the catch up command */
		cu_cmd[0] = ACS_TTY_NEWCHARS;
/* Put in the minor number here, though I don't think we need it. */
		cu_cmd[1] = fg_console + 1;
		cu_cmd[2] = culen;
		cu_cmd[3] = (culen >> 8);
		if (copy_to_user(buf, cu_cmd, 4))
			return -EFAULT;

		if (culen && copy_to_user(buf + 4, cup, culen * 4))
			return -EFAULT;
		bytes_read += (culen + 1) * 4;
		buf += (culen + 1) * 4;
		len -= (culen + 1) * 4;
	}

/* And the rest of the events. */
	j = temp_head - temp_tail;
	if (j > len)
		j = len;	/* should never happen */
	if (j) {
		if (copy_to_user(buf, temp_tail, j))
			return -EFAULT;
		temp_tail += j;
		buf += j;
		bytes_read += j;
		len -= j;
	}

/* Pull the pointers back to start. */
/* This should happen almost every time. */
	raw_spin_lock_irqsave(&acslock, irqflags);
	rbuf_tail = temp_tail;
	if (rbuf_head == rbuf_tail)
		rbuf_head = rbuf_tail = rbuf;
	raw_spin_unlock_irqrestore(&acslock, irqflags);

	*offset += bytes_read;
	return bytes_read;
}				/* device_read */

static ssize_t device_write(struct file *file, const char *buf, size_t len,
			    loff_t * offset)
{
	char c;
	const char *p = buf;
	int j, key, shiftstate, teebit, bytes_write;
	int nn;			/* number of notes */
	short notes[2 * (10 + 1)];
	int isize;		/* size of input to inject */
	int f1, f2, step, duration;	/* for kd_mksteps */
	unsigned long irqflags;

	if (!in_use)
		return 0;	/* should never happen */

	while (len) {
		get_user(c, p++);
		len--;

		switch (c) {
		case ACS_CLEAR_KEYS:
			clear_keys();
			reset_meta();
			break;

		case ACS_SET_KEY:
			if (len < 2)
				break;
			get_user(key, p++);
			key = (unsigned char)key;
			len--;
			get_user(shiftstate, p++);
			len--;
			if (key < ACS_NUM_KEYS) {
				teebit = (shiftstate & ACS_KEY_T);
				shiftstate &= 0xf;
				capture[key] |=
				    (1 << shiftstate);
				if(teebit)
					passt[key] |=
					    (1 << shiftstate);
				else
					passt[key] &=
					    ~(1 << shiftstate);
			}
			break;

		case ACS_UNSET_KEY:
			if (len < 2)
				break;
			get_user(key, p++);
			key = (unsigned char)key;
			len--;
			get_user(shiftstate, p++);
			len--;
			if (key < ACS_NUM_KEYS) {
				passt[key] = 0;
				shiftstate &= 0xf;
				capture[key] &=
				    ~((unsigned short)1 << shiftstate);
			}
			break;

		case ACS_ISMETA:
			if (len < 2)
				break;
			get_user(key, p++);
			key = (unsigned char)key;
			len--;
			get_user(c, p++);
			len--;
			if (key < ACS_NUM_KEYS)
				ismeta[key] = (unsigned char)c;
			break;

		case ACS_CLICK:
			ttyclicks_click();
			break;

		case ACS_CR:
			ttyclicks_cr();
			break;

		case ACS_SOUNDS:
			if (len < 1)
				break;
			get_user(c, p++);
			len--;
			ttyclicks_on = c;
			break;

		case ACS_SOUNDS_TTY:
			if (len < 1)
				break;
			get_user(c, p++);
			len--;
			ttyclicks_tty = c;
			break;

		case ACS_SOUNDS_KMSG:
			if (len < 1)
				break;
			get_user(c, p++);
			len--;
			ttyclicks_kmsg = c;
			break;

		case ACS_NOTES:
			if (len < 1)
				break;
			get_user(nn, p++);
			len--;
			for (j = 0; j < nn && j < 10 && len >= 3; ++j, len -= 3) {
				get_user(c, p++);
				notes[2 * j] = (unsigned char)c;
				get_user(c, p++);
				notes[2 * j] |= ((short)c << 8);
				get_user(c, p++);
				notes[2 * j + 1] = (unsigned char)c;
			}
			notes[2 * j] = 0;
			if (j)
				ttyclicks_notes(notes);
			for (; j < nn && len >= 3; ++j, len -= 3)
				p += 3;
			break;

		case ACS_BYPASS:
			key_bypass = true;
			break;

		case ACS_DIVERT:
			if (len < 1)
				break;
			get_user(c, p++);
			len--;
			key_divert = (c != 0);
			break;

		case ACS_MONITOR:
			if (len < 1)
				break;
			get_user(c, p++);
			len--;
			key_monitor = (c != 0);
			break;

		case ACS_OBREAK:
			if (len < 1)
				break;
			get_user(c, p++);
			len--;
			outputbreak = (unsigned char)c;
			break;

		case ACS_SWOOP:
			if (len < 3)
				break;
			/* not yet implemented */
			len -= 3;
			break;

		case ACS_STEPS:
			if (len < 7)
				break;
			get_user(step, p++);
			len--;
			get_user(c, p++);
			f1 = (unsigned char)c;
			get_user(c, p++);
			f1 |= ((int)(unsigned char)c << 8);
			len -= 2;
			get_user(c, p++);
			f2 = (unsigned char)c;
			get_user(c, p++);
			f2 |= ((int)(unsigned char)c << 8);
			len -= 2;
			get_user(c, p++);
			duration = (unsigned char)c;
			get_user(c, p++);
			duration |= ((int)(unsigned char)c << 8);
			len -= 2;
			ttyclicks_steps(f1, f2, step, duration);
			break;

		case ACS_REFRESH:
			raw_spin_lock_irqsave(&acslock, irqflags);
			if (rbuf_head <= rbuf_end - 4) {
				bool wake = (rbuf_head == rbuf_tail);
				*rbuf_head = ACS_REFRESH;
				rbuf_head += 4;
				if (wake)
					wake_up_interruptible(&wq);
			}
			raw_spin_unlock_irqrestore(&acslock, irqflags);
			break;

		case ACS_PUSH_TTY:
			if (len < 2)
				break;
			get_user(c, p++);
			isize = (unsigned char)c;
			get_user(c, p++);
			isize |= ((int)(unsigned char)c << 8);
			len -= 2;
			if (len < isize)
				break;
			tty_pushstring(p, isize);
			p += isize, len -= isize;
			break;

		case ACS_BUFSIZE:
			if (len < 2)
				break;
			get_user(c, p++);
			isize = (unsigned char)c;
			get_user(c, p++);
			isize |= ((int)(unsigned char)c << 8);
			len -= 2;
			if (isize < 256)
				isize = 256;
			if (isize >= 65536)
				isize = 65535;
			user_bufsize = isize;
			break;

		}		/* switch */
	}			/* loop processing config instructions */

	bytes_write = p - buf;
	*offset += bytes_write;
	return bytes_write;
}				/* device_write */

static unsigned int device_poll(struct file *fp, poll_table * pt)
{
	unsigned int mask = 0;
	if (!in_use)
		return 0;	/* should never happen */
/* we don't support poll writing. How to figure if the buffer is not full? */
	if (rbuf_head > rbuf_tail)
		mask = POLLIN | POLLRDNORM;
	poll_wait(fp, &wq, pt);
	return mask;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = device_open,
	.release = device_close,
	.read = device_read,
	.write = device_write,
	.poll = device_poll,
};

static struct miscdevice acsint_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "acsint",
	.fops = &fops,
};

/* Variables to remember the entered keystrokes, to watch for echo.
 * If these keys reappear on screen in a timely fashion, without other output,
 * they are considered echo chars. */

#define MAXKEYPENDING 8
struct keyhold {
	unsigned int unicode;
	unsigned long when;	/* in jiffies */
	int keytype;		/* code or sym or unicode */
};
static struct keyhold keystack[MAXKEYPENDING];
static short nkeypending;	/* number of keys pending */

/* Key echo states:
 * 0 nothing special
 * 1 tab or ^i match space, more spaces coming
* 2 return matches cr, lf coming back
 * 3 escape matches ^[
 * 4 delete or ^h matches ^h, space is next
 * 5 ^h expected, move back to 0
*/
static short keyechostate;
#define flushInKeyBuffer() (nkeypending = keyechostate = 0)
#define ECHOEXPIRE 3		/* in seconds */

static void dropKeysPending(int mark)
{
	int i, j;
	for (i = 0, j = mark; j < nkeypending; ++i, ++j)
		keystack[i] = keystack[j];
	nkeypending -= mark;
	if (!nkeypending)
		flushInKeyBuffer();
}				/* dropKeysPending */

/* char is displayed on screen; is it echo? */
/* This is run from within a spinlock */
static int isEcho(unsigned int c)
{
	unsigned int d;
	int j;

/* when echo is based only on states */
	if (keyechostate == 1 && c == ' ')
		return 2;
	if (keyechostate == 2 && c == '\n') {
		keyechostate = 0;
		return 2;
	}
	if (keyechostate == 3 && c == '[') {
		keyechostate = 0;
		return 2;
	}
	if (keyechostate == 4 && c == ' ') {
		keyechostate = 5;
		return 2;
	}
	if (keyechostate == 5 && c == '\b') {
		keyechostate = 0;
		return 2;
	}

	keyechostate = 0;
	if (!nkeypending)
		return 0;

/* drop old keys */
	for (j = 0; j < nkeypending; ++j)
		if ((long)jiffies - (long)keystack[j].when <= HZ * ECHOEXPIRE)
			break;
	if (j) {
		dropKeysPending(j);
		if (!nkeypending)
			return 0;
	}

/* we need to match on the first character */
	d = keystack[0].unicode;
	if (d == '\t' && c == ' ') {
		dropKeysPending(1);
		keyechostate = 1;
		return 2;
	}
	if ((d == '\r' || d == '\n') && c == '\r') {
		dropKeysPending(1);
		keyechostate = 2;
		return 1;
	}
	if (d == '\033' && c == '^') {
		dropKeysPending(1);
		keyechostate = 3;
		return 2;
	}
	if ((d == '\b' || d == 0x7f) && c == '\b') {
		dropKeysPending(1);
		keyechostate = 4;
		return 2;
	}

	if(d == c) {
		dropKeysPending(1);
		return 1;
	}

	flushInKeyBuffer();
	return 0;
}				/* isEcho */

/* Post a keystroke on to the pending log, to watch for echo.
 * This is based on key code and shift state.
 * I wanted to base it on KBD_UNICODE, but my system doesn't throw those events.
 * See the sample code in keystroke().
 * Meantime this will have to do.
 * But it assumes ascii, and a qwerty keyboard.
 * Let me know if there's a better way. */
static void post4echo(int keytype, struct keyboard_notifier_param *param)
{
	int key = param->value;
	int ss = param->shift & 0xf;
	int leds = param->ledstate;
	char keychar;
	unsigned long irqflags;
	struct keyhold *kp;	/* key pointer */

	static const char lowercode[] =
	    " \0331234567890-=\177\tqwertyuiop[]\r asdfghjkl;'` \\zxcvbnm,./    ";
	static const char uppercode[] =
	    " \033!@#$%^&*()_+\177\tQWERTYUIOP{}\r ASDFGHJKL:\"~ |ZXCVBNM<>?    ";

	if (keytype == KBD_UNICODE) {
		raw_spin_lock_irqsave(&acslock, irqflags);
/* display key that was pushed because of KEYCODE or KEYSYM */
		if (nkeypending
		    && keystack[nkeypending - 1].keytype != KBD_UNICODE)
			--nkeypending;
		if (nkeypending == MAXKEYPENDING)
			dropKeysPending(1);
		kp = keystack + nkeypending;
		kp->unicode = key;
		kp->when = jiffies;
		kp->keytype = keytype;
		++nkeypending;
		raw_spin_unlock_irqrestore(&acslock, irqflags);
		return;
	}

/* KEYSYM not yet implemented */
	if (keytype != KBD_KEYCODE)
		return;

	if (key == KEY_KPENTER)
		key = KEY_ENTER;

/* pull keycode down to numbers if numlock numpad keys are hit */
	if (leds & K_NUMLOCK && (ss & ACS_SS_ALT) == 0) {
		static const int padnumbers[] = {
			KEY_7, KEY_8, KEY_9, 0,
			KEY_4, KEY_5, KEY_6, 0,
			KEY_1, KEY_2, KEY_3, KEY_0
		};
		if (key == KEY_KPASTERISK)
			key = KEY_8, ss = ACS_SS_SHIFT;
		if (key == KEY_KPSLASH)
			key = KEY_SLASH, ss = 0;
		if (key == KEY_KPPLUS)
			key = KEY_EQUAL, ss = ACS_SS_SHIFT;
		if (key == KEY_KPMINUS)
			key = KEY_MINUS, ss = 0;
		if (key == KEY_KPDOT)
			key = KEY_DOT, ss = 0;
		if (key >= KEY_KP7 && key <= KEY_KP0)
			key = padnumbers[key - KEY_KP7], ss = 0;
	}

	if (key > KEY_SPACE)
		return;

	keychar = (ss & ACS_SS_SHIFT) ? uppercode[key] : lowercode[key];
	if (keychar == ' ' && key != KEY_SPACE)
		return;

	if (keychar == '\r')
		ss = 0;

/* don't know how to echo alt keys */
	if (ss & ACS_SS_ALT)
		return;

/* control letters */
	if (ss & ACS_SS_CTRL && isalpha(keychar))
		keychar = (keychar | 0x20) - ('a' - 1);

	if (leds & K_CAPSLOCK && isalpha(keychar))
		keychar ^= 0x20;

	raw_spin_lock_irqsave(&acslock, irqflags);
	if (nkeypending == MAXKEYPENDING)
		dropKeysPending(1);
	kp = keystack + nkeypending;
	kp->unicode = keychar;
	kp->when = jiffies;
	kp->keytype = keytype;
	++nkeypending;
	raw_spin_unlock_irqrestore(&acslock, irqflags);
}				/* post4echo */

/* Push a character onto the tty log.
 * Called from the vt notifyer and from my printk console. */
static void pushlog(unsigned int c, int mino, bool from_vt)
{
	unsigned long irqflags;
	bool wake = false;
	bool at_head = false;	/* output is at the head */
	bool throw = false;	/* throw the MORECHARS event */
	int echo = 0;
	struct cbuf *cb = cbuf_tty[mino];

	if (!cb)
		return;

	raw_spin_lock_irqsave(&acslock, irqflags);

	if (mino == fg_console) {
		if (from_vt)
			echo = isEcho(c);
		if (cb->mark == cb->head || cb->echopoint == cb->head)
			at_head = true;
		if (at_head || echo)
			throw = true;
		if (!echo) {
			if (last_oj && outputbreak &&
			    (long)jiffies - (long)last_oj <
			    HZ * outputbreak / 10)
				throw = false;
			last_oj = jiffies;
			if (last_oj == 0)
				last_oj = 1;
		}
	}

	cb_append(cb, c);

	if (throw && rbuf_head <= rbuf_end - 8) {
		/* throw the MORECHARS event */
		if (rbuf_head == rbuf_tail)
			wake = true;
		rbuf_head[0] = ACS_TTY_MORECHARS;
		rbuf_head[1] = echo;
			*(unsigned int *)(rbuf_head + 4) = c;
		rbuf_head += 8;
		if (echo)
			cb->echopoint = cb->head;
	}

	if (wake)
		wake_up_interruptible(&wq);
	raw_spin_unlock_irqrestore(&acslock, irqflags);
}				/* pushlog */

/*
 * We need a console to capture printk() text
 * and push it onto the buffer.
 * It didn't come from the tty, but we want to read it nonetheless.
 */

static void my_printk(struct console *cons, const char *msg, unsigned int len)
{
	char c;
	if (!in_use)
		return;
	while (len--) {
		c = *msg++;
		pushlog((unsigned char)c, fg_console, false);
	}
}

static struct console acsintconsole = {
	.name = "acsint",
	.write = my_printk,
	.flags = CON_ENABLED,
};

/* Notifiers: keyboard events and tty events. */

static int
vt_out(struct notifier_block *this_nb, unsigned long type, void *data)
{
	struct vt_notifier_param *param = data;
	struct vc_data *vc = param->vc;
	int mino = vc->vc_num;
	unsigned int unicode = param->c;
	unsigned long irqflags;
	bool wake = false;

	if (!in_use)
		return NOTIFY_DONE;

	if (param->vc->vc_mode == KD_GRAPHICS && type != VT_UPDATE)
		return NOTIFY_DONE;
	switch (type) {
	case VT_UPDATE:
		if (fg_console == last_fgc)
			break;	/* it's the same console */

		last_fgc = fg_console;
/* retry alloc on console switch */
		cb_nomem_alloc[fg_console] = 0;
		checkAlloc(fg_console, true);
		last_oj = 0;
		raw_spin_lock_irqsave(&acslock, irqflags);
		flushInKeyBuffer();
		if (rbuf_head <= rbuf_end - 4) {
			if (rbuf_head == rbuf_tail)
				wake = true;
			rbuf_head[0] = ACS_FGC;
			rbuf_head[1] = fg_console + 1;
			rbuf_head += 4;
			if (wake)
				wake_up_interruptible(&wq);
		}
		raw_spin_unlock_irqrestore(&acslock, irqflags);
		break;

	case VT_PREWRITE:
/* I don't log, or pass back, null bytes in the output stream. */
		if (unicode == 0)
			break;

		checkAlloc(mino, true);
		pushlog(unicode, mino, true);
	}			/* switch */

	return NOTIFY_DONE;
}				/* vt_out */

static struct notifier_block nb_vt = {
	.notifier_call = vt_out,
	.priority = 20
};

static int
keystroke(struct notifier_block *this_nb, unsigned long type, void *data)
{
	struct keyboard_notifier_param *param = data;
	unsigned int key = param->value;
	int downflag = param->down;
	int ss = param->shift;
	int mymeta, mymask;
	int j;
	unsigned short action;
	bool wake = false, keep = false, send = false;
	bool divert, monitor, bypass;
	unsigned long irqflags;

	if (!in_use)
		goto done;

	if (param->vc->vc_mode == KD_GRAPHICS)
		goto done;

/* post any unicode events for echo */
	if (type == KBD_UNICODE && downflag) {
		post4echo(KBD_UNICODE, param);
		goto done;
	}

	if (type != KBD_KEYCODE)
		goto done;

/* Capture and process keys that are meta, but not kernel meta */
	if(key < ACS_NUM_KEYS && (mymeta = ismeta[key]) && mymeta != ACS_SS_KERNEL) {
		mymask = 1;
		for(j=0; j<4; ++j, mymask<<=1)
			if(mymask & mymeta)
				metaflag[j] = (downflag != 0);
		return NOTIFY_STOP;
	}

/* Only the key down events */
	if (downflag == 0)
		goto done;

	ss &= 0xf;
/* Adjust by the user meta keys. */
	mymask = 1;
	for(j=0; j<4; ++j, mymask<<=1) {
		if(metaflag[j])
			ss |= mymask;
	}

	action = 0;
	if (key < ACS_NUM_KEYS)
		action = capture[key];

	divert = key_divert;
	monitor = key_monitor;
	bypass = key_bypass;
/* But we don't redirect the meta keys */
	if (divert || monitor || bypass) {
		if (key < ACS_NUM_KEYS && ismeta[key]) {
			divert = false;
			monitor = false;
			bypass = false;
		}
	}

	if (divert || monitor)
		keep = true;
	if (bypass) {
		key_bypass = 0;
		send = true;
		goto event;
	}

/* keypad is assumed to be numbers with numlock on,
 * and perhaps speech functions otherwise. */
	if (param->ledstate & K_NUMLOCK &&
	    key >= KEY_KP7 && key <= KEY_KPDOT &&
	    key != KEY_KPMINUS && key != KEY_KPPLUS)
		goto regular;

	if (action & (1 << ss)) {
		keep = true;
		if(passt[key] & (1<<ss))
			send = true;
		goto event;
	}

regular:
/* Just a regular key. */
	if (!divert)
		send = true;

event:
	if (keep) {
		/* If this notifier is not called by an interrupt, then we need the spinlock */
		raw_spin_lock_irqsave(&acslock, irqflags);
		if (rbuf_head <= rbuf_end - 4) {
			if (rbuf_head == rbuf_tail)
				wake = true;
			rbuf_head[0] = ACS_KEYSTROKE;
			rbuf_head[1] = key;
			rbuf_head[2] = ss;
			rbuf_head[3] = param->ledstate;
			rbuf_head += 4;
			if (wake)
				wake_up_interruptible(&wq);
		}
		raw_spin_unlock_irqrestore(&acslock, irqflags);
	}

	if (!send)
		return NOTIFY_STOP;

	post4echo(KBD_KEYCODE, param);
	last_oj = 0;

done:
	return NOTIFY_DONE;
}				/* keystroke */

static struct notifier_block nb_key = {
	.notifier_call = keystroke,
	.priority = 20
};

/* load and unload the module */

static int __init acsint_init(void)
{
	int rc;

	in_use = false;
	clear_keys();

	if (major == 0)
		rc = misc_register(&acsint_dev);
	else
		rc = register_chrdev(major, ACS_DEVICE, &fops);
	if (rc)
		return rc;
	if (major == 0)
		printk(KERN_NOTICE "registered acsint, major %d minor %d\n",
		       MISC_MAJOR, acsint_dev.minor);

	rc = register_vt_notifier(&nb_vt);
	if (rc) {
		if (major == 0)
			misc_deregister(&acsint_dev);
		else
			unregister_chrdev(major, ACS_DEVICE);
		return rc;
	}

	rc = register_keyboard_notifier(&nb_key);
	if (rc) {
		unregister_vt_notifier(&nb_vt);
		if (major == 0)
			misc_deregister(&acsint_dev);
		else
			unregister_chrdev(major, ACS_DEVICE);
		return rc;
	}

	register_console(&acsintconsole);

	return 0;
}

static void __exit acsint_exit(void)
{
	int j;

	unregister_console(&acsintconsole);
	unregister_keyboard_notifier(&nb_key);
	unregister_vt_notifier(&nb_vt);
	if (major == 0)
		misc_deregister(&acsint_dev);
	else
		unregister_chrdev(major, ACS_DEVICE);

	for (j = 0; j < MAX_NR_CONSOLES; ++j)
		kfree(cbuf_tty[j]);
}

module_init(acsint_init);
module_exit(acsint_exit);
