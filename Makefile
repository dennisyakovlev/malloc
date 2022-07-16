export PROJECTDIR=$(realpath $(CURDIR))
export BUILDIR=$(PROJECTDIR)/build
export FLAGS=-Wall -ggdb3 -O0

export CC=gcc

OBJECTS=main.o

.PHONY: build code clean

all: build out

out: code $(OBJECTS)
	@echo done
	$(CC) $(FLAGS) -Icode/include -o $(BUILDIR)/$@ $(addprefix $(BUILDIR)/, $(OBJECTS)) -L$(BUILDIR)/code -lmemory

build:
	mkdir -p $@

code:
	$(MAKE) -C $@

%.o: %.c
	$(CC) $(FLAGS) -Icode/include -c -o $(BUILDIR)/$@ $^ -L$(BUILDIR)/code -lmemory

clean:
	rm -fr $(BUILDIR)