/* SPDX-License-Identifier: BSD-3-Clause */
.section .text
.global _start
_start:
    li a0, 0x4442434e
    li a6, 3
    li a7, 0x10
    ecall
    bnez a0, failed
    li t0, 1
    bne a1, t0, failed

    la s0, byte_message
byte_loop:
    lbu a0, 0(s0)
    beqz a0, byte_done
    li a6, 2
    li a7, 0x4442434e
    ecall
    bnez a0, failed
    bnez a1, failed
    addi s0, s0, 1
    j byte_loop
byte_done:
    la a1, bulk_message
    la t0, bulk_end
    sub a0, t0, a1
    mv s1, a0
    li a2, 0
    li a6, 0
    li a7, 0x4442434e
    ecall
    bnez a0, failed
    bne a1, s1, failed

    # Every malicious physical range must fail before M-mode dereferences it.
    li a1, PEER_BASE + 0x80000
    call reject_range
    li a1, 0x90000000
    call reject_range
    li a1, OWN_BASE + OWN_SIZE - 1
    call reject_range
    li a1, -4
    call reject_range
    la a1, bulk_message
    li a0, 8
    li a2, 1
    li a6, 0
    li a7, 0x4442434e
    ecall
    li t0, -3
    bne a0, t0, failed
    bnez a1, failed

    li a0, 0
    li a1, 0
    li a2, 0
    li a6, 0
    li a7, 0x4442434e
    ecall
    bnez a0, failed
    bnez a1, failed
    li a6, 3
    li a7, 0x4442434e
    ecall
    li t0, -2
    bne a0, t0, failed
    bnez a1, failed

    li a0, 8
    la a1, read_buffer
    li a2, 0
    li a6, 1
    li a7, 0x4442434e
    ecall
    bnez a0, failed
    bnez a1, failed
    la t0, read_buffer
    ld t0, 0(t0)
    li t1, 0x123456789abcdef
    bne t0, t1, failed

    la s0, done_message
    j legacy_print

reject_range:
    li a0, 8
    li a2, 0
    li a6, 0
    li a7, 0x4442434e
    ecall
    li t0, -3
    bne a0, t0, failed
    bnez a1, failed
    ret

failed:
    la s0, fail_message
legacy_print:
    lbu a0, 0(s0)
    beqz a0, stopped
    li a7, 1
    ecall
    addi s0, s0, 1
    j legacy_print
stopped:
    j stopped

.section .rodata
#if STATIC_GUEST_ID == 0
byte_message: .asciz "DBCN_VM0_BYTE_OK\n"
bulk_message: .ascii "DBCN_VM0_BULK_OK\n"
bulk_end:
done_message: .asciz "DBCN_VM0_BOUNDS_AND_API_PASS\n"
fail_message: .asciz "DBCN_VM0_FAIL\n"
#else
byte_message: .asciz "DBCN_VM1_BYTE_OK\n"
bulk_message: .ascii "DBCN_VM1_BULK_OK\n"
bulk_end:
done_message: .asciz "DBCN_VM1_BOUNDS_AND_API_PASS\n"
fail_message: .asciz "DBCN_VM1_FAIL\n"
#endif
.section .data
.balign 8
read_buffer: .quad 0x123456789abcdef
