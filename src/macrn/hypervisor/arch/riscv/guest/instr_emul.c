/*
 * Copyright (C) 2023-2024 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Haicheng Li <haicheng.li@intel.com>
 */

#include <errno.h>
#include <asm/guest/instr_emul.h>
#include <asm/guest/vcpu.h>

#define INS16_OPCODE_MASK	0xC000
#define INS16_OPRD_MASK		0x1C
#define INS16_OPRS2_MASK	0x1C

#define INS16_OPCODE_LQ		0x1
#define INS16_OPCODE_LW		0x2
#define INS16_OPCODE_LD		0x3
#define INS16_OPCODE_SQ		0x5
#define INS16_OPCODE_SW		0x6
#define INS16_OPCODE_SD		0x7

#define INS32_OPCODE_MASK	0x7F
#define INS32_OPRD_MASK		0xF80
#define INS32_OPRS1_MASK	0xF8000
#define INS32_OPRS2_MASK	0x1F00000
#define INS32_OPSIZE_MASK	0x7000

#define INS32_OPCODE_LD		0x3
#define INS32_OPCODE_ST		0x23
#define INS32_OPSIZE_BYTE	0x0
#define INS32_OPSIZE_HALF	0x1
#define INS32_OPSIZE_WORD	0x2
#define INS32_OPSIZE_DWORD	0x3
#define INS32_OPSIZE_UBYTE	0x4
#define INS32_OPSIZE_UHALF	0x5
#define INS32_OPSIZE_UWORD	0x7

#define BIT64_MASK		0xffffffffffffffff
#define BIT32_MASK		0xffffffff
#define BIT32_U_MASK		0xffffffff00000000
#define BIT16_MASK		0xffff
#define BIT16_U_MASK		0xffff0000
#define BIT8_MASK		0xff
#define BIT8_U_MASK		0xff00

#ifdef CONFIG_MACRN
static inline uint64_t get_mask(uint32_t size)
{
	uint64_t mask;

	switch (size) {
	case 1:
		mask = BIT8_MASK;
		break;
	case 2:
		mask = BIT16_MASK;
		break;
	case 4:
		mask = BIT32_MASK;
		break;
	case 8:
	default:
		mask = BIT64_MASK;
		break;
	}

	return mask;
}

static int32_t emulate_ins16(struct acrn_vcpu *vcpu, uint32_t ins, uint32_t size)
{
	struct acrn_mmio_request *mmio_req = &vcpu->req.reqs.mmio_request;
	uint64_t pc, value;
	uint32_t opcode = ins >> 13;
	bool store = opcode == INS16_OPCODE_SW || opcode == INS16_OPCODE_SD;
	uint32_t quadrant = ins & 3U;
	int32_t reg;
	uint64_t mask = get_mask(size);
	if ((quadrant != 0U && quadrant != 2U) ||
	    (opcode != INS16_OPCODE_LW && opcode != INS16_OPCODE_LD &&
	     opcode != INS16_OPCODE_SW && opcode != INS16_OPCODE_SD)) return -EFAULT;
	if (quadrant == 2U) reg = store ? (ins >> 2) & 31U : (ins >> 7) & 31U;
	else reg = ((ins >> 2) & 7U) + 8U;
	if (!store && !reg) return -EFAULT; /* Reserved C.L[WD]SP encoding. */
	pc = vcpu_get_gpreg(vcpu, CPU_REG_IP);
	vcpu_set_gpreg(vcpu, CPU_REG_IP, pc + 2);
	if (!store) {
		value = mmio_req->value & mask;
		if (size == 4U) value = (int64_t)(int32_t)value;
		vcpu_set_gpreg(vcpu, reg, value);
	} else {
		mmio_req->value = (reg ? vcpu_get_gpreg(vcpu, reg) : 0) & mask;
	}
	return 0;
}

int32_t emulate_ins32(struct acrn_vcpu *vcpu, uint32_t ins, uint32_t size)
{
	struct acrn_mmio_request *mmio_req = &vcpu->req.reqs.mmio_request;
	uint64_t pc;
	int32_t reg, rc = 0;
	uint64_t mask = get_mask(size), value;

	pc = vcpu_get_gpreg(vcpu, CPU_REG_IP);
	vcpu_set_gpreg(vcpu, CPU_REG_IP, pc + 4);

	if ((INS32_OPCODE_MASK & ins) == 0x3) {
		reg = (ins & INS32_OPRD_MASK) >> 7;
		value = mmio_req->value & mask;
		if (((ins >> 12) & 7U) < 3U)
			value = (int64_t)(value << (64U - 8U * size)) >> (64U - 8U * size);
		if (reg) vcpu_set_gpreg(vcpu, reg, value);
	} else if ((INS32_OPCODE_MASK & ins) == 0x23) {
		reg = (ins & INS32_OPRS2_MASK) >> 20;
		mmio_req->value = (reg ? vcpu_get_gpreg(vcpu, reg) : 0) & mask;
	} else {
		rc = -EFAULT;
	}

	return rc;
}

int32_t emulate_instruction(struct acrn_vcpu *vcpu)
{
	uint32_t ins, xlen, size;
	struct run_context *ctx;

	ctx = &vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;
	ins = get_instruction(ctx, &xlen);
	size = vcpu->req.reqs.mmio_request.size;

	if (xlen == 32)
		return emulate_ins32(vcpu, ins, size);
	else
		return emulate_ins16(vcpu, ins, size);
}

static int32_t decode_ins16(struct acrn_vcpu *vcpu, uint32_t ins)
{
	uint32_t opcode = ins >> 13;
	uint32_t quadrant = ins & 3U;
	if (quadrant != 0U && quadrant != 2U) return -EFAULT;
	if (quadrant == 2U && (opcode == INS16_OPCODE_LW || opcode == INS16_OPCODE_LD) &&
	    !((ins >> 7) & 31U)) return -EFAULT;
	switch (opcode) {
	case INS16_OPCODE_LW:
	case INS16_OPCODE_SW:
		return 4;
	case INS16_OPCODE_LD:
	case INS16_OPCODE_SD:
		return 8;
	default:
		return -EFAULT;
	}
}

static int32_t decode_ins32(struct acrn_vcpu *vcpu, uint32_t ins)
{
	uint32_t opcode = ins & 0x7fU, width = (ins >> 12) & 7U;
	if (opcode == 3U) {
		if (width > 6U) return -EFAULT;
		return 1U << (width & 3U);
	}
	if (opcode == 0x23U && width <= 3U) return 1U << width;
	return -EFAULT;
}

int32_t decode_instruction(struct acrn_vcpu *vcpu, uint32_t ins, uint32_t xlen)
{
	if (xlen == 32)
		return decode_ins32(vcpu, ins);
	else
		return decode_ins16(vcpu, ins);
}

#else /* !CONFIG_MACRN */

int32_t emulate_pseudo(struct acrn_vcpu *vcpu, uint32_t ins, uint32_t size)
{
	int32_t rc = 0;

	return rc;
}

static inline uint64_t get_mask(uint32_t ins)
{
	uint64_t mask;

	int size = (ins & INS32_OPSIZE_MASK) >> 12;

	switch (size) {
	case INS32_OPSIZE_BYTE:
	case INS32_OPSIZE_UBYTE:
		mask = BIT8_MASK;
		break;
	case INS32_OPSIZE_HALF:
	case INS32_OPSIZE_UHALF:
		mask = BIT16_MASK;
		break;
	case INS32_OPSIZE_WORD:
	case INS32_OPSIZE_UWORD:
		mask = BIT32_MASK;
		break;
	case INS32_OPSIZE_DWORD:
	default:
		mask = BIT64_MASK;
		break;
	}

	return mask;
}

int32_t emulate_ins32(struct acrn_vcpu *vcpu, uint32_t ins, uint32_t size)
{
	struct acrn_mmio_request *mmio_req = &vcpu->req.reqs.mmio_request;
	uint64_t pc;
	uint32_t reg, op;
	int32_t rc = 0;
	int64_t mask = get_mask(ins);
	int32_t xlen = (ins & 0x3) == 0x1 ? 2: 4;

	pc = vcpu_get_gpreg(vcpu, CPU_REG_IP);
	vcpu_set_gpreg(vcpu, CPU_REG_IP, pc + xlen);

	op = INS32_OPCODE_MASK & ins;
	if (op == 0x3 || op == 0x1) {
		reg = (ins & INS32_OPRD_MASK) >> 7;
		vcpu_set_gpreg(vcpu, reg, (mmio_req->value) & mask);
	} else if (op == 0x23 || op == 0x21) {
		reg = (ins & INS32_OPRS2_MASK) >> 20;
		mmio_req->value = (vcpu_get_gpreg(vcpu, reg) & mask);
	} else {
		rc = -EFAULT;
	}

	return rc;
}

int32_t emulate_instruction(struct acrn_vcpu *vcpu)
{
	uint32_t ins, xlen, size, ret = 0;
	struct run_context *ctx;

	ctx = &vcpu->arch.contexts[vcpu->arch.cur_context].run_ctx;
	ins = get_instruction(ctx, &xlen);
	size = vcpu->req.reqs.mmio_request.size;

	xlen = ins & 0x3;
	switch (xlen) {
		case 0x0:
			ret = emulate_pseudo(vcpu, ins, size);
			break;
		case 0x1:
		case 0x3:
			ret = emulate_ins32(vcpu, ins, size);
			break;
		default:
			break;
	}

	return ret;
}

static int32_t decode_pseudo(struct acrn_vcpu *vcpu, uint32_t ins)
{
	int size = (ins & INS32_OPSIZE_MASK) >> 12;

	switch (size) {
	case INS32_OPSIZE_WORD:
		size = 4;
		break;
	case INS32_OPSIZE_DWORD:
	default:
		size = 8;
		break;
	}

	return size;
}

static int32_t decode_ins32(struct acrn_vcpu *vcpu, uint32_t ins)
{
	int size = (ins & INS32_OPSIZE_MASK) >> 12;

	switch (size) {
	case INS32_OPSIZE_BYTE:
	case INS32_OPSIZE_UBYTE:
		size = 1;
		break;
	case INS32_OPSIZE_HALF:
	case INS32_OPSIZE_UHALF:
		size = 2;
		break;
	case INS32_OPSIZE_WORD:
	case INS32_OPSIZE_UWORD:
		size = 4;
		break;
	case INS32_OPSIZE_DWORD:
	default:
		size = 8;
		break;
	}

	return size;
}

int32_t decode_instruction(struct acrn_vcpu *vcpu, uint32_t ins, uint32_t xlen)
{
	int32_t ret = -1;

	xlen = ins & 0x3;
	switch (xlen) {
		case 0x0:
			ret = decode_pseudo(vcpu, ins);
			break;
		case 0x1:
		case 0x3:
			ret = decode_ins32(vcpu, ins);
			break;
		default:
			break;
	}

	return ret;
}
#endif
