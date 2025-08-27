
# QNX x86_64 Cross-Compilation Makefile

# Toolchain and flags
QCC = qcc
QCC_TARGET = -V8.3.0,gcc_ntox86_64

CFLAGS = -DPJ_QNX=1 -DPJ_CONFIG_SITE=1 -include stddef.h \
		-Wc,-I../../../include -Wc,-I../../include \
		-Wc,-I../../../pjlib/include -Wc,-I../../../pjlib-util/include \
		-Wc,-I../../../pjmedia/include -Wc,-I../../../pjnath/include
		

LDFLAGS = -L../../../pjlib/lib \
		-L../../../pjmedia/lib \
		-L../../../pjmedia-audiodev/lib \
		-B/home/pankaj/qnx710/target/qnx7/usr/lib \
		-Y_gcc
LDLIBS = -lpj -lpjmedia -lpjmedia-audiodev -lasound -lsocket -lm -lc -lcS -lgcc_s

# Target
TARGET = qnx_test_harness
OBJS = qnx_test_harness.o

# Default rule
all: $(TARGET)

# Build target
$(TARGET): $(OBJS)
	@echo "Compiling with: $(QCC) $(QCC_TARGET) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)"
	$(QCC) $(QCC_TARGET) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

# Compile source
%.o: %.c
	$(QCC) $(QCC_TARGET) $(CFLAGS) -c $< -o $@

# Clean rule
clean:
	@rm -f $(TARGET) $(OBJS)

distclean:
	@rm -f *.o *.a *.so *.out qnx_test_harness
	@rm -rf build/

.PHONY: all clean distclean
