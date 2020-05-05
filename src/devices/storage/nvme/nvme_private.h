#ifndef NVME_PRIV_H
#define NVME_PRIV_H

#define NVME_CLASS 0x01
#define NVME_SUBCLASS 0x08
#define NVME_PROG_IF 0x02

struct nvme_id_power_state {
    uint16_t            max_power;  /* centiwatts */
    uint8_t         rsvd2;
    uint8_t         flags;
    uint32_t            entry_lat;  /* microseconds */
    uint32_t            exit_lat;   /* microseconds */
    uint8_t         read_tput;
    uint8_t         read_lat;
    uint8_t         write_tput;
    uint8_t         write_lat;
    uint16_t            idle_power;
    uint8_t         idle_scale;
    uint8_t         rsvd19;
    uint16_t            active_power;
    uint8_t         active_work_scale;
    uint8_t         rsvd23[9];
};

enum {
    NVME_PS_FLAGS_MAX_POWER_SCALE   = 1 << 0,
    NVME_PS_FLAGS_NON_OP_STATE  = 1 << 1,
};

struct nvme_id_ctrl {
    uint16_t        vid;
    uint16_t        ssvid;
    char            sn[20];
    char            mn[40];
    char            fr[8];
    uint8_t         rab;
    uint8_t         ieee[3];
    uint8_t         mic;
    uint8_t         mdts;
    uint16_t        cntlid;
    uint32_t        ver;
    uint8_t         rsvd84[172];
    uint16_t        oacs;
    uint8_t         acl;
    uint8_t         aerl;
    uint8_t         frmw;
    uint8_t         lpa;
    uint8_t         elpe;
    uint8_t         npss;
    uint8_t         avscc;
    uint8_t         apsta;
    uint16_t        wctemp;
    uint16_t        cctemp;
    uint8_t         rsvd270[242];
    uint8_t         sqes;
    uint8_t         cqes;
    uint8_t         rsvd514[2];
    uint32_t        nn;
    uint16_t        oncs;
    uint16_t        fuses;
    uint8_t         fna;
    uint8_t         vwc;
    uint16_t        awun;
    uint16_t        awupf;
    uint8_t         nvscc;
    uint8_t         rsvd531;
    uint16_t        acwu;
    uint8_t         rsvd534[2];
    uint32_t        sgls;
    uint8_t         rsvd540[1508];
    struct nvme_id_power_state  psd[32];
    uint8_t         vs[1024];
};

struct nvme_lbaf {
    uint16_t            ms;
    uint8_t             ds;
    uint8_t             rp;
};

enum {
    NVME_NS_FEAT_THIN       = 1 << 0,
    NVME_NS_FLBAS_LBA_MASK  = 0xf,
    NVME_NS_FLBAS_META_EXT  = 0x10,
    NVME_LBAF_RP_BEST       = 0,
    NVME_LBAF_RP_BETTER     = 1,
    NVME_LBAF_RP_GOOD       = 2,
    NVME_LBAF_RP_DEGRADED   = 3,
    NVME_NS_DPC_PI_LAST     = 1 << 4,
    NVME_NS_DPC_PI_FIRST    = 1 << 3,
    NVME_NS_DPC_PI_TYPE3    = 1 << 2,
    NVME_NS_DPC_PI_TYPE2    = 1 << 1,
    NVME_NS_DPC_PI_TYPE1    = 1 << 0,
    NVME_NS_DPS_PI_FIRST    = 1 << 3,
    NVME_NS_DPS_PI_MASK     = 0x7,
    NVME_NS_DPS_PI_TYPE1    = 1,
    NVME_NS_DPS_PI_TYPE2    = 2,
    NVME_NS_DPS_PI_TYPE3    = 3,
};

struct nvme_id_ns {
    uint64_t            nsze;
    uint64_t            ncap;
    uint64_t            nuse;
    uint8_t             nsfeat;
    uint8_t             nlbaf;
    uint8_t             flbas;
    uint8_t             mc;
    uint8_t             dpc;
    uint8_t             dps;
    uint8_t             nmic;
    uint8_t             rescap;
    uint8_t             fpi;
    uint8_t             rsvd33;
    uint16_t            nawun;
    uint16_t            nawupf;
    uint16_t            nacwu;
    uint16_t            nabsn;
    uint16_t            nabo;
    uint16_t            nabspf;
    uint16_t            rsvd46;
    uint64_t            nvmcap[2];
    uint8_t             rsvd64[40];
    uint8_t             nguid[16];
    uint8_t             eui64[8];
    struct nvme_lbaf    lbaf[16];
    uint8_t             rsvd192[192];
    uint8_t             vs[3712];
};

/* I/O commands */

enum nvme_opcode {
    nvme_cmd_flush          = 0x00,
    nvme_cmd_write          = 0x01,
    nvme_cmd_read           = 0x02,
    nvme_cmd_write_uncor    = 0x04,
    nvme_cmd_compare        = 0x05,
    nvme_cmd_write_zeroes   = 0x08,
    nvme_cmd_dsm            = 0x09,
    nvme_cmd_resv_register  = 0x0d,
    nvme_cmd_resv_report    = 0x0e,
    nvme_cmd_resv_acquire   = 0x11,
    nvme_cmd_resv_release   = 0x15,
};

struct nvme_common_command {
    uint8_t             opcode;
    uint8_t             flags;
    uint16_t            command_id;
    uint32_t            nsid;
    uint32_t            cdw2[2];
    uint64_t            metadata;
    uint64_t            prp1;
    uint64_t            prp2;
    uint32_t            cdw10[6];
};

struct nvme_rw_command {
    uint8_t             opcode;
    uint8_t             flags;
    uint16_t            command_id;
    uint32_t            nsid;
    uint64_t            rsvd2;
    uint64_t            metadata;
    uint64_t            prp1;
    uint64_t            prp2;
    uint64_t            slba;
    uint16_t            length;
    uint16_t            control;
    uint32_t            dsmgmt;
    uint32_t            reftag;
    uint16_t            apptag;
    uint16_t            appmask;
};

struct nvme_dsm_cmd {
    uint8_t         opcode;
    uint8_t         flags;
    uint16_t            command_id;
    uint32_t            nsid;
    uint64_t            rsvd2[2];
    uint64_t            prp1;
    uint64_t            prp2;
    uint32_t            nr;
    uint32_t            attributes;
    uint32_t            rsvd12[4];
};

/* Admin commands */

enum nvme_admin_opcode {
    nvme_admin_delete_sq        = 0x00,
    nvme_admin_create_sq        = 0x01,
    nvme_admin_get_log_page     = 0x02,
    nvme_admin_delete_cq        = 0x04,
    nvme_admin_create_cq        = 0x05,
    nvme_admin_identify         = 0x06,
    nvme_admin_abort_cmd        = 0x08,
    nvme_admin_set_features     = 0x09,
    nvme_admin_get_features     = 0x0a,
    nvme_admin_async_event      = 0x0c,
    nvme_admin_activate_fw      = 0x10,
    nvme_admin_download_fw      = 0x11,
    nvme_admin_format_nvm       = 0x80,
    nvme_admin_security_send    = 0x81,
    nvme_admin_security_recv    = 0x82,
};

#define NVME_QUEUE_PHYS_CONTIG  (1 << 0)
#define NVME_SQ_PRIO_MEDIUM	    (2 << 1)
#define NVME_CQ_IRQ_ENABLED     (1 << 1)
#define NVME_FEAT_NUM_QUEUES	 0x07

struct nvme_identify {
    uint8_t             opcode;
    uint8_t             flags;
    uint16_t            command_id;
    uint32_t            nsid;
    uint64_t            rsvd2[2];
    uint64_t            prp1;
    uint64_t            prp2;
    uint32_t            cns;
    uint32_t            rsvd11[5];
};

struct nvme_features {
    uint8_t             opcode;
    uint8_t             flags;
    uint16_t            command_id;
    uint32_t            nsid;
    uint64_t            rsvd2[2];
    uint64_t            prp1;
    uint64_t            prp2;
    uint32_t            fid;
    uint32_t            dword11;
    uint32_t            rsvd12[4];
};

struct nvme_create_cq {
    uint8_t             opcode;
    uint8_t             flags;
    uint16_t            command_id;
    uint32_t            rsvd1[5];
    uint64_t            prp1;
    uint64_t            rsvd8;
    uint16_t            cqid;
    uint16_t            qsize;
    uint16_t            cq_flags;
    uint16_t            irq_vector;
    uint32_t            rsvd12[4];
};

struct nvme_create_sq {
    uint8_t             opcode;
    uint8_t             flags;
    uint16_t            command_id;
    uint32_t            rsvd1[5];
    uint64_t            prp1;
    uint64_t            rsvd8;
    uint16_t            sqid;
    uint16_t            qsize;
    uint16_t            sq_flags;
    uint16_t            cqid;
    uint32_t            rsvd12[4];
};

struct nvme_delete_queue {
    uint8_t             opcode;
    uint8_t             flags;
    uint16_t            command_id;
    uint32_t            rsvd1[9];
    uint16_t            qid;
    uint16_t            rsvd10;
    uint32_t            rsvd11[5];
};

struct nvme_abort_cmd {
    uint8_t             opcode;
    uint8_t             flags;
    uint16_t            command_id;
    uint32_t            rsvd1[9];
    uint16_t            sqid;
    uint16_t            cid;
    uint32_t            rsvd11[5];
};

struct nvme_download_firmware {
    uint8_t             opcode;
    uint8_t             flags;
    uint16_t            command_id;
    uint32_t            rsvd1[5];
    uint64_t            prp1;
    uint64_t            prp2;
    uint32_t            numd;
    uint32_t            offset;
    uint32_t            rsvd12[4];
};

struct nvme_format_cmd {
    uint8_t             opcode;
    uint8_t             flags;
    uint16_t            command_id;
    uint32_t            nsid;
    uint64_t            rsvd2[4];
    uint32_t            cdw10;
    uint32_t            rsvd11[5];
};

struct nvme_command {
    union {
        struct nvme_common_command common;
        struct nvme_rw_command rw;
        struct nvme_identify identify;
        struct nvme_features features;
        struct nvme_create_cq create_cq;
        struct nvme_create_sq create_sq;
        struct nvme_delete_queue delete_queue;
        struct nvme_download_firmware dlfw;
        struct nvme_format_cmd format;
        struct nvme_dsm_cmd dsm;
        struct nvme_abort_cmd abort;
    };
};

struct nvme_completion {
    uint32_t    result;     /* Used by admin commands to return data */
    uint32_t    rsvd;
    uint16_t    sq_head;    /* how much of this queue may be reclaimed */
    uint16_t    sq_id;      /* submission queue that generated this entry */
    uint16_t    command_id; /* of the command which completed */
    uint16_t    status;     /* did the command fail, and if so, why? */
};

struct nvme_bar {
    uint64_t cap;   /* Controller Capabilities */
    uint32_t vs;    /* Version */
    uint32_t intms; /* Interrupt Mask Set */
    uint32_t intmc; /* Interrupt Mask Clear */
    uint32_t cc;    /* Controller Configuration */
    uint32_t rsvd1; /* Reserved */
    uint32_t csts;  /* Controller Status */
    uint32_t rsvd2; /* Reserved */
    uint32_t aqa;   /* Admin Queue Attributes */
    uint64_t asq;   /* Admin SQ Base Address */
    uint64_t acq;   /* Admin CQ Base Address */
};

#define NVME_CAP_MQES(cap)      ((cap) & 0xffff)
#define NVME_CAP_TIMEOUT(cap)   (((cap) >> 24) & 0xff)
#define NVME_CAP_STRIDE(cap)    (((cap) >> 32) & 0xf)
#define NVME_CAP_MPSMIN(cap)    (((cap) >> 48) & 0xf)
#define NVME_CAP_MPSMAX(cap)    (((cap) >> 52) & 0xf)

enum {
    NVME_CC_ENABLE          = 1 << 0,
    NVME_CC_CSS_NVM         = 0 << 4,
    NVME_CC_MPS_SHIFT       = 7,
    NVME_CC_ARB_RR          = 0 << 11,
    NVME_CC_ARB_WRRU        = 1 << 11,
    NVME_CC_ARB_VS          = 7 << 11,
    NVME_CC_SHN_NONE        = 0 << 14,
    NVME_CC_SHN_NORMAL      = 1 << 14,
    NVME_CC_SHN_ABRUPT      = 2 << 14,
    NVME_CC_SHN_MASK        = 3 << 14,
    NVME_CC_IOSQES          = 6 << 16,
    NVME_CC_IOCQES          = 4 << 20,
    NVME_CSTS_RDY           = 1 << 0,
    NVME_CSTS_CFS           = 1 << 1,
    NVME_CSTS_SHST_NORMAL   = 0 << 2,
    NVME_CSTS_SHST_OCCUR    = 1 << 2,
    NVME_CSTS_SHST_CMPLT    = 2 << 2,
    NVME_CSTS_SHST_MASK     = 3 << 2,
};

#endif
