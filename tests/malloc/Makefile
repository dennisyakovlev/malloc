OBJECTS=basic zero loop large
CURRDIR=$(BUILDIR)/tests/malloc

all: directory tests

.PHONY: directory tests

tests: $(OBJECTS)

%: %.c
	$(CC) $(FLAGS) -I$(PROJECTDIR)/code/include -o $(CURRDIR)/$@ $^ -L$(BUILDIR)/code -lmemory

directory:
	mkdir -p $(CURRDIR)
