.DEFAULT_GOAL = all

ifeq ($(OS),Windows_NT)
	PLATFORM=windows
else
	PLATFORM=linux
endif

ABSPATH=${CURDIR}

CDIR=$(ABSPATH)/src/$(PLATFORM)
IDIR=$(CDIR)/include

PRE_BDIR=$(ABSPATH)/bin
PRE_ODIR=$(ABSPATH)/obj
BDIR=$(PRE_BDIR)/$(PLATFORM)
ODIR=$(PRE_ODIR)/$(PLATFORM)

CC=gcc
CFLAGS=-I$(IDIR) -lpthread -fopenmp

prepare:
	@mkdir -p {$(BDIR),$(ODIR)}

$(ODIR)/%.o: $(CDIR)/%.c prepare
	$(CC) -g -O -c -o $@ $< $(CFLAGS)

server: $(ODIR)/server.o $(ODIR)/threadpool.o $(ODIR)/cipher.o
	gcc -o $(BDIR)/$@ $^ $(CFLAGS) $(LIBS)

client: $(ODIR)/client.o
	gcc -o $(BDIR)/$@ $^ $(CFLAGS) $(LIBS)

all: server client

clean:
	@rm -rf $(PRE_BDIR) $(PRE_ODIR)
