LIBCSP_C_TESTS       := 
LIBCSP_CXX_TESTS     := $(LIBCSP_DIR)/tests/cpp_test.cc

ifeq ($(LIBCSP_USE_CAN),1)
	LIBCSP_C_TESTS += $(LIBCSP_DIR)/tests/vcan_test.c
endif

ifeq ($(LIBCSP_USE_ZMQ),1)
	LIBCSP_C_TESTS += $(LIBCSP_DIR)/tests/zmq_test.c
endif

ifeq ($(LIBCSP_USE_KISS),1)
	LIBCSP_C_TESTS += $(LIBCSP_DIR)/tests/kiss_test.c
	LIBCSP_CXX_TESTS += $(wildcard $(LIBCSP_DIR)/tests/kiss_tcp_*.cc)
endif

## Common
-include $(EXTERN)/common/inc.mk
$(eval $(call ENABLE_COMMON_CXX_ARCH_LINUX))
##

LIBCSP_C_TESTS_OBJ   := $(LIBCSP_C_TESTS:$(LIBCSP_DIR)/tests/%.c=build/test/libcsp/%)
LIBCSP_CXX_TESTS_OBJ := $(LIBCSP_CXX_TESTS:$(LIBCSP_DIR)/tests/%.cc=build/test/libcsp/%)

TESTS += $(LIBCSP_C_TESTS_OBJ)
TESTS += $(LIBCSP_CXX_TESTS_OBJ)

build/test/libcsp/%: $(LIBCSP_DIR)/tests/%.c $(LIBCSP_OBJECTS)
	@echo "[LINK] $@"
	$(Q)$(CC) $(CCFLAGS) $(CFLAGS) $^ $(LDFLAGS) $(CPPFLAGS) -o $@

build/test/libcsp/%: $(LIBCSP_DIR)/tests/%.cc $(LIBCSP_OBJECTS)
	@echo "[LINK] $@"
	$(Q)$(CXX) $(CXXFLAGS) $(CFLAGS) $^ $(LDFLAGS) $(CPPFLAGS) -o $@

build/test/libcsp/kiss_tcp_test_client: $(LIBCSP_DIR)/tests/kiss_tcp_test_client.cc \
	$(LIBCSP_OBJECTS) $(COMMON_CXX_ARCH_LINUX_OBJECTS)
	@echo "[LINK] $@"
	$(Q)$(CXX) $(CXXFLAGS) $(CFLAGS) $^ $(LDFLAGS) $(CPPFLAGS) -o $@

