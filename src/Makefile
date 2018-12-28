IDIR =../include
CC=gcc
CFLAGS=-I$(IDIR) -I../../minimp3 -Wall -Werror

ODIR=obj
LDIR =../lib

LIBS=-lm -lasound

_DEPS = main.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = main.o myminimp3.o pcm.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

audio_player: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

all: audio_player

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~
	rm audio_player
