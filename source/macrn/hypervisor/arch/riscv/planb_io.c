/* SPDX-License-Identifier: BSD-3-Clause */
/* Bounded virtio-mmio transport for one fixed, protected no-H guest. */
#ifdef CONFIG_PLAN_B_SERVICES
#include <types.h>
#include <errno.h>
#include <asm/lib/string.h>
#include <asm/guest/vm.h>
#include <asm/guest/vcpu.h>
#include <asm/guest/vplic.h>
#include <asm/static_vm.h>
#include <asm/planb_io.h>
#include <asm/planb_runtime.h>
#include <asm/planb_enclave.h>
#include <asm/planb_console.h>
#include <io_req.h>
#include <logmsg.h>
#include <libfdt.h>

#define QUEUE_MAX 128U
#define CHAIN_MAX 32U
#define DESC_NEXT 1U
#define DESC_WRITE 2U
#define DRIVER_OK 4U
#define FEATURES_OK 8U
#define NEEDS_RESET 64U
#define VIRTIO_NET_HEADER 12U
#define DISK_SECTORS (PLANB_DISK_BYTES / 512U)

struct descriptor {
	uint64_t address;
	uint32_t length;
	uint16_t flags, next;
};
struct queue {
	uint64_t descriptors, available, used;
	uint32_t size;
	uint16_t consumed, produced;
	bool ready;
};
struct device {
	struct queue queues[2];
	uint64_t features;
	uint32_t selected_queue, device_feature_select, driver_feature_select;
	uint32_t status, interrupt_status, irq;
	bool network;
};
struct pending {
	struct descriptor parts[CHAIN_MAX];
	unsigned int count;
	uint32_t readable, writable, operation, length;
	uint64_t sector, sequence;
	uint16_t head;
	struct queue *queue;
	struct device *device;
	struct planb_slot *shared;
	bool active;
};
static struct device devices[2];
static struct pending pending[3];
/* Lock order: VM MMIO lock -> this lock -> vPLIC lock. No blocking I/O here. */
static spinlock_t io_lock;
static struct acrn_vcpu *guest_vcpu;
static uint64_t next_sequence;
static uint32_t completed, warnings;
static uint32_t busy_doorbells;
static bool io_online = true;

static bool private_buffer(uint64_t address, uint64_t length)
{
	const struct static_vm_info *vm = &static_vms[1];
	return length && planb_enclave_parent_range(address, length) &&
		address >= vm->base && address - vm->base < vm->size &&
		length <= vm->size - (address - vm->base);
}

void planb_io_acquire(uint64_t *flags) { spin_lock_irqsave(&io_lock, flags); }
void planb_io_release(uint64_t flags) { spin_unlock_irqrestore(&io_lock, flags); }
static bool overlaps_range(uint64_t base, uint64_t length, uint64_t a, uint64_t n)
{
	return length && n && (a >= base ? a - base < length : base - a < n);
}
bool planb_io_busy_range(uint64_t address, uint64_t length)
{
	unsigned int i, j;
	for (i = 0; i < 2; i++) for (j = 0; j < 2; j++) {
		const struct queue *q = &devices[i].queues[j];
		if (!q->ready) continue;
		if (overlaps_range(q->descriptors, 16U * q->size, address, length) ||
		    overlaps_range(q->available, 6U + 2U * q->size, address, length) ||
		    overlaps_range(q->used, 6U + 8U * q->size, address, length)) return true;
	}
	for (i = 0; i < 3; i++) if (pending[i].active)
		for (j = 0; j < pending[i].count; j++)
			if (overlaps_range(pending[i].parts[j].address,
				pending[i].parts[j].length, address, length)) return true;
	return false;
}

bool planb_io_range(uint16_t vm_id, uint64_t address, uint64_t size)
{
 uint64_t base, length;
 if (vm_id == 1 && !planb_runtime_block_enabled())
  return size && address >= PLANB_NET_MMIO && address - PLANB_NET_MMIO < 0x1000U &&
   size <= 0x1000U - (address - PLANB_NET_MMIO);
 if (vm_id == 0 && size && address >= PLANB_CONSOLE_MMIO &&
     address - PLANB_CONSOLE_MMIO < PLANB_CONSOLE_BYTES &&
     size <= PLANB_CONSOLE_BYTES - (address - PLANB_CONSOLE_MMIO)) return true;
 base = vm_id ? PLANB_BLK_MMIO : PLANB_CTL_MMIO;
 length = vm_id ? 0x2000U : 0x1000U;
 return vm_id < 2 && size && address >= base && address - base < length &&
  size <= length - (address - base);
}

static void interrupt_device(struct device *device, uint32_t bits)
{
	device->interrupt_status |= bits;
	if (guest_vcpu) {
		vplic_accept_intr(guest_vcpu, device->irq, true);
		kick_vcpu(guest_vcpu);
	}
}

static void broken_queue(struct device *device)
{
	device->status |= NEEDS_RESET;
	interrupt_device(device, 2U);
}

static bool queue_valid(const struct queue *queue)
{
	return queue->size && queue->size <= QUEUE_MAX &&
		!(queue->size & (queue->size - 1U)) && !(queue->descriptors & 15U) &&
		!(queue->available & 1U) && !(queue->used & 3U) &&
		private_buffer(queue->descriptors, 16U * queue->size) &&
		private_buffer(queue->available, 6U + 2U * queue->size) &&
		private_buffer(queue->used, 6U + 8U * queue->size);
}

static bool snapshot_chain(struct pending *request)
{
	struct queue *queue = request->queue;
	uint64_t seen[2] = {0, 0};
	uint16_t index = request->head;
	bool writable_seen = false;
	request->count = request->readable = request->writable = 0;
	while (request->count < CHAIN_MAX) {
		struct descriptor *part = &request->parts[request->count++];
		if (index >= queue->size || (seen[index / 64U] & (1UL << (index % 64U))))
			return false;
		seen[index / 64U] |= 1UL << (index % 64U);
		memcpy(part, (void *)(queue->descriptors + 16U * index), sizeof(*part));
		/* Indirect descriptors are never advertised. Every chain is bounded. */
		if ((part->flags & ~(DESC_NEXT | DESC_WRITE)) ||
		    (part->length && !private_buffer(part->address, part->length))) return false;
		if (part->flags & DESC_WRITE) {
			writable_seen = true;
			if (part->length > PLANB_BLK_BYTES + 512U - request->writable) return false;
			request->writable += part->length;
		} else {
			if (writable_seen || part->length > PLANB_BLK_BYTES + 512U - request->readable)
				return false;
			request->readable += part->length;
		}
		if (!(part->flags & DESC_NEXT)) return true;
		index = part->next;
	}
	return false;
}

/* Descriptor addresses are private snapshots, and are checked again at use.
 * Device-written buffers and device-read buffers may be split arbitrarily.
 */
static bool transfer(struct pending *request, bool writable, uint32_t offset,
	void *buffer, uint32_t length, bool to_guest)
{
	unsigned int i;
	uint8_t *bytes = buffer;
	for (i = 0; i < request->count && length; i++) {
		const struct descriptor *part = &request->parts[i];
		uint32_t count;
		if (!!(part->flags & DESC_WRITE) != writable) continue;
		if (offset >= part->length) {
			offset -= part->length;
			continue;
		}
		count = part->length - offset;
		if (count > length) count = length;
		if (!private_buffer(part->address + offset, count)) return false;
		if (to_guest) memcpy((void *)(part->address + offset), bytes, count);
		else memcpy(bytes, (void *)(part->address + offset), count);
		bytes += count;
		length -= count;
		offset = 0;
	}
	return length == 0;
}

static void finish(struct pending *request, uint32_t written)
{
	struct queue *queue = request->queue;
	struct { uint32_t id, length; } element = {request->head, written};
	request->active = false;
	if (!queue_valid(queue)) {
		broken_queue(request->device);
		return;
	}
	memcpy((void *)(queue->used + 4U + 8U * (queue->produced % queue->size)),
		&element, sizeof(element));
	queue->produced++;
	__atomic_store_n((uint16_t *)(queue->used + 2U), queue->produced, __ATOMIC_RELEASE);
	__atomic_add_fetch(&completed, 1U, __ATOMIC_RELAXED);
	interrupt_device(request->device, 1U);
}

static void block_status(struct pending *request, uint8_t status, uint32_t written)
{
	if (!request->writable || !transfer(request, true, request->writable - 1U,
		&status, 1U, true)) {
		request->active = false;
		broken_queue(request->device);
		return;
	}
	finish(request, written + 1U);
}

static void publish(struct pending *request)
{
	struct planb_slot header;
	memset(&header, 0, sizeof(header));
	header.sequence = request->sequence = ++next_sequence;
	header.operation = request->operation;
	header.length = request->length;
	header.sector = request->sector;
	memcpy(request->shared, &header, sizeof(header));
	request->active = true;
	__atomic_store_n(&request->shared->state, PLANB_REQUEST, __ATOMIC_RELEASE);
}

static void submit_block(struct pending *request)
{
	struct { uint32_t type, reserved; uint64_t sector; } header;
	uint8_t *data = (uint8_t *)request->shared + PLANB_SLOT_HEADER;
	if (request->readable < sizeof(header) || !request->writable ||
	    !transfer(request, false, 0, &header, sizeof(header), false)) {
		broken_queue(request->device);
		return;
	}
	request->sector = header.sector;
	switch (header.type) {
	case 0: /* Read. */
		if (request->readable != sizeof(header)) break;
		request->operation = PLANB_BLK_READ;
		request->length = request->writable - 1U;
		goto data_request;
	case 1: /* Write. */
		if (request->writable != 1U) break;
		request->operation = PLANB_BLK_WRITE;
		request->length = request->readable - sizeof(header);
		goto data_request;
	case 4: /* Flush. */
		if (request->readable != sizeof(header) || request->writable != 1U) break;
		request->operation = PLANB_BLK_FLUSH;
		request->sector = request->length = 0;
		publish(request);
		return;
	case 8: { /* Device serial. */
		char serial[20] = "SG2044-PLANB-CVM1";
		if (request->readable != sizeof(header) || request->writable < sizeof(serial) + 1U)
			break;
		if (!transfer(request, true, 0, serial, sizeof(serial), true)) break;
		block_status(request, 0, sizeof(serial));
		return;
	}
	default:
		block_status(request, 2U, 0);
		return;
	}
	block_status(request, 1U, 0);
	return;
data_request:
	if (!request->length || request->length > PLANB_BLK_BYTES ||
	    (request->length & 511U) || request->sector >= DISK_SECTORS ||
	    request->length / 512U > DISK_SECTORS - request->sector) {
		block_status(request, 1U, 0);
		return;
	}
	if (request->operation == PLANB_BLK_WRITE && !transfer(request, false,
		sizeof(header), data, request->length, false)) {
		block_status(request, 1U, 0);
		return;
	}
	publish(request);
}

static void submit_network(struct pending *request, bool receive)
{
	uint8_t *data = (uint8_t *)request->shared + PLANB_SLOT_HEADER;
	request->sector = 0;
	if (receive) {
		if (request->readable || request->writable <= VIRTIO_NET_HEADER) goto invalid;
		request->operation = PLANB_NET_RX;
		request->length = request->writable - VIRTIO_NET_HEADER;
		if (request->length > PLANB_NET_BYTES) request->length = PLANB_NET_BYTES;
	} else {
		uint8_t header[VIRTIO_NET_HEADER];
		if (request->writable || request->readable <= sizeof(header) ||
		    request->readable - sizeof(header) > PLANB_NET_BYTES ||
		    !transfer(request, false, 0, header, sizeof(header), false) ||
		    header[0] || header[1]) goto invalid;
		request->operation = PLANB_NET_TX;
		request->length = request->readable - sizeof(header);
		if (!transfer(request, false, sizeof(header), data, request->length, false))
			goto invalid;
	}
	publish(request);
	return;
invalid:
	broken_queue(request->device);
}

static void process_queue(struct pending *request)
{
	struct queue *queue = request->queue;
	uint16_t available;
	unsigned int budget = QUEUE_MAX;
	if (!guest_vcpu || !(request->device->status & DRIVER_OK) ||
	    (request->device->status & NEEDS_RESET)) return;
	while (!request->active && queue->ready && budget--) {
		if (!queue_valid(queue)) goto invalid;
		available = __atomic_load_n((uint16_t *)(queue->available + 2U), __ATOMIC_ACQUIRE);
		if (available == queue->consumed) return;
		if ((uint16_t)(available - queue->consumed) > queue->size) goto invalid;
		memcpy(&request->head, (void *)(queue->available + 4U +
			2U * (queue->consumed % queue->size)), sizeof(request->head));
		queue->consumed++;
		if (!snapshot_chain(request)) goto invalid;
		if (request->device->network) submit_network(request, request == &pending[2]);
		else submit_block(request);
		if (request->device->status & NEEDS_RESET) return;
	}
	return;
invalid:
	broken_queue(request->device);
}

static void check_saved_registers(struct planb_slot *shared, const struct planb_slot *snapshot)
{
	unsigned int i;
	for (i = 0; i < 9U; i++) {
		if (!snapshot->saved_s2_s10[i]) continue;
		if (!warnings) {
			__atomic_store_n(&warnings, 1U, __ATOMIC_RELAXED);
			planb_runtime_tamper_detected();
			pr_warn("[ZION VCPU ALERT] hart=1 source=HOST-SHARED-CHANNEL register=S%u expected=0x0 observed=0x%lx",
				i + 2U, snapshot->saved_s2_s10[i]);
			pr_warn("[ZION VCPU ALERT] classification=SUSPECTED-TAMPER-OR-ABI-MISMATCH; private saved state is retained");
		}
		shared->saved_s2_s10[i] = 0;
	}
}

static void consume_response(struct pending *request)
{
	struct planb_slot response;
	uint8_t *data = (uint8_t *)request->shared + PLANB_SLOT_HEADER;
	bool valid;
	if (!io_online || !request->active || __atomic_load_n(&request->shared->state,
		__ATOMIC_ACQUIRE) != PLANB_RESPONSE) return;
	memcpy(&response, request->shared, sizeof(response));
	check_saved_registers(request->shared, &response);
	valid = response.sequence == request->sequence &&
		response.response_sequence == request->sequence &&
		response.operation == request->operation && response.length == request->length &&
		response.sector == request->sector && response.response_length <= request->length &&
		response.reserved == 0 && response.reserved_tail == 0 && response.status == 0;
	__atomic_store_n(&request->shared->state, PLANB_FREE, __ATOMIC_RELEASE);
	if (!request->device->network) {
		if (request->operation == PLANB_BLK_READ || request->operation == PLANB_BLK_WRITE)
			valid = valid && response.response_length == request->length;
		if (valid && request->operation == PLANB_BLK_READ)
			valid = transfer(request, true, 0, data, request->length, true);
		block_status(request, valid ? 0U : 1U,
			valid && request->operation == PLANB_BLK_READ ? request->length : 0U);
	} else if (request->operation == PLANB_NET_RX) {
		uint8_t header[VIRTIO_NET_HEADER] = {0};
		header[10] = 1; /* A modern non-mergeable receive uses one buffer. */
		valid = valid && response.response_length >= 14U &&
			transfer(request, true, 0, header, sizeof(header), true) &&
			transfer(request, true, sizeof(header), data, response.response_length, true);
		finish(request, valid ? sizeof(header) + response.response_length : 0U);
	} else {
		finish(request, 0);
	}
	process_queue(request);
}

static uint64_t supported_features(const struct device *device)
{
	/* VERSION_1; no indirect, packed-ring, checksum or segmentation offload. */
	return (1ULL << 32) | (device->network ? (1U << 5) | (1U << 16) :
		(1U << 1) | (1U << 2) | (1U << 6) | (1U << 9));
}

static void reset_device(struct device *device)
{
	unsigned int i;
	for (i = 0; i < 3U; i++) if (pending[i].device == device) {
		pending[i].active = false;
		__atomic_store_n(&pending[i].shared->state, PLANB_FREE, __ATOMIC_RELEASE);
	}
	memset(device->queues, 0, sizeof(device->queues));
	device->features = 0;
	device->selected_queue = device->device_feature_select = device->driver_feature_select = 0;
	device->status = device->interrupt_status = 0;
	if (guest_vcpu) vplic_accept_intr(guest_vcpu, device->irq, false);
}

void planb_io_quiesce(void)
{
 uint64_t flags;
 unsigned int i;
 spin_lock_irqsave(&io_lock, &flags);
 io_online = false;
 for (i = 0; i < 2; i++) reset_device(&devices[i]);
 for (i = 0; i < 3; i++) {
  /* Preserve binding pointers; remove every old private descriptor. */
  memset(&pending[i], 0, __builtin_offsetof(struct pending, queue));
  pending[i].active = false;
 }
 memset((void *)PLANB_SLOT_BLK, 0, PLANB_SLOT_HEADER + PLANB_BLK_BYTES);
 memset((void *)PLANB_SLOT_TX, 0, PLANB_SLOT_HEADER + PLANB_NET_BYTES);
 memset((void *)PLANB_SLOT_RX, 0, PLANB_SLOT_HEADER + PLANB_NET_BYTES);
 /* next_sequence, completed and warnings belong to this Host boot. */
 spin_unlock_irqrestore(&io_lock, flags);
}
void planb_io_online(void)
{
 uint64_t flags;
 spin_lock_irqsave(&io_lock, &flags);
 io_online = true;
 spin_unlock_irqrestore(&io_lock, flags);
}

static uint32_t read_register(struct device *device, uint32_t offset)
{
	struct queue *queue = device->selected_queue < (device->network ? 2U : 1U) ?
		&device->queues[device->selected_queue] : NULL;
	switch (offset) {
	case 0x000: return 0x74726976U;
	case 0x004: return 2U;
	case 0x008: return device->network ? 1U : 2U;
	case 0x00c: return 0x4d414352U;
	case 0x010: return device->device_feature_select < 2U ?
		(uint32_t)(supported_features(device) >> (32U * device->device_feature_select)) : 0;
	case 0x034: return queue ? QUEUE_MAX : 0;
	case 0x044: return queue && queue->ready;
	case 0x060: return device->interrupt_status;
	case 0x070: return device->status;
	case 0x0fc: return 0;
	default: break;
	}
	if (device->network) {
		/* Locally administered unicast MAC, and link-up status. */
		if (offset == 0x100) return 0xff804402U;
		if (offset == 0x104) return 0x00010100U;
	} else {
		if (offset == 0x100) return (uint32_t)DISK_SECTORS;
		if (offset == 0x104) return (uint32_t)(DISK_SECTORS >> 32);
		if (offset == 0x108) return 4096U;
		if (offset == 0x10c) return 8U;
		if (offset == 0x114) return 512U;
	}
	return 0;
}

static void set_address_half(uint64_t *address, uint32_t value, bool high)
{
	if (high) *address = (*address & 0xffffffffULL) | ((uint64_t)value << 32);
	else *address = (*address & 0xffffffff00000000ULL) | value;
}

static void write_register(struct device *device, uint32_t offset, uint32_t value)
{
	struct queue *queue = device->selected_queue < (device->network ? 2U : 1U) ?
		&device->queues[device->selected_queue] : NULL;
	unsigned int i;
	switch (offset) {
	case 0x014: device->device_feature_select = value; return;
	case 0x024: device->driver_feature_select = value; return;
	case 0x020:
		if (device->driver_feature_select < 2U && !(device->status & FEATURES_OK))
			set_address_half(&device->features, value, device->driver_feature_select != 0);
		return;
	case 0x030: device->selected_queue = value; return;
	case 0x064:
		device->interrupt_status &= ~value;
		if (!device->interrupt_status && guest_vcpu)
			vplic_accept_intr(guest_vcpu, device->irq, false);
		return;
	case 0x070:
		if (!value) { reset_device(device); return; }
		device->status = value;
		if ((value & FEATURES_OK) && (!(device->features & (1ULL << 32)) ||
			(device->features & ~supported_features(device)))) device->status &= ~FEATURES_OK;
		if ((value & DRIVER_OK) && !(device->status & FEATURES_OK)) broken_queue(device);
		for (i = 0; i < 3U; i++) if (pending[i].device == device) process_queue(&pending[i]);
		return;
	case 0x050:
		for (i = 0; i < 3U; i++) if (pending[i].device == device &&
			pending[i].queue == &device->queues[value < 2U ? value : 0U] && value < 2U)
			process_queue(&pending[i]);
		return;
	default: break;
	}
	if (!queue || queue->ready) return;
	switch (offset) {
	case 0x038: queue->size = value; break;
	case 0x044:
		if (value == 1U) {
			queue->ready = queue_valid(queue);
			if (!queue->ready) broken_queue(device);
		}
		break;
	case 0x080: case 0x084: set_address_half(&queue->descriptors, value, offset & 4U); break;
	case 0x090: case 0x094: set_address_half(&queue->available, value, offset & 4U); break;
	case 0x0a0: case 0x0a4: set_address_half(&queue->used, value, offset & 4U); break;
	default: break;
	}
}

static int32_t virtio_access(struct io_request *request, void *context)
{
	struct device *device = context;
	struct acrn_mmio_request *mmio = &request->reqs.mmio_request;
	uint64_t flags;
	uint64_t base = device->network ? PLANB_NET_MMIO : PLANB_BLK_MMIO;
	uint32_t offset = (uint32_t)(mmio->address - base);
	if (mmio->direction == ACRN_IOREQ_DIR_READ && mmio->size == 8U &&
	    offset >= 0x100U && !(offset & 7U)) {
		spin_lock_irqsave(&io_lock, &flags);
		mmio->value = read_register(device, offset) |
			((uint64_t)read_register(device, offset + 4U) << 32);
		spin_unlock_irqrestore(&io_lock, flags);
		return 0;
	}
	/* Configuration bytes may be read with smaller accesses. */
	if ((mmio->size != 4U || (offset & 3U)) &&
	    !(mmio->direction == ACRN_IOREQ_DIR_READ && offset >= 0x100U &&
	      mmio->size <= 4U && mmio->size && (offset & 3U) + mmio->size <= 4U)) return -EINVAL;
	spin_lock_irqsave(&io_lock, &flags);
	if (mmio->direction == ACRN_IOREQ_DIR_WRITE) write_register(device, offset, mmio->value);
	else mmio->value = (read_register(device, offset & ~3U) >> (8U * (offset & 3U))) &
		(UINT64_MAX >> (64U - 8U * mmio->size));
	spin_unlock_irqrestore(&io_lock, flags);
	return 0;
}

static int32_t control_access(struct io_request *request, __unused void *context)
{
	struct acrn_mmio_request *mmio = &request->reqs.mmio_request;
	uint32_t offset = (uint32_t)(mmio->address - PLANB_CTL_MMIO);
	uint64_t flags, ticket;
	unsigned int i;
	if (mmio->size != 4U || (offset & 3U)) return -EINVAL;
	/* Status and immutable console metadata must remain readable even if
	 * VM1 is delayed while owning the queue lock. VM0 has only one Hart.
	 */
	if (mmio->direction == ACRN_IOREQ_DIR_READ) {
	 switch (offset) {
	case 0: mmio->value = (uint32_t)PLANB_MAGIC; break;
	case 4: mmio->value = PLANB_MAGIC >> 32; break;
	case 12: mmio->value = PLANB_ABI; break;
	case 16: mmio->value = (uint32_t)PLANB_SLOT_BLK; break;
	case 20: mmio->value = PLANB_SLOT_BLK >> 32; break;
	case 24: mmio->value = __atomic_load_n(&warnings, __ATOMIC_RELAXED); break;
	case 28: mmio->value = __atomic_load_n(&completed, __ATOMIC_RELAXED); break;
	case 32: mmio->value = PLANB_FEATURE_MAGIC; break;
	case 36: mmio->value = PLANB_FEATURE_CONSOLE | PLANB_FEATURE_NONBLOCK_IO; break;
	case 40: mmio->value = planb_console_size(); break;
	case 44: mmio->value = planb_console_dropped(); break;
	case 48: mmio->value = planb_console_generation(); break;
	case 52: mmio->value = __atomic_load_n(&busy_doorbells, __ATOMIC_RELAXED); break;
	case 56: mmio->value = PLANB_CONSOLE_BYTES; break;
	default: mmio->value = 0; break;
	 }
	 return 0;
	}
	if (offset != 8U) return -EINVAL;
	/* A delayed Guest must not make the management Hart spin forever.
	 * Failed attempts do not enqueue a ticket or consume a response. The
	 * matching backend retries the doorbell on its normal bounded poll.
	 */
	local_irq_save(&flags);
	ticket = __atomic_load_n(&io_lock.tail, __ATOMIC_ACQUIRE);
	if (!__atomic_compare_exchange_n(&io_lock.head, &ticket, ticket + 1U,
		false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
		__atomic_add_fetch(&busy_doorbells, 1U, __ATOMIC_RELAXED);
		local_irq_restore(flags);
		return 0;
	}
	if (io_online) for (i = 0; i < 3U; i++) consume_response(&pending[i]);
	spin_unlock_irqrestore(&io_lock, flags);
	return 0;
}

static int32_t console_access(struct io_request *request, __unused void *context)
{
	struct acrn_mmio_request *mmio = &request->reqs.mmio_request;
	uint64_t offset = mmio->address - PLANB_CONSOLE_MMIO;
	if (mmio->direction != ACRN_IOREQ_DIR_READ || mmio->size != 4U ||
	    (offset & 3U) || offset >= PLANB_CONSOLE_BYTES) return -EINVAL;
	mmio->value = planb_console_word((uint32_t)offset);
	return 0;
}

void planb_io_init(struct acrn_vm *vm)
{
	if (vm->vm_id == 0) {
		spinlock_init(&io_lock);
		memset((void *)PLANB_SLOT_BLK, 0, PLANB_SLOT_HEADER + PLANB_BLK_BYTES);
		memset((void *)PLANB_SLOT_TX, 0, PLANB_SLOT_HEADER + PLANB_NET_BYTES);
		memset((void *)PLANB_SLOT_RX, 0, PLANB_SLOT_HEADER + PLANB_NET_BYTES);
		devices[0].irq = PLANB_BLK_IRQ;
		devices[1].irq = PLANB_NET_IRQ;
		devices[1].network = true;
		pending[0].device = &devices[0]; pending[0].queue = &devices[0].queues[0];
		pending[0].shared = (void *)PLANB_SLOT_BLK;
		pending[1].device = &devices[1]; pending[1].queue = &devices[1].queues[1];
		pending[1].shared = (void *)PLANB_SLOT_TX;
		pending[2].device = &devices[1]; pending[2].queue = &devices[1].queues[0];
		pending[2].shared = (void *)PLANB_SLOT_RX;
		register_mmio_emulation_handler(vm, control_access, PLANB_CTL_MMIO,
			PLANB_CTL_MMIO + 0x1000U, NULL, false);
		register_mmio_emulation_handler(vm, console_access, PLANB_CONSOLE_MMIO,
			PLANB_CONSOLE_MMIO + PLANB_CONSOLE_BYTES, NULL, false);
	} else if (vm->vm_id == 1) {
		guest_vcpu = &vm->hw.vcpu[0];
		register_mmio_emulation_handler(vm, virtio_access, PLANB_BLK_MMIO,
			PLANB_BLK_MMIO + 0x1000U, &devices[0], false);
		register_mmio_emulation_handler(vm, virtio_access, PLANB_NET_MMIO,
			PLANB_NET_MMIO + 0x1000U, &devices[1], false);
	}
}

static int add_device_fdt(void *dtb, const char *name, uint64_t base, uint32_t irq,
	uint32_t plic)
{
	int parent = fdt_path_offset(dtb, "/soc"), node, ret;
	fdt64_t region[2] = {cpu_to_fdt64(base), cpu_to_fdt64(0x1000U)};
	if (parent < 0) return parent;
	node = fdt_add_subnode(dtb, parent, name);
	if (node < 0) return node;
	ret = fdt_setprop_string(dtb, node, "compatible", "virtio,mmio");
	if (ret) return ret;
	ret = fdt_setprop(dtb, node, "reg", region, sizeof(region));
	if (ret) return ret;
	ret = fdt_setprop_u32(dtb, node, "interrupt-parent", plic);
	if (ret) return ret;
	ret = fdt_setprop_u32(dtb, node, "interrupts", irq);
	if (ret) return ret;
	return fdt_setprop(dtb, node, "dma-coherent", NULL, 0);
}

int planb_io_add_fdt(void *dtb)
{
	int node = -1, depth = 0, ret;
	uint32_t phandle = 0;
	while ((node = fdt_next_node(dtb, node, &depth)) >= 0) {
		if (!fdt_node_check_compatible(dtb, node, "riscv,plic0") ||
		    !fdt_node_check_compatible(dtb, node, "riscv,plic1") ||
		    !fdt_node_check_compatible(dtb, node, "thead,c900-plic")) {
			phandle = fdt_get_phandle(dtb, node);
			break;
		}
	}
	if (!phandle) return -EINVAL;
	if (planb_runtime_block_enabled()) {
		ret = add_device_fdt(dtb, "virtio_mmio@93000000", PLANB_BLK_MMIO, PLANB_BLK_IRQ, phandle);
		if (ret) return ret;
	}
	return add_device_fdt(dtb, "virtio_mmio@93001000", PLANB_NET_MMIO, PLANB_NET_IRQ, phandle);
}
#endif
