LIBOUT:=libxbee
LIBMAJ:=2
LIBMIN:=0
LIBREV:=0

BUILDDIR:=.build
SRCS:=io listen ll xbee_s1 xbee_s2 xbee_sG conn mode
LIBS:=rt pthread

GCC:=gcc

CFLAGS:=-Wall -Wstrict-prototypes -Wno-variadic-macros -c -fPIC $(DEBUG)
#CFLAGS+=-pedantic
CLINKS:=$(addprefix -l,$(LIBS)) $(DEBUG)


#######################################################################################

.PHONY: all clean new
.PRECIOUS: $(BUILDDIR)/%.d

all: $(LIBOUT).so

new: clean all

clean:
	rm -rdf $(BUILDDIR) $(LIBOUT).so
	mkdir $(BUILDDIR)

$(LIBOUT).so: $(addprefix $(BUILDDIR)/,$(addsuffix .o,$(SRCS)))
	$(GCC) -shared -Wl,-soname,$(LIBOUT).so.$(LIBMAJ) $(CLINKS) $(filter %.o,$^) -o $@

#$(BUILDDIR)/:
#	mkdir -p $@

#$(BUILDDIR)/%.o: $(BUILDDIR)/ $(BUILDDIR)/%.d %.c
$(BUILDDIR)/%.o: %.c
	$(GCC) $(CFLAGS) $(filter %.c,$^) -o $@

#$(BUILDDIR)/%.d: $(BUILDDIR)/ %.c
$(BUILDDIR)/%.d: %.c
	$(GCC) -MM $(addprefix -I,$(INCLUDES)) $(filter %.c,$^) -o $@

include $(wildcard $(BUILDDIR)/*.d)

