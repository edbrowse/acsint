// injkey: inject a key into another tty
// this has to run as root; you might have to chmod 4755 injkey

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <asm/termbits.h>

int main(int argc, char **argv)
{
const char *s;
if(argc != 3) {
fprintf(stderr, "Usage: injkey console_number string\n");
fprintf(stderr, "Use 0 to queue up characters in the current tty.\n");
exit(1);
}
char ttybuf[20];
sprintf(ttybuf, "/dev/tty%s", argv[1]);
int fd = open(ttybuf, O_RDWR);
if(fd < 0) {
perror("cannot open tty");
exit(2);
}
setuid(geteuid());
for(s = argv[2]; *s; ++s) {
if(ioctl(fd, TIOCSTI, s)) {
perror("cannot inject character into the tty\n");
exit(3);
}
}
close(fd);
exit(0);
}
