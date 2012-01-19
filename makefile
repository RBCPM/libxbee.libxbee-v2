include makefile.generic

BUILDDIR:=.build
DESTDIR:=lib
LIBFULLREV:=$(LIBMAJ).$(LIBMIN).$(LIBREV)

RELEASE_ITEMS:=$(DESTDIR)/$(LIBOUT).so.$(LIBFULLREV)       \
               $(DESTDIR)/$(LIBOUT).so                     \
               $(DESTDIR)/$(LIBOUT).so.$(LIBFULLREV).dbg   \
               $(DESTDIR)/$(LIBOUT).a.$(LIBFULLREV)        \
               $(DESTDIR)/$(LIBOUT).a                      \
               $(SYS_HEADERS)                              \
               $(RELEASE_FILES)

AR:=$(CROSS_COMPILE)ar
LD:=$(CROSS_COMPILE)ld
GCC:=$(CROSS_COMPILE)gcc
OBJCOPY:=$(CROSS_COMPILE)objcopy

DEBUG:=-g
CFLAGS:=-Wall -Wstrict-prototypes -Wno-variadic-macros -c -fPIC $(DEBUG) $(addprefix -D,$(OPTIONS))
CFLAGS+=-fvisibility=hidden
#CFLAGS+=-pedantic
CLINKS:=$(addprefix -l,$(LIBS)) $(DEBUG)

###############################################################################

.PHONY: all install install_dbg install_sudo install_dbg_sudo clean spotless new release .%.dir
.PRECIOUS: .%.dir $(BUILDDIR)/%.d

OBJS:=$(addprefix $(BUILDDIR)/,$(addsuffix .o,$(SRCS)))


all: $(DESTDIR)/$(LIBOUT).so $(DESTDIR)/$(LIBOUT).a

install: all
	sudo make install_sudo

install_dbg: all
	sudo make install_dbg_sudo

install_sudo: all $(addprefix $(SYS_INCDIR)/,$(SYS_HEADERS)) $(SYS_LIBDIR)/$(LIBOUT).so.$(LIBFULLREV) $(SYS_LIBDIR)/$(LIBOUT).a.$(LIBFULLREV)

install_dbg_sudo: install_sudo $(SYS_LIBDIR)/$(LIBOUT).so.$(LIBFULLREV).dbg


$(SYS_LIBDIR)/$(LIBOUT).%.$(LIBFULLREV): $(DESTDIR)/$(LIBOUT).%.$(LIBFULLREV)
	cp -f $^ $@
	chmod 644 $@
	ln -fs $@ $(subst .$(LIBFULLREV),,$@)

$(SYS_LIBDIR)/$(LIBOUT).so.$(LIBFULLREV).dbg: $(DESTDIR)/$(LIBOUT).so.$(LIBFULLREV).dbg
	cp -f $^ $@
	chmod 644 $@

$(SYS_INCDIR)/%.h: %.h
	cp -f $^ $@
	chmod 644 $@


new: clean
	@$(MAKE) --no-print-directory all

clean:
	rm -rdf $(BUILDDIR)/*.o
	rm -rdf $(DESTDIR)/*

spotless: clean
	rm -rdf $(BUILDDIR) .$(BUILDDIR).dir
	rm -rdf $(DESTDIR) .$(DESTDIR).dir


release: all
	tar -cjvf $(LIBOUT)_v$(LIBFULLREV)_`date +%Y-%m-%d`_`git rev-parse --verify --short HEAD`_`uname -m`.tar.bz2 $(RELEASE_ITEMS)


.%.dir:
	@if [ ! -d $* ]; then echo "mkdir -p $*"; mkdir -p $*; else echo "!mkdir $*"; fi
	@touch $@


$(DESTDIR)/$(LIBOUT).so: $(DESTDIR)/$(LIBOUT).so.$(LIBFULLREV)
	ln -fs `basename $^` $@

$(DESTDIR)/$(LIBOUT).so.$(LIBFULLREV): .$(DESTDIR).dir $(DESTDIR)/$(LIBOUT).o
	$(GCC) -shared -Wl,-soname,$(LIBOUT).so.$(LIBFULLREV) $(CLINKS) $(filter %.o,$^) -o $@
	$(OBJCOPY) --only-keep-debug $@ $@.dbg
	$(OBJCOPY) --add-gnu-debuglink=$@.dbg $@
	$(OBJCOPY) --strip-debug $@

$(DESTDIR)/$(LIBOUT).a: $(DESTDIR)/$(LIBOUT).a.$(LIBFULLREV)
	ln -fs `basename $^` $@

$(DESTDIR)/$(LIBOUT).a.$(LIBFULLREV): .$(DESTDIR).dir $(DESTDIR)/$(LIBOUT).o
	$(AR) rcs $@ $(filter %.o,$^)

$(DESTDIR)/$(LIBOUT).o: .$(DESTDIR).dir $(OBJS)
	$(LD) -r $(filter %.o,$^) -o $@


$(BUILDDIR)/%.d: .$(BUILDDIR).dir %.c
	$(GCC) -MM -MT $(addprefix $(BUILDDIR)/,$*.o) $*.c -o $@

$(BUILDDIR)/ver.o: $(BUILDDIR)/ver.d *.c *.h
	$(GCC) $(CFLAGS) $(VER_DEFINES) ver.c -o $@

$(BUILDDIR)/%.o: $(BUILDDIR)/%.d
	$(GCC) $(CFLAGS) $*.c -o $@

include $(wildcard $(BUILDDIR)/*.d)

