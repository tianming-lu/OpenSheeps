SUBDIRS = third Starter
SODIRS = Sheeps
OTDIRS = example
OUTFILE = starter
INSTALLDIR = ./out

all:
	@list='$(SUBDIRS)'; for subdir in $$list; do \
	echo "MAKING ALL IN $$subdir"; \
	(cd $$subdir && make OUTFILE=$(OUTFILE)); \
	done;
	@list='$(SODIRS)'; for subdir in $$list; do \
	echo "MAKING ALL IN $$subdir"; \
	(cd $$subdir && make OUTFILE=$(OUTFILE)); \
	done;
	@list='$(OTDIRS)'; for subdir in $$list; do \
	echo "MAKING ALL IN $$subdir"; \
	(cd $$subdir && make OUTFILE=$(OUTFILE)); \
	done;

clean:
	@list='$(SUBDIRS)'; for subdir in $$list; do \
	echo "MAKINT CLEAN IN $$subdir"; \
	(cd $$subdir && make clean OUTFILE=$(OUTFILE)); \
	done;
	@list='$(SODIRS)'; for subdir in $$list; do \
	echo "MAKINT CLEAN IN $$subdir"; \
	(cd $$subdir && make clean OUTFILE=$(OUTFILE)); \
	done;
	@list='$(OTDIRS)'; for subdir in $$list; do \
	echo "MAKINT CLEAN IN $$subdir"; \
	(cd $$subdir && make clean OUTFILE=$(OUTFILE)); \
	done;

install:
	rm -rf $(INSTALLDIR)/*;
	mkdir $(INSTALLDIR)/bin -p;
	cp Starter/$(OUTFILE) $(INSTALLDIR)/bin;
	cp example/example $(INSTALLDIR)/bin;
	cp x64/sheeps/Release/sheeps.ini $(INSTALLDIR)/bin;
	@list='$(SODIRS)'; for subdir in $$list; do \
	echo "cp $$subdir/*.so $(INSTALLDIR)/bin/"; \
	(cp $$subdir/*.so $(INSTALLDIR)/bin/); \
	done;