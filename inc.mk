ifndef LIBCSP_INCLUDED
export LIBCSP_INCLUDED=1

SHELL      := /bin/bash
THIS_DIR_  := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
LIBCSP_DIR := $(THIS_DIR_:/=)
Q          ?= @

PKGCONFIG  ?= pkg-config

LIBCSP_ARCH               ?= posix
LIBCSP_DEBUG              ?= 1
LIBCSP_RTABLE             ?= cidr
LIBCSP_ENDIAN             ?= little
LIBCSP_USE_CAN            ?= 0
LIBCSP_USE_ZMQ            ?= 0
LIBCSP_USE_KISS           ?= 0
LIBCSP_USE_I2C            ?= 0
LIBCSP_USE_QOS            ?= 1
LIBCSP_USE_RDP            ?= 1
LIBCSP_USE_CRC32          ?= 1
LIBCSP_USE_DEDUP          ?= 0
LIBCSP_USE_HMAC           ?= 1
LIBCSP_USE_XTEA           ?= 1
LIBCSP_USE_PROMISC        ?= 1
LIBCSP_USE_ALIGNEDBUFFERS ?= 1

LIBCSP_CONN_MAX          ?= 10
LIBCSP_CONN_QUEUE_LENGTH ?= 100
LIBCSP_FIFO_INPUT        ?= 10
LIBCSP_MAX_BIND_PORT     ?= 31
LIBCSP_RDP_MAX_WINDOW    ?= 20
LIBCSP_PADDING_BYTES     ?= 8
LIBCSP_CONNECTION_SO     ?= 0

# Should be empty in this file
LIBCSP_DRIVERS ?=

ifeq ($(filter $(LIBCSP_ARCH), posix freertos),)
$(error "LIBCSP_ARCH not valid: '$(LIBCSP_ARCH)'")
endif

ifeq ($(filter $(LIBCSP_RTABLE), static cidr),)
$(error "LIBCSP_RTABLE not valid: '$(LIBCSP_RTABLE)'")
endif

ifeq ($(filter $(LIBCSP_ENDIAN), little big),)
$(error "LIBCSP_ENDIAN not valid: '$(LIBCSP_ENDIAN)'")
endif

LIBCSP_CCFLAGS  += -std=gnu99
LIBCSP_CFLAGS   += -I $(LIBCSP_DIR)/include

ifeq ($(LIBCSP_ENDIAN),little)
LIBCSP_CPPFLAGS += -DCSP_LITTLE_ENDIAN=1
else
ifeq ($(LIBCSP_ENDIAN),big)
LIBCSP_CPPFLAGS += -DCSP_BIG_ENDIAN=1
endif
endif

LIBCSP_CCFLAGS_posix  += -pthread
LIBCSP_CXXFLAGS_posix += -std=c++14 -pthread
LIBCSP_LDFLAGS_posix  += -lrt -lpthread
LIBCSP_CPPFLAGS_posix += -DCSP_POSIX=1

LIBCSP_CPPFLAGS_freertos += -DCSP_FREERTOS=1

# External dependencies via pkg-config.
# Only use pkg-config if the target platform is posix. I know
# pkg-config is not target platform specific, but currently
# libcsp has only been tested with libsocketcan and libzmq
# on posix platforms (linux).
ifeq ($(LIBCSP_ARCH),posix)

ifeq ($(LIBCSP_USE_ZMQ),1)
LIBCSP_HAVE_LIBZMQ_CHECK := $(shell $(PKGCONFIG) libzmq)

ifneq ($(.SHELLSTATUS),0)
$(error "Failed to find libzmq support via '$(PKGCONFIG)'")
else
LIBCSP_HAVE_LIBZMQ := 1
LIBCSP_CFLAGS += $(shell $(PKGCONFIG) libzmq --cflags)
LIBCSP_LDFLAGS_posix += $(shell $(PKGCONFIG) libzmq --libs)
endif
endif

ifeq ($(LIBCSP_USE_CAN),1)
LIBCSP_HAVE_LIBSOCKETCAN_CHECK := $(shell $(PKGCONFIG) libsocketcan)

ifneq ($(.SHELLSTATUS),0)
$(error "Failed to find libsocketcan support via '$(PKGCONFIG)'")
else
LIBCSP_HAVE_LIBSOCKETCAN := 1
LIBCSP_CFLAGS += $(shell $(PKGCONFIG) libsocketcan --cflags)
LIBCSP_LDFLAGS_posix += $(shell $(PKGCONFIG) libsocketcan --libs)
endif
endif

LIBCSP_HAVE_LIBPROCPS_CHECK := $(shell $(PKGCONFIG) libprocps)
ifeq ($(.SHELLSTATUS),0)
LIBCSP_HAVE_LIBPROCPS := 1
LIBCSP_CFLAGS += $(shell $(PKGCONFIG) libprocps --cflags)
LIBCSP_LDFLAGS_posix += $(shell $(PKGCONFIG) libprocps --libs)
LIBCSP_CPPFLAGS += -DCSP_HAVE_LIBPROCPS=1
endif

endif

# Sources list.
# There are base sources in src/, and some optional sources
# based on the configuration variables above.
LIBCSP_SOURCES := $(wildcard $(LIBCSP_DIR)/src/*.c)
LIBCSP_SOURCES += $(wildcard $(LIBCSP_DIR)/src/arch/$(LIBCSP_ARCH)/*.c)
LIBCSP_SOURCES += $(wildcard $(LIBCSP_DIR)/src/crypto/*.c)
LIBCSP_SOURCES += $(wildcard $(LIBCSP_DIR)/src/transport/*.c)
LIBCSP_SOURCES += $(LIBCSP_DIR)/src/interfaces/csp_if_lo.c
LIBCSP_SOURCES += $(LIBCSP_DIR)/src/rtable/csp_rtable_$(LIBCSP_RTABLE).c
LIBCSP_SOURCES += $(wildcard $(LIBCSP_DIR)/src/drivers/*_stub.c)

ifneq ($(LIBCSP_DEBUG),1)
LIBCSP_SOURCES := $(filter-out $(LIBCSP_DIR)/src/csp_debug.c, $(LIBCSP_SOURCES))
else
LIBCSP_CPPFLAGS += -DCSP_DEBUG=1
LIBCSP_CPPFLAGS += -DCSP_LOG_LEVEL_DEBUG=1
LIBCSP_CPPFLAGS += -DCSP_LOG_LEVEL_INFO=1
LIBCSP_CPPFLAGS += -DCSP_LOG_LEVEL_WARN=1
LIBCSP_CPPFLAGS += -DCSP_LOG_LEVEL_ERROR=1
endif

ifeq ($(LIBCSP_USE_CAN),1)
# Add the interface
LIBCSP_SOURCES += $(LIBCSP_DIR)/src/interfaces/csp_if_can.c

# Add driver selections
ifneq ($(filter can_socketcan,$(LIBCSP_DRIVERS)),)
ifneq ($(LIBCSP_HAVE_LIBSOCKETCAN),1)
$(error "can_socketcan driver selected by you do not have libsocketcan")
endif
LIBCSP_SOURCES += $(LIBCSP_DIR)/src/drivers/can_socketcan.c
LIBCSP_CPPFLAGS += -DCSP_HAVE_LIBSOCKETCAN=1
endif
endif

ifeq ($(LIBCSP_USE_ZMQ),1)
	# Add the interface
	LIBCSP_SOURCES += $(LIBCSP_DIR)/src/interfaces/csp_if_zmq.c
endif

ifeq ($(LIBCSP_USE_I2C),1)
	# Add the interface
	LIBCSP_SOURCES += $(LIBCSP_DIR)/src/interfaces/csp_if_i2c.c
endif

ifeq ($(LIBCSP_USE_KISS),1)
	# Add the interface
	LIBCSP_SOURCES += $(LIBCSP_DIR)/src/interfaces/csp_if_kiss.c
endif

ifneq ($(LIBCSP_USE_RDP),1)
	LIBCSP_SOURCES := $(filter-out $(LIBCSP_DIR)/src/transport/csp_rdp.c, $(LIBCSP_SOURCES))
else
	LIBCSP_CPPFLAGS += -DCSP_USE_RDP=1
endif

ifneq ($(LIBCSP_USE_DEDUP),1)
	LIBCSP_SOURCES := $(filter-out $(LIBCSP_DIR)/src/csp_dedup.c, $(LIBCSP_SOURCES))
else
	LIBCSP_CPPFLAGS += -DCSP_USE_DEDUP=1
endif

ifneq ($(LIBCSP_USE_CRC32),1)
	LIBCSP_SOURCES := $(filter-out $(LIBCSP_DIR)/src/csp_crc32.c, $(LIBCSP_SOURCES))
else
	LIBCSP_CPPFLAGS += -DCSP_USE_CRC32=1
endif

ifneq ($(LIBCSP_USE_HMAC),1)
	LIBCSP_SOURCES := $(filter-out $(LIBCSP_DIR)/src/crypto/csp_hmac.c, $(LIBCSP_SOURCES))
else
	LIBCSP_CPPFLAGS += -DCSP_USE_HMAC=1
endif

ifneq ($(LIBCSP_USE_XTEA),1)
	LIBCSP_SOURCES := $(filter-out $(LIBCSP_DIR)/src/crypto/csp_xtea.c, $(LIBCSP_SOURCES))
else
	LIBCSP_CPPFLAGS += -DCSP_USE_XTEA=1
endif

ifneq ($(LIBCSP_USE_PROMISC),1)
	LIBCSP_SOURCES := $(filter-out $(LIBCSP_DIR)/src/csp_promisc.c, $(LIBCSP_SOURCES))
else
	LIBCSP_CPPFLAGS += -DCSP_USE_PROMISC=1
endif

ifeq ($(USE_COMMANDGEN),1)
-include $(EXTERN)/commandgen/inc.mk
COMMANDGEN_SOURCES += $(abspath $(LIBCSP_DIR)/src/asi/interface.cmd.c)
endif

# Build output list
LIBCSP_OBJECTS := $(LIBCSP_SOURCES:$(LIBCSP_DIR)/%.c=build/$(TARGET)/obj/extern/libcsp/%.o)
LIBCSP_DEPENDS := $(LIBCSP_OBJECTS:%.o=%.d)

# Build CPPFLAGS to control execution
LIBCSP_CPPFLAGS += -DCSP_CONN_MAX=$(LIBCSP_CONN_MAX) \
				   -DCSP_CONN_QUEUE_LENGTH=$(LIBCSP_CONN_QUEUE_LENGTH) \
				   -DCSP_FIFO_INPUT=$(LIBCSP_FIFO_INPUT) \
				   -DCSP_MAX_BIND_PORT=$(LIBCSP_MAX_BIND_PORT) \
				   -DCSP_RDP_MAX_WINDOW=$(LIBCSP_RDP_MAX_WINDOW) \
				   -DCSP_PADDING_BYTES=$(LIBCSP_PADDING_BYTES) \
				   -DCSP_CONNECTION_SO=$(LIBCSP_CONNECTION_SO) \
				   -DCSP_USE_QOS=$(LIBCSP_USE_QOS) \
				   -DGIT_REV="\"$(VCS_REV)\""

CFLAGS += $(LIBCSP_CFLAGS) $(LIBCSP_CFLAGS_$(LIBCSP_ARCH))
CCFLAGS += $(LIBCSP_CCFLAGS_$(LIBCSP_ARCH))
CXXFLAGS += $(LIBCSP_CXXFLAGS_$(LIBCSP_ARCH))
LDFLAGS += $(LIBCSP_LDFLAGS_$(LIBCSP_ARCH))
CPPFLAGS += $(LIBCSP_CPPFLAGS) $(LIBCSP_CPPFLAGS_$(LIBCSP_ARCH))

OBJECTS += $(LIBCSP_OBJECTS)

build/$(TARGET)/obj/extern/libcsp/%.o : $(LIBCSP_DIR)/%.c
	@echo "[CC  ] $(shell echo $< | sed 's/^.*libcsp/libcsp/')"
	$(Q)$(CC) $(CCFLAGS) $(CFLAGS) $< -c -MP -MMD -o $@ $(CPPFLAGS)

-include $(LIBCSP_DEPENDS)
-include $(LIBCSP_DIR)/tests.mk
-include $(LIBCSP_DIR)/apps.mk

endif
