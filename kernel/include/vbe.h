#ifndef __VBE_H__
#define __VBE_H__

#include <stdint.h>

typedef struct {
    uint8_t version_min;
    uint8_t version_maj;
    uint32_t oem;   // is a 32 bit pointer to char
    uint32_t capabilities;
    uint32_t vid_modes;     // is a 32 bit pointer to uint16_t
    uint16_t vid_mem_blocks;
    uint16_t software_rev;
    uint32_t vendor;   // is a 32 bit pointer to char
    uint32_t prod_name;   // is a 32 bit pointer to char
    uint32_t prod_rev;   // is a 32 bit pointer to char
} __attribute__((packed)) vbe_info_struct_t;

typedef struct {
    uint8_t padding[8];
    uint16_t manufacturer_id_be;
    uint16_t edid_id_code;
    uint32_t serial_num;
    uint8_t man_week;
    uint8_t man_year;
    uint8_t edid_version;
    uint8_t edid_revision;
    uint8_t video_input_type;
    uint8_t max_hor_size;
    uint8_t max_ver_size;
    uint8_t gamma_factor;
    uint8_t dpms_flags;
    uint8_t chroma_info[10];
    uint8_t est_timings1;
    uint8_t est_timings2;
    uint8_t man_res_timing;
    uint16_t std_timing_id[8];
    uint8_t det_timing_desc1[18];
    uint8_t det_timing_desc2[18];
    uint8_t det_timing_desc3[18];
    uint8_t det_timing_desc4[18];
    uint8_t unused;
    uint8_t checksum;
} __attribute__((packed)) edid_info_struct_t;

typedef struct {
    uint8_t pad0[16];
    uint16_t pitch;
    uint16_t res_x;
    uint16_t res_y;
    uint8_t pad1[3];
    uint8_t bpp;
    uint8_t pad2[14];
    uint32_t framebuffer;
    uint8_t pad3[212];
} __attribute__((packed)) vbe_mode_info_t;

typedef struct {
    uint32_t vbe_mode_info;      // is a 32 bit pointer to vbe_mode_info_t
    uint16_t mode;
} get_vbe_t;

extern uint32_t *vbe_framebuffer;
extern int vbe_width;
extern int vbe_height;
extern int vbe_pitch;

extern int vbe_available;

void get_vbe_info(vbe_info_struct_t *);
void get_edid_info(edid_info_struct_t *);
void get_vbe_mode_info(get_vbe_t *);
void set_vbe_mode(uint16_t);

void init_vbe(void);

#endif
