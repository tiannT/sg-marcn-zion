API_MAJOR_VERSION=1
API_MINOR_VERSION=0

CROSS_COMPILE ?= riscv64-unknown-linux-gnu-
PLATFORM ?= qemu
ifeq ($(PLATFORM),sg2044)
CONFIG_MACRN := 1
endif

#enable stack overflow check
BASEDIR := $(shell pwd)
HV_OBJDIR ?= $(CURDIR)/build/$(PLATFORM)
HV_MODDIR ?= $(HV_OBJDIR)/modules
HV_FILE := acrn

BOOT_MOD = $(HV_MODDIR)/boot_mod.a

# initialize the flags we used
CFLAGS := -D__riscv64__
ASFLAGS := -D__riscv64__
LDFLAGS :=
ARFLAGS :=
ifdef CONFIG_MACRN
RISCV_ISA ?= rv64gc_zbb
else
RISCV_ISA ?= rv64g_h
endif
ARCH_CFLAGS := -march=$(RISCV_ISA) -mabi=lp64d -mcmodel=medany
ARCH_ASFLAGS := -march=$(RISCV_ISA) -mabi=lp64d
ARCH_ARFLAGS :=
ARCH_LDFLAGS := -mcmodel=medany


.PHONY: default
default: all

SCENARIO := test
BOARD := $(PLATFORM)

BOARD_INFO_DIR := $(HV_CONFIG_DIR)/boards
SCENARIO_CFG_DIR := $(HV_CONFIG_DIR)/scenarios/$(SCENARIO)
BOARD_CFG_DIR := $(SCENARIO_CFG_DIR)

#include scripts/makefile/config.mk

include ../paths.make

LD_IN_TOOL = scripts/genld.sh
BASH = $(shell which bash)

ARFLAGS += crs

# ACRN depends on zero length array. Silence the gcc if Warrary-bounds is default option
CFLAGS += -Wno-array-bounds
ifeq (y, $(CONFIG_RELOC))
CFLAGS += -fpie
else
CFLAGS += -static
endif

ifdef STACK_PROTECTOR
ifeq (true, $(shell [ $(GCC_MAJOR) -gt 4 ] && echo true))
CFLAGS += -fstack-protector-strong
else
ifeq (true, $(shell [ $(GCC_MAJOR) -eq 4 ] && [ $(GCC_MINOR) -ge 9 ] && echo true))
CFLAGS += -fstack-protector-strong
else
CFLAGS += -fstack-protector
endif
endif
CFLAGS += -DSTACK_PROTECTOR
endif

CFLAGS += -DCONFIG_RISCV64
ifeq ($(PLATFORM),sg2044)
CFLAGS += -DCONFIG_PLATFORM_SG2044
ASFLAGS += -DCONFIG_PLATFORM_SG2044
endif

#ASFLAGS += -nostdlib
ASFLAGS += -nostdinc

ifeq (y, $(CONFIG_RELOC))
ASFLAGS += -DCONFIG_RELOC
endif

#LDFLAGS += -Wl,--gc-sections -nostartfiles -nostdlib 
LDFLAGS += -Wl,--gc-sections -Wl,--build-id=none -nostartfiles
LDFLAGS += -Wl,-n,-z,max-page-size=0x1000
LDFLAGS += -Wl,--no-dynamic-linker
LDFLAGS += -static

ARCH_CFLAGS += -DBUILD_ID -fno-strict-aliasing -Werror -Wall -Wstrict-prototypes -Wdeclaration-after-statement -Wno-unused-but-set-variable -Wno-unused-local-typedefs -O1 -fno-omit-frame-pointer -fno-builtin -fno-common -Wredundant-decls -Wno-pointer-arith -Wvla -pipe -Wa,--strip-local-absolute -g -mcmodel=medany -fno-stack-protector -fno-exceptions -fno-asynchronous-unwind-tables -Wnested-externs

ARCH_CFLAGS += -D__ACRN__
CFLAGS += -std=gnu99

ARCH_ASFLAGS += -D__ASSEMBLY__ -DBUILD_ID -fno-strict-aliasing -Werror -Wall -Wstrict-prototypes -Wdeclaration-after-statement -Wno-unused-but-set-variable -Wno-unused-local-typedefs -O1 -fno-omit-frame-pointer  -fno-builtin -fno-common -Wredundant-decls -Wno-pointer-arith -Wvla -pipe  -Wa,--strip-local-absolute -g -mcmodel=medany -fno-stack-protector -fno-exceptions -fno-asynchronous-unwind-tables -Wnested-externs
ARCH_ASFLAGS += -DCONFIG_RISCV64
ARCH_ASFLAGS += -D__ACRN__
ARCH_ASFLAGS += -xassembler-with-cpp

ARCH_LDSCRIPT = rv_link.ld
ARCH_LDSCRIPT_IN = arch/riscv/rv_link.lds.S

INCLUDE_PATH += boot/include/
INCLUDE_PATH += include/lib/
INCLUDE_PATH += include/
INCLUDE_PATH += include/hw
INCLUDE_PATH += include/common/
INCLUDE_PATH += include/acrn/
INCLUDE_PATH += include/arch/
INCLUDE_PATH += include/debug/
INCLUDE_PATH += include/public/
INCLUDE_PATH += include/dm/
INCLUDE_PATH += include/arch/$(ARCH)/
INCLUDE_PATH += lib/libfdt/
#INCLUDE_PATH += /usr/include

CC := $(CROSS_COMPILE)gcc
AS := $(CROSS_COMPILE)as
AR := $(CROSS_COMPILE)ar
LD := $(CROSS_COMPILE)ld
OBJCOPY ?= $(CROSS_COMPILE)objcopy

CFLAGS += -DCONFIG_RETPOLINE

# m-mode hypervisor
#CONFIG_MACRN := 1

# unit testing framework with builtin fake kernel
#CONFIG_KTEST := 1

ifdef CONFIG_MACRN
CFLAGS += -DCONFIG_MACRN
ASFLAGS += -DCONFIG_MACRN
ifneq ($(STATIC_GUESTS),)
ifneq ($(filter $(STATIC_GUESTS),smoke linux),$(STATIC_GUESTS))
$(error STATIC_GUESTS must be smoke or linux)
endif
ifneq ($(PCPU_COUNT),2)
$(error Static two-VM profile requires PCPU_COUNT=2)
endif
ifneq ($(BRINGUP_GUEST),)
$(error STATIC_GUESTS and BRINGUP_GUEST cannot be combined)
endif
CFLAGS += -DCONFIG_STATIC_DUAL
ASFLAGS += -DCONFIG_STATIC_DUAL
ifeq ($(PLAN_B_SERVICES),1)
ifneq ($(STATIC_GUESTS),linux)
$(error PLAN_B_SERVICES requires STATIC_GUESTS=linux)
endif
CFLAGS += -DCONFIG_PLAN_B_SERVICES
ASFLAGS += -DCONFIG_PLAN_B_SERVICES
endif
ifeq ($(PLAN_B_ENCLAVE),1)
ifneq ($(PLAN_B_SERVICES),1)
$(error PLAN_B_ENCLAVE requires PLAN_B_SERVICES=1)
endif
CFLAGS += -DCONFIG_PLAN_B_ENCLAVE
ASFLAGS += -DCONFIG_PLAN_B_ENCLAVE
endif
ifeq ($(PLAN_B_RUNTIME),1)
ifneq ($(PLAN_B_ENCLAVE),1)
$(error PLAN_B_RUNTIME requires PLAN_B_ENCLAVE=1)
endif
CFLAGS += -DCONFIG_PLAN_B_RUNTIME
ASFLAGS += -DCONFIG_PLAN_B_RUNTIME
BOOT_C_SRCS += arch/riscv/planb_runtime.c arch/riscv/planb_sha3/sha3.c
endif
ifeq ($(LIVE_PMP_TEST),1)
ifneq ($(STATIC_GUESTS),linux)
$(error LIVE_PMP_TEST requires STATIC_GUESTS=linux)
endif
CFLAGS += -DCONFIG_STATIC_LIVE_TEST
ASFLAGS += -DCONFIG_STATIC_LIVE_TEST
endif
ifeq ($(STATIC_GUESTS),smoke)
CFLAGS += -DCONFIG_STATIC_SMOKE
ASFLAGS += -DCONFIG_STATIC_SMOKE
STATIC_PROBE_VM ?= 0
STATIC_PROBE_ACCESS ?= read
ASFLAGS += -DSTATIC_PROBE_VM=$(STATIC_PROBE_VM)
ifeq ($(STATIC_PROBE_ACCESS),write)
ASFLAGS += -DSTATIC_PROBE_WRITE
endif
ifeq ($(STATIC_PROBE_ACCESS),execute)
ASFLAGS += -DSTATIC_PROBE_EXECUTE
endif
BOOT_S_SRCS += arch/riscv/static_guests.s
endif
endif
ifdef PCPU_COUNT
CFLAGS += -DCONFIG_NR_CPUS=$(PCPU_COUNT)
ASFLAGS += -DCONFIG_NR_CPUS=$(PCPU_COUNT)
endif
ifdef SOS_VCPUS
CFLAGS += -DCONFIG_MAX_VCPU=$(SOS_VCPUS)
ASFLAGS += -DCONFIG_MAX_VCPU=$(SOS_VCPUS)
endif
ifneq ($(BRINGUP_GUEST),)
ifneq ($(filter $(BRINGUP_GUEST),smoke pmp-probe),$(BRINGUP_GUEST))
$(error BRINGUP_GUEST must be smoke or pmp-probe)
endif
ifneq ($(PCPU_COUNT),1)
$(error Embedded bring-up Guest requires PCPU_COUNT=1)
endif
CFLAGS += -DCONFIG_BRINGUP_GUEST
ASFLAGS += -DCONFIG_BRINGUP_GUEST
ifeq ($(BRINGUP_GUEST),pmp-probe)
CFLAGS += -DCONFIG_BRINGUP_PMP_PROBE
ASFLAGS += -DCONFIG_BRINGUP_PMP_PROBE
endif
endif
endif

ifdef CONFIG_KTEST
CFLAGS += -DCONFIG_KTEST
ASFLAGS += -DCONFIG_KTEST
endif

# platform boot component
BOOT_S_SRCS += arch/riscv/start.s
BOOT_S_SRCS += arch/riscv/intr.s
ifneq ($(BRINGUP_GUEST),)
BOOT_S_SRCS += arch/riscv/embedded_guest.s
endif
ifdef CONFIG_MACRN
BOOT_S_SRCS += arch/riscv/guest/pmp.s
ifeq ($(PLAN_B_ENCLAVE),1)
BOOT_S_SRCS += arch/riscv/planb_context.s
endif
else
BOOT_S_SRCS += arch/riscv/guest/virt.s
endif
BOOT_S_SRCS += arch/riscv/sched.s
ifdef CONFIG_KTEST
BOOT_S_SRCS += arch/riscv/ktest/mmu.s
BOOT_S_SRCS += arch/riscv/ktest/start.s
BOOT_S_SRCS += arch/riscv/ktest/intr.s
endif

BOOT_C_SRCS += arch/riscv/mtrap.c
BOOT_C_SRCS += arch/riscv/trap.c
BOOT_C_SRCS += arch/riscv/smp.c
ifdef CONFIG_SIFIVE_UART
BOOT_C_SRCS += arch/riscv/sifive_uart.c
else
BOOT_C_SRCS += arch/riscv/uart.c
endif
BOOT_C_SRCS += arch/riscv/setup.c
BOOT_C_SRCS += arch/riscv/percpu.c
BOOT_C_SRCS += arch/riscv/smpboot.c
BOOT_C_SRCS += arch/riscv/timer.c
BOOT_C_SRCS += arch/riscv/irq.c
#ifdef CONFIG_MACRN
BOOT_C_SRCS += arch/riscv/clint.c
#else
BOOT_C_SRCS += arch/riscv/sbi.c
#endif
BOOT_C_SRCS += arch/riscv/plic.c
BOOT_C_SRCS += arch/riscv/notify.c
BOOT_C_SRCS += arch/riscv/boot.c
ifdef CONFIG_MACRN
BOOT_C_SRCS += arch/riscv/platform_boot.c
BOOT_C_SRCS += arch/riscv/static_vm.c
BOOT_C_SRCS += arch/riscv/pmp_live.c
BOOT_C_SRCS += arch/riscv/planb_io.c
BOOT_C_SRCS += arch/riscv/planb_enclave.c
BOOT_C_SRCS += arch/riscv/sg2044_static.c
BOOT_C_SRCS += lib/libfdt/fdt.c lib/libfdt/fdt_ro.c lib/libfdt/fdt_rw.c
BOOT_C_SRCS += lib/libfdt/fdt_wip.c lib/libfdt/fdt_sw.c lib/libfdt/fdt_check.c
BOOT_C_SRCS += lib/libfdt/fdt_strerror.c lib/libfdt/fdt_addresses.c
BOOT_C_SRCS += lib/libfdt/fdt_empty_tree.c
endif
BOOT_C_SRCS += arch/riscv/lib/bits.c
BOOT_C_SRCS += arch/riscv/lib/memory.c

BOOT_C_SRCS += arch/riscv/guest/vmcs.c
BOOT_C_SRCS += arch/riscv/guest/vm.c
BOOT_C_SRCS += arch/riscv/guest/vio.c
BOOT_C_SRCS += arch/riscv/guest/sbi.c
BOOT_C_SRCS += arch/riscv/guest/vuart.c
BOOT_C_SRCS += arch/riscv/guest/vpci/vuart.c
BOOT_C_SRCS += arch/riscv/guest/vpci/vdev.c
BOOT_C_SRCS += arch/riscv/guest/vpci/vhostbridge.c

BOOT_C_SRCS += arch/riscv/mem.c
BOOT_C_SRCS += arch/riscv/pgtable.c
BOOT_C_SRCS += arch/riscv/pager.c
BOOT_C_SRCS += arch/riscv/float.c

ifndef CONFIG_MACRN
BOOT_C_SRCS += arch/riscv/guest/s2vm.c
endif

BOOT_C_SRCS += arch/riscv/guest/vcpu.c
BOOT_C_SRCS += arch/riscv/guest/vcsr.c
BOOT_C_SRCS += arch/riscv/guest/virq.c
BOOT_C_SRCS += arch/riscv/guest/vclint.c
BOOT_C_SRCS += arch/riscv/guest/vplic.c
BOOT_C_SRCS += arch/riscv/guest/vmexit.c
BOOT_C_SRCS += arch/riscv/guest/vmcall.c
BOOT_C_SRCS += arch/riscv/guest/guest_memory.c
BOOT_C_SRCS += arch/riscv/guest/instr_emul.c
BOOT_C_SRCS += arch/riscv/guest/tee.c

BOOT_C_SRCS += release/profiling.c
BOOT_C_SRCS += release/trace.c
BOOT_C_SRCS += lib/sprintf.c
BOOT_C_SRCS += lib/string.c
BOOT_C_SRCS += common/timer.c
BOOT_C_SRCS += common/irq.c
BOOT_C_SRCS += common/sbuf.c
BOOT_C_SRCS += common/schedule.c
BOOT_C_SRCS += common/sched_iorr.c
BOOT_C_SRCS += common/softirq.c
BOOT_C_SRCS += common/event.c
BOOT_C_SRCS += common/ticks.c
BOOT_C_SRCS += common/hv_main.c
#BOOT_C_SRCS += common/hypercall.c
BOOT_C_SRCS += debug/printf.c
BOOT_C_SRCS += release/sbuf.c
BOOT_C_SRCS += debug/shell.c
BOOT_C_SRCS += debug/string.c
BOOT_C_SRCS += debug/logmsg.c
BOOT_C_SRCS += debug/console.c
BOOT_C_SRCS += release/hypercall.c
#BOOT_C_SRCS += dm/vpic.c
#BOOT_C_SRCS += dm/vuart.c
BOOT_C_SRCS += dm/io_req.c
#BOOT_C_SRCS += dm/vpci/vdev.c

ifdef CONFIG_KTEST
BOOT_C_SRCS += arch/riscv/ktest/app.c
BOOT_C_SRCS += arch/riscv/ktest/smp.c
endif

BOOT_C_OBJS := $(patsubst %.c,$(HV_OBJDIR)/%.o,$(BOOT_C_SRCS))
BOOT_S_OBJS := $(patsubst %.s,$(HV_OBJDIR)/%.o,$(BOOT_S_SRCS))

MODULES += $(BOOT_MOD)

DISTCLEAN_OBJS := $(shell find $(BASEDIR) -name '*.o')

VERSION_H := $(HV_OBJDIR)/include/version.h
HV_CONFIG_H := $(HV_OBJDIR)/include/config.h
BOARD_INFO_H := $(HV_OBJDIR)/include/board_info.h
BUILD_OPTIONS := $(HV_OBJDIR)/build-options.txt

# Compiler options are dependencies too: changing the board/CPU count must
# never silently reuse an object from another configuration.
.PHONY: FORCE
FORCE:

$(BUILD_OPTIONS): FORCE
	@mkdir -p $(HV_OBJDIR)
	@printf '%s\n' 'CC=$(CC)' 'CFLAGS=$(CFLAGS) $(ARCH_CFLAGS)' \
		'ASFLAGS=$(ASFLAGS) $(ARCH_ASFLAGS)' 'LDFLAGS=$(LDFLAGS) $(ARCH_LDFLAGS)' \
		'CONFIG_RELEASE=$(CONFIG_RELEASE)' 'SOURCE_DATE_EPOCH=$(SOURCE_DATE_EPOCH)' > $@.tmp
	@cmp -s $@.tmp $@ && rm $@.tmp || mv $@.tmp $@

.PHONY: all
all: $(VERSION_H) $(HV_OBJDIR)/$(HV_FILE).elf
ifdef CONFIG_MACRN
all: $(HV_OBJDIR)/fw_dynamic.bin

$(HV_OBJDIR)/monitor.bin: $(HV_OBJDIR)/$(HV_FILE).elf
	$(OBJCOPY) -O binary $< $@

ifeq ($(STATIC_GUESTS),smoke)
$(HV_OBJDIR)/static-guest0.elf: ../tests/riscv/static-guest.s ../tests/riscv/pmp-recover-guest.s $(HV_CONFIG_H) $(BUILD_OPTIONS)
	$(CC) $(patsubst %, -I%, $(INCLUDE_PATH)) $(ASFLAGS) $(ARCH_ASFLAGS) \
		-include $(HV_CONFIG_H) -DSTATIC_GUEST_ID=0 -nostdlib -static -Wl,--build-id=none \
		-Wl,--no-relax -Wl,-Ttext=0xa0200000 $< -o $@

$(HV_OBJDIR)/static-guest1.elf: ../tests/riscv/static-guest.s ../tests/riscv/pmp-recover-guest.s $(HV_CONFIG_H) $(BUILD_OPTIONS)
	$(CC) $(patsubst %, -I%, $(INCLUDE_PATH)) $(ASFLAGS) $(ARCH_ASFLAGS) \
		-include $(HV_CONFIG_H) -DSTATIC_GUEST_ID=1 -nostdlib -static -Wl,--build-id=none \
		-Wl,--no-relax -Wl,-Ttext=$(if $(filter sg2044,$(PLATFORM)),0x400200000,0x140200000) $< -o $@

$(HV_OBJDIR)/static-guest%.bin: $(HV_OBJDIR)/static-guest%.elf
	$(OBJCOPY) -O binary $< $@

$(HV_OBJDIR)/arch/riscv/static_guests.o: arch/riscv/static_guests.s $(HV_OBJDIR)/static-guest0.bin $(HV_OBJDIR)/static-guest1.bin
	@mkdir -p $(dir $@)
	$(CC) $(patsubst %, -I%, $(INCLUDE_PATH)) $(ASFLAGS) $(ARCH_ASFLAGS) \
		-Wa,-I$(HV_OBJDIR) -include $(HV_CONFIG_H) -c $< -o $@ -MMD -MT $@
endif

ifneq ($(BRINGUP_GUEST),)
$(HV_OBJDIR)/bringup-guest.elf: ../tests/riscv/board-guest.s ../tests/riscv/board-guest.lds $(HV_CONFIG_H) $(BUILD_OPTIONS)
	$(CC) $(patsubst %, -I%, $(INCLUDE_PATH)) $(ASFLAGS) $(ARCH_ASFLAGS) \
		-include $(HV_CONFIG_H) -nostdlib -static -Wl,--build-id=none -Wl,--no-relax \
		-Wl,-T,../tests/riscv/board-guest.lds $< -o $@

$(HV_OBJDIR)/bringup-guest.bin: $(HV_OBJDIR)/bringup-guest.elf
	$(OBJCOPY) -O binary $< $@

$(HV_OBJDIR)/arch/riscv/embedded_guest.o: arch/riscv/embedded_guest.s $(HV_OBJDIR)/bringup-guest.bin
	@mkdir -p $(dir $@)
	$(CC) $(patsubst %, -I%, $(INCLUDE_PATH)) $(ASFLAGS) $(ARCH_ASFLAGS) \
		-Wa,-I$(HV_OBJDIR) -include $(HV_CONFIG_H) -c $< -o $@ -MMD -MT $@
endif

$(HV_OBJDIR)/mode_entry.o: arch/riscv/mode_entry.s $(HV_OBJDIR)/monitor.bin $(HV_CONFIG_H)
	$(CC) $(patsubst %, -I%, $(INCLUDE_PATH)) $(ASFLAGS) $(ARCH_ASFLAGS) -Wa,-I$(HV_OBJDIR) -include $(HV_CONFIG_H) -c $< -o $@

$(HV_OBJDIR)/mode_entry.elf: $(HV_OBJDIR)/mode_entry.o arch/riscv/mode_entry.lds
	$(CC) $(ARCH_LDFLAGS) -nostdlib -static -Wl,--build-id=none -Tarch/riscv/mode_entry.lds -o $@ $<

$(HV_OBJDIR)/fw_dynamic.bin: $(HV_OBJDIR)/mode_entry.elf
	$(OBJCOPY) -O binary $< $@
endif


.PHONY: boot-mod 

$(BOOT_MOD): $(BOOT_S_OBJS) $(BOOT_C_OBJS)
	$(AR) $(ARFLAGS) $(BOOT_MOD) $(BOOT_S_OBJS) $(BOOT_C_OBJS)

boot-mod: $(BOOT_MOD)

$(HV_OBJDIR)/$(HV_FILE).elf: $(MODULES) $(HV_OBJDIR)/$(ARCH_LDSCRIPT)
	$(CC) -Wl,-Map=$(HV_OBJDIR)/$(HV_FILE).map -o $@ $(LDFLAGS) $(ARCH_LDFLAGS) -T$(HV_OBJDIR)/$(ARCH_LDSCRIPT) \
		-Wl,--start-group $(MODULES) -Wl,--end-group

$(HV_OBJDIR)/$(ARCH_LDSCRIPT): $(ARCH_LDSCRIPT_IN)
	#cp $< $@
	$(CC) -E -P $(patsubst %, -I%, $(INCLUDE_PATH)) $(ASFLAGS) $(ARCH_ASFLAGS) -include $(HV_OBJDIR)/include/config.h -MMD -MT $@ -o $@ $<


$(HV_CONFIG_H):
	mkdir -p $(HV_OBJDIR)/include
	touch $(HV_CONFIG_H)
	echo "#ifndef HV_CONFIG_H" >> $(HV_CONFIG_H); \
	echo "#define HV_CONFIG_H" >> $(HV_CONFIG_H); \
	echo "#include <asm/config.h>" >> $(HV_CONFIG_H); \
	echo "#include <asm/board.h>" >> $(HV_CONFIG_H); \
	echo "#endif" >> $(HV_CONFIG_H)

$(BOARD_INFO_H):
	mkdir -p $(HV_OBJDIR)/include
	touch $(BOARD_INFO_H)
	echo "#ifndef BOARD_INFO_H" >> $(BOARD_INFO_H); \
	echo "#define BOARD_INFO_H" >> $(BOARD_INFO_H); \
	echo "#endif" >> $(BOARD_INFO_H)

$(VERSION_H): $(HV_CONFIG_H) $(BOARD_INFO_H) $(BUILD_OPTIONS)
	mkdir -p $(HV_OBJDIR)/include
	touch $(VERSION_H)
	COMMIT_TAGS=$$(git tag --points-at HEAD 2>/dev/null|tr -s "\n" " "); \
	COMMIT_TAGS=$$(eval echo $$COMMIT_TAGS);\
	COMMIT_TIME=$$(git log -1 --date=format:"%Y-%m-%d-%T" --format=%cd 2>/dev/null || echo source-archive); \
	TIME=$$(date -u -d "@$${SOURCE_DATE_EPOCH:-$$(date +%s)}" "+%Y-%m-%d %H:%M:%S"); \
	USER="$${USER:-$$(id -u -n)}"; \
	if [ x$(CONFIG_RELEASE) = "xy" ];then BUILD_TYPE="REL";else BUILD_TYPE="DBG";fi;\
	echo "/*" > $(VERSION_H); \
	sed 's/^/ * /' ../LICENSE >> $(VERSION_H);\
	echo " */" >> $(VERSION_H);\
	echo "" >> $(VERSION_H);\
	echo "#ifndef VERSION_H" >> $(VERSION_H); \
	echo "#define VERSION_H" >> $(VERSION_H); \
	echo "#define HV_API_MAJOR_VERSION $(API_MAJOR_VERSION)U" >> $(VERSION_H);\
	echo "#define HV_API_MINOR_VERSION $(API_MINOR_VERSION)U" >> $(VERSION_H);\
	echo "#define HV_BRANCH_VERSION "\"$(BRANCH_VERSION)\""" >> $(VERSION_H);\
	echo "#define HV_COMMIT_DIRTY "\""$$PATCH"\""" >> $(VERSION_H);\
	echo "#define HV_COMMIT_TAGS "\"$$COMMIT_TAGS\""" >> $(VERSION_H);\
	echo "#define HV_COMMIT_TIME "\"$$COMMIT_TIME\""" >> $(VERSION_H);\
	echo "#define HV_BUILD_TYPE "\""$$BUILD_TYPE"\""" >> $(VERSION_H);\
	echo "#define HV_BUILD_TIME "\""$$TIME"\""" >> $(VERSION_H);\
	echo "#define HV_BUILD_USER "\""$$USER"\""" >> $(VERSION_H);\
	echo "#define HV_BUILD_SCENARIO "\"$(SCENARIO)\""" >> $(VERSION_H);\
	echo "#define HV_BUILD_BOARD "\"$(BOARD)\""" >> $(VERSION_H);\
	echo "#endif" >> $(VERSION_H)


.PHONY: clean
clean:
	rm -rf $(HV_OBJDIR)


.PHONY: distclean
distclean:
	rm -f $(DISTCLEAN_OBJS)
	rm -rf $(HV_OBJDIR)
	rm -f tags TAGS cscope.files cscope.in.out cscope.out cscope.po.out GTAGS GPATH GRTAGS GSYMS

$(HV_OBJDIR)/%.o: %.c
	[ ! -e $@ ] && mkdir -p $(dir $@) && mkdir -p $(HV_MODDIR); \
	$(CC) $(patsubst %, -I%, $(INCLUDE_PATH)) $(CFLAGS) $(ARCH_CFLAGS) -include $(HV_OBJDIR)/include/config.h -c  $< -o $@ -MMD -MT $@

$(HV_OBJDIR)/%.o: %.s
	[ ! -e $@ ] && mkdir -p $(dir $@) && mkdir -p $(HV_MODDIR); \
	$(CC) $(patsubst %, -I%, $(INCLUDE_PATH)) $(ASFLAGS) $(ARCH_ASFLAGS) -include $(HV_OBJDIR)/include/config.h -c $< -o $@ -MMD -MT $@

.DEFAULT_GOAL := all

$(BOOT_S_OBJS) $(BOOT_C_OBJS): $(HV_CONFIG_H) $(BOARD_INFO_H) $(VERSION_H) $(BUILD_OPTIONS)
$(HV_OBJDIR)/$(ARCH_LDSCRIPT): $(HV_CONFIG_H) $(BUILD_OPTIONS)
-include $(BOOT_S_OBJS:.o=.d) $(BOOT_C_OBJS:.o=.d)

.PHONY: showconfig
showconfig:
	@echo "Build directory: $(HV_OBJDIR)"
	@echo "This build directory is configured with the settings below."
	@echo "- BOARD = $(BOARD)"
	@echo "- SCENARIO = $(SCENARIO)"
	@echo "- RELEASE = $(RELEASE)"
