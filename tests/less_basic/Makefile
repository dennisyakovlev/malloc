# OBJECTS=loop large mix
OBJECTS=mix
CURRDIR=$(BUILDIR)/tests/less_basic

all: directory tests

.PHONY: directory tests

tests: $(OBJECTS)

%: %.c
	$(CC) $(FLAGS) -I$(PROJECTDIR)/code/include -o $(CURRDIR)/$@ $^ -L$(BUILDIR)/code -lmemory

directory:
	mkdir -p $(CURRDIR)