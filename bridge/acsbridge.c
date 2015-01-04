/*********************************************************************
File: acsbridge.c
Description: Encapsulates communication between userspace and kernel via
the /dev/acsint device.  This file implements the interface described
and declared in acsbridge.h.
*********************************************************************/

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <memory.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>

#include <linux/vt.h>

#include "acsbridge.h"

#define stringEqual !strcmp

#define MAXNOTES 10 // how many notes to play in one call
#define INBUFSIZE (TTYLOGSIZE*4 + 400) /* size of input buffer */
/* Output buffer could be 40 bytes, except for injectstring() */
#define OUTBUFSIZE 20000
/* I assume the screen doesn't have more than 20000 cells,
 * and TTYLOGSIZE is at least 2.5 times 20000.
 * 48 rows by 170 columns is, for instance, 8160 */
#define SCREENCELLS 20000
#define ATTRIBOFFSET SCREENCELLS
#define VCREADOFFSET (2*SCREENCELLS)

int acs_fd = -1; /* file descriptor for /dev/acsint */
static int vcs_fd; /* file descriptor for /dev/vcsa */

static unsigned char vcs_header[4];
/* Make cursor coordinates available to the adapter */
int acs_vc_nrows, acs_vc_ncols;
int acs_vc_row, acs_vc_col;

int acs_fgc = 1; // current foreground console

int acs_lang = ACS_LANG_EN; /* language that the adapter is running in */

/* postprocess the text from the tty */
int acs_postprocess = ACS_PP_CTRL_H | ACS_PP_CRLF |
ACS_PP_CTRL_OTHER | ACS_PP_ESCB;
int acs_debug = 0;
static const char debuglog[] = "/var/log/acslog";

// for debugging: save a message, without sending it to tty,
// which would just generate more events.
int acs_log(const char *msg, ...)
{
va_list args;
static FILE *f;

if(!acs_debug) return 0;

if(!f) {
f = fopen(debuglog, "a");
if(!f)
return -1;
setlinebuf(f);
}

va_start(args, msg);
vfprintf(f, msg, args);
va_end(args);
return 0;
} // acs_log

key_handler_t acs_key_h;
acs_more_handler_t acs_more_h;
acs_fgc_handler_t acs_fgc_h;
ks_echo_handler_t acs_ks_echo_h;


static unsigned char inbuf[INBUFSIZE]; /* input buffer for acsint */
static unsigned char outbuf[OUTBUFSIZE]; /* output buffer for acsint */

// Maintain the tty log for each virtual console.
static struct acs_readingBuffer *tty_log[MAX_NR_CONSOLES];
static struct acs_readingBuffer tty_nomem; /* in case we can't allocate */
static const char nomem_message[] = "Acsint bridge cannot allocate space for this console";
static struct acs_readingBuffer *tl; // current tty log
static struct acs_readingBuffer screenBuf;
static int screenmode; // 1 = screen, 0 = tty log
struct acs_readingBuffer *acs_mb; /* manipulation buffer */
struct acs_readingBuffer *acs_tb; /* tty buffer */
struct acs_readingBuffer *acs_rb; /* current reading buffer */

// cp437 code page, for English.
static const unsigned int cp437[] = {
0x0000,0x263a,0x263b,0x2665,0x2666,0x2663,0x2660,0x2022,0x25d8,0x25cb,0x25d9,0x2642,0x2640,0x266a,0x266b,0x263c,
0x25b6,0x25c0,0x2195,0x203c,0x00b6,0x00a7,0x25ac,0x21a8,0x2191,0x2193,0x2192,0x2190,0x221f,0x2194,0x25b2,0x25bc,
0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,0x0028,0x0029,0x002a,0x002b,0x002c,0x002d,0x002e,0x002f,
0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,0x0038,0x0039,0x003a,0x003b,0x003c,0x003d,0x003e,0x003f,
0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,0x0048,0x0049,0x004a,0x004b,0x004c,0x004d,0x004e,0x004f,
0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,0x0058,0x0059,0x005a,0x005b,0x005c,0x005d,0x005e,0x005f,
0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,0x0068,0x0069,0x006a,0x006b,0x006c,0x006d,0x006e,0x006f,
0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,0x0078,0x0079,0x007a,0x007b,0x007c,0x007d,0x007e,0x2302,
0x00c7,0x00fc,0x00e9,0x00e2,0x00e4,0x00e0,0x00e5,0x00e7,0x00ea,0x00eb,0x00e8,0x00ef,0x00ee,0x00ec,0x00c4,0x00c5,
0x00c9,0x00e6,0x00c6,0x00f4,0x00f6,0x00f2,0x00fb,0x00f9,0x00ff,0x00d6,0x00dc,0x00a2,0x00a3,0x00a5,0x20a7,0x0192,
0x00e1,0x00ed,0x00f3,0x00fa,0x00f1,0x00d1,0x00aa,0x00ba,0x00bf,0x2310,0x00ac,0x00bd,0x00bc,0x00a1,0x00ab,0x00bb,
0x2591,0x2592,0x2593,0x2502,0x2524,0x2561,0x2562,0x2556,0x2555,0x2563,0x2551,0x2557,0x255d,0x255c,0x255b,0x2510,
0x2514,0x2534,0x252c,0x251c,0x2500,0x253c,0x255e,0x255f,0x255a,0x2554,0x2569,0x2566,0x2560,0x2550,0x256c,0x2567,
0x2568,0x2564,0x2565,0x2559,0x2558,0x2552,0x2553,0x256b,0x256a,0x2518,0x250c,0x2588,0x2584,0x258c,0x2590,0x2580,
0x03b1,0x03b2,0x0393,0x03c0,0x03a3,0x03c3,0x00b5,0x03c4,0x03a6,0x0398,0x03a9,0x03b4,0x221e,0x03c6,0x03b5,0x2229,
0x2261,0x00b1,0x2265,0x2264,0x2320,0x2321,0x00f7,0x2248,0x00b0,0x2219,0x00b7,0x221a,0x207f,0x00b2,0x25a0,0x00a0,
};

// cp850 code page, for Portuguese Brazilian
static const unsigned int cp850[] = {
0x0000,0x263a,0x263b,0x2665,0x2666,0x2663,0x2660,0x2022,0x25d8,0x25cb,0x25d9,0x2642,0x2640,0x266a,0x266b,0x263c,
0x25b6,0x25c0,0x2195,0x203c,0x00b6,0x00a7,0x25ac,0x21a8,0x2191,0x2193,0x2192,0x2190,0x221f,0x2194,0x25b2,0x25bc,
0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,0x0028,0x0029,0x002a,0x002b,0x002c,0x002d,0x002e,0x002f,
0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,0x0038,0x0039,0x003a,0x003b,0x003c,0x003d,0x003e,0x003f,
0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,0x0048,0x0049,0x004a,0x004b,0x004c,0x004d,0x004e,0x004f,
0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,0x0058,0x0059,0x005a,0x005b,0x005c,0x005d,0x005e,0x005f,
0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,0x0068,0x0069,0x006a,0x006b,0x006c,0x006d,0x006e,0x006f,
0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,0x0078,0x0079,0x007a,0x007b,0x007c,0x007d,0x007e,0x2302,
0x00c7,0x00fc,0x00e9,0x00e2,0x00e4,0x00e0,0x00e5,0x00e7,0x00ea,0x00eb,0x00e8,0x00ef,0x00ee,0x00ec,0x00c4,0x00c5,
0x00c9,0x00e6,0x00c6,0x00f4,0x00f6,0x00f2,0x00fb,0x00f9,0x00ff,0x00d6,0x00dc,0x00f8,0x00a3,0x00d8,0x00d7,0x0192,
0x00e1,0x00ed,0x00f3,0x00fa,0x00f1,0x00d1,0x00aa,0x00ba,0x00bf,0x00ae,0x00ac,0x00bd,0x00bc,0x00a1,0x00ab,0x00bb,
0x2591,0x2592,0x2593,0x2502,0x2524,0x00c1,0x00c2,0x00c0,0x00a9,0x2563,0x2551,0x2557,0x255d,0x00a2,0x00a5,0x2510,
0x2514,0x2534,0x252c,0x251c,0x2500,0x253c,0x00e3,0x00c3,0x255a,0x2554,0x2569,0x2566,0x2560,0x2550,0x256c,0x00a4,
0x00f0,0x00d0,0x00ca,0x00cb,0x00c8,0x0131,0x00cd,0x00ce,0x00cf,0x2518,0x250c,0x2588,0x2584,0x00a6,0x00cc,0x2580,
0x00d3,0x03b2,0x00d4,0x00d2,0x00f5,0x00d5,0x00b5,0x00fe,0x00de,0x00da,0x00db,0x00d9,0x00fd,0x00dd,0x03b5,0x00b4,
0x2261,0x00b1,0x2265,0x00be,0x2320,0x2321,0x00f7,0x00b8,0x00b0,0x00a8,0x00b7,0x00b9,0x00b3,0x00b2,0x25a0,0x00a0,
};

// code page per language
static const unsigned int *cp_lang[] = {0,
cp437, cp437, cp850,
};

void acs_vc(void)
{
lseek(vcs_fd, 0, 0);
read(vcs_fd, vcs_header, 4);
screenBuf.area[0] = 0;
screenBuf.start = screenBuf.area + 1;
acs_vc_nrows = vcs_header[0];
acs_vc_ncols = vcs_header[1];
acs_vc_row = vcs_header[3];
acs_vc_col = vcs_header[2];
screenBuf.v_cursor = screenBuf.start + acs_vc_row * (acs_vc_ncols+1) + acs_vc_col;
} /* acs_vc */

void acs_screensnap(void)
{
unsigned int *t;
unsigned char *a, *s;
int i, j;

acs_vc();

t = screenBuf.start;
screenBuf.attribs = a = (unsigned char *) (screenBuf.area + ATTRIBOFFSET);
s = (unsigned char *) (screenBuf.area + VCREADOFFSET);
read(vcs_fd, s, 2*acs_vc_nrows*acs_vc_ncols);

for(i=0; i<acs_vc_nrows; ++i) {
for(j=0; j<acs_vc_ncols; ++j) {
*t++ = cp_lang[acs_lang][*s++];
*a++ = *s++;
}
*t++ = '\n';
*a++ = 0; // should this be 7?
}
*t = 0;
screenBuf.end = t;
} // acs_screensnap

static void screenBlank(void)
{
int i, j, top;
unsigned int *s;

if(!screenmode) return; // should never happen
top = acs_vc_nrows * (acs_vc_ncols + 1);
if(top > SCREENCELLS) return; // should never happen

s = screenBuf.area;
*s++ = 0;
screenBuf.v_cursor = screenBuf.cursor = s;
for(i=0; i<acs_vc_nrows; ++i) {
for(j=0; j<acs_vc_ncols; ++j) *s++ = ' ';
*s++ = '\n';
}
*s = 0;
screenBuf.end = s;
} /* screenBlank */

/* check to see if a tty reading buffer has been allocated */
static void
checkAlloc(void)
{
acs_mb = acs_tb = tty_log[acs_fgc - 1];
if(acs_tb && acs_tb != &tty_nomem)
return; /* already allocated */

acs_tb = malloc(sizeof(struct acs_readingBuffer));
if(acs_tb) acs_log("allocate %d\n", acs_fgc);
else acs_tb = &tty_nomem;
tty_log[acs_fgc-1] = acs_mb = acs_tb;

acs_tb->start = acs_tb->area + 1;
acs_tb->area[0] = 0;
if(acs_tb == &tty_nomem) {
int j;
for(j=0; nomem_message[j]; ++j)
acs_tb->start[j] = nomem_message[j];
acs_tb->start[j] = 0;
acs_tb->end = acs_tb->start + j;
} else {
acs_tb->end = acs_tb->start;
acs_tb->area[1] = 0;
}

acs_tb->cursor = acs_tb->start;
acs_tb->v_cursor = 0;
acs_tb->attribs = 0;
} /* checkAlloc */

int
acs_screenmode(int enabled)
{
acs_imark_start = 0;
screenmode = 0;
checkAlloc();
if(!enabled) return 0;
acs_vc();
if(acs_vc_nrows * (acs_vc_ncols + 1) > SCREENCELLS) return -1;
screenmode = 1;
acs_mb = &screenBuf;
screenBlank();
memset(acs_mb->marks, 0, sizeof(acs_mb->marks));
return 0;
} /* acs_screenmode */


// Open and close the device.

static int acs_bufsize(int n);

int
acs_open(const char *devname)
{
if(acs_fd >= 0) {
// already open
errno = EEXIST;
return -1;
}

if(acs_debug) unlink(debuglog);

vcs_fd = open("/dev/vcsa", O_RDONLY | O_CLOEXEC);
if(vcs_fd < 0)
return -1;

acs_fd = open(devname, O_RDWR | O_CLOEXEC);
if(acs_fd < 0) {
close(vcs_fd);
return -1;
}

errno = 0;
acs_reset_configure();
acs_bufsize(TTYLOGSIZE);

return acs_fd;
} // acs_open

int
acs_close(void)
{
int rc = 0;
errno = 0;
if(acs_fd < 0) return 0; // already closed
if(close(acs_fd) < 0)
rc = -1;
/* Close it regardless. */
acs_fd = -1;
return rc;
} // acs_close

void
acs_nodecheck(const char *devname)
{
int fd, uid;
int m1, m2;
struct stat buf;
char line[20];
char *s;

uid = geteuid();
if(uid) return; /* not root */

fd = open("/sys/devices/virtual/misc/acsint/dev", O_RDONLY);
if(fd < 0) return; /* nothing in /sys to help us */
if(read(fd, line, sizeof(line)) <= 0) {
close(fd);
return;
}
m1 = strtol(line, &s, 10);
++s;
m2 = strtol(s, &s, 10);
close(fd);

/* if stat fails I'm going to assume it's not there and create it */
if(stat(devname, &buf)) goto create;
if(major(buf.st_rdev) == m1 && minor(buf.st_rdev) == m2) return;
unlink(devname);

create:
mknod(devname, S_IFCHR|0666, makedev(m1, m2));
} /* acs_nodecheck */

// Write a command to /dev/acsint.

static int
acs_write(int n)
{
errno = 0;
if(acs_fd < 0) {
errno = ENXIO;
return -1;
}
if(write(acs_fd, outbuf, n) < n)
return -1;
return 0;
} // acs_write

/* Pass the size of our tty buffer to the driver */
static int acs_bufsize(int n)
{
outbuf[0] = ACS_BUFSIZE;
outbuf[1] = n;
outbuf[2] = n >> 8;
return acs_write(3);
}

/* Which sounds are generated automatically? */

int
acs_sounds(int enabled) 
{
outbuf[0] = ACS_SOUNDS;
outbuf[1] = enabled;
return acs_write(2);
} // acs_sounds

int
acs_tty_clicks(int enabled) 
{
outbuf[0] = ACS_SOUNDS_TTY;
outbuf[1] = enabled;
return acs_write(2);
} // acs_tty_clicks

int
acs_kmsg_tones(int enabled) 
{
outbuf[0] = ACS_SOUNDS_KMSG;
outbuf[1] = enabled;
return acs_write(2);
} // acs_kmsg_tones

// Various routines to make sounds through the pc speaker.

int
acs_click(void)
{
outbuf[0] = ACS_CLICK;
return acs_write(1);
} // acs_click

int
acs_cr(void)
{
outbuf[0] = ACS_CR;
return acs_write(1);
} // acs_cr

int
acs_notes(const short *notelist)
{
int j;
outbuf[0] = ACS_NOTES;
for(j=0; j<MAXNOTES; ++j) {
if(!notelist[2*j]) break;
outbuf[2+3*j] = notelist[2*j];
outbuf[2+3*j+1] = notelist[2*j]>>8;
outbuf[2+3*j+2] = notelist[2*j+1];
}
outbuf[1] = j;
return acs_write(2+3*j);
} /* acs_notes */

int acs_scale(int f1, int f2, int step, int duration)
{
outbuf[0] = ACS_STEPS;
outbuf[1] = step;
outbuf[2] = f1;
outbuf[3] = (f1 >> 8);
outbuf[4] = f2;
outbuf[5] = (f2 >> 8);
outbuf[6] = duration;
outbuf[7] = (duration >> 8);
return acs_write(8);
} /* acs_scale */

int acs_bell(void)
{
static const short bellsnd[] = {
        1800,10,0,0     };
return acs_notes(bellsnd);
} // acs_bell

int acs_buzz(void)
{
static const short buzzsnd[] = {
        120,20,0,0     };
return acs_notes(buzzsnd);
} // acs_buzz

int acs_highcap(void)
{
static const short capsnd[] = {
        3000,3,0,0     };
return acs_notes(capsnd);
} // acs_highcap

int acs_highbeeps(void)
{
static const short boundsnd[] = {
        2800,4,3300,3,0,0       };
return acs_notes(boundsnd);
} // acs_highbeeps

int
acs_tone_onoff(int enabled)
{
static const short offsnd[] = {
	220,6,0,0	};
static const short onsnd[] = {
	700,12,0,0	};
return acs_notes(enabled ? onsnd : offsnd);
} // acs_tone_onoff

// redirect keystrokes for capture, monitor, and bypass

int
acs_divert(int enabled) 
{
outbuf[0] = ACS_DIVERT;
outbuf[1] = enabled;
return acs_write(2);
} // acs_divert

int acs_monitor(int enabled) 
{
outbuf[0] = ACS_MONITOR;
outbuf[1] = enabled;
return acs_write(2);
} // acs_monitor

int
acs_bypass(void)
{
outbuf[0] = ACS_BYPASS;
return acs_write(1);
} // acs_bypass

int acs_obreak(int gap) 
{
outbuf[0] = ACS_OBREAK;
outbuf[1] = gap;
return acs_write(2);
} // acs_obreak

/* Use divert to swallow a string.
 * This is not unicode at present. */
static char *swallow_string;
static int swallow_max, swallow_len;
static int swallow_prop, swallow_rc;
static key_handler_t save_key_h;
static void swallow_key_h(int key, int ss, int leds);
static void swallow1_h(int key, int ss, int leds);

int acs_keystring(char *buf, int buflen, int properties)
{
if(buflen <= 0) {
errno = ENOMEM;
return -1;
}

if(buflen == 1) {
*buf = 0;
return 0;
}

swallow_string = buf;
swallow_max = buflen;
swallow_len = 0;
swallow_prop = properties;
if(acs_divert(1)) return -1;
save_key_h = acs_key_h;
acs_key_h = swallow_key_h;

while(acs_key_h == swallow_key_h)
acs_events();

// At this point the key handler has put everything back.
return swallow_rc;
} // acs_keystring

const char lowercode[] =
" \0331234567890-=\b qwertyuiop[]\r asdfghjkl;'` \\zxcvbnm,./    ";
const char uppercode[] =
" \033!@#$%^&*()_+\b QWERTYUIOP{}\r ASDFGHJKL:\"~ |ZXCVBNM<>?    ";

// special handler for keystring()
static void swallow_key_h(int key, int ss, int leds)
{
char keychar;

if(key > KEY_SPACE) goto bad;
if(ss&ACS_SS_ALT) goto bad;
if(ss&ACS_SS_CTRL) {
if(key != KEY_H) goto bad;
if(!(swallow_prop&ACS_KS_BACKUP)) goto bad;
backup:
if(swallow_len) --swallow_len;
if(swallow_prop&ACS_KS_GOODCLICK) acs_click();
if(acs_ks_echo_h) (*acs_ks_echo_h)('\b');
return;
}

if(key == KEY_BACKSPACE) goto backup;

if(key == KEY_ESC) {
if(swallow_prop&ACS_KS_ESCCR) acs_cr();
swallow_rc = -1;
swallow_len = 0;
goto cleanup;
}

if(key == KEY_ENTER) {
swallow_rc = 0;
goto cleanup;
}

// This only works on a qwerty layout; not sure how to fix this.
keychar = (ss&ACS_SS_SHIFT) ? uppercode[key] : lowercode[key];
if(keychar == ' ' && key != KEY_SPACE) goto bad;
if(leds & K_CAPSLOCK && isalpha(keychar))
keychar ^= 0x20;

// looks ok, but is there room?
if(swallow_len+1 == swallow_max) {
if(swallow_prop&ACS_KS_BOUNDARYBEEPS) acs_highbeeps();
if(!(swallow_prop&ACS_KS_BOUNDARYSTOP)) return;
swallow_rc = -1;
goto cleanup;
}

if(swallow_prop&ACS_KS_GOODCLICK) {
if(isupper(keychar)) acs_highcap();
else acs_click();
}
if(acs_ks_echo_h) (*acs_ks_echo_h)(keychar);
swallow_string[swallow_len++] = keychar;
return;

bad:
if(swallow_prop&ACS_KS_BADBELL) acs_bell();
if(!(swallow_prop&ACS_KS_BADSTOP)) return;
swallow_rc = -1;

cleanup:
swallow_string[swallow_len] = 0;
acs_key_h = save_key_h;
acs_divert(0);
} // swallow_key_h

static int key1key, key1ss;
int acs_get1key(int *key_p, int *ss_p)
{
if(acs_divert(1)) return -1;
save_key_h = acs_key_h;
acs_key_h = swallow1_h;
while(acs_key_h == swallow1_h)
acs_events();
*key_p = key1key;
*ss_p = key1ss;
return 0;
} // acs_get1key

// special handler for get1key()
static void swallow1_h(int key, int ss, int leds)
{
key1key = key;
key1ss = ss;
acs_key_h = save_key_h;
acs_divert(0);
} // swallow1_h

int acs_get1char(char *p)
{
int key, state;
char keychar;
*p = 0;
if(acs_get1key(&key, &state)) return -1;
if(key > KEY_SPACE ||
state&(ACS_SS_ALT|ACS_SS_CTRL)) return -1;
// This only works on a qwerty layout; not sure how to fix this.
keychar = (state&ACS_SS_SHIFT) ? uppercode[key] : lowercode[key];
if(!isalnum(keychar)) return -1;
*p = keychar;
return 0;
} // acs_get1char


// set and unset keys

int acs_setkey(int key, int ss)
{
outbuf[0] = ACS_SET_KEY;
outbuf[1] = key;
outbuf[2] = ss;
return acs_write(3);
} // acs_setkey

int acs_unsetkey(int key, int ss)
{
outbuf[0] = ACS_UNSET_KEY;
outbuf[1] = key;
outbuf[2] = ss;
return acs_write(3);
} // acs_unsetkey

int acs_ismeta(int key, int enabled)
{
outbuf[0] = ACS_ISMETA;
outbuf[1] = key;
outbuf[2] = enabled;
return acs_write(3);
} // acs_ismeta

int acs_clearkeys(void)
{
outbuf[0] = ACS_CLEAR_KEYS;
return acs_write(1);
} // acs_clearkeys

static void
postprocess(unsigned int *s)
{
unsigned int *t, *u;
int j;

if(!acs_postprocess) return;

// in case we had part of an ansi escape code
s -= 100;
if(s < tl->start) s = tl->start;
t = s;

while(*s) {

// crlf
if(*s == '\r' && s[1] == '\n' &&
acs_postprocess&ACS_PP_CRLF) {
++s;
continue;
}

if(s[0] == '\7' && acs_postprocess&ACS_PP_CTRL_G) {
++s;
continue;
}

/* ^h is backspace.
 * Check to see if we have backed over the reading cursor or the marks.
 * Because of the way Jupiter reads, a mark could be at end of buffer.
 * In that case keep it at end of buffer. */
if(s[0] == '\b' && acs_postprocess&ACS_PP_CTRL_H) {
++s;
if(t[-1] == 0) continue; /* buffer was empty */
--t;
/* Now check the cursor and the marks */
if(tl->cursor && tl->cursor >= t)
tl->cursor = (t > tl->start ? t-1 : t);
// marks, but not the last mark, which is continuous reading
for(j=0; j<27; ++j) {
u = tl->marks[j];
if(u && u >= t) tl->marks[j] = 0;
}
// the continuous reading mark
if(tl->marks[27] && tl->marks[27] > t)
tl->marks[27] = t;
continue;
}

/* ansi escape sequences */
if(*s == '\33' &&ACS_PP_ESCB) {
for(++s; *s == '\33'; ++s)  ;
--s;
j = 1;
if(!s[j]) goto advance;
if(s[j] != '[') {
cut_j:
s += j+1;
// Could the cursor have read into the escape sequence, then we pulled it back?
// I assume you didn't have time to set a mark in the middle of the sequence.
if(tl->cursor && tl->cursor >= t)
tl->cursor = (t > tl->start ? t-1 : t);
continue;
		}
// escape [ stuff letter
for(++j; s[j] && j<20; ++j)
if(s[j] < 256 && isalpha(s[j])) goto cut_j;
goto advance;
}

// control chars
if(*s < ' ' && !strchr("\t\b\r\n\7", *s) &&
acs_postprocess&ACS_PP_CTRL_OTHER) {
++s;
continue;
}

advance:
*t++ = *s++;
}

tl->end = t;
*t = 0;
} /* postprocess */

void acs_clearbuf(void)
{
if(screenmode) return;
acs_imark_start = 0;
if(acs_mb && acs_mb != &tty_nomem) {
acs_mb->end = acs_mb->start;
memset(acs_mb->marks, 0, sizeof(acs_mb->marks));
}
acs_mb->cursor = acs_mb->start;
} // acs_clearbuf

/*********************************************************************
Read events from the acsint device driver.
Warning!!  This routine is not rentrant.
Your handlers should not call acs_events(), even indirectly.
That means they should not call acs_refresh, acs_keystring, etc.
The best design simply sets variables, and then, once acs_events()
returns, you can act on those variables, execute the key command,
start reading, etc.
*********************************************************************/

int acs_events(void)
{
int nr; // number of bytes read
int i, j;
int culen; /* catch up length */
unsigned int *custart; // where does catch up start
unsigned int *sp; // screen pointer
int nlen; // length of new area
int diff;
int m2;
char refreshed = 0;
unsigned int d;
int lastrow = acs_vc_row, lastcol = acs_vc_col;

errno = 0;
if(acs_fd < 0) {
errno = ENXIO;
return -1;
}

nr = read(acs_fd, inbuf, INBUFSIZE);
acs_log("acsint read %d bytes\n", nr);
if(nr < 0)
return -1;

i = 0;
while(i <= nr-4) {
switch(inbuf[i]) {
case ACS_KEYSTROKE:
acs_log("key %d,%d\n", inbuf[i+1], inbuf[i+2]);
// keystroke refreshes automatically in line mode;
// we have to do it here for screen mode.
if(screenmode && !refreshed) {
acs_screensnap();
if(!acs_mb->cursor)
acs_mb->cursor = acs_mb->v_cursor;
refreshed = 1;
}
// check for macro here.
if(acs_key_h != swallow_key_h && acs_key_h != swallow1_h) {
// get the modified key code.
int mkcode = acs_build_mkcode(inbuf[i+1], inbuf[i+2]);
// see if this key has a macro
char *m = acs_getmacro(mkcode);
if(m) {
if(*m == '|')
system(m+1);
else
acs_injectstring(m);
i += 4;
break;
}
}
if(acs_key_h) acs_key_h(inbuf[i+1], inbuf[i + 2], inbuf[i+3]);
i += 4;
break;

case ACS_FGC:
acs_log("fg %d\n", inbuf[i+1]);
acs_fgc = inbuf[i+1];
checkAlloc();
if(screenmode) {
/* Oops, the checkAlloc function changed acs_mb out from under us. */
acs_mb = &screenBuf;
screenBlank();
memset(acs_mb->marks, 0, sizeof(acs_mb->marks));
}
if(acs_fgc_h) acs_fgc_h();
i += 4;
break;

case ACS_TTY_MORECHARS:
if(i > nr-8) break;
d = *(unsigned int *) (inbuf+i+4);
acs_log("output echo %d", inbuf[i+1]);
if(d >= ' ' && d < 0x7f) acs_log("/%c\n", d);
else acs_log(";%x\n", d);
/* If echo is nonzero, then the refresh has already been done. */
if(acs_more_h) acs_more_h(inbuf[i+1], d);
i += 8;
break;

case ACS_REFRESH:
acs_log("ack refresh\n");
i += 4;
break;

case ACS_TTY_NEWCHARS:
/* this is the refresh data in line mode
 * m2 is always the foreground console; we could probably discard it. */
m2 = inbuf[i+1];
culen = inbuf[i+2] | ((unsigned short)inbuf[i+3]<<8);
acs_log("new %d\n", culen);
i += 4;
if(!culen) break;
if(acs_debug) {
for(j=0; j<culen; ++j) {
d = * (int*) (inbuf + i + 4*j);
if(d < ' ' || d >= 0x7f)
acs_log("<%x>", d);
else
acs_log("%c", d);
}
acs_log("\n");
}
if(nr-i < culen*4) break;

// The reprint detector
if(screenmode && culen <= 10 &&
acs_postprocess&ACS_PP_CTRL_OTHER) {
sp = screenBuf.start + lastrow * (acs_vc_ncols+1) + lastcol;
for(j=0; j<culen; ++j) {
d = * (int*) (inbuf + i + 4*j);
if(d == '\b') {
if(lastcol) --lastcol, --sp;
continue;
}
if(d == '\r') {
sp -= lastcol, lastcol = 0;
continue;
}
if(d != '\33') break;
// only 2 or 3 escape sequences do I recognize here.
// Only the really short ones would be used anyways.
// esc[A and esc[8d
if(++j == culen) goto inbuffer;
d = * (int*) (inbuf + i + 4*j);
if(d != '[') goto inbuffer;
if(++j == culen) goto inbuffer;
d = * (int*) (inbuf + i + 4*j);
if(d == 'A') {
if(lastrow) --lastrow, sp -= (acs_vc_ncols+1);
continue;
}
if(d >= 0x80) goto inbuffer;
if(!isdigit(d)) goto inbuffer;
diff = d - '0';
if(++j == culen) goto inbuffer;
d = * (int*) (inbuf + i + 4*j);
if(d < 0x80 && isdigit(d)) {
diff = 10*diff + d - '0';
if(++j == culen) goto inbuffer;
d = * (int*) (inbuf + i + 4*j);
}
if(d == 'd') {
lastrow = diff - 1;
sp = screenBuf.start + lastrow * (acs_vc_ncols+1) + lastcol;
continue;
}
goto inbuffer; // unknown escape sequence
}
// little cursor motions are done
for(; j<culen; ++j) {
d = * (int*) (inbuf + i + 4*j);
if(d != *sp++) break;
}
if(j == culen) {
acs_log("reprint %d\n", culen );
i += culen*4;
break;
}
}

inbuffer:
tl = tty_log[m2 - 1];
if(!tl || tl == &tty_nomem) {
/* not allocated; no room for this data */
i += culen*4;
break;
}

nlen = tl->end - tl->start + culen;
diff = nlen - TTYLOGSIZE;

if(diff >= tl->end-tl->start) {
/* a complete replacement
 * should never be greater; diff = tl->end - tl->start
 * copy the new stuff */
custart = tl->start;
memcpy(custart, inbuf+i, TTYLOGSIZE*4);
tl->end = tl->start + TTYLOGSIZE;
tl->end[0] = 0;
tl->cursor = 0;
memset(tl->marks, 0, sizeof(tl->marks));
if(!screenmode) acs_imark_start = 0;
} else {
if(diff > 0) {
// partial replacement
memmove(tl->start, tl->start+diff, (tl->end-tl->start - diff)*4);
tl->end -= diff;
if(tl->cursor) {
tl->cursor -= diff;
if(tl->cursor < tl->start) tl->cursor = 0;
}
if(acs_imark_start && !screenmode) {
acs_imark_start -= diff;
if(acs_imark_start < tl->start) acs_imark_start = 0;
}
for(j=0; j<=27; ++j) {
if(!tl->marks[j]) continue;
tl->marks[j] -= diff;
if(tl->marks[j] < tl->start) tl->marks[j] = 0;
}
}
/* copy the new stuff */
custart = tl->end;
memcpy(custart, inbuf+i, culen*4);
tl->end += culen;
tl->end[0] = 0;
}

postprocess(custart);

/* If you're in screen mode, I haven't moved your reading cursor,
 * or imark _start, or the pointers in marks[], appropriately.
 * See the todo file for tracking the cursor in screen mode. */

i += culen*4;
break;

default:
/* Perhaps a phase error.
 * Not sure what to do here.
 * Just give up. */
acs_log("unknown command %d\n", inbuf[i]);
i += 4;
} // switch
} // looping through events

return 0;
} // acs_events

int acs_refresh(void)
{
acs_log("get refresh\n");
outbuf[0] = ACS_REFRESH;
if(acs_write(1)) return -1;
return acs_events();
} // acs_refresh


// cursor commands.
static unsigned int *tc; // temp cursor

void acs_cursorset(void)
{
tc = acs_mb->cursor;
} // acs_cursorset

void acs_cursorsync(void)
{
acs_mb->cursor = tc;
} // acs_cursorsync

int acs_cursorvalid(void)
{
return tc != 0;
} // acs_cursorvalid

unsigned int acs_getc(void)
{
return (tc ? *tc : 0);
} // acs_getc

int acs_forward(void)
{
if(acs_mb->end == acs_mb->start) return 0;
if(!tc) return 0;
if(++tc == acs_mb->end) return 0;
return 1;
} // acs_forward

int acs_back(void)
{
if(acs_mb->end == acs_mb->start) return 0;
if(!tc) return 0;
if(tc-- == acs_mb->start) return 0;
return 1;
} // acs_back

int acs_startline(void)
{
int colno = 0;
if(acs_mb->end == acs_mb->start) return 0;
if(!tc) return 0;
do ++colno;
while(acs_back() && acs_getc() != '\n');
acs_forward();
return colno;
} // acs_startline

int acs_endline(void)
{
if(acs_mb->end == acs_mb->start) return 0;
if(!tc) return 0;
while(acs_getc() != '\n') {
if(acs_forward()) continue;
acs_back();
break;
}
return 1;
} // acs_endline

// put the cursor back to a known location.
// Internal use only.
static void putback(int n)
{
	if(!n) return;
	if(n > 0) {
		while(n--) acs_forward();
	} else {
		while(n++) acs_back();
	}
} // putback

// start of word (actually token/symbol)
int acs_startword(void)
{
	int forward, backward;
	char apos, apos1;
	unsigned int c = acs_getc();

if(!c) return 0;

	if(!acs_isalnum(c)) {
		if(c == '\n' || c == ' ' || c == '\7') return 1;
		// a punctuation mark, an atomic token.
		// But wait, if there are more than four in a row,
		// we have a linear token.  Pull back to the start.
		for(forward=0; acs_forward(); ++forward)
			if(c != acs_getc()) break;
		putback(-(forward+1));
		for(backward=0; acs_back(); ++backward)
			if(c != acs_getc()) break;
		acs_forward();
		if(forward+backward < 4) putback(backward);
		return 1;
} // punctuation

	apos = apos1 = 0;
	do {
		c = acs_getc();
		if(c == '\'') {
			if(apos1) break;
			apos = apos1 = 1;
			continue;
		}
		if(!acs_isalnum(c)) break;
		apos = 0;
	} while(acs_back());
	acs_forward();
	if(apos) acs_forward();

return 1;
} // acs_startword

// end of word
int acs_endword(void)
{
	int forward, backward;
	char apos, apos1;
	unsigned int c = acs_getc();

if(!c) return 0;

	if(!acs_isalnum(c)) {
		if(c == '\n' || c == ' ' || c == '\7') return 1;
		for(backward=0; acs_back(); ++backward)
			if(c != acs_getc()) break;
		putback(backward+1);
		for(forward=0; acs_forward(); ++forward)
			if(c != acs_getc()) break;
		acs_back();
		if(forward+backward < 4) putback(-forward);
		return 1;
} // punctuation

	apos = apos1 = 0;
	do {
		c = acs_getc();
		if(c == '\'') {
			if(apos1) break;
			 apos = apos1 = 1;
			continue;
		}
		if(!acs_isalnum(c)) break;
		apos = 0;
	} while(acs_forward());
	acs_back();
	if(apos) acs_back();

return 1;
} // acs_endword

void acs_startbuf(void)
{
tc = acs_mb->start;
} // acs_startbuf

void acs_endbuf(void)
{
tc = acs_mb->end;
if(tc != acs_mb->start) --tc;
} // acs_endbuf

// skip past left spaces
void acs_lspc(void)
{
if(acs_mb->end == acs_mb->start) return;
if(!tc) return;
	if(!acs_back()) goto done;
	while(acs_getc() == ' ')
		if(!acs_back()) break;
done:
	acs_forward();
} // acs_lspc

// skip past right spaces
void acs_rspc(void)
{
if(acs_mb->end == acs_mb->start) return;
if(!tc) return;
	if(!acs_forward()) goto done;
	while(acs_getc() == ' ')
		if(!acs_forward()) break;
done:
	acs_back();
} // acs_rspc

/* Case insensitive match.  Assumes the first letters already match.
 * This is an ascii match on the unaccented letters. */
static int stringmatch(const char *s)
{
	char x, y;
	short count = 0;

	if(!(y = *++s)) return 1;
	y = tolower(y);

	while(++count, acs_forward()) {
		x = acs_unaccent(acs_getc());
if(x != y) break;
		if(!(y = (unsigned char)*++s)) return 1;
y = tolower(y);
	} /* while matches */

	// put the cursor back
	putback(-count);
	return 0;
} // stringmatch

int acs_bufsearch(const char *string, int back, int newline)
{
	int ok;
	unsigned int c, first;

if(acs_mb->end == acs_mb->start) return 0;
if(!tc) return 0;

	if(newline) {
		if(back) acs_startline(); else acs_endline();
	}

	ok = back ? acs_back() : acs_forward();
	if(!ok) return 0;

first = (unsigned char) *string;
first = tolower(first);

	do {
		c = acs_unaccent(acs_getc());
if(c == first && stringmatch(string)) return 1;
		ok = back ? acs_back() : acs_forward();
	} while(ok);

	return 0;
} // acs_bufsearch

// inject chars into the stream
int acs_injectstring(const char *s)
{
int len = strlen(s);
if(len+4 >= OUTBUFSIZE) {
errno = ENOMEM;
return -1;
}

outbuf[0] = ACS_PUSH_TTY;
outbuf[1] = len;
outbuf[2] = len>>8;
strcpy((char*)outbuf+3, s);
return acs_write(len+3);
} // acs_injectstring

static const char *lengthword[] = {
	" ",
	" length ",
	" langes ",
	" cumprimento ",
};

int acs_getsentence(unsigned int *dest, int destlen, acs_ofs_type *offsets, int prop)
{
const unsigned int *s;
unsigned int *t, *destend;
acs_ofs_type *o;
int j, l;
unsigned int c;
char c1; /* cut c down to 1 byte */
char spaces = 1, alnum = 0; // flags

if(!dest || !acs_rb || !(s = acs_rb->cursor)) {
errno = EFAULT;
return -1;
}

t = dest;
destend = dest + destlen - 1; /* end of destination array */
o = offsets;

if(destlen <= 0) {
errno = ENOMEM;
return -1;
}

if(destlen == 1) {
*dest = 0;
if(o) *o = 0;
return 0;
}

// zero offsets by default
if(o) memset(o, 0, sizeof(acs_ofs_type)*destlen);

while((c = *s) && t < destend) {
if(c == '\n' && prop&ACS_GS_NLSPACE)
c = ' ';

if(c == '\r' && !(prop&ACS_GS_ONEWORD))
c = ' ';

c1 = acs_unaccent(c);

if(c1 == ' ') {
alnum = 0;
if(prop&ACS_GS_ONEWORD) {
if(t == dest) *t++ = c, ++s;
break;
}
if(!spaces) *t++ = ' ';
spaces = 1;
++s;
continue;
}

if(c == '\n' || c == '\7') {
alnum = 0;
if(t > dest && t[-1] == ' ') --t;
if(prop&ACS_GS_ONEWORD) {
if(t == dest) *t++ = c, ++s;
break;
}
if(o) o[t-dest] = s-acs_rb->cursor;
*t++ = c;
++s;
if(prop&ACS_GS_STOPLINE) break;
spaces = 1;
continue;
}

spaces = 0;

if(acs_isalnum(c)) {
if(!alnum) { // new word
if(o) o[t-dest] = s-acs_rb->cursor;
}
// building our word
*t++ = c;
++s;
alnum = 1;
continue;
}

if(c1 == '\'' && alnum && acs_isalpha(s[1])) {
const unsigned int *v;
char v0;
/* this is treated as a letter, as in wouldn't,
 * unless there is another apostrophe before or after,
 * or digits are involved. */
for(v=t-1; v>=dest && acs_isalpha(*v); --v)  ;
if(v >= dest) {
v0 = acs_unaccent(*v);
if(v0 == '\'') goto punc;
if(isdigit(v0)) goto punc;
}
for(v=s+1; acs_isalpha(*v); ++v)  ;
v0 = acs_unaccent(*v);
if(v0 == '\'') goto punc;
if(isdigit(v0)) goto punc;
// keep alnum alive
*t++ = '\'';
++s;
continue;
}

// punctuation
punc:
alnum = 0;
if(t > dest && prop&ACS_GS_ONEWORD) break;
if(o) o[t-dest] = s-acs_rb->cursor;

// check for repeat
if(prop&ACS_GS_REPEAT &&
c == s[1] &&
c == s[2] &&
c == s[3] &&
c == s[4]) {
char reptoken[60];
const char *pname = acs_getpunc(c); /* punctuation name */
if(pname) {
strncpy(reptoken, pname, 30);
reptoken[30] = 0;
} else {
reptoken[0] = c1;
reptoken[1] = 0;
}
strcat(reptoken, lengthword[acs_lang]);
for(j=5; c == s[j]; ++j)  ;
sprintf(reptoken+strlen(reptoken), "%d", j);
l = strlen(reptoken);
if(t+l+2 > destend) break; // no room
if(t > dest && t[-1] != ' ')
*t++ = ' ';
for(l=0; reptoken[l]; ++l)
*t++ = reptoken[l];
*t++ = ' ';
spaces = 1;
s += j;
if(prop & ACS_GS_ONEWORD) break;
continue;
}

// just a punctuation mark on its own
*t++ = c;
++s;
if(prop & ACS_GS_ONEWORD) break;
} // loop over characters in the tty buffer

*t = 0;
if(o) o[t-dest] = s-acs_rb->cursor;

return 0;
} /* acs_getsentence */

