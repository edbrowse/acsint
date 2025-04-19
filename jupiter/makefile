#  Makefile for the jupiter speech adapter

prefix = /usr/local
exec_prefix = ${prefix}
bindir = ${exec_prefix}/bin

LDFLAGS = -s
ifneq ($(JDEB),)
LDFLAGS =
CFLAGS += -g -ggdb
endif

DRIVERPATH = ../drivers
#ifneq "$(DRIVERPATH)" ""
#	CFLAGS += -I$(DRIVERPATH)
#	endif

SRCS = jupiter.c tpxlate.c acsbind.c acsbridge.c acstalk.c
OBJS = $(SRCS:.c=.o)

all : jupiter

jupiter : $(OBJS)
	cc $(LDFLAGS) -o jupiter $(OBJS) -lao

clean :
	rm -f $(OBJS) jupiter

install : jupiter
	install -d ${bindir}
	install jupiter ${bindir}

uninstall :
	rm -f ${bindir}/jupiter

-include $(SRCS:.c=.d)
