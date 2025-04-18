#  Makefile for the jupiter speech adapter

prefix = /usr/local
exec_prefix = ${prefix}
bindir = ${exec_prefix}/bin

CFLAGS += -MMD

LDFLAGS = -s
ifneq ($(JDEB),)
LDFLAGS =
CFLAGS += -g -ggdb
endif

DRIVERPATH = ../drivers
#ifneq "$(DRIVERPATH)" ""
#	CFLAGS += -I$(DRIVERPATH)
#	endif

ACSLIB = ../bridge/libacs.a

SRCS = jupiter.c tpxlate.c
OBJS = $(SRCS:.c=.o)

all : jupiter

jupiter : $(OBJS) $(ACSLIB)
	cc $(LDFLAGS) -o jupiter $(OBJS) $(ACSLIB) -lao

clean :
	rm -f $(OBJS) jupiter

install : jupiter
	install -d ${bindir}
	install jupiter ${bindir}

uninstall :
	rm -f ${bindir}/jupiter

-include $(SRCS:.c=.d)
