CC=gcc

IDIR=inc
_DEPS = kvec.h cimgui.h cimgui_impl.h
DEPS=$(patsubst %,$(IDIR)/%,$(_DEPS))

ODIR=obj
_OBJ = main.o
OBJ=$(patsubst %,$(ODIR)/%,$(_OBJ))

LDIRS=-Llib -Wl,-rpath=./lib/
LIBS=-lglfw -lm -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi -l:cimgui.so

CFLAGS=$(LDIRS) -I$(IDIR) -Wall
ifeq ($(DEBUG), 1)
	CFLAGS+=-g -O0 -DDEBUG
else
	CFLAGS+=-O3
endif

$(ODIR)/%.o: src/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: all run clean

all: $(OBJ)
	$(CC) $^ $(CFLAGS) $(LIBS) -o out 

run: all
	./out

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~ out