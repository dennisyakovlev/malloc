export PROJECTDIR=$(realpath $(CURDIR))
export BUILDIR=$(PROJECTDIR)/build
export FLAGS=-Wall -Wpedantic -Wshadow

export CC=gcc

OBJECTS=main.o

.PHONY: build code tests clean profile profile_flags debug debug_flags

profile: profile_flags all
debug: debug_flags all
release: release_flags all

all: build out

out: code $(OBJECTS)
	@echo done
	$(CC) $(FLAGS) -Icode/include -o $(BUILDIR)/$@ $(addprefix $(BUILDIR)/, $(OBJECTS)) -L$(BUILDIR)/code -lmemory

build:
	mkdir -p $@

code:
	@$(MAKE) -C $@

%.o: %.c
	$(CC) $(FLAGS) -Icode/include -c -o $(BUILDIR)/$@ $^ -L$(BUILDIR)/code -lmemory

tests: 
	@$(MAKE) -C $@

clean:
	rm -fr $(BUILDIR)

profile_flags:
	$(eval FLAGS += -pg)

debug_flags:
	$(eval FLAGS += -ggdb3 -O0)

release_flags:
	$(eval FLAGS += -O3)
