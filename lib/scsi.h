#ifndef __SCSI_H__
#define __SCSI_H__

#include <stddef.h>
#include <stdint.h>

struct scsi_read_capacity_10_t {
    uint8_t op_code;
    uint8_t reserved;
    uint32_t lba;
    uint16_t reserved1;
    uint8_t reserved2;
    uint8_t control;
} __attribute__((packed));

struct scsi_read_10_t {
    uint8_t op_code;
    uint8_t options;
    uint8_t lba[4];
    uint8_t group_number;
    uint8_t length[2];
    uint8_t control;
} __attribute__((packed));

struct scsi_read_12_t {
    uint8_t op_code;
    uint8_t flags;
    uint32_t lba;
    uint32_t length;
    uint8_t group_number;
    uint8_t control;
} __attribute__((packed));

struct scsi_read_capacity_16_t {
    uint8_t op_code;
    uint8_t flags;
    uint64_t lba;
    uint32_t length;
    uint8_t group_number;
    uint8_t control;
} __attribute__((packed));

int scsi_register(int, int (*)(int, char *, size_t, char *, size_t, int),
                  size_t);

#endif
