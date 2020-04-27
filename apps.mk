LIBCSP_APPS := 

ifeq ($(LIBCSP_USE_ZMQ),1)
LIBCSP_APPS += $(LIBCSP_DIR)/apps/zmqhub.c
endif

LIBCSP_APPS_BINS := $(LIBCSP_APPS:$(LIBCSP_DIR)/apps/%.c=build/apps/libcsp/%)

APPS += $(LIBCSP_APPS_BINS)

build/apps/libcsp/%: $(LIBCSP_DIR)/apps/%.c $(LIBCSP_OBJECTS)
	@echo "[LINK] $@"
	$(Q)$(CC) $(CFLAGS) $(CCFLAGS) $^ $(LDFLAGS) $(CPPFLAGS) -o $@

