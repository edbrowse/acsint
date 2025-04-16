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
Sound parameters: 44100 kHZ, 1 channel, 16-bit signed.
Use tools such as sox to convert to .wav or play on a soundcard.
Yes, I should probably generate .wav, instead of a headerless stream.
Or, soundcard I/O with libao.  If you have libao installed
and properly configured, compile with -DUSE_LIBAO, to use the soundcard.
Nothing is written to stdout in this case.
apt install libao-dev
Usage: clicksamples [volume] [speed]
These are numbers from 1 to 9.
speed is actually the delay value, so higher speed is slower clicks.
A good test is clicksamples vol speed <clicksamples.c
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

static short speed = 3, vol = 5, state;

static void toggle(void)
{
state ^= (1000*vol);
}

#ifdef USE_LIBAO
static ao_device *device;
#endif

static void gen_samples(int numSamples)
{
short *buf = malloc(numSamples * sizeof(short));
int i;
if(buf == NULL) bailout("Out of memory.  Terminating.");
for(i = 0; i < numSamples; i++)
buf[i] = state;
#ifdef USE_LIBAO
int success = ao_play(device, (char *) buf, numSamples * sizeof(short));
if(!success) bailout("Error writing audio to the soundcard...");
#else
fwrite(buf, sizeof(short), numSamples, stdout);
#endif
}

static void click(void)
{
toggle();
gen_samples(8 * speed);
toggle();
gen_samples(36 * speed);
}

static void pause(void)
{
gen_samples(56 * speed);
}

// this one does not change with speed
static void chirp(void)
{
int i;
for(i = 54; i >= 0; i -= 2) {
toggle();
gen_samples(i);
}
}

// high unicodes generate one click per byte instead of one click per
// character, which isn't really right.
static void char2click(int ch)
{
if(ch == '\n') chirp();
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
fmt.rate = 44100;
fmt.byte_format = AO_FMT_NATIVE;
ao_initialize();
int driver_id = ao_default_driver_id();
if(driver_id < 0) bailout("Unable to find default audio device.");
device = ao_open_live(driver_id, &fmt, NULL);
if(device == NULL) bailout("Error opening audio device.");
#endif

while((ch = getchar()) != EOF)
char2click(ch);

#ifdef USE_LIBAO
ao_close(device);
ao_shutdown();
#endif
}
