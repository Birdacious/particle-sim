CC=gcc

IDIR=inc
_DEPS = kvec.h cimgui.h cimgui_impl.h stb_image.h tinyobj_loader_c.h
DEPS=$(patsubst %,$(IDIR)/%,$(_DEPS))

ODIR=obj
_OBJ = main.o
OBJ=$(patsubst %,$(ODIR)/%,$(_OBJ))

LDIRS=-Llib -Wl,-rpath=./lib/
LIBS=-lglfw -lm -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi -l:cimgui.so

CFLAGS=$(LDIRS) -I$(IDIR) -Wall
ifeq ($(DEBUG), 1)
	CFLAGS+=-g -O0 -DDEBUG
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