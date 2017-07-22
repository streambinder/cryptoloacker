.DEFAULT_GOAL = all

SRVR=cryptoloackerd
CLNT=cryptoloacker

ifeq ($(OS),Windows_NT)
	PLATFORM=windows
	PLATFORM_EXC=linux
else
	PLATFORM=linux
	PLATFORM_EXC=windows
endif

ABSPATH=${CURDIR}

CDIR=$(ABSPATH)/src
IDIR=$(CDIR)/include/common
IDIR_P=$(CDIR)/include/$(PLATFORM)
BDIR=$(ABSPATH)/bin
ODIR=$(ABSPATH)/obj
TDIR=$(ABSPATH)/tmp

CC=gcc
CFLAGS=-O -lm -lpthread -fopenmp $(ADDITIONAL)

$(ODIR)/%:
	$(eval SRC := $(shell echo $@ | sed 's/\.o/\.c/g' | sed "s/\/obj\//\/tmp\//g"))
	$(CC) -c -o $@ $(SRC) -I$(TDIR)/$(TRGT) $(CFLAGS)

$(BDIR)/%:
	$(eval HDRS := $(shell find $(CDIR)/$(TRGT) -name '*.h' | grep -v "$(PLATFORM_EXC)"))
	$(eval HDRS := $(HDRS) $(shell find $(CDIR)/common -name '*.h' | grep -v "$(PLATFORM_EXC)"))
	$(eval SRCS := $(shell find $(CDIR)/$(TRGT) -name '*.c' | grep -v "$(PLATFORM_EXC)"))
	$(eval SRCS := $(SRCS) $(shell find $(CDIR)/common -name '*.c'))
	$(eval TMPS := $(SRCS:$(CDIR)%=$(TDIR)%))
	$(eval TMPS := $(shell echo $(TMPS) | sed 's/include\/common\///g' | sed 's/include\/$(PLATFORM)\///g' | sed 's/\/common\//\/$(TRGT)\//g'))
	$(eval OBJS := $(TMPS:$(TDIR)%.c=$(ODIR)%.o))
	@cp $(SRCS) $(HDRS) $(TDIR)/$(TRGT)/
	@$(MAKE) TRGT=$@ $(OBJS)
	$(CC) -o $@ $(OBJS) $(CFLAGS) -I$(TDIR)/$(TRGT) $(LIBS)

server:
	@$(MAKE) pre-build
	@$(MAKE) TRGT=$@ $(BDIR)/$(SRVR)
	@$(MAKE) post-build

client:
	@$(MAKE) pre-build
	@$(MAKE) TRGT=$@ $(BDIR)/$(CLNT)
	@$(MAKE) post-build

pre-build:
	@mkdir -p {$(BDIR),$(ODIR)/{server,client},$(TDIR)/{server,client}}

post-build:
	@rm -rf $(TDIR)

debug:
	@$(MAKE) ADDITIONAL=-g all

all: server client

clean:
	@rm -rf $(BDIR) $(ODIR) $(TDIR)
