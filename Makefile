CC:=$(shell sh -c 'type $(CC) >/dev/null 2>/dev/null && echo $(CC) || echo gcc')
export CC

# If DEBUG environment variable is set, we compile with "debug" cflags
DEBUGFLAGS = -g -ggdb -O3
ifeq ($(DEBUG), 1)
	DEBUGFLAGS = -g -ggdb -O0
endif

# Default CFLAGS
CFLAGS += -Wall -fPIC -D_GNU_SOURCE -std=gnu99 -I"$(shell pwd)" \
-DREDIS_MODULE_TARGET -DREDISMODULE_EXPERIMENTAL_API
CFLAGS += $(DEBUGFLAGS)

# Compile flags
SHOBJ_LDFLAGS ?= -shared -Bsymbolic -Bsymbolic-functions -ldl -lpthread
export CFLAGS

# Sources
SOURCEDIR:=$(shell pwd -P)
CC_SOURCES := $(wildcard $(SOURCEDIR)/*.c)

# Convert all sources to objects
CC_OBJECTS = $(sort $(patsubst %.c, %.o, $(CC_SOURCES)))

# .d files for each c file. These make sure that changing a header file will
# also change the dependent .c files of it
CC_DEPS = $(patsubst %.c, %.d, $(CC_SOURCES))

MODULE_SO=streaming.so

MODULE=$(CC_OBJECTS)

%.c: %.y

# Compile C file while generating a .d file for it
%.o: %.c
%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $< -o $@ -MMD -MF $(@:.o=.d)

all: $(MODULE_SO)

# Include all dependency files for C files
-include $(CC_DEPS)

$(MODULE_SO): $(shell rm -f module.o)
$(MODULE_SO): $(MODULE) version.h
	@# Just to make sure old versions of the modules are deleted
	rm -f $(MODULE_SO)
	$(CC) -o $@ $(MODULE) $(SHOBJ_LDFLAGS) -lc -lm $(LDFLAGS)

clean:
	rm -fv *.[oad] $(MODULE_SO)

.PHONY: all clean
