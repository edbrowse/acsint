/*********************************************************************
clicksamples.c: generate clicks and chirps for ascii characters,
as wave samples.  I had a version that actually hit the pc speaker,
but I have (sadly) lost the source.
This is a test version of the functionality of ttyclicks.c
in the drrivers directory, though ttyclicks.c is a lodable module
that uses the pc inbuilt speaker, not the sound card.
Of course some computers redirect the toggle speaker into the sound card,
but the ttyclicks module has no way to know that.
Anyways, use this program to get an idea of what console output
will sound like when that module is built and installed.
This program generates clicks and chirps on stdout.
Send it some text via stdin, and it'll represent that text with sound.
Sound parameters: 11025 kHZ, 1 channel, 16-bit signed.
Use tools such as sox to convert to .wav or play on a soundcard.
Yes, I should probably generate .wav, instead of a headerless stream.
Or, soundcard I/O with libao.  If you have libao installed
and properly configured, compile with -DUSE_LIBAO, to use the soundcard.
Nothing is written to stdout in this case.
apt install libao-dev
Henceforth that is assumed.
make clicksamples
Usage: clicksamples [volume] [speed]
These are numbers from 1 to 9.
speed is actually the delay value, so higher speed is slower clicks.
A good test is clicksamples vol speed <clicksamples.c
In fact I'll put in a couple of control g bells  just for that purpose.
I play the jupiter start notes at the start.
I play the high beeps at the end, used by jupiter to indicate a boundary condition.
*********************************************************************/

#define USE_LIBAO

#include <stdio.h>
#include <stdlib.h>
#ifdef USE_LIBAO
#include <ao/ao.h>
#endif

static void bailout(const char *msg)
{
fprintf(stderr, "%s\n", msg);
exit(1);
}

static short speed = 3, vol = 5, state = 0;
#define SAMPRATE 11025

// the toggle speaker gives a square wave, but we can do a little better.
#define SINLENGTH 16
static const short sinwave[SINLENGTH] =
{500,691,892,962,1000,962,892,691,500,309,108,38,0,38,108,309,};

static void toggle(void)
{
state ^= 1;
}

#ifdef USE_LIBAO
static ao_device *device;
#endif

// 4 tenths of a second
static short samples[SAMPRATE*4/10];

static void playsamples(int numsamples)
{
#ifdef USE_LIBAO
int success = ao_play(device, (char *) samples, numsamples * sizeof(short));
if(!success) bailout("Error writing audio to the soundcard...");
#else
fwrite(buf, sizeof(short), numsamples, stdout);
#endif
}

static void gensamples(int numsamples)
{
int i;
for(i = 0; i < numsamples; i++)
samples[i] = state * 1000 * vol;
playsamples(numsamples);
}

static void click(void)
{
toggle();
gensamples(3 * speed);
toggle();
gensamples(9 * speed);
}

static void pause(void)
{
gensamples(12 * speed);
}

// this one does not change with speed
static void chirp(void)
{
int i;
for(i = 16; i >= 8; --i) {
toggle();
gensamples(i);
toggle();
gensamples(i);
toggle();
gensamples(i);
}
}

static void playnote(int hz, int duration)
{
int i, phase;
// duration is in jiffies
if(duration > 40) duration = 40;
duration = SAMPRATE * duration / 100;
for(i = 0; i < duration; ++i) {
phase = (i * hz * 32 / SAMPRATE + 1) / 2;
phase %= 16;
samples[i] = sinwave[phase] * vol;
}
playsamples(duration);
}

static void bell(void)
{
playnote(1800, 10);
// need a small pause if there are several bells in a row
playnote(0, 4);
}

#define MAXNOTES 20

static void playnotes(const short *notelist)
{
int j;
for(j=0; j<MAXNOTES; ++j) {
if(!notelist[2*j]) break;
playnote(notelist[2*j], notelist[2*j+1]);
}
}

static const short startnotes[] = { 		476,5, 530,5, 596,5, 662,5, 762,5, 858,5, 942,5, 0,0};

static int highbeeps(void)
{
static const short boundsnd[] = {
        2800,4,3300,3,0,0       };
playnotes(boundsnd);
}

static void playscale(int f1, int f2, int step, int duration)
{
while(1) {
// the native version has resolution of milliseconds, this has to be brought down to jiffies
playnote(f1, duration/10);
f1 = f1 * (100+step) / 100;
if(f1 < f2 && step < 0 || f1 > f2 && step > 0) break;
}
}

static void playharp(void)
{
playscale(4400,200,-10,40);
}

// high unicodes generate one click per byte instead of one click per
// character, which isn't really right.
static void char2click(int ch)
{
if(ch == '\n') chirp();
else if(ch == '\7') bell();
else if(ch > ' ') click();
else pause();
}

int main(int argc, char *argv[])
{
int ch;
char f;

if(argc > 1 && (f = argv[1][0]) >= '1' && f <= '9' && !argv[1][1])
vol = f - '0';
if(argc > 2 && (f = argv[2][0]) >= '1' && f <= '9' && !argv[2][1])
speed = f - '0';

#ifdef USE_LIBAO
ao_sample_format fmt;
fmt.bits = sizeof(short) * 8;
fmt.channels = 1;
fmt.rate = SAMPRATE;
fmt.byte_format = AO_FMT_NATIVE;
ao_initialize();
int driver_id = ao_default_driver_id();
if(driver_id < 0) bailout("Unable to find default audio device.");
device = ao_open_live(driver_id, &fmt, NULL);
if(device == NULL) bailout("Error opening audio device.");
#endif

playnotes(startnotes);
//playharp();
while((ch = getchar()) != EOF)
char2click(ch);
highbeeps();

#ifdef USE_LIBAO
ao_close(device);
ao_shutdown();
#endif
}
