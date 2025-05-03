/*********************************************************************
File: acstalk.c
Description: communicate with a synthesizer over a serial port, socket, or pipe.
*********************************************************************/

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <ctype.h>
#include <unistd.h>
#include <stdarg.h>
#include <signal.h>

#include "acsbridge.h"

#define stringEqual !strcmp

#define SSBUFSIZE 64 // synthesizer buffer for events
static char ss_inbuf[SSBUFSIZE]; /* input buffer for the synthesizer */

/* The file descriptors to and from the synth. */
/* These are the same for serial port or socket; different if over a pipe. */
int acs_sy_fd0 = -1, acs_sy_fd1 = -1;

static int fifo_fd = -1; /* file descriptor for the interprocess fifo */
static char *ipmsg; /* interprocess message */
acs_fifo_handler_t acs_fifo_h;

/* parent process, if a child is forked to manage the software synth. */
pid_t pss_pid;
/* Set this for broken pipe - need to respawnw the child */
int acs_pipe_broken;

/* What is the style of synthesizer? */
int acs_style;

/* current synth parameters */
int acs_curvolume, acs_curpitch, acs_currange, acs_curspeed, acs_curvoice;

/* What are the speech parameters when the unit is first turned on? */
void
acs_style_defaults(void)
{
acs_curvoice = 0;
acs_curspeed = acs_curvolume = acs_currange = 5;
acs_curpitch = 3;

switch(acs_style) {
case ACS_SY_STYLE_DOUBLE:
case ACS_SY_STYLE_ESPEAKUP:
acs_curpitch = 4;
break;

case ACS_SY_STYLE_BNS:
case ACS_SY_STYLE_ACE:
acs_curvolume = 7;
break;

} // switch
}

/* Handler - as index markers are returned to us */
acs_imark_handler_t acs_imark_h;

/* send return to the synth - start speaking */
static const char kbyte = '\13';
static const char crbyte = '\r';
static void ss_cr(void)
{
if(acs_style == ACS_SY_STYLE_DECEXP || acs_style == ACS_SY_STYLE_DECPC)
write(acs_sy_fd1, &kbyte, 1);
write(acs_sy_fd1, &crbyte, 1);
}

/* The start of the sentence that is sent with index markers. */
unsigned int *acs_imark_start;

/* location of each index marker relative to acs_imark_start */
static acs_ofs_type imark_loc[100];
static int imark_first, imark_end;
static int bnsf; // counting control f's from bns

/* move the cursor to the returned index marker. */
static void indexSet(int n)
{
if(acs_style == ACS_SY_STYLE_BNS || acs_style == ACS_SY_STYLE_ACE) {
++bnsf;
n = imark_end + bnsf;
} else {
n -= imark_first;
}

if(!acs_imark_start) return;
if(!acs_rb) return;
if(n < 0 || n >= imark_end) return;

acs_rb->cursor = acs_imark_start + imark_loc[n];
acs_log("imark %d cursor now base+%d\n", n, imark_loc[n]);

/* should never be past the end of buffer, but let's check */
if(acs_rb->cursor >= acs_rb->end) {
acs_rb->cursor = acs_rb->end;
if(acs_rb->end > acs_rb->start) --acs_rb->cursor;
acs_imark_start = 0;
acs_log("cursor ran past the end of buffer\n");
}

if(n == imark_end - 1) {
/* last index marker, sentence is finished */
acs_log("sentence spoken\n");
acs_imark_start = 0;
}

if(acs_imark_h) (*acs_imark_h)(n+1, imark_end);
}

static struct termios tio; // tty io control

/* Set up tty with either hardware or software flow control */
static unsigned int thisbaud = B9600;
int acs_serial_flow(int hw)
{
tio.c_iflag = IGNBRK | ISTRIP | IGNPAR;
if(!hw) tio.c_iflag |= IXON | IXOFF;
tio.c_oflag = 0;
// Ok PARENB causes trouble for some doubletalk units, so I leave it out.
// But hmm - maybe other external synths require parity.
// Maybe it should depend on acs_style.
tio.c_cflag = HUPCL | CS8 | CREAD | thisbaud | CLOCAL;
if(hw) tio.c_cflag |= CRTSCTS;
tio.c_lflag = 0;
tio.c_cc[VSTOP] = 17;
tio.c_cc[VSTART] = 19;
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 0;

return tcsetattr(acs_sy_fd0, TCSANOW, &tio);
}

/* Get status lines from the serial device.
 * Active status lines are indicated with the following bit definitions:
 * TIOCM_RTS Request To Send (output signal)
 * TIOCM_DTR Data Terminal Ready (output signal)
 * TIOCM_CAR Data Carrier Detect (input signal)
 * TIOCM_RNG Ring Indicator (input signal)
 * TIOCM_DSR Data Set Ready (input signal)
 * TIOCM_CTS Clear To Send (input signal) */
static int
getModemStatus(void)
{
int sigs, rc;
rc = ioctl(acs_sy_fd0, TIOCMGET, &sigs);
if(rc) sigs = 0;
return sigs;
/* We should never need to do this one.
sigs = TIOCM_RTS | TIOCM_DTR;
rc = ioctl(fd, TIOCMBIS, &sigs);
*/
}

/* For debugging */
static void
printModemStatus(void)
{
int sigs = getModemStatus();
printf("serial");
if(sigs&TIOCM_DSR) printf(" dsr");
if(sigs&TIOCM_CAR) printf(" carrier");
if(sigs&TIOCM_RNG) printf(" ring");
if(sigs&TIOCM_CTS) printf(" cts");
puts("");
}

int acs_serial_open(const char *devname, int baud)
{
static const int baudvalues[] = {
1200,
2400,
4800,
9600,
19200,
38400,
115200,
0};
static unsigned int baudbits[] = {
B1200,
B2400,
B4800,
B9600,
B19200,
B38400,
B115200,
0};
int j;

if(acs_sy_fd0 >= 0) {
// already open
errno = EEXIST;
return -1;
}

acs_sy_fd0 = open(devname, O_RDWR|O_NONBLOCK|O_CLOEXEC);
if(acs_sy_fd0 < 0) return 0;
acs_sy_fd1 = acs_sy_fd0;

// Set up the tty characteristics.
// Especially important to have no echo and no cooked mode and clocal.
for(j=0; baudvalues[j]; ++j)
if(baud == baudvalues[j]) thisbaud = baudbits[j];
// Hardware flow by default, but you can change that.
if(acs_serial_flow(1)) {
// ioctl failure ; don't understand.
// Hope errno helps.
close(acs_sy_fd0);
acs_sy_fd0 = acs_sy_fd1 = -1;
return -1;
}

/* Now that clocal is set, go back to blocking mode
 * In other words, clear the nonblock bit.
 * The other bits can all be zero, they don't mean anything on a serial port. */
fcntl(acs_sy_fd0, F_SETFL, 0);

	// Send an initial CR.
		// Some units like to see this to establish baud rate.
usleep(100000);
write(acs_sy_fd1, &crbyte, 1);
usleep(100000);

return 0;
}

void acs_sy_close(void)
{
if(acs_sy_fd0 < 0) return; // already closed
close(acs_sy_fd0);
if(acs_sy_fd1 != acs_sy_fd0)
close(acs_sy_fd1);
acs_sy_fd0 = acs_sy_fd1 = -1;
}

static fd_set channels;

/*********************************************************************
Ok, we have some splainin to do.
There is a race condition in the linux kernel that seems to have no workaround.
It affects my acsint device driver.
I wait for something to happen then respond to it,
and something else could happen before I respond, and it's too complicated to explain here.
The effect is, a key could be struck but select doesn't see it.
I use to work around it by
	echo wake up >/etc/jupiter/fifo
That sends another event through the kernel, and select sees it,
and the console wakes up and everybody's happy.
But that's pretty awkward.
Now I add a timeout to the select.
So this daemon wakes up a couple times a second, that's not a big price to pay.
*********************************************************************/

int acs_wait(void)
{
int rc;
int nfds;
struct timeval now;

memset(&channels, 0, sizeof(channels));
FD_SET(acs_fd, &channels);
if(acs_sy_fd0 >= 0)
FD_SET(acs_sy_fd0, &channels);
if(fifo_fd >= 0)
FD_SET(fifo_fd, &channels);

nfds = acs_fd;
if(acs_sy_fd0 > nfds) nfds = acs_sy_fd0;
if(fifo_fd > nfds) nfds = fifo_fd;
++nfds;
now.tv_sec = 0;
now.tv_usec = 400000;
rc = select(nfds, &channels, 0, 0, &now);

if(rc < 0) return 0; // should never happen

rc = 0;
if(FD_ISSET(acs_fd, &channels)) rc |= 1;
if(acs_sy_fd0 >= 0 && FD_ISSET(acs_sy_fd0, &channels)) rc |= 2;
if(fifo_fd >= 0 && FD_ISSET(fifo_fd, &channels)) rc |= 4;
return rc;
}

int acs_sy_events(void)
{
int nr; // number of bytes read
int i;
static int leftover = 0;

if(acs_sy_fd0 < 0) {
errno = ENXIO;
return -1;
}

nr = read(acs_sy_fd0, ss_inbuf+leftover, SSBUFSIZE-leftover);
acs_log("synth read %d bytes\n", nr);
if(nr < 0) return -1;

i = 0;
nr += leftover;
while(i < nr) {
char c = ss_inbuf[i];

switch(acs_style) {
case ACS_SY_STYLE_DOUBLE:
case ACS_SY_STYLE_ESPEAKUP:
if(c >= 1 && c <= 99) {
acs_log("index %d\n", c);
indexSet(c);
++i;
continue;
}
acs_log("unknown byte %d\n", c);
++i;
break;

case ACS_SY_STYLE_DECPC: case ACS_SY_STYLE_DECEXP:
// This is butt ugly compared to the Doubletalk!
if(c == '\33') {
if(nr-i < 8) break;
if(!strncmp(ss_inbuf+i, "\33P0;32;", 7)) {
if(ss_inbuf[i+7] == 'z') {
acs_log("index %d\n", 0);
indexSet(0);
i += 8;
continue;
}
if(nr-i < 9) break;
if(ss_inbuf[i+8] == 'z' && isdigit((unsigned char)ss_inbuf[i+7])) {
c = ss_inbuf[i+7] - '0';
acs_log("index %d\n", c);
indexSet(c);
i += 9;
continue;
}
if(nr-i < 10) break;
if(ss_inbuf[i+9] == 'z' && isdigit((unsigned char)ss_inbuf[i+7]) && isdigit((unsigned char)ss_inbuf[i+8])) {
c = ss_inbuf[i+7] - '0';
c = 10*c + ss_inbuf[i+8] - '0';
acs_log("index %d\n", c);
indexSet(c);
i += 10;
continue;
}
}
}
acs_log("unknown byte %d\n", c);
++i;
break;

case ACS_SY_STYLE_BNS:
case ACS_SY_STYLE_ACE:
if(c == 6) {
acs_log("index f\n", 0);
indexSet(0);
++i;
continue;
}
acs_log("unknown byte %d\n", c);
++i;
break;

default:
acs_log("no style, synth data discarded\n", 0);
leftover = 0;
return -1;
} // switch

} // looping through input characters

leftover = nr - i;
if(leftover) memmove(ss_inbuf, ss_inbuf+i, leftover);

return 0;
}

static void ip_more(void); /* more data for an interprocess message */

int acs_all_events(void)
{
int source = acs_wait();
if(source&4) ip_more();
if(source&2) acs_sy_events();
if(source&1) acs_events();
}

/* string has to be ascii or utf8 */
void acs_say_string(const char *s)
{
int l = strlen(s);
if(l) write(acs_sy_fd1, s, l);
ss_cr();
}

void acs_say_string_n(const char *s)
{
int l = strlen(s);
if(l) write(acs_sy_fd1, s, l);
}

void acs_say_char(unsigned int c)
{
const char *s = acs_getpunc(c);
if(s) acs_say_string_n(s);
else
acs_write_mix(acs_sy_fd1, &c, 1);
ss_cr();
}

void acs_say_string_uc(const unsigned int *s)
{
int l = acs_unilen(s);
if(l) acs_write_mix(acs_sy_fd1, s, l);
ss_cr();
}

void acs_say_indexed(const unsigned int *s, const acs_ofs_type *o, int mark)
{
const unsigned int *t;
char ibuf[30]; // index mark buffer
const acs_ofs_type *o0 = o;

acs_imark_start = 0;
if(acs_rb) acs_imark_start = acs_rb->cursor;
imark_end = 0;

if(acs_style == ACS_SY_STYLE_BNS || acs_style == ACS_SY_STYLE_ACE) mark = 0;
imark_first = mark;

t = s;
while(1) {
if(*o && mark >= 0 && mark <= 100) { // mark here
// have to send the prior word
if(s > t)
acs_write_mix(acs_sy_fd1, t, s-t);
t = s;
// set the index marker
imark_loc[imark_end++] = *o;
// send the index marker
ibuf[0] = 0;
switch(acs_style) {
case ACS_SY_STYLE_DOUBLE:
/* The following if statement addresses a bug that is, as far as I know,
 * specific to doubletalk.
 * We can't send a single letter, and then an index marker
 * on the next word.  It screws everything up!
 * But we have to send it if that is the end of the sentence. */
if(o-o0 > 2 || !*s)
sprintf(ibuf, "\1%di", mark);
break;
case ACS_SY_STYLE_ESPEAKUP:
sprintf(ibuf, "<mark name=\"%d\"/>", mark);
break;
case ACS_SY_STYLE_BNS:
case ACS_SY_STYLE_ACE:
strcpy(ibuf, "\06");
-- bnsf; // you owe me another control f
break;
case ACS_SY_STYLE_DECPC: case ACS_SY_STYLE_DECEXP:
/* Send this the most compact way we can - 9600 baud can be kinda slow. */
sprintf(ibuf, "[:i r %d]", mark);
break;
} // switch
if(ibuf[0])
write(acs_sy_fd1, ibuf, strlen(ibuf));
++mark;
}
if(!*s) break;
++s, ++o;
}

/* End of string should always be an index marker,
 * so there should be nothing else to send.
 * But just in case ... */
if(s > t)
acs_write_mix(acs_sy_fd1, t, s-t);

ss_cr();
acs_log("sent %d markers, last offset %d\n", imark_end, imark_loc[imark_end-1]);
}

void acs_shutup(void)
{
char ibyte; // interrupt byte

switch(acs_style) {
case ACS_SY_STYLE_DOUBLE:
case ACS_SY_STYLE_ESPEAKUP:
case ACS_SY_STYLE_BNS:
case ACS_SY_STYLE_ACE:
ibyte = 24;
break;
default:
ibyte = 3;
break;
} // switch

write(acs_sy_fd1, &ibyte, 1);

acs_imark_start = 0;
bnsf = 0;
acs_log("shutup\n");
}

static void
ss_writeString(const char *s)
{
write(acs_sy_fd1, s, strlen(s));
}

int acs_setvolume(int n)
{
static char doublestring[] = "\01xv";
static char dtpcstring[] = "[:vo set dd]";
static char extstring[] = "[:dv g5 dd]";
static char bnsstring[10];
static char acestring[] = "\33A5";
int n0 = n;

if(n < 0 || n > 9) return -1;

switch(acs_style) {
case ACS_SY_STYLE_DOUBLE:
case ACS_SY_STYLE_ESPEAKUP:
doublestring[1] = '0' + n;
ss_writeString(doublestring);
break;

case ACS_SY_STYLE_DECPC:
n = 10 + 8*n;
dtpcstring[9] = '0' + n/10;
dtpcstring[10] = '0' + n%10;
ss_writeString(dtpcstring);
break;

case ACS_SY_STYLE_DECEXP:
/* The Dec Express takes volume levels from 60 to 86. */
/* This code gives a range from 60 to 85. */
n = 60 + n*72/25;
extstring[8] = '0' + n / 10;
extstring[9] = '0' + n % 10;
ss_writeString(extstring);
break;

case ACS_SY_STYLE_BNS:
sprintf(bnsstring, "\x05%02dV", (n+1) * 16 / 10);
ss_writeString(bnsstring);
break;

case ACS_SY_STYLE_ACE:
acestring[2] = '0' + n;
ss_writeString(acestring);
break;

default:
return -2;
} // switch

acs_curvolume = n0;
return 0;
}

int acs_incvolume(void)
{
if(acs_curvolume == 9) return -1;
if(acs_setvolume(acs_curvolume+1))
return -2;
return 0;
}

int acs_decvolume(void)
{
if(acs_curvolume == 0) return -1;
if(acs_setvolume(acs_curvolume-1))
return -2;
return 0;
}

int acs_setspeed(int n)
{
static char doublestring[] = "\1xs\1xa";
static char decstring[] = "[:ra ddd]";
static char bnsstring[10];
static char acestring[] = "\33R5";
static const char acerate[] ="02468ACEGH";
int n0 = n;

if(n < 0 || n > 9) return -1;

switch(acs_style) {
case ACS_SY_STYLE_DOUBLE:
case ACS_SY_STYLE_ESPEAKUP:
doublestring[1] = doublestring[4] = '0' + n;
ss_writeString(doublestring);
break;

case ACS_SY_STYLE_DECEXP: case ACS_SY_STYLE_DECPC:
n = 50*n + 120;
sprintf(decstring+5, "%03d]", n);
ss_writeString(decstring);
break;

case ACS_SY_STYLE_BNS:
sprintf(bnsstring, "\x05%02dE", (n+1) * 14 / 10);
ss_writeString(bnsstring);
break;

case ACS_SY_STYLE_ACE:
acestring[2] = acerate[n];
ss_writeString(acestring);
break;

default:
return -2;
} // switch

acs_curspeed = n0;
return 0;
}

int acs_incspeed(void)
{
if(acs_curspeed == 9) return -1;
if(acs_setspeed(acs_curspeed+1))
return -2;
return 0;
}

int acs_decspeed(void)
{
if(acs_curspeed == 0) return -1;
if(acs_setspeed(acs_curspeed-1))
return -2;
return 0;
}

int acs_setpitch(int n)
{
static char doublestring[] = "\01xxp";
static char espeakupstring[] = "\01xp";
static const short tohurtz[] = {
66, 80, 98, 120, 144, 170, 200, 240, 290, 340};
static char decstring[] = "[:dv ap xxx]";
static char bnsstring[10];
static char acestring[] = "\33P5";
int n0 = n;

if(n < 0 || n > 9) return -1;

switch(acs_style) {
case ACS_SY_STYLE_DOUBLE:
n = 9*n + 10;
doublestring[1] = '0' + n/10;
doublestring[2] = '0' + n%10;
ss_writeString(doublestring);
break;

case ACS_SY_STYLE_ESPEAKUP:
espeakupstring[1] = '0' + n;
ss_writeString(espeakupstring);
break;

case ACS_SY_STYLE_DECEXP: case ACS_SY_STYLE_DECPC:
n = tohurtz[n];
sprintf(decstring+8, "%d]", n);
ss_writeString(decstring);
break;

case ACS_SY_STYLE_BNS:
/* BNS pitch is 01 through 63.  An increment of 6, giving levels from 6 . 60
should work well. */
sprintf(bnsstring, "\x05%02dP", (n+1) * 6);
ss_writeString(bnsstring);
break;

case ACS_SY_STYLE_ACE:
acestring[2] = '0' + n;
ss_writeString(acestring);
break;

default:
return -2;
} // switch

acs_curpitch = n0;
return 0;
}

int acs_incpitch(void)
{
if(acs_curpitch == 9) return -1;
if(acs_setpitch(acs_curpitch+1))
return -2;
return 0;
}

int acs_decpitch(void)
{
if(acs_curpitch == 0) return -1;
if(acs_setpitch(acs_curpitch-1))
return -2;
return 0;
}

int acs_setrange(int n)
{
static char doublestring[] = "\01xe";
static char espeakupstring[] = "\01xr";

if(n < 0 || n > 9) return -1;

switch(acs_style) {
case ACS_SY_STYLE_DOUBLE:
doublestring[1] = '0' + n;
ss_writeString(doublestring);
break;

case ACS_SY_STYLE_ESPEAKUP:
espeakupstring[1] = '0' + n;
ss_writeString(espeakupstring);
break;

default:
return -2;
} // switch

acs_currange = n;
return 0;
}

int acs_incrange(void)
{
if(acs_currange == 9) return -1;
if(acs_setrange(acs_currange+1))
return -2;
return 0;
}

int acs_decrange(void)
{
if(acs_currange == 0) return -1;
if(acs_setrange(acs_currange-1))
return -2;
return 0;
}

/* Changing voice could reset the pitch */
// Return -1 if the synthesizer cannot support that voice.
int acs_setvoice(int v)
{
	char buf[8];
static const short doublepitch[] = {
2,4,2,4,6,4,5,1,8,2};
static const char decChars[] = "xphfdburwk";
static const short decpitch[] = {
-1,3,1,4,3,6,7,6,2,8};
static char acestring[] = "\33V5";

switch(acs_style) {
case ACS_SY_STYLE_DOUBLE:
case ACS_SY_STYLE_ESPEAKUP:
if(v < 1 || v > 8) return -1;
		sprintf(buf, "\1%do", v-1);
		ss_writeString(buf);
ss_cr();
acs_curpitch = doublepitch[v];
break;

case ACS_SY_STYLE_DECEXP: case ACS_SY_STYLE_DECPC:
if(v < 1 || v > 8) return -1;
		sprintf(buf, "[:n%c]", decChars[v]);
		ss_writeString(buf);
ss_cr();
acs_curpitch = decpitch[v];
break;

case ACS_SY_STYLE_ACE:
acestring[2] = '0' + v;
ss_writeString(acestring);
break;

default:
return -2; /* no voice function for this synth */
} // switch

return 0;
}

/* Would the synth block if we sent it more text? */
int ss_blocking(void)
{
int rc;
int nfds;
struct timeval now;

memset(&channels, 0, sizeof(channels));
FD_SET(acs_sy_fd1, &channels);
now.tv_sec = 0;
now.tv_usec = 0;
nfds = acs_sy_fd1 + 1;
rc = select(nfds, 0, &channels, 0, &now);
if(rc < 0) return 0; // should never happen
rc = 0;
if(FD_ISSET(acs_sy_fd1, &channels)) rc = 1;
return rc ^ 1;
}

/* Is the synth still talking? */
int acs_stillTalking(void)
{
/* If we're blocked then we're definitely still talking. */
if(ss_blocking()) return 1;

/* Might put in some special code for doubletalk,
 * as they use ring indicator to indicate speech in progress.
 * Maybe later. */

/* If there is no index marker, then we're not speaking a sentence.
 * Just a word or letter or command confirmation phrase.
 * We don't need to interrupt that, and it's ok to send the next thing. */
if(!acs_imark_start) return 0;

/* If we have reached the last index marker, the sentence is done.
 * Or nearly done - still speaking the last word.
 * In that case acs_imark_start should be 0, and we shouldn't be here.
 * But we are here, so return 1. */
return 1;
}

/* Signal handler, to watch for broken pipe, or death of child,
 * which ever is more convenient. */

static void sig_h (int n)
{
int status;
/* You have to call wait to properly dispose of the defunct child process. */
wait(&status);
acs_pipe_broken = 1;
}

/* Spin off the child process with a vector of args */
/* I wanted to use const char * const, but that's not how execvp works. */
int acs_pipe_openv(const char *progname,  char * const  alist[])
{
int p0[2]; /* pipe reading acs_sy_fd0 */
int p1[2]; /* pipe writing acs_sy_fd1 */

if (pipe(p0) == -1 || pipe(p1) == -1)
return -1;

pss_pid = fork();
switch(pss_pid) {
case -1:
perror("fork");
return -1;

case 0: /* child */
if (dup2(p0[1], STDOUT_FILENO) == -1 ||
dup2(p1[0], STDIN_FILENO) == -1) {
perror("dup2");
exit(1);
}
/* close the unneeded fds */
close(p0[0]);
close(p1[1]);
if(p0[1] != STDOUT_FILENO) close(p0[1]);
if(p1[0] != STDIN_FILENO) close(p1[0]);
/* run the program */
execvp(progname, alist);
perror("execvp");
exit(1);

default: /* parent */
acs_sy_fd0 = p0[0];
acs_sy_fd1 = p1[1];
close(p0[1]);
close(p1[0]);
} /* switch */

/* watch for broken pipe, indicating no child process */
signal(SIGPIPE, sig_h);
acs_pipe_broken = 0;

return 0;
}

/* This isn't like printfv; I don't have a string with percent directives
 * to tell me how many args you are passing, or the type of each arg.
 * So each arg must be a string, and you must end the list with NULL.
 * Unfortunately I have to repack everything to make arg0 the program name. */
#define MAX_ARGS 16
int acs_pipe_open(const char *progname, ...)
{
char * alist[MAX_ARGS+2];
int count = 0;
va_list ap;
va_start(ap, progname);
alist[count++] = (char*)progname;
while(count <= MAX_ARGS+1) {
alist[count] = va_arg(ap, char*);
if(!alist[count]) break;
++count;
}
alist[count] = 0;
va_end(ap);
return acs_pipe_openv(progname, alist);
}

int acs_pipe_system(const char *cmd)
{
if (!cmd) return -1;
return acs_pipe_open("/bin/sh", "-c", cmd, 0);
}

int acs_startfifo(const char *pathname)
{
if(fifo_fd >= 0) {
errno = EBUSY;
return -1;
}

fifo_fd = open(pathname, O_RDWR|O_CLOEXEC);
if(fifo_fd < 0)
return -1;

return 0;
}

void acs_stopfifo(void)
{
if(fifo_fd >= 0)
close(fifo_fd);
fifo_fd = -1;

if(ipmsg) free(ipmsg);
ipmsg = 0;
}

static void ip_more(void)
{
int i, nr;
char *s;
char buf[512];

nr = read(fifo_fd, buf, sizeof(buf));
/* don't know why nr would ever be <= 0 */
if(nr <= 0) return;

/* no nulls in the message */
for(i=0; i<nr; ++i)
if(buf[i] == 0) buf[i] = ' ';

if(ipmsg) {
i = strlen(ipmsg);
ipmsg = realloc(ipmsg, i+nr+1);
if(!ipmsg) return;
memcpy(ipmsg+i, buf, nr);
ipmsg[i+nr] = 0;
} else {
ipmsg = malloc(nr+1);
if(!ipmsg) return;
memcpy(ipmsg, buf, nr);
ipmsg[nr] = 0;
}

/* send text a line at a time */
while(s = strchr(ipmsg, '\n')) {
*s = 0;
i = s - ipmsg;
if(i && acs_fifo_h) (*acs_fifo_h)(ipmsg);
++s;
nr = strlen(s);
memmove(ipmsg, s, nr+1);
}
}

