savedcmd_/work/zion-env/zion-enclave/driver/kernel/.module-common.o := riscv64-unknown-linux-gnu-gcc -Wp,-MMD,/work/zion-env/zion-enclave/driver/kernel/..module-common.o.d -nostdinc -I/work/linux-cvm-kernel/arch/riscv/include -I./arch/riscv/include/generated -I/work/linux-cvm-kernel/include -I./include -I/work/linux-cvm-kernel/arch/riscv/include/uapi -I./arch/riscv/include/generated/uapi -I/work/linux-cvm-kernel/include/uapi -I./include/generated/uapi -include /work/linux-cvm-kernel/include/linux/compiler-version.h -include /work/linux-cvm-kernel/include/linux/kconfig.h -include /work/linux-cvm-kernel/include/linux/compiler_types.h -D__KERNEL__ -fmacro-prefix-map=/work/linux-cvm-kernel/= -std=gnu11 -fshort-wchar -funsigned-char -fno-common -fno-PIE -fno-strict-aliasing -mabi=lp64 -march=rv64imac_zicsr_zifencei -mno-save-restore -DCONFIG_PAGE_OFFSET=0xff60000000000000 -mcmodel=medany -fno-asynchronous-unwind-tables -fno-unwind-tables -mno-riscv-attribute -Wa,-mno-arch-attr -mstrict-align -fno-delete-null-pointer-checks -O2 -fno-allow-store-data-races -fno-stack-protector -fomit-frame-pointer -ftrivial-auto-var-init=zero -fno-stack-clash-protection -fstrict-flex-arrays=3 -fno-strict-overflow -fno-stack-check -fconserve-stack -fno-builtin-wcslen -fno-builtin-wcslen -Wall -Wextra -Wundef -Werror=implicit-function-declaration -Werror=implicit-int -Werror=return-type -Werror=strict-prototypes -Wno-format-security -Wno-trigraphs -Wno-frame-address -Wno-address-of-packed-member -Wmissing-declarations -Wmissing-prototypes -Wframe-larger-than=2048 -Wno-main -Wno-dangling-pointer -Wvla -Wno-pointer-sign -Wcast-function-type -Wno-array-bounds -Wno-stringop-overflow -Wno-alloc-size-larger-than -Wimplicit-fallthrough=5 -Werror=date-time -Werror=incompatible-pointer-types -Werror=designated-init -Wenum-conversion -Wunused -Wno-unused-but-set-variable -Wno-unused-const-variable -Wno-packed-not-aligned -Wno-format-overflow -Wno-format-truncation -Wno-stringop-truncation -Wno-override-init -Wno-missing-field-initializers -Wno-type-limits -Wno-shift-negative-value -Wno-maybe-uninitialized -Wno-sign-compare -Wno-unused-parameter -DMODULE -mno-relax -DKBUILD_BASENAME='".module_common"' -DKBUILD_MODNAME='".module_common.o"' -D__KBUILD_MODNAME=kmod_.module_common.o -c -o /work/zion-env/zion-enclave/driver/kernel/.module-common.o /work/linux-cvm-kernel/scripts/module-common.c

source_/work/zion-env/zion-enclave/driver/kernel/.module-common.o := /work/linux-cvm-kernel/scripts/module-common.c

deps_/work/zion-env/zion-enclave/driver/kernel/.module-common.o := \
    $(wildcard include/config/UNWINDER_ORC) \
    $(wildcard include/config/MITIGATION_RETPOLINE) \
  /work/linux-cvm-kernel/include/linux/compiler-version.h \
    $(wildcard include/config/CC_VERSION_TEXT) \
  /work/linux-cvm-kernel/include/linux/kconfig.h \
    $(wildcard include/config/CPU_BIG_ENDIAN) \
    $(wildcard include/config/BOOGER) \
    $(wildcard include/config/FOO) \
  /work/linux-cvm-kernel/include/linux/compiler_types.h \
    $(wildcard include/config/DEBUG_INFO_BTF) \
    $(wildcard include/config/PAHOLE_HAS_BTF_TAG) \
    $(wildcard include/config/FUNCTION_ALIGNMENT) \
    $(wildcard include/config/CC_HAS_SANE_FUNCTION_ALIGNMENT) \
    $(wildcard include/config/X86_64) \
    $(wildcard include/config/ARM64) \
    $(wildcard include/config/LD_DEAD_CODE_DATA_ELIMINATION) \
    $(wildcard include/config/LTO_CLANG) \
    $(wildcard include/config/HAVE_ARCH_COMPILER_H) \
    $(wildcard include/config/CC_HAS_COUNTED_BY) \
    $(wildcard include/config/UBSAN_SIGNED_WRAP) \
    $(wildcard include/config/CC_HAS_ASM_INLINE) \
  /work/linux-cvm-kernel/include/linux/compiler_attributes.h \
  /work/linux-cvm-kernel/include/linux/compiler-gcc.h \
    $(wildcard include/config/ARCH_USE_BUILTIN_BSWAP) \
    $(wildcard include/config/SHADOW_CALL_STACK) \
    $(wildcard include/config/KCOV) \
  /work/linux-cvm-kernel/include/linux/module.h \
    $(wildcard include/config/MODULES) \
    $(wildcard include/config/SYSFS) \
    $(wildcard include/config/MODULES_TREE_LOOKUP) \
    $(wildcard include/config/LIVEPATCH) \
    $(wildcard include/config/STACKTRACE_BUILD_ID) \
    $(wildcard include/config/ARCH_USES_CFI_TRAPS) \
    $(wildcard include/config/MODULE_SIG) \
    $(wildcard include/config/GENERIC_BUG) \
    $(wildcard include/config/KALLSYMS) \
    $(wildcard include/config/SMP) \
    $(wildcard include/config/TRACEPOINTS) \
    $(wildcard include/config/TREE_SRCU) \
    $(wildcard include/config/BPF_EVENTS) \
    $(wildcard include/config/DEBUG_INFO_BTF_MODULES) \
    $(wildcard include/config/JUMP_LABEL) \
    $(wildcard include/config/TRACING) \
    $(wildcard include/config/EVENT_TRACING) \
    $(wildcard include/config/FTRACE_MCOUNT_RECORD) \
    $(wildcard include/config/KPROBES) \
    $(wildcard include/config/HAVE_STATIC_CALL_INLINE) \
    $(wildcard include/config/KUNIT) \
    $(wildcard include/config/PRINTK_INDEX) \
    $(wildcard include/config/MODULE_UNLOAD) \
    $(wildcard include/config/MITIGATION_ITS) \
    $(wildcard include/config/CONSTRUCTORS) \
    $(wildcard include/config/FUNCTION_ERROR_INJECTION) \
    $(wildcard include/config/DYNAMIC_DEBUG_CORE) \
  /work/linux-cvm-kernel/include/linux/list.h \
    $(wildcard include/config/LIST_HARDENED) \
    $(wildcard include/config/DEBUG_LIST) \
  /work/linux-cvm-kernel/include/linux/container_of.h \
  /work/linux-cvm-kernel/include/linux/build_bug.h \
  /work/linux-cvm-kernel/include/linux/compiler.h \
    $(wildcard include/config/TRACE_BRANCH_PROFILING) \
    $(wildcard include/config/PROFILE_ALL_BRANCHES) \
    $(wildcard include/config/OBJTOOL) \
    $(wildcard include/config/64BIT) \
  arch/riscv/include/generated/asm/rwonce.h \
  /work/linux-cvm-kernel/include/asm-generic/rwonce.h \
  /work/linux-cvm-kernel/include/linux/kasan-checks.h \
    $(wildcard include/config/KASAN_GENERIC) \
    $(wildcard include/config/KASAN_SW_TAGS) \
  /work/linux-cvm-kernel/include/linux/types.h \
    $(wildcard include/config/HAVE_UID16) \
    $(wildcard include/config/UID16) \
    $(wildcard include/config/ARCH_DMA_ADDR_T_64BIT) \
    $(wildcard include/config/PHYS_ADDR_T_64BIT) \
    $(wildcard include/config/ARCH_32BIT_USTAT_F_TINODE) \
  /work/linux-cvm-kernel/include/uapi/linux/types.h \
  arch/riscv/include/generated/uapi/asm/types.h \
  /work/linux-cvm-kernel/include/uapi/asm-generic/types.h \
  /work/linux-cvm-kernel/include/asm-generic/int-ll64.h \
  /work/linux-cvm-kernel/include/uapi/asm-generic/int-ll64.h \
  /work/linux-cvm-kernel/arch/riscv/include/uapi/asm/bitsperlong.h \
  /work/linux-cvm-kernel/include/asm-generic/bitsperlong.h \
  /work/linux-cvm-kernel/include/uapi/asm-generic/bitsperlong.h \
  /work/linux-cvm-kernel/include/uapi/linux/posix_types.h \
  /work/linux-cvm-kernel/include/linux/stddef.h \
  /work/linux-cvm-kernel/include/uapi/linux/stddef.h \
  arch/riscv/include/generated/uapi/asm/posix_types.h \
  /work/linux-cvm-kernel/include/uapi/asm-generic/posix_types.h \
  /work/linux-cvm-kernel/include/linux/kcsan-checks.h \
    $(wildcard include/config/KCSAN) \
    $(wildcard include/config/KCSAN_WEAK_MEMORY) \
    $(wildcard include/config/KCSAN_IGNORE_ATOMICS) \
  /work/linux-cvm-kernel/include/linux/poison.h \
    $(wildcard include/config/ILLEGAL_POINTER_VALUE) \
  /work/linux-cvm-kernel/include/linux/const.h \
  /work/linux-cvm-kernel/include/vdso/const.h \
  /work/linux-cvm-kernel/include/uapi/linux/const.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/barrier.h \
    $(wildcard include/config/RISCV_ISA_ZAWRS) \
  /work/linux-cvm-kernel/arch/riscv/include/asm/cmpxchg.h \
  /work/linux-cvm-kernel/include/linux/bug.h \
    $(wildcard include/config/BUG_ON_DATA_CORRUPTION) \
  /work/linux-cvm-kernel/arch/riscv/include/asm/bug.h \
    $(wildcard include/config/GENERIC_BUG_RELATIVE_POINTERS) \
    $(wildcard include/config/DEBUG_BUGVERBOSE) \
  /work/linux-cvm-kernel/arch/riscv/include/asm/asm.h \
    $(wildcard include/config/32BIT) \
  /work/linux-cvm-kernel/include/asm-generic/bug.h \
    $(wildcard include/config/BUG) \
  /work/linux-cvm-kernel/include/linux/instrumentation.h \
    $(wildcard include/config/NOINSTR_VALIDATION) \
  /work/linux-cvm-kernel/include/linux/once_lite.h \
  /work/linux-cvm-kernel/include/linux/panic.h \
    $(wildcard include/config/PANIC_TIMEOUT) \
  /work/linux-cvm-kernel/include/linux/printk.h \
    $(wildcard include/config/MESSAGE_LOGLEVEL_DEFAULT) \
    $(wildcard include/config/CONSOLE_LOGLEVEL_DEFAULT) \
    $(wildcard include/config/CONSOLE_LOGLEVEL_QUIET) \
    $(wildcard include/config/EARLY_PRINTK) \
    $(wildcard include/config/PRINTK) \
    $(wildcard include/config/DYNAMIC_DEBUG) \
  /work/linux-cvm-kernel/include/linux/stdarg.h \
  /work/linux-cvm-kernel/include/linux/init.h \
    $(wildcard include/config/MEMORY_HOTPLUG) \
    $(wildcard include/config/HAVE_ARCH_PREL32_RELOCATIONS) \
  /work/linux-cvm-kernel/include/linux/stringify.h \
  /work/linux-cvm-kernel/include/linux/kern_levels.h \
  /work/linux-cvm-kernel/include/linux/linkage.h \
    $(wildcard include/config/ARCH_USE_SYM_ANNOTATIONS) \
  /work/linux-cvm-kernel/include/linux/export.h \
    $(wildcard include/config/MODVERSIONS) \
  /work/linux-cvm-kernel/arch/riscv/include/asm/linkage.h \
  /work/linux-cvm-kernel/include/linux/ratelimit_types.h \
  /work/linux-cvm-kernel/include/linux/bits.h \
  /work/linux-cvm-kernel/include/vdso/bits.h \
  /work/linux-cvm-kernel/include/uapi/linux/bits.h \
  /work/linux-cvm-kernel/include/uapi/linux/param.h \
  arch/riscv/include/generated/uapi/asm/param.h \
  /work/linux-cvm-kernel/include/asm-generic/param.h \
    $(wildcard include/config/HZ) \
  /work/linux-cvm-kernel/include/uapi/asm-generic/param.h \
  /work/linux-cvm-kernel/include/linux/spinlock_types_raw.h \
    $(wildcard include/config/DEBUG_SPINLOCK) \
    $(wildcard include/config/DEBUG_LOCK_ALLOC) \
  arch/riscv/include/generated/asm/spinlock_types.h \
  /work/linux-cvm-kernel/include/asm-generic/spinlock_types.h \
  arch/riscv/include/generated/asm/qrwlock_types.h \
  /work/linux-cvm-kernel/include/asm-generic/qrwlock_types.h \
  /work/linux-cvm-kernel/arch/riscv/include/uapi/asm/byteorder.h \
  /work/linux-cvm-kernel/include/linux/byteorder/little_endian.h \
  /work/linux-cvm-kernel/include/uapi/linux/byteorder/little_endian.h \
  /work/linux-cvm-kernel/include/linux/swab.h \
  /work/linux-cvm-kernel/include/uapi/linux/swab.h \
  arch/riscv/include/generated/uapi/asm/swab.h \
  /work/linux-cvm-kernel/include/uapi/asm-generic/swab.h \
  /work/linux-cvm-kernel/include/linux/byteorder/generic.h \
  /work/linux-cvm-kernel/include/linux/lockdep_types.h \
    $(wildcard include/config/PROVE_RAW_LOCK_NESTING) \
    $(wildcard include/config/LOCKDEP) \
    $(wildcard include/config/LOCK_STAT) \
  /work/linux-cvm-kernel/arch/riscv/include/asm/alternative-macros.h \
    $(wildcard include/config/RISCV_ALTERNATIVE) \
    $(wildcard include/config/k) \
    $(wildcard include/config/k_1) \
    $(wildcard include/config/k_2) \
  /work/linux-cvm-kernel/arch/riscv/include/asm/fence.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/hwcap.h \
    $(wildcard include/config/RISCV_M_MODE) \
  /work/linux-cvm-kernel/arch/riscv/include/uapi/asm/hwcap.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/insn-def.h \
    $(wildcard include/config/AS_HAS_INSN) \
  /work/linux-cvm-kernel/include/asm-generic/barrier.h \
  /work/linux-cvm-kernel/include/linux/stat.h \
  arch/riscv/include/generated/uapi/asm/stat.h \
  /work/linux-cvm-kernel/include/uapi/asm-generic/stat.h \
  /work/linux-cvm-kernel/include/uapi/linux/stat.h \
  /work/linux-cvm-kernel/include/linux/time.h \
    $(wildcard include/config/POSIX_TIMERS) \
  /work/linux-cvm-kernel/include/linux/cache.h \
    $(wildcard include/config/ARCH_HAS_CACHE_LINE_SIZE) \
  /work/linux-cvm-kernel/include/uapi/linux/kernel.h \
  /work/linux-cvm-kernel/include/uapi/linux/sysinfo.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/cache.h \
    $(wildcard include/config/RISCV_DMA_NONCOHERENT) \
    $(wildcard include/config/MMU) \
  /work/linux-cvm-kernel/include/linux/math64.h \
    $(wildcard include/config/ARCH_SUPPORTS_INT128) \
  /work/linux-cvm-kernel/include/linux/math.h \
  arch/riscv/include/generated/asm/div64.h \
  /work/linux-cvm-kernel/include/asm-generic/div64.h \
  /work/linux-cvm-kernel/include/vdso/math64.h \
  /work/linux-cvm-kernel/include/linux/time64.h \
  /work/linux-cvm-kernel/include/vdso/time64.h \
  /work/linux-cvm-kernel/include/uapi/linux/time.h \
  /work/linux-cvm-kernel/include/uapi/linux/time_types.h \
  /work/linux-cvm-kernel/include/linux/time32.h \
  /work/linux-cvm-kernel/include/linux/timex.h \
  /work/linux-cvm-kernel/include/uapi/linux/timex.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/timex.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/csr.h \
  /work/linux-cvm-kernel/include/vdso/time32.h \
  /work/linux-cvm-kernel/include/vdso/time.h \
  /work/linux-cvm-kernel/include/linux/uidgid.h \
    $(wildcard include/config/MULTIUSER) \
    $(wildcard include/config/USER_NS) \
  /work/linux-cvm-kernel/include/linux/uidgid_types.h \
  /work/linux-cvm-kernel/include/linux/highuid.h \
  /work/linux-cvm-kernel/include/linux/buildid.h \
    $(wildcard include/config/VMCORE_INFO) \
  /work/linux-cvm-kernel/include/linux/kmod.h \
  /work/linux-cvm-kernel/include/linux/umh.h \
  /work/linux-cvm-kernel/include/linux/gfp.h \
    $(wildcard include/config/HIGHMEM) \
    $(wildcard include/config/ZONE_DMA) \
    $(wildcard include/config/ZONE_DMA32) \
    $(wildcard include/config/ZONE_DEVICE) \
    $(wildcard include/config/NUMA) \
    $(wildcard include/config/COMPACTION) \
    $(wildcard include/config/CONTIG_ALLOC) \
  /work/linux-cvm-kernel/include/linux/gfp_types.h \
    $(wildcard include/config/KASAN_HW_TAGS) \
    $(wildcard include/config/SLAB_OBJ_EXT) \
  /work/linux-cvm-kernel/include/linux/mmzone.h \
    $(wildcard include/config/ARCH_FORCE_MAX_ORDER) \
    $(wildcard include/config/CMA) \
    $(wildcard include/config/MEMORY_ISOLATION) \
    $(wildcard include/config/ZSMALLOC) \
    $(wildcard include/config/UNACCEPTED_MEMORY) \
    $(wildcard include/config/IOMMU_SUPPORT) \
    $(wildcard include/config/SWAP) \
    $(wildcard include/config/NUMA_BALANCING) \
    $(wildcard include/config/TRANSPARENT_HUGEPAGE) \
    $(wildcard include/config/LRU_GEN) \
    $(wildcard include/config/LRU_GEN_STATS) \
    $(wildcard include/config/LRU_GEN_WALKS_MMU) \
    $(wildcard include/config/MEMCG) \
    $(wildcard include/config/SPARSEMEM) \
    $(wildcard include/config/MEMORY_FAILURE) \
    $(wildcard include/config/FLATMEM) \
    $(wildcard include/config/PAGE_EXTENSION) \
    $(wildcard include/config/DEFERRED_STRUCT_PAGE_INIT) \
    $(wildcard include/config/HAVE_MEMORYLESS_NODES) \
    $(wildcard include/config/SPARSEMEM_VMEMMAP) \
    $(wildcard include/config/SPARSEMEM_EXTREME) \
    $(wildcard include/config/HAVE_ARCH_PFN_VALID) \
  /work/linux-cvm-kernel/include/linux/spinlock.h \
    $(wildcard include/config/PREEMPTION) \
    $(wildcard include/config/PREEMPT_RT) \
  /work/linux-cvm-kernel/include/linux/typecheck.h \
  /work/linux-cvm-kernel/include/linux/preempt.h \
    $(wildcard include/config/PREEMPT_COUNT) \
    $(wildcard include/config/DEBUG_PREEMPT) \
    $(wildcard include/config/TRACE_PREEMPT_TOGGLE) \
    $(wildcard include/config/PREEMPT_NOTIFIERS) \
    $(wildcard include/config/PREEMPT_DYNAMIC) \
    $(wildcard include/config/PREEMPT_NONE) \
    $(wildcard include/config/PREEMPT_VOLUNTARY) \
    $(wildcard include/config/PREEMPT) \
  /work/linux-cvm-kernel/include/linux/cleanup.h \
  arch/riscv/include/generated/asm/preempt.h \
  /work/linux-cvm-kernel/include/asm-generic/preempt.h \
    $(wildcard include/config/HAVE_PREEMPT_DYNAMIC_KEY) \
  /work/linux-cvm-kernel/include/linux/thread_info.h \
    $(wildcard include/config/THREAD_INFO_IN_TASK) \
    $(wildcard include/config/GENERIC_ENTRY) \
    $(wildcard include/config/HAVE_ARCH_WITHIN_STACK_FRAMES) \
    $(wildcard include/config/HARDENED_USERCOPY) \
    $(wildcard include/config/SH) \
  /work/linux-cvm-kernel/include/linux/limits.h \
  /work/linux-cvm-kernel/include/uapi/linux/limits.h \
  /work/linux-cvm-kernel/include/vdso/limits.h \
  /work/linux-cvm-kernel/include/linux/restart_block.h \
  /work/linux-cvm-kernel/include/linux/errno.h \
  /work/linux-cvm-kernel/include/uapi/linux/errno.h \
  arch/riscv/include/generated/uapi/asm/errno.h \
  /work/linux-cvm-kernel/include/uapi/asm-generic/errno.h \
  /work/linux-cvm-kernel/include/uapi/asm-generic/errno-base.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/current.h \
  /work/linux-cvm-kernel/include/linux/bitops.h \
  /work/linux-cvm-kernel/include/asm-generic/bitops/generic-non-atomic.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/bitops.h \
    $(wildcard include/config/RISCV_ISA_ZBB) \
  /work/linux-cvm-kernel/include/linux/irqflags.h \
    $(wildcard include/config/PROVE_LOCKING) \
    $(wildcard include/config/TRACE_IRQFLAGS) \
    $(wildcard include/config/IRQSOFF_TRACER) \
    $(wildcard include/config/PREEMPT_TRACER) \
    $(wildcard include/config/DEBUG_IRQFLAGS) \
    $(wildcard include/config/TRACE_IRQFLAGS_SUPPORT) \
  /work/linux-cvm-kernel/include/linux/irqflags_types.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/irqflags.h \
  arch/riscv/include/generated/asm/percpu.h \
  /work/linux-cvm-kernel/include/asm-generic/percpu.h \
    $(wildcard include/config/HAVE_SETUP_PER_CPU_AREA) \
  /work/linux-cvm-kernel/include/linux/threads.h \
    $(wildcard include/config/NR_CPUS) \
    $(wildcard include/config/BASE_SMALL) \
  /work/linux-cvm-kernel/include/linux/percpu-defs.h \
    $(wildcard include/config/DEBUG_FORCE_WEAK_PER_CPU) \
    $(wildcard include/config/AMD_MEM_ENCRYPT) \
  /work/linux-cvm-kernel/include/asm-generic/bitops/__ffs.h \
  /work/linux-cvm-kernel/include/asm-generic/bitops/__fls.h \
  /work/linux-cvm-kernel/include/asm-generic/bitops/ffs.h \
  /work/linux-cvm-kernel/include/asm-generic/bitops/fls.h \
  /work/linux-cvm-kernel/include/asm-generic/bitops/ffz.h \
  /work/linux-cvm-kernel/include/asm-generic/bitops/fls64.h \
  /work/linux-cvm-kernel/include/asm-generic/bitops/sched.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/arch_hweight.h \
  /work/linux-cvm-kernel/include/asm-generic/bitops/const_hweight.h \
  /work/linux-cvm-kernel/include/asm-generic/bitops/instrumented-atomic.h \
  /work/linux-cvm-kernel/include/linux/instrumented.h \
  /work/linux-cvm-kernel/include/linux/kmsan-checks.h \
    $(wildcard include/config/KMSAN) \
  /work/linux-cvm-kernel/include/asm-generic/bitops/instrumented-lock.h \
  /work/linux-cvm-kernel/include/asm-generic/bitops/non-atomic.h \
  /work/linux-cvm-kernel/include/asm-generic/bitops/non-instrumented-non-atomic.h \
  /work/linux-cvm-kernel/include/asm-generic/bitops/le.h \
  /work/linux-cvm-kernel/include/asm-generic/bitops/ext2-atomic.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/thread_info.h \
    $(wildcard include/config/KASAN) \
    $(wildcard include/config/THREAD_SIZE_ORDER) \
    $(wildcard include/config/VMAP_STACK) \
  /work/linux-cvm-kernel/arch/riscv/include/asm/page.h \
    $(wildcard include/config/PAGE_SHIFT) \
    $(wildcard include/config/PAGE_OFFSET) \
    $(wildcard include/config/RISCV_ISA_ZICBOZ) \
    $(wildcard include/config/XIP_KERNEL) \
    $(wildcard include/config/DEBUG_VIRTUAL) \
  /work/linux-cvm-kernel/include/linux/pfn.h \
  /work/linux-cvm-kernel/include/asm-generic/memory_model.h \
  /work/linux-cvm-kernel/include/asm-generic/getorder.h \
  /work/linux-cvm-kernel/include/linux/log2.h \
    $(wildcard include/config/ARCH_HAS_ILOG2_U32) \
    $(wildcard include/config/ARCH_HAS_ILOG2_U64) \
  /work/linux-cvm-kernel/include/linux/sizes.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/processor.h \
    $(wildcard include/config/RISCV_ISA_V) \
  /work/linux-cvm-kernel/include/uapi/linux/prctl.h \
  /work/linux-cvm-kernel/include/vdso/processor.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/vdso/processor.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/ptrace.h \
  /work/linux-cvm-kernel/arch/riscv/include/uapi/asm/ptrace.h \
  /work/linux-cvm-kernel/include/linux/cpumask.h \
    $(wildcard include/config/FORCE_NR_CPUS) \
    $(wildcard include/config/HOTPLUG_CPU) \
    $(wildcard include/config/DEBUG_PER_CPU_MAPS) \
    $(wildcard include/config/CPUMASK_OFFSTACK) \
  /work/linux-cvm-kernel/include/linux/kernel.h \
    $(wildcard include/config/PREEMPT_VOLUNTARY_BUILD) \
    $(wildcard include/config/HAVE_PREEMPT_DYNAMIC_CALL) \
    $(wildcard include/config/PREEMPT_) \
    $(wildcard include/config/DEBUG_ATOMIC_SLEEP) \
  /work/linux-cvm-kernel/include/linux/align.h \
  /work/linux-cvm-kernel/include/linux/array_size.h \
  /work/linux-cvm-kernel/include/linux/hex.h \
  /work/linux-cvm-kernel/include/linux/kstrtox.h \
  /work/linux-cvm-kernel/include/linux/minmax.h \
  /work/linux-cvm-kernel/include/linux/sprintf.h \
  /work/linux-cvm-kernel/include/linux/static_call_types.h \
    $(wildcard include/config/HAVE_STATIC_CALL) \
  /work/linux-cvm-kernel/include/linux/instruction_pointer.h \
  /work/linux-cvm-kernel/include/linux/wordpart.h \
  /work/linux-cvm-kernel/include/linux/bitmap.h \
  /work/linux-cvm-kernel/include/linux/find.h \
  /work/linux-cvm-kernel/include/linux/string.h \
    $(wildcard include/config/BINARY_PRINTF) \
    $(wildcard include/config/FORTIFY_SOURCE) \
  /work/linux-cvm-kernel/include/linux/args.h \
  /work/linux-cvm-kernel/include/linux/err.h \
  /work/linux-cvm-kernel/include/linux/overflow.h \
  /work/linux-cvm-kernel/include/uapi/linux/string.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/string.h \
  /work/linux-cvm-kernel/include/linux/bitmap-str.h \
  /work/linux-cvm-kernel/include/linux/cpumask_types.h \
  /work/linux-cvm-kernel/include/linux/atomic.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/atomic.h \
    $(wildcard include/config/GENERIC_ATOMIC64) \
  /work/linux-cvm-kernel/include/linux/atomic/atomic-arch-fallback.h \
  /work/linux-cvm-kernel/include/linux/atomic/atomic-long.h \
  /work/linux-cvm-kernel/include/linux/atomic/atomic-instrumented.h \
  /work/linux-cvm-kernel/include/linux/numa.h \
    $(wildcard include/config/NODES_SHIFT) \
    $(wildcard include/config/NUMA_KEEP_MEMINFO) \
    $(wildcard include/config/HAVE_ARCH_NODE_DEV_GROUP) \
  /work/linux-cvm-kernel/include/linux/bottom_half.h \
  /work/linux-cvm-kernel/include/linux/lockdep.h \
    $(wildcard include/config/DEBUG_LOCKING_API_SELFTESTS) \
  /work/linux-cvm-kernel/include/linux/smp.h \
    $(wildcard include/config/UP_LATE_INIT) \
    $(wildcard include/config/CSD_LOCK_WAIT_DEBUG) \
  /work/linux-cvm-kernel/include/linux/smp_types.h \
  /work/linux-cvm-kernel/include/linux/llist.h \
    $(wildcard include/config/ARCH_HAVE_NMI_SAFE_CMPXCHG) \
  /work/linux-cvm-kernel/arch/riscv/include/asm/smp.h \
  /work/linux-cvm-kernel/include/linux/irqreturn.h \
  /work/linux-cvm-kernel/include/linux/jump_label.h \
    $(wildcard include/config/HAVE_ARCH_JUMP_LABEL_RELATIVE) \
  /work/linux-cvm-kernel/arch/riscv/include/asm/mmiowb.h \
  /work/linux-cvm-kernel/include/asm-generic/mmiowb.h \
    $(wildcard include/config/MMIOWB) \
  /work/linux-cvm-kernel/include/asm-generic/mmiowb_types.h \
  /work/linux-cvm-kernel/include/linux/spinlock_types.h \
  /work/linux-cvm-kernel/include/linux/rwlock_types.h \
  arch/riscv/include/generated/asm/spinlock.h \
  /work/linux-cvm-kernel/include/asm-generic/spinlock.h \
  arch/riscv/include/generated/asm/qrwlock.h \
  /work/linux-cvm-kernel/include/asm-generic/qrwlock.h \
  /work/linux-cvm-kernel/include/linux/rwlock.h \
  /work/linux-cvm-kernel/include/linux/spinlock_api_smp.h \
    $(wildcard include/config/INLINE_SPIN_LOCK) \
    $(wildcard include/config/INLINE_SPIN_LOCK_BH) \
    $(wildcard include/config/INLINE_SPIN_LOCK_IRQ) \
    $(wildcard include/config/INLINE_SPIN_LOCK_IRQSAVE) \
    $(wildcard include/config/INLINE_SPIN_TRYLOCK) \
    $(wildcard include/config/INLINE_SPIN_TRYLOCK_BH) \
    $(wildcard include/config/UNINLINE_SPIN_UNLOCK) \
    $(wildcard include/config/INLINE_SPIN_UNLOCK_BH) \
    $(wildcard include/config/INLINE_SPIN_UNLOCK_IRQ) \
    $(wildcard include/config/INLINE_SPIN_UNLOCK_IRQRESTORE) \
    $(wildcard include/config/GENERIC_LOCKBREAK) \
  /work/linux-cvm-kernel/include/linux/rwlock_api_smp.h \
    $(wildcard include/config/INLINE_READ_LOCK) \
    $(wildcard include/config/INLINE_WRITE_LOCK) \
    $(wildcard include/config/INLINE_READ_LOCK_BH) \
    $(wildcard include/config/INLINE_WRITE_LOCK_BH) \
    $(wildcard include/config/INLINE_READ_LOCK_IRQ) \
    $(wildcard include/config/INLINE_WRITE_LOCK_IRQ) \
    $(wildcard include/config/INLINE_READ_LOCK_IRQSAVE) \
    $(wildcard include/config/INLINE_WRITE_LOCK_IRQSAVE) \
    $(wildcard include/config/INLINE_READ_TRYLOCK) \
    $(wildcard include/config/INLINE_WRITE_TRYLOCK) \
    $(wildcard include/config/INLINE_READ_UNLOCK) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK) \
    $(wildcard include/config/INLINE_READ_UNLOCK_BH) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK_BH) \
    $(wildcard include/config/INLINE_READ_UNLOCK_IRQ) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK_IRQ) \
    $(wildcard include/config/INLINE_READ_UNLOCK_IRQRESTORE) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK_IRQRESTORE) \
  /work/linux-cvm-kernel/include/linux/list_nulls.h \
  /work/linux-cvm-kernel/include/linux/wait.h \
  /work/linux-cvm-kernel/include/linux/seqlock.h \
  /work/linux-cvm-kernel/include/linux/mutex.h \
    $(wildcard include/config/DEBUG_MUTEXES) \
  /work/linux-cvm-kernel/include/linux/osq_lock.h \
  /work/linux-cvm-kernel/include/linux/debug_locks.h \
  /work/linux-cvm-kernel/include/linux/mutex_types.h \
    $(wildcard include/config/MUTEX_SPIN_ON_OWNER) \
  /work/linux-cvm-kernel/include/linux/seqlock_types.h \
  /work/linux-cvm-kernel/include/linux/nodemask.h \
  /work/linux-cvm-kernel/include/linux/nodemask_types.h \
  /work/linux-cvm-kernel/include/linux/random.h \
    $(wildcard include/config/VMGENID) \
  /work/linux-cvm-kernel/include/uapi/linux/random.h \
  /work/linux-cvm-kernel/include/uapi/linux/ioctl.h \
  arch/riscv/include/generated/uapi/asm/ioctl.h \
  /work/linux-cvm-kernel/include/asm-generic/ioctl.h \
  /work/linux-cvm-kernel/include/uapi/asm-generic/ioctl.h \
  /work/linux-cvm-kernel/include/linux/irqnr.h \
  /work/linux-cvm-kernel/include/uapi/linux/irqnr.h \
  /work/linux-cvm-kernel/include/linux/prandom.h \
  /work/linux-cvm-kernel/include/linux/once.h \
  /work/linux-cvm-kernel/include/linux/pageblock-flags.h \
    $(wildcard include/config/HUGETLB_PAGE) \
    $(wildcard include/config/HUGETLB_PAGE_SIZE_VARIABLE) \
  /work/linux-cvm-kernel/include/linux/page-flags-layout.h \
  include/generated/bounds.h \
  /work/linux-cvm-kernel/include/linux/mm_types.h \
    $(wildcard include/config/HAVE_ALIGNED_STRUCT_PAGE) \
    $(wildcard include/config/HUGETLB_PMD_PAGE_TABLE_SHARING) \
    $(wildcard include/config/USERFAULTFD) \
    $(wildcard include/config/ANON_VMA_NAME) \
    $(wildcard include/config/PER_VMA_LOCK) \
    $(wildcard include/config/SCHED_MM_CID) \
    $(wildcard include/config/HAVE_ARCH_COMPAT_MMAP_BASES) \
    $(wildcard include/config/MEMBARRIER) \
    $(wildcard include/config/AIO) \
    $(wildcard include/config/MMU_NOTIFIER) \
    $(wildcard include/config/SPLIT_PMD_PTLOCKS) \
    $(wildcard include/config/ARCH_WANT_BATCHED_UNMAP_TLB_FLUSH) \
    $(wildcard include/config/IOMMU_MM_DATA) \
    $(wildcard include/config/KSM) \
  /work/linux-cvm-kernel/include/linux/mm_types_task.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/tlbbatch.h \
  /work/linux-cvm-kernel/include/linux/auxvec.h \
  /work/linux-cvm-kernel/include/uapi/linux/auxvec.h \
  /work/linux-cvm-kernel/arch/riscv/include/uapi/asm/auxvec.h \
  /work/linux-cvm-kernel/include/linux/kref.h \
  /work/linux-cvm-kernel/include/linux/refcount.h \
  /work/linux-cvm-kernel/include/linux/refcount_types.h \
  /work/linux-cvm-kernel/include/linux/rbtree.h \
  /work/linux-cvm-kernel/include/linux/rbtree_types.h \
  /work/linux-cvm-kernel/include/linux/rcupdate.h \
    $(wildcard include/config/PREEMPT_RCU) \
    $(wildcard include/config/TINY_RCU) \
    $(wildcard include/config/RCU_STRICT_GRACE_PERIOD) \
    $(wildcard include/config/RCU_LAZY) \
    $(wildcard include/config/TASKS_RCU_GENERIC) \
    $(wildcard include/config/RCU_STALL_COMMON) \
    $(wildcard include/config/NO_HZ_FULL) \
    $(wildcard include/config/KVM_XFER_TO_GUEST_WORK) \
    $(wildcard include/config/RCU_NOCB_CPU) \
    $(wildcard include/config/TASKS_RCU) \
    $(wildcard include/config/TASKS_TRACE_RCU) \
    $(wildcard include/config/TASKS_RUDE_RCU) \
    $(wildcard include/config/TREE_RCU) \
    $(wildcard include/config/DEBUG_OBJECTS_RCU_HEAD) \
    $(wildcard include/config/PROVE_RCU) \
    $(wildcard include/config/ARCH_WEAK_RELEASE_ACQUIRE) \
  /work/linux-cvm-kernel/include/linux/context_tracking_irq.h \
    $(wildcard include/config/CONTEXT_TRACKING_IDLE) \
  /work/linux-cvm-kernel/include/linux/rcutree.h \
  /work/linux-cvm-kernel/include/linux/maple_tree.h \
    $(wildcard include/config/MAPLE_RCU_DISABLED) \
    $(wildcard include/config/DEBUG_MAPLE_TREE) \
  /work/linux-cvm-kernel/include/linux/rwsem.h \
    $(wildcard include/config/RWSEM_SPIN_ON_OWNER) \
    $(wildcard include/config/DEBUG_RWSEMS) \
  /work/linux-cvm-kernel/include/linux/completion.h \
  /work/linux-cvm-kernel/include/linux/swait.h \
  /work/linux-cvm-kernel/include/linux/uprobes.h \
    $(wildcard include/config/UPROBES) \
  /work/linux-cvm-kernel/include/linux/workqueue.h \
    $(wildcard include/config/DEBUG_OBJECTS_WORK) \
    $(wildcard include/config/FREEZER) \
    $(wildcard include/config/WQ_WATCHDOG) \
  /work/linux-cvm-kernel/include/linux/timer.h \
    $(wildcard include/config/DEBUG_OBJECTS_TIMERS) \
  /work/linux-cvm-kernel/include/linux/ktime.h \
  /work/linux-cvm-kernel/include/linux/jiffies.h \
  /work/linux-cvm-kernel/include/vdso/jiffies.h \
  include/generated/timeconst.h \
  /work/linux-cvm-kernel/include/vdso/ktime.h \
  /work/linux-cvm-kernel/include/linux/timekeeping.h \
    $(wildcard include/config/GENERIC_CMOS_UPDATE) \
  /work/linux-cvm-kernel/include/linux/clocksource_ids.h \
  /work/linux-cvm-kernel/include/linux/debugobjects.h \
    $(wildcard include/config/DEBUG_OBJECTS) \
    $(wildcard include/config/DEBUG_OBJECTS_FREE) \
  /work/linux-cvm-kernel/include/linux/timer_types.h \
  /work/linux-cvm-kernel/include/linux/workqueue_types.h \
  /work/linux-cvm-kernel/include/linux/percpu_counter.h \
  /work/linux-cvm-kernel/include/linux/percpu.h \
    $(wildcard include/config/MEM_ALLOC_PROFILING) \
    $(wildcard include/config/RANDOM_KMALLOC_CACHES) \
    $(wildcard include/config/PAGE_SIZE_4KB) \
    $(wildcard include/config/NEED_PER_CPU_PAGE_FIRST_CHUNK) \
  /work/linux-cvm-kernel/include/linux/alloc_tag.h \
    $(wildcard include/config/MEM_ALLOC_PROFILING_DEBUG) \
    $(wildcard include/config/MEM_ALLOC_PROFILING_ENABLED_BY_DEFAULT) \
  /work/linux-cvm-kernel/include/linux/codetag.h \
    $(wildcard include/config/CODE_TAGGING) \
  /work/linux-cvm-kernel/include/linux/static_key.h \
  /work/linux-cvm-kernel/include/linux/mmdebug.h \
    $(wildcard include/config/DEBUG_VM) \
    $(wildcard include/config/DEBUG_VM_IRQSOFF) \
    $(wildcard include/config/DEBUG_VM_PGFLAGS) \
  /work/linux-cvm-kernel/include/linux/sched.h \
    $(wildcard include/config/VIRT_CPU_ACCOUNTING_NATIVE) \
    $(wildcard include/config/SCHED_INFO) \
    $(wildcard include/config/SCHEDSTATS) \
    $(wildcard include/config/SCHED_CORE) \
    $(wildcard include/config/FAIR_GROUP_SCHED) \
    $(wildcard include/config/RT_GROUP_SCHED) \
    $(wildcard include/config/RT_MUTEXES) \
    $(wildcard include/config/UCLAMP_TASK) \
    $(wildcard include/config/UCLAMP_BUCKETS_COUNT) \
    $(wildcard include/config/KMAP_LOCAL) \
    $(wildcard include/config/SCHED_CLASS_EXT) \
    $(wildcard include/config/CGROUP_SCHED) \
    $(wildcard include/config/BLK_DEV_IO_TRACE) \
    $(wildcard include/config/MEMCG_V1) \
    $(wildcard include/config/COMPAT_BRK) \
    $(wildcard include/config/CGROUPS) \
    $(wildcard include/config/BLK_CGROUP) \
    $(wildcard include/config/PSI) \
    $(wildcard include/config/PAGE_OWNER) \
    $(wildcard include/config/EVENTFD) \
    $(wildcard include/config/ARCH_HAS_CPU_PASID) \
    $(wildcard include/config/CPU_SUP_INTEL) \
    $(wildcard include/config/TASK_DELAY_ACCT) \
    $(wildcard include/config/STACKPROTECTOR) \
    $(wildcard include/config/ARCH_HAS_SCALED_CPUTIME) \
    $(wildcard include/config/VIRT_CPU_ACCOUNTING_GEN) \
    $(wildcard include/config/POSIX_CPUTIMERS) \
    $(wildcard include/config/POSIX_CPU_TIMERS_TASK_WORK) \
    $(wildcard include/config/KEYS) \
    $(wildcard include/config/SYSVIPC) \
    $(wildcard include/config/DETECT_HUNG_TASK) \
    $(wildcard include/config/IO_URING) \
    $(wildcard include/config/AUDIT) \
    $(wildcard include/config/AUDITSYSCALL) \
    $(wildcard include/config/UBSAN) \
    $(wildcard include/config/UBSAN_TRAP) \
    $(wildcard include/config/TASK_XACCT) \
    $(wildcard include/config/CPUSETS) \
    $(wildcard include/config/X86_CPU_RESCTRL) \
    $(wildcard include/config/FUTEX) \
    $(wildcard include/config/COMPAT) \
    $(wildcard include/config/PERF_EVENTS) \
    $(wildcard include/config/RSEQ) \
    $(wildcard include/config/FAULT_INJECTION) \
    $(wildcard include/config/LATENCYTOP) \
    $(wildcard include/config/FUNCTION_GRAPH_TRACER) \
    $(wildcard include/config/BCACHE) \
    $(wildcard include/config/SECURITY) \
    $(wildcard include/config/BPF_SYSCALL) \
    $(wildcard include/config/GCC_PLUGIN_STACKLEAK) \
    $(wildcard include/config/X86_MCE) \
    $(wildcard include/config/KRETPROBES) \
    $(wildcard include/config/RETHOOK) \
    $(wildcard include/config/ARCH_HAS_PARANOID_L1D_FLUSH) \
    $(wildcard include/config/RV) \
    $(wildcard include/config/USER_EVENTS) \
  /work/linux-cvm-kernel/include/uapi/linux/sched.h \
  /work/linux-cvm-kernel/include/linux/pid_types.h \
  /work/linux-cvm-kernel/include/linux/sem_types.h \
  /work/linux-cvm-kernel/include/linux/shm.h \
  arch/riscv/include/generated/asm/shmparam.h \
  /work/linux-cvm-kernel/include/asm-generic/shmparam.h \
  /work/linux-cvm-kernel/include/linux/kmsan_types.h \
  /work/linux-cvm-kernel/include/linux/plist_types.h \
  /work/linux-cvm-kernel/include/linux/hrtimer_types.h \
  /work/linux-cvm-kernel/include/linux/timerqueue_types.h \
  /work/linux-cvm-kernel/include/linux/seccomp_types.h \
    $(wildcard include/config/SECCOMP) \
  /work/linux-cvm-kernel/include/linux/resource.h \
  /work/linux-cvm-kernel/include/uapi/linux/resource.h \
  arch/riscv/include/generated/uapi/asm/resource.h \
  /work/linux-cvm-kernel/include/asm-generic/resource.h \
  /work/linux-cvm-kernel/include/uapi/asm-generic/resource.h \
  /work/linux-cvm-kernel/include/linux/latencytop.h \
  /work/linux-cvm-kernel/include/linux/sched/prio.h \
  /work/linux-cvm-kernel/include/linux/sched/types.h \
  /work/linux-cvm-kernel/include/linux/signal_types.h \
    $(wildcard include/config/OLD_SIGACTION) \
  /work/linux-cvm-kernel/include/uapi/linux/signal.h \
  arch/riscv/include/generated/uapi/asm/signal.h \
  /work/linux-cvm-kernel/include/asm-generic/signal.h \
  /work/linux-cvm-kernel/include/uapi/asm-generic/signal.h \
  /work/linux-cvm-kernel/include/uapi/asm-generic/signal-defs.h \
  /work/linux-cvm-kernel/arch/riscv/include/uapi/asm/sigcontext.h \
  arch/riscv/include/generated/uapi/asm/siginfo.h \
  /work/linux-cvm-kernel/include/uapi/asm-generic/siginfo.h \
  /work/linux-cvm-kernel/include/linux/syscall_user_dispatch_types.h \
  /work/linux-cvm-kernel/include/linux/netdevice_xmit.h \
    $(wildcard include/config/NET_EGRESS) \
  /work/linux-cvm-kernel/include/linux/task_io_accounting.h \
    $(wildcard include/config/TASK_IO_ACCOUNTING) \
  /work/linux-cvm-kernel/include/linux/posix-timers_types.h \
  /work/linux-cvm-kernel/include/uapi/linux/rseq.h \
  /work/linux-cvm-kernel/include/linux/kcsan.h \
  /work/linux-cvm-kernel/include/linux/rv.h \
    $(wildcard include/config/RV_REACTORS) \
  /work/linux-cvm-kernel/include/linux/livepatch_sched.h \
  arch/riscv/include/generated/asm/kmap_size.h \
  /work/linux-cvm-kernel/include/asm-generic/kmap_size.h \
    $(wildcard include/config/DEBUG_KMAP_LOCAL) \
  /work/linux-cvm-kernel/include/linux/sched/ext.h \
    $(wildcard include/config/EXT_GROUP_SCHED) \
  /work/linux-cvm-kernel/arch/riscv/include/asm/mmu.h \
    $(wildcard include/config/BINFMT_ELF_FDPIC) \
  /work/linux-cvm-kernel/include/linux/page-flags.h \
    $(wildcard include/config/PAGE_IDLE_FLAG) \
    $(wildcard include/config/ARCH_USES_PG_ARCH_2) \
    $(wildcard include/config/ARCH_USES_PG_ARCH_3) \
    $(wildcard include/config/HUGETLB_PAGE_OPTIMIZE_VMEMMAP) \
    $(wildcard include/config/DEBUG_KMAP_LOCAL_FORCE_MAP) \
  /work/linux-cvm-kernel/include/linux/local_lock.h \
  /work/linux-cvm-kernel/include/linux/local_lock_internal.h \
  /work/linux-cvm-kernel/include/linux/zswap.h \
    $(wildcard include/config/ZSWAP) \
  /work/linux-cvm-kernel/include/linux/memory_hotplug.h \
    $(wildcard include/config/ARCH_HAS_ADD_PAGES) \
    $(wildcard include/config/MEMORY_HOTREMOVE) \
  /work/linux-cvm-kernel/include/linux/notifier.h \
  /work/linux-cvm-kernel/include/linux/srcu.h \
    $(wildcard include/config/TINY_SRCU) \
    $(wildcard include/config/NEED_SRCU_NMI_SAFE) \
  /work/linux-cvm-kernel/include/linux/rcu_segcblist.h \
  /work/linux-cvm-kernel/include/linux/srcutree.h \
  /work/linux-cvm-kernel/include/linux/rcu_node_tree.h \
    $(wildcard include/config/RCU_FANOUT) \
    $(wildcard include/config/RCU_FANOUT_LEAF) \
  /work/linux-cvm-kernel/include/linux/topology.h \
    $(wildcard include/config/USE_PERCPU_NUMA_NODE_ID) \
    $(wildcard include/config/SCHED_SMT) \
  /work/linux-cvm-kernel/include/linux/arch_topology.h \
    $(wildcard include/config/GENERIC_ARCH_TOPOLOGY) \
  /work/linux-cvm-kernel/arch/riscv/include/asm/topology.h \
  /work/linux-cvm-kernel/include/asm-generic/topology.h \
  /work/linux-cvm-kernel/include/linux/sysctl.h \
    $(wildcard include/config/SYSCTL) \
  /work/linux-cvm-kernel/include/uapi/linux/sysctl.h \
  /work/linux-cvm-kernel/include/linux/elf.h \
    $(wildcard include/config/ARCH_HAVE_EXTRA_ELF_NOTES) \
    $(wildcard include/config/ARCH_USE_GNU_PROPERTY) \
    $(wildcard include/config/ARCH_HAVE_ELF_PROT) \
  /work/linux-cvm-kernel/arch/riscv/include/asm/elf.h \
  /work/linux-cvm-kernel/include/uapi/linux/elf.h \
  /work/linux-cvm-kernel/include/uapi/linux/elf-em.h \
  /work/linux-cvm-kernel/include/linux/compat.h \
    $(wildcard include/config/ARCH_HAS_SYSCALL_WRAPPER) \
    $(wildcard include/config/X86_X32_ABI) \
    $(wildcard include/config/COMPAT_OLD_SIGACTION) \
    $(wildcard include/config/ODD_RT_SIGACTION) \
  /work/linux-cvm-kernel/include/linux/sem.h \
  /work/linux-cvm-kernel/include/uapi/linux/sem.h \
  /work/linux-cvm-kernel/include/linux/ipc.h \
  /work/linux-cvm-kernel/include/linux/rhashtable-types.h \
  /work/linux-cvm-kernel/include/uapi/linux/ipc.h \
  arch/riscv/include/generated/uapi/asm/ipcbuf.h \
  /work/linux-cvm-kernel/include/uapi/asm-generic/ipcbuf.h \
  arch/riscv/include/generated/uapi/asm/sembuf.h \
  /work/linux-cvm-kernel/include/uapi/asm-generic/sembuf.h \
  /work/linux-cvm-kernel/include/linux/socket.h \
    $(wildcard include/config/PROC_FS) \
  arch/riscv/include/generated/uapi/asm/socket.h \
  /work/linux-cvm-kernel/include/uapi/asm-generic/socket.h \
  arch/riscv/include/generated/uapi/asm/sockios.h \
  /work/linux-cvm-kernel/include/uapi/asm-generic/sockios.h \
  /work/linux-cvm-kernel/include/uapi/linux/sockios.h \
  /work/linux-cvm-kernel/include/linux/uio.h \
    $(wildcard include/config/ARCH_HAS_UACCESS_FLUSHCACHE) \
    $(wildcard include/config/ARCH_HAS_COPY_MC) \
  /work/linux-cvm-kernel/include/uapi/linux/uio.h \
  /work/linux-cvm-kernel/include/uapi/linux/socket.h \
  /work/linux-cvm-kernel/include/uapi/linux/if.h \
  /work/linux-cvm-kernel/include/uapi/linux/libc-compat.h \
  /work/linux-cvm-kernel/include/uapi/linux/hdlc/ioctl.h \
  /work/linux-cvm-kernel/include/linux/fs.h \
    $(wildcard include/config/READ_ONLY_THP_FOR_FS) \
    $(wildcard include/config/FS_POSIX_ACL) \
    $(wildcard include/config/CGROUP_WRITEBACK) \
    $(wildcard include/config/IMA) \
    $(wildcard include/config/FILE_LOCKING) \
    $(wildcard include/config/FSNOTIFY) \
    $(wildcard include/config/FS_ENCRYPTION) \
    $(wildcard include/config/FS_VERITY) \
    $(wildcard include/config/EPOLL) \
    $(wildcard include/config/UNICODE) \
    $(wildcard include/config/QUOTA) \
    $(wildcard include/config/FS_DAX) \
    $(wildcard include/config/BLOCK) \
  /work/linux-cvm-kernel/include/linux/wait_bit.h \
  /work/linux-cvm-kernel/include/linux/kdev_t.h \
  /work/linux-cvm-kernel/include/uapi/linux/kdev_t.h \
  /work/linux-cvm-kernel/include/linux/dcache.h \
  /work/linux-cvm-kernel/include/linux/rculist.h \
    $(wildcard include/config/PROVE_RCU_LIST) \
  /work/linux-cvm-kernel/include/linux/rculist_bl.h \
  /work/linux-cvm-kernel/include/linux/list_bl.h \
  /work/linux-cvm-kernel/include/linux/bit_spinlock.h \
  /work/linux-cvm-kernel/include/linux/lockref.h \
    $(wildcard include/config/ARCH_USE_CMPXCHG_LOCKREF) \
  /work/linux-cvm-kernel/include/linux/stringhash.h \
    $(wildcard include/config/DCACHE_WORD_ACCESS) \
  /work/linux-cvm-kernel/include/linux/hash.h \
    $(wildcard include/config/HAVE_ARCH_HASH) \
  /work/linux-cvm-kernel/include/linux/path.h \
  /work/linux-cvm-kernel/include/linux/list_lru.h \
  /work/linux-cvm-kernel/include/linux/shrinker.h \
    $(wildcard include/config/SHRINKER_DEBUG) \
  /work/linux-cvm-kernel/include/linux/xarray.h \
    $(wildcard include/config/XARRAY_MULTI) \
  /work/linux-cvm-kernel/include/linux/sched/mm.h \
    $(wildcard include/config/MMU_LAZY_TLB_REFCOUNT) \
    $(wildcard include/config/ARCH_HAS_MEMBARRIER_CALLBACKS) \
  /work/linux-cvm-kernel/include/linux/sync_core.h \
    $(wildcard include/config/ARCH_HAS_SYNC_CORE_BEFORE_USERMODE) \
    $(wildcard include/config/ARCH_HAS_PREPARE_SYNC_CORE_CMD) \
  /work/linux-cvm-kernel/arch/riscv/include/asm/sync_core.h \
  /work/linux-cvm-kernel/include/linux/sched/coredump.h \
    $(wildcard include/config/CORE_DUMP_DEFAULT_ELF_HEADERS) \
  /work/linux-cvm-kernel/arch/riscv/include/asm/membarrier.h \
  /work/linux-cvm-kernel/include/linux/radix-tree.h \
  /work/linux-cvm-kernel/include/linux/pid.h \
  /work/linux-cvm-kernel/include/linux/capability.h \
  /work/linux-cvm-kernel/include/uapi/linux/capability.h \
  /work/linux-cvm-kernel/include/linux/semaphore.h \
  /work/linux-cvm-kernel/include/linux/fcntl.h \
    $(wildcard include/config/ARCH_32BIT_OFF_T) \
  /work/linux-cvm-kernel/include/uapi/linux/fcntl.h \
  arch/riscv/include/generated/uapi/asm/fcntl.h \
  /work/linux-cvm-kernel/include/uapi/asm-generic/fcntl.h \
  /work/linux-cvm-kernel/include/uapi/linux/openat2.h \
  /work/linux-cvm-kernel/include/linux/migrate_mode.h \
  /work/linux-cvm-kernel/include/linux/percpu-rwsem.h \
  /work/linux-cvm-kernel/include/linux/rcuwait.h \
  /work/linux-cvm-kernel/include/linux/sched/signal.h \
    $(wildcard include/config/SCHED_AUTOGROUP) \
    $(wildcard include/config/BSD_PROCESS_ACCT) \
    $(wildcard include/config/TASKSTATS) \
    $(wildcard include/config/STACK_GROWSUP) \
  /work/linux-cvm-kernel/include/linux/signal.h \
    $(wildcard include/config/DYNAMIC_SIGFRAME) \
  /work/linux-cvm-kernel/include/linux/sched/jobctl.h \
  /work/linux-cvm-kernel/include/linux/sched/task.h \
    $(wildcard include/config/HAVE_EXIT_THREAD) \
    $(wildcard include/config/ARCH_WANTS_DYNAMIC_TASK_STRUCT) \
    $(wildcard include/config/HAVE_ARCH_THREAD_STRUCT_WHITELIST) \
  /work/linux-cvm-kernel/include/linux/uaccess.h \
    $(wildcard include/config/ARCH_HAS_SUBPAGE_FAULTS) \
  /work/linux-cvm-kernel/include/linux/fault-inject-usercopy.h \
    $(wildcard include/config/FAULT_INJECTION_USERCOPY) \
  /work/linux-cvm-kernel/include/linux/nospec.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/uaccess.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/asm-extable.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/gpr-num.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/pgtable.h \
    $(wildcard include/config/XIP_PHYS_ADDR) \
    $(wildcard include/config/PHYS_RAM_BASE) \
    $(wildcard include/config/RISCV_ISA_SVNAPOT) \
    $(wildcard include/config/ARCH_HAS_PTE_DEVMAP) \
    $(wildcard include/config/PAGE_TABLE_CHECK) \
    $(wildcard include/config/ARCH_ENABLE_THP_MIGRATION) \
  /work/linux-cvm-kernel/arch/riscv/include/asm/pgtable-bits.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/tlbflush.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/errata_list.h \
    $(wildcard include/config/ERRATA_ANDES) \
    $(wildcard include/config/ERRATA_SIFIVE) \
    $(wildcard include/config/ERRATA_THEAD) \
    $(wildcard include/config/ERRATA_SIFIVE_CIP_453) \
    $(wildcard include/config/ERRATA_SIFIVE_CIP_1200) \
    $(wildcard include/config/RISCV_ISA_SVPBMT) \
    $(wildcard include/config/ERRATA_THEAD_MAE) \
    $(wildcard include/config/RISCV_ISA_ZICBOM) \
  /work/linux-cvm-kernel/arch/riscv/include/asm/alternative.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/vendorid_list.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/compat.h \
  /work/linux-cvm-kernel/include/linux/sched/task_stack.h \
    $(wildcard include/config/DEBUG_STACK_USAGE) \
  /work/linux-cvm-kernel/include/uapi/linux/magic.h \
  /work/linux-cvm-kernel/include/linux/kasan.h \
    $(wildcard include/config/KASAN_STACK) \
    $(wildcard include/config/KASAN_VMALLOC) \
  /work/linux-cvm-kernel/include/linux/kasan-enabled.h \
  /work/linux-cvm-kernel/include/linux/kasan-tags.h \
  /work/linux-cvm-kernel/include/asm-generic/compat.h \
    $(wildcard include/config/COMPAT_FOR_U64_ALIGNMENT) \
  /work/linux-cvm-kernel/arch/riscv/include/asm/pgtable-64.h \
  /work/linux-cvm-kernel/include/linux/page_table_check.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/extable.h \
    $(wildcard include/config/BPF_JIT) \
    $(wildcard include/config/ARCH_RV64I) \
  /work/linux-cvm-kernel/include/asm-generic/access_ok.h \
    $(wildcard include/config/ALTERNATE_USER_ADDRESS_SPACE) \
  /work/linux-cvm-kernel/include/linux/cred.h \
  /work/linux-cvm-kernel/include/linux/key.h \
    $(wildcard include/config/KEY_NOTIFICATIONS) \
    $(wildcard include/config/NET) \
  /work/linux-cvm-kernel/include/linux/assoc_array.h \
    $(wildcard include/config/ASSOCIATIVE_ARRAY) \
  /work/linux-cvm-kernel/include/linux/sched/user.h \
    $(wildcard include/config/VFIO_PCI_ZDEV_KVM) \
    $(wildcard include/config/IOMMUFD) \
    $(wildcard include/config/WATCH_QUEUE) \
  /work/linux-cvm-kernel/include/linux/ratelimit.h \
  /work/linux-cvm-kernel/include/linux/posix-timers.h \
  /work/linux-cvm-kernel/include/linux/alarmtimer.h \
    $(wildcard include/config/RTC_CLASS) \
  /work/linux-cvm-kernel/include/linux/hrtimer.h \
    $(wildcard include/config/HIGH_RES_TIMERS) \
    $(wildcard include/config/TIME_LOW_RES) \
    $(wildcard include/config/TIMERFD) \
  /work/linux-cvm-kernel/include/linux/hrtimer_defs.h \
  /work/linux-cvm-kernel/include/linux/timerqueue.h \
  /work/linux-cvm-kernel/include/linux/rcu_sync.h \
  /work/linux-cvm-kernel/include/linux/delayed_call.h \
  /work/linux-cvm-kernel/include/linux/uuid.h \
  /work/linux-cvm-kernel/include/linux/errseq.h \
  /work/linux-cvm-kernel/include/linux/ioprio.h \
  /work/linux-cvm-kernel/include/linux/sched/rt.h \
  /work/linux-cvm-kernel/include/linux/iocontext.h \
    $(wildcard include/config/BLK_ICQ) \
  /work/linux-cvm-kernel/include/uapi/linux/ioprio.h \
  /work/linux-cvm-kernel/include/linux/fs_types.h \
  /work/linux-cvm-kernel/include/linux/mount.h \
  /work/linux-cvm-kernel/include/linux/mnt_idmapping.h \
  /work/linux-cvm-kernel/include/linux/slab.h \
    $(wildcard include/config/FAILSLAB) \
    $(wildcard include/config/KFENCE) \
    $(wildcard include/config/SLUB_TINY) \
    $(wildcard include/config/SLUB_DEBUG) \
    $(wildcard include/config/SLAB_FREELIST_HARDENED) \
    $(wildcard include/config/SLAB_BUCKETS) \
  /work/linux-cvm-kernel/include/linux/percpu-refcount.h \
  /work/linux-cvm-kernel/include/linux/rw_hint.h \
  /work/linux-cvm-kernel/include/uapi/linux/fs.h \
  /work/linux-cvm-kernel/include/linux/quota.h \
    $(wildcard include/config/QUOTA_NETLINK_INTERFACE) \
  /work/linux-cvm-kernel/include/uapi/linux/dqblk_xfs.h \
  /work/linux-cvm-kernel/include/linux/dqblk_v1.h \
  /work/linux-cvm-kernel/include/linux/dqblk_v2.h \
  /work/linux-cvm-kernel/include/linux/dqblk_qtree.h \
  /work/linux-cvm-kernel/include/linux/projid.h \
  /work/linux-cvm-kernel/include/uapi/linux/quota.h \
  /work/linux-cvm-kernel/include/uapi/linux/aio_abi.h \
  /work/linux-cvm-kernel/include/uapi/linux/unistd.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/unistd.h \
  /work/linux-cvm-kernel/arch/riscv/include/uapi/asm/unistd.h \
  arch/riscv/include/generated/uapi/asm/unistd_64.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/syscall_wrapper.h \
  /work/linux-cvm-kernel/arch/riscv/include/uapi/asm/elf.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/cacheinfo.h \
  /work/linux-cvm-kernel/include/linux/cacheinfo.h \
    $(wildcard include/config/ACPI_PPTT) \
    $(wildcard include/config/ARCH_HAS_CPU_CACHE_ALIASING) \
  /work/linux-cvm-kernel/include/linux/cpuhplock.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/cpufeature.h \
    $(wildcard include/config/RISCV_MISALIGNED) \
    $(wildcard include/config/RISCV_PROBE_UNALIGNED_ACCESS) \
    $(wildcard include/config/HAVE_EFFICIENT_UNALIGNED_ACCESS) \
  /work/linux-cvm-kernel/include/linux/kobject.h \
    $(wildcard include/config/UEVENT_HELPER) \
    $(wildcard include/config/DEBUG_KOBJECT_RELEASE) \
  /work/linux-cvm-kernel/include/linux/sysfs.h \
  /work/linux-cvm-kernel/include/linux/kernfs.h \
    $(wildcard include/config/KERNFS) \
  /work/linux-cvm-kernel/include/linux/idr.h \
  /work/linux-cvm-kernel/include/linux/kobject_ns.h \
  /work/linux-cvm-kernel/include/linux/moduleparam.h \
    $(wildcard include/config/ALPHA) \
    $(wildcard include/config/PPC64) \
  /work/linux-cvm-kernel/include/linux/rbtree_latch.h \
  /work/linux-cvm-kernel/include/linux/error-injection.h \
  /work/linux-cvm-kernel/include/asm-generic/error-injection.h \
  /work/linux-cvm-kernel/include/linux/tracepoint-defs.h \
  /work/linux-cvm-kernel/include/linux/dynamic_debug.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/module.h \
    $(wildcard include/config/MODULE_SECTIONS) \
  /work/linux-cvm-kernel/include/asm-generic/module.h \
    $(wildcard include/config/HAVE_MOD_ARCH_SPECIFIC) \
    $(wildcard include/config/MODULES_USE_ELF_REL) \
    $(wildcard include/config/MODULES_USE_ELF_RELA) \
  /work/linux-cvm-kernel/include/linux/build-salt.h \
    $(wildcard include/config/BUILD_SALT) \
  /work/linux-cvm-kernel/include/linux/elfnote.h \
  /work/linux-cvm-kernel/include/linux/elfnote-lto.h \
    $(wildcard include/config/LTO) \
  /work/linux-cvm-kernel/include/linux/vermagic.h \
    $(wildcard include/config/PREEMPT_BUILD) \
  include/generated/utsrelease.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/vermagic.h \

/work/zion-env/zion-enclave/driver/kernel/.module-common.o: $(deps_/work/zion-env/zion-enclave/driver/kernel/.module-common.o)

$(deps_/work/zion-env/zion-enclave/driver/kernel/.module-common.o):
