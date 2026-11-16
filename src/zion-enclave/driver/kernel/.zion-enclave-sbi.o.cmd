savedcmd_/work/zion-env/zion-enclave/driver/kernel/zion-enclave-sbi.o := riscv64-unknown-linux-gnu-gcc -Wp,-MMD,/work/zion-env/zion-enclave/driver/kernel/.zion-enclave-sbi.o.d -nostdinc -I/work/linux-cvm-kernel/arch/riscv/include -I./arch/riscv/include/generated -I/work/linux-cvm-kernel/include -I./include -I/work/linux-cvm-kernel/arch/riscv/include/uapi -I./arch/riscv/include/generated/uapi -I/work/linux-cvm-kernel/include/uapi -I./include/generated/uapi -include /work/linux-cvm-kernel/include/linux/compiler-version.h -include /work/linux-cvm-kernel/include/linux/kconfig.h -include /work/linux-cvm-kernel/include/linux/compiler_types.h -D__KERNEL__ -fmacro-prefix-map=/work/linux-cvm-kernel/= -std=gnu11 -fshort-wchar -funsigned-char -fno-common -fno-PIE -fno-strict-aliasing -mabi=lp64 -march=rv64imac_zicsr_zifencei -mno-save-restore -DCONFIG_PAGE_OFFSET=0xff60000000000000 -mcmodel=medany -fno-asynchronous-unwind-tables -fno-unwind-tables -mno-riscv-attribute -Wa,-mno-arch-attr -mstrict-align -fno-delete-null-pointer-checks -O2 -fno-allow-store-data-races -fno-stack-protector -fomit-frame-pointer -ftrivial-auto-var-init=zero -fno-stack-clash-protection -fstrict-flex-arrays=3 -fno-strict-overflow -fno-stack-check -fconserve-stack -fno-builtin-wcslen -fno-builtin-wcslen -Wall -Wextra -Wundef -Werror=implicit-function-declaration -Werror=implicit-int -Werror=return-type -Werror=strict-prototypes -Wno-format-security -Wno-trigraphs -Wno-frame-address -Wno-address-of-packed-member -Wmissing-declarations -Wmissing-prototypes -Wframe-larger-than=2048 -Wno-main -Wno-dangling-pointer -Wvla -Wno-pointer-sign -Wcast-function-type -Wno-array-bounds -Wno-stringop-overflow -Wno-alloc-size-larger-than -Wimplicit-fallthrough=5 -Werror=date-time -Werror=incompatible-pointer-types -Werror=designated-init -Wenum-conversion -Wunused -Wno-unused-but-set-variable -Wno-unused-const-variable -Wno-packed-not-aligned -Wno-format-overflow -Wno-format-truncation -Wno-stringop-truncation -Wno-override-init -Wno-missing-field-initializers -Wno-type-limits -Wno-shift-negative-value -Wno-maybe-uninitialized -Wno-sign-compare -Wno-unused-parameter -I/work/zion-env/zion-enclave/sdk/include/shared  -DMODULE -mno-relax  -DKBUILD_BASENAME='"zion_enclave_sbi"' -DKBUILD_MODNAME='"zion_driver"' -D__KBUILD_MODNAME=kmod_zion_driver -c -o /work/zion-env/zion-enclave/driver/kernel/zion-enclave-sbi.o /work/zion-env/zion-enclave/driver/kernel/zion-enclave-sbi.c  

source_/work/zion-env/zion-enclave/driver/kernel/zion-enclave-sbi.o := /work/zion-env/zion-enclave/driver/kernel/zion-enclave-sbi.c

deps_/work/zion-env/zion-enclave/driver/kernel/zion-enclave-sbi.o := \
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
    $(wildcard include/config/MITIGATION_RETPOLINE) \
    $(wildcard include/config/ARCH_USE_BUILTIN_BSWAP) \
    $(wildcard include/config/SHADOW_CALL_STACK) \
    $(wildcard include/config/KCOV) \
  /work/zion-env/zion-enclave/driver/kernel/zion-enclave-sbi.h \
  /work/zion-env/zion-enclave/sdk/include/shared/zion_user.h \
  /work/linux-cvm-kernel/include/uapi/linux/ioctl.h \
  arch/riscv/include/generated/uapi/asm/ioctl.h \
  /work/linux-cvm-kernel/include/asm-generic/ioctl.h \
  /work/linux-cvm-kernel/include/uapi/asm-generic/ioctl.h \
  /work/linux-cvm-kernel/include/linux/types.h \
    $(wildcard include/config/HAVE_UID16) \
    $(wildcard include/config/UID16) \
    $(wildcard include/config/ARCH_DMA_ADDR_T_64BIT) \
    $(wildcard include/config/PHYS_ADDR_T_64BIT) \
    $(wildcard include/config/64BIT) \
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
  /work/zion-env/zion-enclave/sdk/include/shared/sm_call.h \
  /work/zion-env/zion-enclave/sdk/include/shared/sm_err.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/sbi.h \
    $(wildcard include/config/RISCV_SBI) \
    $(wildcard include/config/RISCV_SBI_V01) \
    $(wildcard include/config/SMP) \
  /work/linux-cvm-kernel/include/linux/cpumask.h \
    $(wildcard include/config/FORCE_NR_CPUS) \
    $(wildcard include/config/HOTPLUG_CPU) \
    $(wildcard include/config/DEBUG_PER_CPU_MAPS) \
    $(wildcard include/config/CPUMASK_OFFSTACK) \
  /work/linux-cvm-kernel/include/linux/cleanup.h \
  /work/linux-cvm-kernel/include/linux/compiler.h \
    $(wildcard include/config/TRACE_BRANCH_PROFILING) \
    $(wildcard include/config/PROFILE_ALL_BRANCHES) \
    $(wildcard include/config/OBJTOOL) \
  arch/riscv/include/generated/asm/rwonce.h \
  /work/linux-cvm-kernel/include/asm-generic/rwonce.h \
  /work/linux-cvm-kernel/include/linux/kasan-checks.h \
    $(wildcard include/config/KASAN_GENERIC) \
    $(wildcard include/config/KASAN_SW_TAGS) \
  /work/linux-cvm-kernel/include/linux/kcsan-checks.h \
    $(wildcard include/config/KCSAN) \
    $(wildcard include/config/KCSAN_WEAK_MEMORY) \
    $(wildcard include/config/KCSAN_IGNORE_ATOMICS) \
  /work/linux-cvm-kernel/include/linux/kernel.h \
    $(wildcard include/config/PREEMPT_VOLUNTARY_BUILD) \
    $(wildcard include/config/PREEMPT_DYNAMIC) \
    $(wildcard include/config/HAVE_PREEMPT_DYNAMIC_CALL) \
    $(wildcard include/config/HAVE_PREEMPT_DYNAMIC_KEY) \
    $(wildcard include/config/PREEMPT_) \
    $(wildcard include/config/DEBUG_ATOMIC_SLEEP) \
    $(wildcard include/config/MMU) \
    $(wildcard include/config/PROVE_LOCKING) \
    $(wildcard include/config/TRACING) \
    $(wildcard include/config/FTRACE_MCOUNT_RECORD) \
  /work/linux-cvm-kernel/include/linux/stdarg.h \
  /work/linux-cvm-kernel/include/linux/align.h \
  /work/linux-cvm-kernel/include/linux/const.h \
  /work/linux-cvm-kernel/include/vdso/const.h \
  /work/linux-cvm-kernel/include/uapi/linux/const.h \
  /work/linux-cvm-kernel/include/linux/array_size.h \
  /work/linux-cvm-kernel/include/linux/limits.h \
  /work/linux-cvm-kernel/include/uapi/linux/limits.h \
  /work/linux-cvm-kernel/include/vdso/limits.h \
  /work/linux-cvm-kernel/include/linux/linkage.h \
    $(wildcard include/config/ARCH_USE_SYM_ANNOTATIONS) \
  /work/linux-cvm-kernel/include/linux/stringify.h \
  /work/linux-cvm-kernel/include/linux/export.h \
    $(wildcard include/config/MODVERSIONS) \
  /work/linux-cvm-kernel/arch/riscv/include/asm/linkage.h \
  /work/linux-cvm-kernel/include/linux/container_of.h \
  /work/linux-cvm-kernel/include/linux/build_bug.h \
  /work/linux-cvm-kernel/include/linux/bitops.h \
  /work/linux-cvm-kernel/include/linux/bits.h \
  /work/linux-cvm-kernel/include/vdso/bits.h \
  /work/linux-cvm-kernel/include/uapi/linux/bits.h \
  /work/linux-cvm-kernel/include/linux/typecheck.h \
  /work/linux-cvm-kernel/include/uapi/linux/kernel.h \
  /work/linux-cvm-kernel/include/uapi/linux/sysinfo.h \
  /work/linux-cvm-kernel/include/asm-generic/bitops/generic-non-atomic.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/barrier.h \
    $(wildcard include/config/RISCV_ISA_ZAWRS) \
  /work/linux-cvm-kernel/arch/riscv/include/asm/cmpxchg.h \
  /work/linux-cvm-kernel/include/linux/bug.h \
    $(wildcard include/config/GENERIC_BUG) \
    $(wildcard include/config/BUG_ON_DATA_CORRUPTION) \
  /work/linux-cvm-kernel/arch/riscv/include/asm/bug.h \
    $(wildcard include/config/GENERIC_BUG_RELATIVE_POINTERS) \
    $(wildcard include/config/DEBUG_BUGVERBOSE) \
  /work/linux-cvm-kernel/arch/riscv/include/asm/asm.h \
    $(wildcard include/config/32BIT) \
    $(wildcard include/config/KPROBES) \
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
    $(wildcard include/config/PRINTK_INDEX) \
    $(wildcard include/config/DYNAMIC_DEBUG) \
    $(wildcard include/config/DYNAMIC_DEBUG_CORE) \
  /work/linux-cvm-kernel/include/linux/init.h \
    $(wildcard include/config/MEMORY_HOTPLUG) \
    $(wildcard include/config/HAVE_ARCH_PREL32_RELOCATIONS) \
  /work/linux-cvm-kernel/include/linux/kern_levels.h \
  /work/linux-cvm-kernel/include/linux/ratelimit_types.h \
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
  /work/linux-cvm-kernel/arch/riscv/include/asm/bitops.h \
    $(wildcard include/config/RISCV_ISA_ZBB) \
  /work/linux-cvm-kernel/include/linux/irqflags.h \
    $(wildcard include/config/TRACE_IRQFLAGS) \
    $(wildcard include/config/PREEMPT_RT) \
    $(wildcard include/config/IRQSOFF_TRACER) \
    $(wildcard include/config/PREEMPT_TRACER) \
    $(wildcard include/config/DEBUG_IRQFLAGS) \
    $(wildcard include/config/TRACE_IRQFLAGS_SUPPORT) \
  /work/linux-cvm-kernel/include/linux/irqflags_types.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/irqflags.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/csr.h \
  arch/riscv/include/generated/asm/percpu.h \
  /work/linux-cvm-kernel/include/asm-generic/percpu.h \
    $(wildcard include/config/DEBUG_PREEMPT) \
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
  /work/linux-cvm-kernel/include/linux/hex.h \
  /work/linux-cvm-kernel/include/linux/kstrtox.h \
  /work/linux-cvm-kernel/include/linux/log2.h \
    $(wildcard include/config/ARCH_HAS_ILOG2_U32) \
    $(wildcard include/config/ARCH_HAS_ILOG2_U64) \
  /work/linux-cvm-kernel/include/linux/math.h \
  arch/riscv/include/generated/asm/div64.h \
  /work/linux-cvm-kernel/include/asm-generic/div64.h \
  /work/linux-cvm-kernel/include/linux/minmax.h \
  /work/linux-cvm-kernel/include/linux/sprintf.h \
  /work/linux-cvm-kernel/include/linux/static_call_types.h \
    $(wildcard include/config/HAVE_STATIC_CALL) \
    $(wildcard include/config/HAVE_STATIC_CALL_INLINE) \
  /work/linux-cvm-kernel/include/linux/instruction_pointer.h \
  /work/linux-cvm-kernel/include/linux/wordpart.h \
  /work/linux-cvm-kernel/include/linux/bitmap.h \
  /work/linux-cvm-kernel/include/linux/errno.h \
  /work/linux-cvm-kernel/include/uapi/linux/errno.h \
  arch/riscv/include/generated/uapi/asm/errno.h \
  /work/linux-cvm-kernel/include/uapi/asm-generic/errno.h \
  /work/linux-cvm-kernel/include/uapi/asm-generic/errno-base.h \
  /work/linux-cvm-kernel/include/linux/find.h \
  /work/linux-cvm-kernel/include/linux/string.h \
    $(wildcard include/config/BINARY_PRINTF) \
    $(wildcard include/config/FORTIFY_SOURCE) \
  /work/linux-cvm-kernel/include/linux/args.h \
  /work/linux-cvm-kernel/include/linux/err.h \
  /work/linux-cvm-kernel/include/linux/overflow.h \
  /work/linux-cvm-kernel/include/uapi/linux/string.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/string.h \
    $(wildcard include/config/KASAN) \
  /work/linux-cvm-kernel/include/linux/bitmap-str.h \
  /work/linux-cvm-kernel/include/linux/cpumask_types.h \
  /work/linux-cvm-kernel/include/linux/atomic.h \
  /work/linux-cvm-kernel/arch/riscv/include/asm/atomic.h \
    $(wildcard include/config/GENERIC_ATOMIC64) \
  /work/linux-cvm-kernel/include/linux/atomic/atomic-arch-fallback.h \
  /work/linux-cvm-kernel/include/linux/atomic/atomic-long.h \
  /work/linux-cvm-kernel/include/linux/atomic/atomic-instrumented.h \
  /work/linux-cvm-kernel/include/linux/gfp_types.h \
    $(wildcard include/config/KASAN_HW_TAGS) \
    $(wildcard include/config/SLAB_OBJ_EXT) \
  /work/linux-cvm-kernel/include/linux/numa.h \
    $(wildcard include/config/NODES_SHIFT) \
    $(wildcard include/config/NUMA_KEEP_MEMINFO) \
    $(wildcard include/config/NUMA) \
    $(wildcard include/config/HAVE_ARCH_NODE_DEV_GROUP) \
  /work/linux-cvm-kernel/include/linux/jump_label.h \
    $(wildcard include/config/JUMP_LABEL) \
    $(wildcard include/config/HAVE_ARCH_JUMP_LABEL_RELATIVE) \

/work/zion-env/zion-enclave/driver/kernel/zion-enclave-sbi.o: $(deps_/work/zion-env/zion-enclave/driver/kernel/zion-enclave-sbi.o)

$(deps_/work/zion-env/zion-enclave/driver/kernel/zion-enclave-sbi.o):
