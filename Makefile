THIS_DIR_ := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
THIS_DIR  := $(THIS_DIR_:/=)

all:: | prep ;

ROOT   := $(abspath ..)
EXTERN := $(abspath ..)
TARGET := libcsp

V    ?= 0
Q_0  := @
Q    := $(Q_$(V))

LIBCSP_USE_ZMQ  := 1
LIBCSP_USE_CAN  := 1
LIBCSP_USE_KISS := 1
LIBCSP_DRIVERS  := can_socketcan

VCS_REV := $(shell git rev-parse --short HEAD)

#CFLAGS   += -ggdb3
CCFLAGS  += -Wjump-misses-init
CFLAGS   += -Wextra -Wall -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wshadow -Wformat=2
#CFLAGS   += -fsanitize=leak,address,undefined
CPPFLAGS += -DVCS_REV="\"$(VCS_REV)\""

include inc.mk

# HACK have to do these here... fixing soon
$(eval $(call COMMON_CXX_TESTS))
$(eval $(call COMMON_CXX_APPS))
$(eval $(call COMMON_CXX_DEPENDS))

all:: $(OBJECTS) $(TESTS) $(APPS)

prep::
	@$(shell [[ -n"$(OBJECTS)" ]] && mkdir -p $(sort $(dir $(OBJECTS))))
	@$(shell [[ -n "$(TESTS)" ]] && mkdir -p $(sort $(dir $(TESTS))))
	@$(shell [[ -n "$(APPS)" ]] && mkdir -p $(sort $(dir $(APPS))))

print-%:
	@echo $* = $($*)

check:
	$(Q)cppcheck --std=c99 src/*.c src/**/*.c include/**/*.h --enable=all

clean:
	$(Q)$(RM) -r build
