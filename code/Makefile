OBJECTS=malloc.o
CURRDIR=$(BUILDIR)/code

# here i'd use VPATH
VPATH=source

all: code libmemory.a

.PHONY: code

libmemory.a: $(OBJECTS)
	ar rcs $(CURRDIR)/$@ $(addprefix $(CURRDIR)/, $^)

%.o: %.c
	$(CC) -Iinclude -Wall -c -o $(CURRDIR)/$@ $^

code:
	mkdir -p $(BUILDIR)/$@