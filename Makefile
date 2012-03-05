SRC = src/bpm_lfo.c
OBJ = $(SRC:.c=.o)
SONAME = bpm_effect.so
CFLAGS = -Wall -Werror -O3 -fPIC -g


all: $(OBJ)
	ld -shared -o ./$(SONAME) $(OBJ)

clean:
	rm -f $(OBJ) $(SONAME)
