LIBOUT:=libxbee
LIBMAJ:=2
LIBMIN:=0
LIBREV:=0

BUILDDIR:=.build
DESTDIR:=lib
SRCS:=conn io ll log mode rx tx xbee xbee_s1 xbee_s2 xbee_sG xsys
RELEASE_ITEMS:=lib/libxbee.so lib/libxbee.so.dbg lib/libxbee.a xbee.h
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

.PHONY: all clean new release
.PRECIOUS: .%.dir $(BUILDDIR)/%.d

OBJS:=$(addprefix $(BUILDDIR)/,$(addsuffix .o,$(SRCS)))

all: $(DESTDIR)/$(LIBOUT).so $(DESTDIR)/$(LIBOUT).a

new: clean all

clean:
	rm -rdf $(BUILDDIR) .$(BUILDDIR).dir
	rm -rdf $(DESTDIR) .$(DESTDIR).dir


release: all
	tar -cjvf libxbee_v$(LIBMAJ).$(LIBMIN).$(LIBREV)_`date +%Y-%m-%d`_`git rev-parse --verify --short HEAD`.tar.gz $(RELEASE_ITEMS)


%.dir:
	mkdir -p $*
	@touch .$@


$(DESTDIR)/$(LIBOUT).so: $(DESTDIR)/$(LIBOUT).o
	$(GCC) -shared -Wl,-soname,$(LIBOUT).so.$(LIBMAJ).$(LIBMIN).$(LIBREV) $(CLINKS) $(filter %.o,$^) -o $@
	$(OBJCOPY) --only-keep-debug $@ $@.dbg
	$(OBJCOPY) --add-gnu-debuglink=$@.dbg $@
	$(OBJCOPY) --strip-debug $@

$(DESTDIR)/$(LIBOUT).a: $(DESTDIR)/$(LIBOUT).o
	$(AR) rcs $@ $(filter %.o,$^)

$(DESTDIR)/$(LIBOUT).o: $(DESTDIR).dir $(OBJS)
	$(LD) -r $(filter %.o,$^) -o $@


$(BUILDDIR)/%.o: $(BUILDDIR).dir $(BUILDDIR)/%.d %.c
	$(GCC) $(CFLAGS) $(firstword $(filter %.c,$^)) -o $@

$(BUILDDIR)/%.d: $(BUILDDIR).dir %.c
	$(GCC) -MM -MT $(@:.d=.o) $(filter %.c,$^) -o $@

include $(wildcard $(BUILDDIR)/*.d)
