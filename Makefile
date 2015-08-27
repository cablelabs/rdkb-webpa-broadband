
include $(CCSP_ROOT_DIR)/arch/ccsp_common.mk

#
#	Set up include directories
#

INCPATH += $(CCSP_ROOT_DIR)/hal/include
INCPATH += $(CCSP_ROOT_DIR)/webpa/webpaclient/source/include
INCPATH += $(CCSP_ROOT_DIR)/webpa/webpaclient/source/wal/include

CFLAGS += $(addprefix -I, $(INCPATH))


CFLAGS += -Wall
LDFLAGS+= -lccsp_common -lm -lpthread 

target := $(ComponentBuildDir)/libwal.so
source_files := $(call add_files_from_base,,'*.c')
obj_files := $(addprefix $(ComponentBuildDir)/, $(source_files:%.c=%.o))
-include $(obj_files:.o=.d)
$(target): $(obj_files)

#
#	Build targets
#
all: $(target) install

.PHONY: all clean

clean:
	rm -Rf $(ComponentBuildDir)

install:
	@echo "Installing wal Installables"
	@install -d -m 0755 $(CCSP_OUT_DIR)/webpa
	@install -m 0755 $(target) $(CCSP_OUT_DIR)/webpa
	@cp -f $(target) $(CCSP_OUT_DIR)/
	@cp $(target) $(CCSP_STAGING_ROOT)/lib


#
# include custom post makefile, if exists
#
ifneq ($(findstring $(CCSP_CMPNT_BUILD_CUSTOM_MK_POST), $(wildcard $(ComponentBoardDir)/*.mk)), )
    include $(ComponentBoardDir)/$(CCSP_CMPNT_BUILD_CUSTOM_MK_POST)
endif

