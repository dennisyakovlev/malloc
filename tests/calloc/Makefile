OBJECTS=basic overflow
CURRDIR=$(BUILDIR)/tests/calloc

all: directory tests

.PHONY: directory tests

tests: $(OBJECTS)

%: %.c
	$(CC) $(FLAGS) -I$(PROJECTDIR)/code/include -o $(CURRDIR)/$@ $^ -L$(BUILDIR)/code -lmemory

directory:
	mkdir -p $(CURRDIR)