
$(call standard_dirs)

$c_OBJECTS := main.o mbus.pb.o input.pb.o
$c_OBJECT_PATHS := $(addprefix $($c_OBJDIR)/,$($c_OBJECTS))

all-$c: $($c_BINDIR)/kbd

install-$c: $($c_BINDIR)/kbd
	install $($d_BINDIR)/kbd $(SYSROOT_PATH)/usr/bin

$c_CXX = x86_64-managarm-g++

$c_PKGCONF := PKG_CONFIG_SYSROOT_DIR=$(SYSROOT_PATH) \
	PKG_CONFIG_LIBDIR=$(SYSROOT_PATH)/usr/lib/pkgconfig pkg-config

$c_INCLUDES := -I$($c_GENDIR) -I$(TREE_PATH)/frigg/include \
	$(shell $($c_PKGCONF) --cflags protobuf-lite)

$c_CXXFLAGS := $(CXXFLAGS) $($c_INCLUDES)
$c_CXXFLAGS += -std=c++1y -Wall
$c_CXXFLAGS += -DFRIGG_HAVE_LIBC

$c_LIBS := -lbragi_mbus \
	$(shell $($c_PKGCONF) --libs protobuf-lite)

$($c_BINDIR)/kbd: $($c_OBJECT_PATHS) | $($c_BINDIR)
	$($d_CXX) -o $@ $($d_LDFLAGS) $($d_OBJECT_PATHS) $($d_LIBS)

$($c_OBJDIR)/%.o: $($c_SRCDIR)/%.cpp | $($c_OBJDIR)
	$($d_CXX) -c -o $@ $($d_CXXFLAGS) $<
	$($d_CXX) $($d_CXXFLAGS) -MM -MP -MF $(@:%.o=%.d) -MT "$@" -MT "$(@:%.o=%.d)" $<

# compile protobuf files
gen-$c: $($c_GENDIR)/mbus.pb.tag  $($c_GENDIR)/input.pb.tag

$($c_GENDIR)/%.pb.tag: $(TREE_PATH)/bragi/proto/%.proto | $($c_GENDIR)
	$(PROTOC) --cpp_out=$($d_GENDIR) --proto_path=$(TREE_PATH)/bragi/proto $<
	touch $@

$($c_OBJDIR)/%.o: $($c_GENDIR)/%.cc | $($c_OBJDIR)
	$($d_CXX) -c -o $@ $($d_CXXFLAGS) $<
	$($d_CXX) $($d_CXXFLAGS) -MM -MP -MF $(@:%.o=%.d) -MT "$@" -MT "$(@:%.o=%.d)" $<

