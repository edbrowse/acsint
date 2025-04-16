BRIDGEPATH = ../bridge
DRIVERPATH = ../drivers

DEPFLAG = -MMD
CFLAGS += $(DEPFLAG)

ifneq "$(BRIDGEPATH)" ""
CFLAGS += -I$(BRIDGEPATH)
LDFLAGS += -L$(BRIDGEPATH)
endif

ifneq "$(DRIVERPATH)" ""
	CFLAGS += -I$(DRIVERPATH)
	endif

LDLIBS = -lacs

SRCS = acstest.c pipetest.c 

all : acstest pipetest

acstest : acstest.o

pipetest : pipetest.o

-include $(SRCS:.c=.d)

clicksamples : clicksamples.c
	cc clicksamples.c -o clicksamples -lao

