// injkey: inject a key into another tty
// this has to run as root; you might have to chmod 4755 injkey

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <asm/termbits.h>
// my compiler barfs if I include both termbits and termios

extern int tcgetattr(int, struct termios*);
extern int tcsetattr(int, int, struct termios*);

static char noecho = 0;
static struct termios ttbuf;

int main(int argc, char **argv)
{
const char *s;
char tttystring[20];
if(argc > 1 && !strcmp(argv[1], "-e"))
++argv, --argc, noecho = 1;
if(argc != 3) {
fprintf(stderr, "Usage: injkey [-e] console_number string\n");
fprintf(stderr, "Use 0 to queue up characters in the foreground console.\n");
exit(1);
}
sprintf(tttystring, "/dev/tty%s", argv[1]);
int fd = open(tttystring, O_RDWR);
if(fd < 0) {
perror("cannot open tty");
exit(2);
}
setuid(geteuid());
if(noecho) {
tcgetattr(fd, &ttbuf);
if(ttbuf.c_lflag & ECHO) {
ttbuf.c_lflag &= ~ECHO;
	tcsetattr(fd, TCSANOW, &ttbuf);
} else noecho = 0;
}
for(s = argv[2]; *s; ++s) {
if(ioctl(fd, TIOCSTI, s)) {
perror("cannot inject character into the tty\n");
exit(3);
}
}
if(noecho) {
ttbuf.c_lflag |= ECHO;
	tcsetattr(fd, TCSANOW, &ttbuf);
}
close(fd);
exit(0);
}
