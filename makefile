LIBOUT:=libxbee
LIBMAJ:=2
LIBMIN:=0
LIBREV:=1
LIBFULLREV:=$(LIBMAJ).$(LIBMIN).$(LIBREV)

BUILDDIR:=.build
DESTDIR:=lib

SRCS:=conn io ll log mode rx tx xbee xbee_s1 xbee_s2 xbee_sG xsys thread
RELEASE_ITEMS:=lib/libxbee.so.$(LIBFULLREV) \
               lib/libxbee.so \
               lib/libxbee.so.$(LIBFULLREV).dbg \
               lib/libxbee.a.$(LIBFULLREV) \
               lib/libxbee.a \
               xbee.h
LIBS:=rt pthread

AR:=ar
LD:=ld
GCC:=gcc
OBJCOPY:=objcopy

DEBUG:=-g
CFLAGS:=-Wall -Wstrict-prototypes -Wno-variadic-macros -c -fPIC -fvisibility=hidden $(DEBUG)
#CFLAGS+=-pedantic
CLINKS:=$(addprefix -l,$(LIBS)) $(DEBUG)


###############################################################################

.PHONY: all clean spotless new release .%.dir
.PRECIOUS: .%.dir $(BUILDDIR)/%.d

OBJS:=$(addprefix $(BUILDDIR)/,$(addsuffix .o,$(SRCS)))

all: $(DESTDIR)/$(LIBOUT).so $(DESTDIR)/$(LIBOUT).a

new: clean
	@$(MAKE) --no-print-directory all

clean:
	rm -rdf $(BUILDDIR) .$(BUILDDIR).dir
	rm -rdf $(DESTDIR) .$(DESTDIR).dir

spotless: clean
	rm -f .*.d


release: all
	tar -cjvf libxbee_v$(LIBFULLREV)_`date +%Y-%m-%d`_`git rev-parse --verify --short HEAD`.tar.bz2 $(RELEASE_ITEMS)


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

$(DESTDIR)/$(LIBOUT).o: .$(DESTDIR).dir $(addprefix .,$(addsuffix .d,$(SRCS))) $(OBJS)
	$(LD) -r $(filter %.o,$^) -o $@


.%.d: %.c
	$(GCC) -MM -MT $(@:.d=.o) $(filter %.c,$^) -o $@

$(BUILDDIR)/%.o: .$(BUILDDIR).dir %.c
	$(GCC) $(CFLAGS) $(firstword $(filter %.c,$^)) -o $@

include $(wildcard .*.d)
