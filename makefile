.PHONY: clean dist

dist: clean
	tar -hzcf "$(CURDIR).tar.gz" hashtable/* holdall/* xwc/* sbuffer/* makefile 

clean:
	$(MAKE) -C xwc clean
