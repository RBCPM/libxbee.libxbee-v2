include makefile.generic

BUILDDIR:=.build
DESTDIR:=lib

SYS_LIBDIR:=/usr/lib
SYS_INCDIR:=/usr/include

RELEASE_ITEMS:=$(DESTDIR)/$(LIBOUT).so.$(LIBFULLREV) \
               $(DESTDIR)/$(LIBOUT).so \
               $(DESTDIR)/$(LIBOUT).so.$(LIBFULLREV).dbg \
               $(DESTDIR)/$(LIBOUT).a.$(LIBFULLREV) \
               $(DESTDIR)/$(LIBOUT).a \
               xbee.h
LIBS:=rt pthread dl

CROSS_COMPILE?=
AR:=$(CROSS_COMPILE)ar
LD:=$(CROSS_COMPILE)ld
GCC:=$(CROSS_COMPILE)gcc
OBJCOPY:=$(CROSS_COMPILE)objcopy

DEBUG:=-g
CFLAGS:=-Wall -Wstrict-prototypes -Wno-variadic-macros -c -fPIC -fvisibility=hidden $(DEBUG)
#CFLAGS+=-pedantic
CLINKS:=$(addprefix -l,$(LIBS)) $(DEBUG)

### un-comment to remove ALL logging (smaller & faster binary)
#CFLAGS+=-DXBEE_DISABLE_LOGGING
### un-comment to turn off hardware flow control
#CFLAGS+=-DXBEE_NO_RTSCTS

###############################################################################

.PHONY: all install install_dbg install_sudo install_dbg_sudo clean spotless new release .%.dir
.PRECIOUS: .%.dir $(BUILDDIR)/%.d

OBJS:=$(addprefix $(BUILDDIR)/,$(addsuffix .o,$(SRCS)))

all: $(DESTDIR)/$(LIBOUT).so $(DESTDIR)/$(LIBOUT).a

install: all
	sudo make install_sudo

install_dbg: all
	sudo make install_dbg_sudo

install_sudo: all
	cp -f $(DESTDIR)/$(LIBOUT).so.$(LIBFULLREV) $(SYS_LIBDIR)/$(LIBOUT).so.$(LIBFULLREV)
	chmod 644 $(SYS_LIBDIR)/$(LIBOUT).so.$(LIBFULLREV)
	ln -fs $(SYS_LIBDIR)/$(LIBOUT).so.$(LIBFULLREV) $(SYS_LIBDIR)/$(LIBOUT).so
	cp -f $(DESTDIR)/$(LIBOUT).a.$(LIBFULLREV) $(SYS_LIBDIR)
	chmod 644 $(SYS_LIBDIR)/$(LIBOUT).a.$(LIBFULLREV)
	ln -fs $(SYS_LIBDIR)/$(LIBOUT).a.$(LIBFULLREV) $(SYS_LIBDIR)/$(LIBOUT).a
	cp -f xbee.h $(SYS_INCDIR)/xbee.h
	chmod 644 $(SYS_INCDIR)/xbee.h

install_dbg_sudo: install_sudo
	cp -f $(DESTDIR)/$(LIBOUT).so.$(LIBFULLREV).dbg $(SYS_LIBDIR)/$(LIBOUT).so.$(LIBFULLREV).dbg
	chmod 644 $(SYS_LIBDIR)/$(LIBOUT).so.$(LIBFULLREV).dbg

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
	$(GCC) -MM -MT $(addprefix $(BUILDDIR)/,$(filter %.o,$(^:.c=.o))) $(filter %.c,$^) -o $@

$(BUILDDIR)/ver.o: .$(BUILDDIR).dir $(BUILDDIR)/ver.d *.c *.h
	$(GCC) $(CFLAGS) -DLIBXBEE_REVISION="\"$(LIBFULLREV)\"" -DLIBXBEE_COMMIT="\"$(shell git log -1 --format="%H")\"" -DLIBXBEE_COMMITTER="\"$(shell git log -1 --format="%cn <%ce>")\"" -DLIBXBEE_BUILDTIME="\"$(shell date)\"" ver.c -o $@

$(BUILDDIR)/%.o: .$(BUILDDIR).dir $(BUILDDIR)/%.d %.c
	$(GCC) $(CFLAGS) $(firstword $(filter %.c,$^)) -o $@

include $(wildcard $(BUILDDIR)/*.d)
