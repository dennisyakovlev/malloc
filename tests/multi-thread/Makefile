OBJECTS=basic mix
CURRDIR=$(BUILDIR)/tests/multi-thread

all: directory tests

.PHONY: tests directory

tests: $(OBJECTS)

%: %.c
	$(CC) $(FLAGS) -I$(PROJECTDIR)/code/include -o $(CURRDIR)/$@ $^ -L$(BUILDIR)/code -lmemory -lpthread

directory:
	mkdir -p $(CURRDIR)
