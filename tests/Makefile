# statically link into every test, allowing for easy debugging

DIRS=basic less_basic

all: directory tests 

tests: libmemory.a
	for dir in $(DIRS); do \
		$(MAKE) -C $$dir; \
	done

libmemory.a:
	@$(MAKE) -C $(PROJECTDIR)/code

directory:
	mkdir -p $(BUILDIR)/tests
