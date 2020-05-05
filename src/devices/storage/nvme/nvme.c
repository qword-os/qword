#include <devices/storage/nvme/nvme.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/pci.h>
#include "nvme_private.h"
#include <lib/cmem.h>
#include "nvme_private.h"
#include <lib/klib.h>
#include <devices/dev.h>
#include <lib/bit.h>
#include <mm/mm.h>
#include <lib/part.h>
#include <sys/panic.h>

#define CACHE_NOT_READY 0
#define CACHE_READY 1
#define CACHE_DIRTY 2
#define MAX_CACHED_BLOCKS 512

struct nvme_queue {
    volatile struct nvme_command *submit;
    volatile struct nvme_completion *completion;
    volatile uint32_t *submit_db;
    volatile uint32_t *complete_db;
    uint16_t queue_elements;
    uint16_t cq_vector;
    uint16_t sq_head;
    uint16_t sq_tail;
    uint16_t cq_head;
    uint8_t  cq_phase;
    uint16_t qid;
    uint32_t command_id;

    uint64_t *prps;
};

typedef struct {
    uint8_t *cache;
    uint64_t block;
    uint64_t end_block;
    int status;
} cached_block_t;

typedef struct {
    volatile struct nvme_bar *nvme_base;
    size_t doorbell_stride;
    size_t queue_slots;
    size_t lba_size;
    cached_block_t *cache;
    struct nvme_queue queues[2];
    int max_prps;
    lock_t nvme_lock;
    size_t overwritten_slot;
    size_t num_lbas;
    size_t cache_block_size;
    size_t max_transfer_shift;
} nvme_device_t;

nvme_device_t *nvme_devices;

void nvme_initialize_queue(int device, struct nvme_queue *queue, size_t queue_slots, size_t qid) {
    queue->submit = kalloc(sizeof(struct nvme_command) * queue_slots);
    queue->completion = kalloc(sizeof(struct nvme_completion) * queue_slots);
    queue->submit_db = (uint32_t*)((size_t)nvme_devices[device].nvme_base + PAGE_SIZE + (2 * qid * (4 << nvme_devices[device].doorbell_stride)));
    queue->complete_db = (uint32_t*)((size_t)nvme_devices[device].nvme_base + PAGE_SIZE + ((2 * qid + 1) * (4 << nvme_devices[device].doorbell_stride)));
    queue->queue_elements = queue_slots;
    queue->cq_vector = 0;
    queue->sq_head = 0;
    queue->sq_tail = 0;
    queue->cq_head = 0;
    queue->cq_phase = 1;
    queue->qid = qid;
    queue->command_id = 0;

    queue->prps = kalloc(nvme_devices[device].max_prps * queue_slots * sizeof(uint64_t));
}

void nvme_submit_cmd(struct nvme_queue *queue, struct nvme_command command) {
    uint16_t tail = queue->sq_tail;
    queue->submit[tail] = command;
    tail++;
    if (tail == queue->queue_elements)
        tail = 0;
    *(queue->submit_db) = tail;
    queue->sq_tail = tail;
}

uint16_t nvme_submit_cmd_wait(struct nvme_queue *queue, struct nvme_command command) {
    uint16_t head = queue->cq_head;
    uint16_t phase = queue->cq_phase;
    command.common.command_id = queue->command_id++;
    nvme_submit_cmd(queue, command);
    uint16_t status = 0;

    while(1) {
        status = queue->completion[queue->cq_head].status;
        if ((status & 0x1) == phase) {
            break;
        }
    }

    status >>= 1;
    if (status) {
        kprint(KPRN_ERR, "nvme: command error %X", status);
        return status;
    }

    head++;
    if (head == queue->queue_elements) {
        head = 0;
        queue->cq_phase = !(queue->cq_phase);
    }
    *(queue->complete_db) = head;
    queue->cq_head = head;

    return status;
}

int nvme_identify(int device, struct nvme_id_ctrl *id) {
    int length = sizeof(struct nvme_id_ctrl);
    struct nvme_command command = {0};
    command.identify.opcode = nvme_admin_identify;
    command.identify.nsid = 0;
    command.identify.cns = 1;
    command.identify.prp1 = (size_t)id - MEM_PHYS_OFFSET;
    int offset = (size_t)id & (PAGE_SIZE - 1);
    length -= (PAGE_SIZE - offset);
    if (length <= 0) {
        command.identify.prp2 = (size_t)0;
    } else {
        size_t addr = (size_t)id + (PAGE_SIZE - offset);
        command.identify.prp2 = (size_t)addr;
    }

    uint16_t status = nvme_submit_cmd_wait(&nvme_devices[device].queues[0], command);
    if (status != 0) {
        return -1;
    }

    int shift = 12 + NVME_CAP_MPSMIN(nvme_devices[device].nvme_base->cap);
    size_t max_transf_shift;
    if(id->mdts) {
        max_transf_shift = shift + id->mdts;
    } else {
        max_transf_shift = 20;
    }
    nvme_devices[device].max_transfer_shift = max_transf_shift;
    return 0;
}

int nvme_get_ns_info(int device, int ns_num, struct nvme_id_ns *id_ns) {
    struct nvme_command command = {0};
    command.identify.opcode = nvme_admin_identify;
    command.identify.nsid = ns_num;
    command.identify.cns = 0;
    command.identify.prp1 = (size_t)id_ns - MEM_PHYS_OFFSET;

    uint16_t status = nvme_submit_cmd_wait(&nvme_devices[device].queues[0], command);
    if (status != 0) {
        return -1;
    }

    return 0;
}


int nvme_set_queue_count(int device, int count) {
    struct nvme_command command = {0};
    command.features.opcode = nvme_admin_set_features;
    command.features.prp1 = 0;
    command.features.fid = NVME_FEAT_NUM_QUEUES;
    command.features.dword11 = (count - 1) | ((count - 1) << 16);
    uint16_t status = nvme_submit_cmd_wait(&nvme_devices[device].queues[0], command);
    if (status != 0) {
        return -1;
    }
    return 0;
}

int nvme_create_queue_pair(int device, uint16_t qid) {
    nvme_set_queue_count(device, 4);
    nvme_initialize_queue(device, &nvme_devices[device].queues[1], nvme_devices[device].queue_slots, 1);
    struct nvme_command cq_command = {0};
    cq_command.create_cq.opcode = nvme_admin_create_cq;
    cq_command.create_cq.prp1 = ((size_t)((nvme_devices[device].queues[1].completion))) - MEM_PHYS_OFFSET;
    cq_command.create_cq.cqid = qid;
    cq_command.create_cq.qsize = nvme_devices[device].queue_slots - 1;
    cq_command.create_cq.cq_flags = NVME_QUEUE_PHYS_CONTIG;
    cq_command.create_cq.irq_vector = 0;
    uint16_t status = nvme_submit_cmd_wait(&nvme_devices[device].queues[0], cq_command);
    if (status != 0) {
        return -1;
    }

    struct nvme_command sq_command = {0};
    sq_command.create_sq.opcode = nvme_admin_create_sq;
    sq_command.create_sq.prp1 = ((size_t)((nvme_devices[device].queues[1].submit)) - MEM_PHYS_OFFSET);
    sq_command.create_sq.sqid = qid;
    sq_command.create_sq.cqid = qid;
    sq_command.create_sq.qsize = nvme_devices[device].queue_slots - 1;
    sq_command.create_sq.sq_flags = NVME_QUEUE_PHYS_CONTIG | NVME_SQ_PRIO_MEDIUM;
    status = nvme_submit_cmd_wait(&nvme_devices[device].queues[0], sq_command);
    if (status != 0) {
        return -1;
    }
    return 0;
 }

int nvme_rw_lba(int device, void *buf, size_t lba_start, size_t lba_count, int write) {
    if (lba_start + lba_count >= nvme_devices[device].num_lbas) {
        lba_count -= (lba_start + lba_count) - nvme_devices[device].num_lbas;
    }
    int page_offset = (size_t)buf & (PAGE_SIZE - 1);
    int use_prp2 = 0;
    int use_prp_list = 0;
    uint32_t command_id = nvme_devices[device].queues[1].command_id;
    if ((lba_count * nvme_devices[device].lba_size) > PAGE_SIZE) {
        if ((lba_count * nvme_devices[device].lba_size) > (PAGE_SIZE * 2)) {
            int prp_num = ((lba_count - 1) * nvme_devices[device].lba_size) / PAGE_SIZE;
            if (prp_num > nvme_devices[device].max_prps) {
                kprint(KPRN_ERR, "nvme: max prps exceeded");
                return -1;
            }
            for(int i = 0; i < prp_num; i++) {
                nvme_devices[device].queues[1].prps[i + command_id * nvme_devices[device].max_prps] = ((size_t)(buf - MEM_PHYS_OFFSET - page_offset) + PAGE_SIZE + i * PAGE_SIZE);
            }
            use_prp2 = 0;
            use_prp_list = 1;
        } else {
            use_prp2 = 1;
        }
    }
    struct nvme_command command = {0};
    command.rw.opcode = write ? nvme_cmd_write : nvme_cmd_read;
    command.rw.flags = 0;
    command.rw.nsid = 1;
    command.rw.control = 0;
    command.rw.dsmgmt = 0;
    command.rw.reftag = 0;
    command.rw.apptag = 0;
    command.rw.appmask = 0;
    command.rw.metadata = 0;
    command.rw.slba = lba_start;
    command.rw.length = lba_count - 1;
    if (use_prp_list) {
        command.rw.prp1 = (size_t)((size_t)buf - MEM_PHYS_OFFSET);
        command.rw.prp2 = (size_t)(&nvme_devices[device].queues[1].prps[command_id * nvme_devices[device].max_prps]) - MEM_PHYS_OFFSET;
    } else if(use_prp2) {
        command.rw.prp2 = (size_t)((size_t)(buf) + PAGE_SIZE - MEM_PHYS_OFFSET);
    } else {
        command.rw.prp1 = (size_t)((size_t)buf - MEM_PHYS_OFFSET);
    }
    uint16_t status = nvme_submit_cmd_wait(&nvme_devices[device].queues[1], command);
    if (status != 0) {
        kprint(KPRN_ERR, "nvme: read/write operation failed with status %x", status);
        return -1;
    }
    return 0;
}

static int find_block(int device, uint64_t block) {
    for (size_t i = 0; i < MAX_CACHED_BLOCKS; i++)
        if ((nvme_devices[device].cache[i].block == block)
            && (nvme_devices[device].cache[i].status))
            return i;

    return -1;
}

static int cache_block(int device, uint64_t block) {
    int targ;
    int ret;

    //Find empty block
    for (targ = 0; targ < MAX_CACHED_BLOCKS; targ++)
        if (!nvme_devices[device].cache[targ].status) goto fnd;

    //Slot not found, overwrite another
    if (nvme_devices[device].overwritten_slot == MAX_CACHED_BLOCKS)
        nvme_devices[device].overwritten_slot = 0;

    targ = nvme_devices[device].overwritten_slot++;

    //Flush device cache
    if (nvme_devices[device].cache[targ].status == CACHE_DIRTY) {
        ret = nvme_rw_lba(device,
                nvme_devices[device].cache[targ].cache,
                (nvme_devices[device].cache_block_size / nvme_devices[device].lba_size) * nvme_devices[device].cache[targ].block,
                    (nvme_devices[device].cache_block_size / nvme_devices[device].lba_size), 1);

        if (ret == -1) return -1;
    }

    goto notfnd;

fnd:
    nvme_devices[device].cache[targ].cache = kalloc(nvme_devices[device].cache_block_size);

notfnd:
    ret = nvme_rw_lba(device,
            nvme_devices[device].cache[targ].cache,
            (nvme_devices[device].cache_block_size / nvme_devices[device].lba_size) * block,
                (nvme_devices[device].cache_block_size / nvme_devices[device].lba_size), 0);

    if (ret == -1) return -1;

    nvme_devices[device].cache[targ].block = block;
    nvme_devices[device].cache[targ].status = CACHE_READY;

    return targ;
}

static int nvme_write(int device, const void *buf, uint64_t loc, size_t count) {
    spinlock_acquire((&nvme_devices[device].nvme_lock));

    uint64_t progress = 0;
    while (progress < count) {
        //cache the block
        uint64_t sect = (loc + progress) / (nvme_devices[device].cache_block_size);
        int slot = find_block(device, sect);
        if (slot == -1) {
            slot = cache_block(device, sect);
            if (slot == -1) {
                spinlock_release((&nvme_devices[device].nvme_lock));
                return -1;
            }
        }

        uint64_t chunk = count - progress;
        uint64_t offset = (loc + progress) % nvme_devices[device].cache_block_size;
        if (chunk > nvme_devices[device].cache_block_size - offset)
            chunk = nvme_devices[device].cache_block_size - offset;

        memcpy(&(nvme_devices[device].cache[slot].cache[offset]), buf + progress, chunk);
        nvme_devices[device].cache[slot].status = CACHE_DIRTY;
        progress += chunk;
    }

    spinlock_release((&nvme_devices[device].nvme_lock));
    return (int)count;
}

static int nvme_flush_cache(int device) {
    spinlock_acquire((&nvme_devices[device].nvme_lock));
    for (size_t i = 0; i < MAX_CACHED_BLOCKS; i++) {
        if (nvme_devices[device].cache[i].status == CACHE_DIRTY) {
            int ret = nvme_rw_lba(device,
                nvme_devices[device].cache[i].cache,
                (nvme_devices[device].cache_block_size / nvme_devices[device].lba_size) * nvme_devices[device].cache[i].block,
                    (nvme_devices[device].cache_block_size / nvme_devices[device].lba_size), 1);

            if (ret == -1) {
                spinlock_release((&nvme_devices[device].nvme_lock));
                return -1;
            }

            nvme_devices[device].cache[i].status = CACHE_READY;
        }
    }

    spinlock_release((&nvme_devices[device].nvme_lock));
    return 0;
}

static int nvme_read(int device, void *buf, uint64_t loc, size_t count) {
    spinlock_acquire((&nvme_devices[device].nvme_lock));

    uint64_t progress = 0;
    while (progress < count) {
        //cache the block
        uint64_t sect = (loc + progress) / nvme_devices[device].cache_block_size;
        int slot = find_block(device, sect);
        if (slot == -1) {
            slot = cache_block(device, sect);
            if (slot == -1) {
                spinlock_release((&nvme_devices[device].nvme_lock));
                return -1;
            }
        }

        uint64_t chunk = count - progress;
        uint64_t offset = (loc + progress) % nvme_devices[device].cache_block_size;
        if (chunk > nvme_devices[device].cache_block_size - offset)
            chunk = nvme_devices[device].cache_block_size - offset;

        memcpy(buf + progress, (&nvme_devices[device].cache[slot].cache[offset]), chunk);
        progress += chunk;
    }

    spinlock_release(&(nvme_devices[device].nvme_lock));
    return (int)count;
}

int nvme_init_device(struct pci_device_t *ndevice, int num) {
    nvme_device_t device = {0};
    device.nvme_lock = new_lock;
    struct pci_bar_t bar = {0};

    panic_if(pci_read_bar(ndevice, 0, &bar));
    panic_unless(bar.is_mmio);

    panic_unless((pci_read_device_dword(ndevice, 0x10) & 0b111) == 0b100);

    volatile struct nvme_bar *nvme_base = (struct nvme_bar*)(bar.base + MEM_PHYS_OFFSET);
    device.nvme_base = nvme_base;
    pci_enable_busmastering(ndevice);

    //Enable mmio.
    pci_write_device_dword(ndevice, 0x4, pci_read_device_dword(ndevice, 0x4) | (1 << 1));

    //The controller needs to be disabled in order to configure the admin queues.
    uint32_t cc = nvme_base->cc;
    if (cc & NVME_CC_ENABLE) {
        cc &= ~NVME_CC_ENABLE;
        nvme_base->cc = cc;
    }
    while(((nvme_base->csts) & NVME_CSTS_RDY) != 0){};
    kprint(KPRN_INFO, "nvme: controller disabled");

    device.queue_slots = NVME_CAP_MQES(nvme_base->cap);
    device.doorbell_stride = NVME_CAP_STRIDE(nvme_base->cap);
    nvme_devices[num] = device;
    nvme_initialize_queue(num, &nvme_devices[num].queues[0], nvme_devices[num].queue_slots, 0);

    //Initialize admin queues
    //admin queue attributes
    uint32_t aqa = nvme_devices[num].queue_slots - 1;
    aqa |= aqa << 16;
    aqa |= aqa << 16;
    nvme_base->aqa = aqa;
    cc = NVME_CC_CSS_NVM
        | NVME_CC_ARB_RR | NVME_CC_SHN_NONE
        | NVME_CC_IOSQES | NVME_CC_IOCQES
        | NVME_CC_ENABLE;
    nvme_base->asq = (size_t)nvme_devices[num].queues[0].submit - MEM_PHYS_OFFSET;
    nvme_base->acq = (size_t)nvme_devices[num].queues[0].completion - MEM_PHYS_OFFSET;
    nvme_base->cc = cc;
    //enable the controller and wait for it to be ready.
    while(1) {
        uint32_t status = nvme_base->csts;
        if (status & NVME_CSTS_RDY) {
            break;
        } else if (status & NVME_CSTS_CFS) {
            kprint(KPRN_ERR, "nvme: controller fatal status");
            return -1;
        }
    }
    kprint(KPRN_INFO, "nvme: controller restarted");

    struct nvme_id_ctrl *id = (struct nvme_id_ctrl *)kalloc(sizeof(struct nvme_id_ctrl));
    int status = nvme_identify(num, id);
    if (status != 0) {
        kprint(KPRN_ERR, "nvme: Failed to identify NVME device");
        return -1;
    }

	struct nvme_id_ns *id_ns = kalloc(sizeof(struct nvme_id_ns));
    nvme_get_ns_info(num, 1, id_ns);
    if (status != 0) {
        kprint(KPRN_ERR, "nvme: Failed to get namespace info for namespace 1");
        return -1;
    }

    //The maximum transfer size is in units of 2^(min page size)
    size_t lba_shift = id_ns->lbaf[id_ns->flbas & NVME_NS_FLBAS_LBA_MASK].ds;
    size_t max_lbas = 1 << (nvme_devices[num].max_transfer_shift - lba_shift);
    nvme_devices[num].max_prps = (max_lbas * (1 << lba_shift)) / PAGE_SIZE;
    nvme_devices[num].cache_block_size = (max_lbas * (1 << lba_shift));

    status = nvme_create_queue_pair(num, 1);
    if (status != 0) {
        kprint(KPRN_ERR, "nvme: Failed to create i/o queues");
        return -1;
    }

    size_t formatted_lba = id_ns->flbas & NVME_NS_FLBAS_LBA_MASK;
    nvme_devices[num].lba_size = 1 << id_ns->lbaf[formatted_lba].ds;
    kprint(KPRN_INFO, "nvme: namespace 1 size %X lbas, lba size: %X bytes", id_ns->nsze, nvme_devices[num].lba_size);
    nvme_devices[num].num_lbas = id_ns->nsze;
    nvme_devices[num].cache = kalloc(sizeof(cached_block_t) * MAX_CACHED_BLOCKS);

    static const char *nvme_basename = "nvme";
    struct device_t vfs_device = {0};
    vfs_device.calls = default_device_calls;
    char *dev_name = prefixed_itoa(nvme_basename, num, 10);
    strcpy(vfs_device.name, dev_name);
    kprint(KPRN_INFO, "nvme: Initialised /dev/%s", dev_name);
    kfree(dev_name);
    vfs_device.intern_fd = num;
    vfs_device.size = id_ns->nsze * nvme_devices[num].lba_size;
    vfs_device.calls.read = nvme_read;
    vfs_device.calls.write = nvme_write;
    vfs_device.calls.flush = nvme_flush_cache;
    device_add(&vfs_device);
    enum_partitions(dev_name, &vfs_device);
    return 0;
}

void init_dev_nvme(void) {
    struct pci_device_t *ndevice = pci_get_device(NVME_CLASS, NVME_SUBCLASS, NVME_PROG_IF, 0);
    if (!ndevice) {
        kprint(KPRN_INFO, "nvme: Failed to locate NVME controller");
        return;
    }
    kprint(KPRN_INFO, "nvme: Found NVME controller");
    nvme_devices = kalloc(sizeof(nvme_device_t));
    nvme_init_device(ndevice, 0);
}
