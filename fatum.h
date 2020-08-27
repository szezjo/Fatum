#ifndef FATUM_H
#define FATUM_H

#include <stdint.h>

struct __attribute__ ((__packed__)) boot {
    char assembly_code[3]; // instructions to jump to boot code
    char oem[8]; // in ASCII
    short bytes_per_sector; // 512,1024,2048,4096
    char sectors_per_cluster; // must be a power of 2 and cluster size must be <=32 KB
    short reserved_area_size; // in sectors
    char number_of_fats; // usually 2
    short max_files_in_root; // 0 for FAT32
    short sectors_in_fs; // if 2B is not large enough, set to 0 and user 4B value in bytes 32-35
    char media_type; // 0xf0 = removable, 0xf8 = fixed
    short size_of_fat; // in sectors, 0 for FAT32
    short sectors_per_track;
    short heads;
    uint32_t sectors_before_start; // before the start partition
    uint32_t sectors_in_fs_large;
    char drive_number; // BIOS INT 13h (low level disk services) drive number
    char not_used_01;
    char boot_signature; // to validate next three fields (0x29)
    uint32_t vol_serial_number;
    char vol_label[11]; // in ASCII
    char fs_type_level[8]; // in ASCII
    char not_used_02[448];
    short signature_value; // 0xaa55
}

// http://www.c-jump.com/CIS24/Slides/FAT/lecture.html#F01_0030_layout

#endif //FATUM_H