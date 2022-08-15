OBJECTS=malloc.o
CURRDIR=$(BUILDIR)/code

VPATH=source

all: code libmemory.a

.PHONY: code

libmemory.a: $(OBJECTS)
	ar rcs $(CURRDIR)/$@ $(addprefix $(CURRDIR)/, $^)

%.o: %.c
	$(CC) $(FLAGS) -Iinclude -c -o $(CURRDIR)/$@ $^

code:
	mkdir -p $(BUILDIR)/$@