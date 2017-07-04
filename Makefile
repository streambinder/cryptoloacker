.DEFAULT_GOAL = all

ifeq ($(OS),Windows_NT)
	PLATFORM=windows
else
	PLATFORM=linux
endif

ABSPATH=${CURDIR}

CDIR=$(ABSPATH)/src/$(PLATFORM)
IDIR=$(CDIR)/include
BDIR=$(ABSPATH)/bin/$(PLATFORM)
ODIR=$(ABSPATH)/obj/$(PLATFORM)

CC=gcc
CFLAGS=-I$(IDIR) -lpthread

prepare:
	@mkdir -p {$(BDIR),$(ODIR)}

$(ODIR)/%.o: $(CDIR)/%.c prepare
	$(CC) -g -O -c -o $@ $< $(CFLAGS)

server: $(ODIR)/server.o $(ODIR)/threadpool.o
	gcc -o $(BDIR)/$@ $^ $(CFLAGS) $(LIBS)

client: $(ODIR)/client.o
	gcc -o $(BDIR)/$@ $^ $(CFLAGS) $(LIBS)

all: server client

clean:
	@rm -rf $(BDIR) $(ODIR)
