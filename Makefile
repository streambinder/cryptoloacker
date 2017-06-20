# .DEFAULT_GOAL = all
#
# ifeq ($(OS),Windows_NT)
# 	include windows.mk
# else
# 	include linux.mk
# endif
#
# CDIR = src/$(PATTERN)
# IDIR = $(ABSDIR)/include
#
# DEPS_HEADERS = $(IDIR)/*.h
# DEPS = $(patsubst %,$(IDIR)/%,$(DEPS_HEADERS))
#
# CC = gcc
# CFLAGS = -I$(IDIR)
#
# ODIR = obj/
# # LDIR =../lib
#
# LIBS=-lm
#
# OBJ_OBJECTS = server.o client.o
# OBJ = $(patsubst %,$(ODIR)/%,$(OBJ_OBJECTS))
#
# $(ODIR)/%.o: %.c $(DEPS)
# 	$(CC) -c -o $@ $< $(CFLAGS)
#
# .PHONY: clean
#
# all: $(OBJ)
# 	gcc -o $@ $^ $(CFLAGS) $(LIBS)
#
# clean:
# 	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~
.DEFAULT_GOAL = all

ifeq ($(OS),Windows_NT)
	include Makefile.windows
else
	include Makefile.linux
endif

ABSPATH=${CURDIR}
# ABSPATH=.

CDIR=$(ABSPATH)/src/$(PLATFORM)
IDIR=$(CDIR)/include
BDIR=$(ABSPATH)/bin/$(PLATFORM)
ODIR=$(ABSPATH)/obj/$(PLATFORM)

CC=gcc
CFLAGS=-I$(IDIR)

prepare:
	@mkdir -p {$(BDIR),$(ODIR)}

$(ODIR)/%.o: $(CDIR)/%.c prepare
	$(CC) -g -O -c -o $@ $< $(CFLAGS)

server: $(ODIR)/server.o
	gcc -o $(BDIR)/$@ $^ $(CFLAGS) $(LIBS)

client: $(ODIR)/client.o
	gcc -o $(BDIR)/$@ $^ $(CFLAGS) $(LIBS)

all: server client

clean:
	@rm -rf $(BDIR) $(ODIR)
