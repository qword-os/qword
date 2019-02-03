#ifndef __NVME_PRIVATE_H__
#define __NVME_PRIVATE_H__

/* register offsets */
#define REG_CAP      0x0
#define REG_VS       0x8
#define REG_INTMS    0xc
#define REG_INTMC    0x10
#define REG_CC       0x14
#define REG_CSTS     0x1c
#define REG_NSSR     0x20
#define REG_AQA      0x24
#define REG_ASQ      0x28
#define REG_ACQ      0x30
#define REG_CMBLOC   0x38
#define REG_CMBSZ    0x3C
#define REG_BPINFO   0x40
#define REG_BPRSEL   0x44
#define REG_BPMBL    0x48
#define REG_SQ0TDBL  0x1000

/* structure of CDW0
 offset (bits)   desc
 31:16           command id - provides unique id for this command when used with sqid
 15:14           psdt - use prps or sgls for data transfers
 13:10           reserved
 09:08           fused operation - specifies whether this op is a fused op or not
 07:00           opcode - specifies the opcode of the command
*/

// these fields are common to all commands
struct subqueue_common_t {
    // command dword 0
    uint32_t cdw0;
    uint32_t nsid;
    uint64_t resv0;
    uint64_t mptr;
    uint64_t dptr;
};

// vendor specific command
struct subqueue_vendor_command_t {
    struct subqueue_common_t common;
    // no. of dwords in data transfer
    uint32_t ndt;
    // no. of dwords in metadata transfer
    uint32_t ndm;
    uint32_t cdw[4];
};

struct subqueue_command_t {
    struct subqueue_common_t common;
    uint32_t cdw[6];
};

// structure of a completion queue entry
struct comp_queue_entry_t {
    // command-specific. If the command utilises DW0,
    // then the structure of DW0 will be specified
    // by the command
    uint32_t command_info;
    uint32_t resv0;
    // sub queue id
    uint16_t sqid;
    uint16_t sqhd;
    // status
    uint16_t sts : 15;
    uint8_t p : 1;
    uint16_t cid;
};

#endif
