/* clicksamples.c: generate clicks and chirps for ascii characters,
 * as wave samples.  I had a version that actually hit the pc speaker,
 * but I have (sadly) lost the source.
 *
 * This is a test version of the functionality of ttyclicks.c
 * in the drrivers directory, though ttyclicks.c is a lodable module
 * that uses the pc inbuilt speaker, not the sound card.
 * Of course some computers redirect the toggle speaker into the sound card,
 * but the ttyclicks module has no way to know that.
 * Anyways, use this program to get an idea of what console output
 * will sound like when that module is built and installed. */

#define USE_LIBAO
// apt install libao-dev

#include <stdio.h>
#include <stdlib.h>
#ifdef USE_LIBAO
#include <ao/ao.h>
#endif

/*
This program generates clicks and chirps on stdout.
Send it some text via stdin, and it'll represent that text with sound.
Sound parameters: 44100 kHZ, 1 channel, 16-bit signed.
Use tools such as sox to convert to .wav or play on a soundcard.
Yes, I should probably generate .wav, instead of a headerless format.
PS.  Just added soundcard I/O with libao.  If you have libao installed
and properly configured, compile with -DUSE_LIBAO, to use the soundcard.
Nothing is written to stdout in this case.
*/

void gen_samples(int);
void toggle(void);
void click(void);
void chirp(void);
void pause(void);
void char2click(int);

static short state = 0;

void toggle(void) {
state ^= 0x7fff;
}

/* Sorry for the raw numbers in the code.  Derived from experimentation. */
void click(void) {
toggle();
gen_samples(24);
toggle();
gen_samples(144);
}

void chirp(void) {
int i;
for(i = 54; i >= 0; i -= 2) {
toggle();
gen_samples(i);
}
}

void pause(void) {
gen_samples(168);
}

void char2click(int ch) {
if(ch == '\n')
chirp();
else
if((ch > ' ') && (ch <= '~')) /* Printable.  Assumes ASCII. */
click();
else
pause();
}

#ifdef USE_LIBAO
ao_device *device;
#endif

void gen_samples(int numSamples) {
short *buf = malloc(numSamples * sizeof(short));
int i;
if(buf == NULL) {
fprintf(stderr, "Out of memory.  Terminating.\n");
exit(1);
} 
for(i = 0; i < numSamples; i++)
buf[i] = state;
#ifdef USE_LIBAO
int success = ao_play(device, (char *) buf, numSamples * sizeof(short));
if(!success) {
fprintf(stderr, "Error writing audio to the soundcard...\n");
exit(1);
}
#else
fwrite(buf, sizeof(short), numSamples, stdout);
#endif
}

int main(int argc, char *argv[]) {
int ch;

#ifdef USE_LIBAO
ao_sample_format fmt;
fmt.bits = sizeof(short) * 8;
fmt.channels = 1;
fmt.rate = 44100;
fmt.byte_format = AO_FMT_NATIVE;
ao_initialize();
int driver_id = ao_default_driver_id();
if(driver_id < 0) {
fprintf(stderr, "Unable to find default audio device.\n");
exit(1);
}
device = ao_open_live(driver_id, &fmt, NULL);
if(device == NULL) {
fprintf(stderr, "Error opening audio device.\n");
exit(1);
}
#endif

while((ch = getchar()) != EOF)
char2click(ch);

#ifdef USE_LIBAO
ao_close(device);
ao_shutdown();
#endif
}
