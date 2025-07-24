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
if(argc != 3) {
fprintf(stderr, "Usage: injkey console_number character\n");
exit(1);
}
char ttybuf[20];
sprintf(ttybuf, "/dev/tty%s", argv[1]);
int fd = open(ttybuf, O_RDWR);
if(fd < 0) {
perror("cannot open tty");
exit(2);
}
if(ioctl(fd, TIOCSTI, argv[2])) {
perror("cannot inject character into the tty\n");
exit(3);
}
close(fd);
exit(0);
}
