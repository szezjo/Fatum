#ifndef FATUM_H
#define FATUM_H

#include <stdint.h>

// File Attributes Flags
#define FAF_READ_ONLY 0x01
#define FAF_HIDDEN_FILE 0x02
#define FAF_SYSTEM_FILE 0x04
#define FAF_VOL_LABEL 0x08
#define FAF_LFN 0x0F
#define FAF_DIR 0x10
#define FAF_ARCHIVE 0x20

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

struct __attribute__ ((__packed__)) entry_data {
    char filename[11]; // 8 - filename, 3 - extension, also first char is an allocation status: 0x00=unallocated, 0xe5=deleted
    char attributes;
    char reserved;
    char creation_time_tenths; // in tenths of seconds
    short creation_time; // hr=5b, min=6b, sec=5b
    short creation_date; // yr=7b, mon=4b, day=5b
    short access_date; // yr=7b, mon=4b, day=5b
    short high_order_address_bytes; // High-order 2 bytes of address of first cluster (0 for FAT12/16)
    short modified_time; // hr=5b, min=6b, sec=5b
    short modified_date; // yr=7b, mon=4b, day=5b
    short low_order_address_bytes; // Low-order 2 bytes of address of first cluster
    uint32_t file_size; // if directory = 0
}

struct __attribute__ ((__packed__)) longfilename {
    char entry_order; // The order of this entry in the sequence of long file name entries. This value helps you to know where in the file's name the characters from this entry should be placed. 
    short filename1[5]; // first 5 characters
    char attributes; // always should equal 0x0F (the long file name attribute)
    char long_entry_type; // 0 for name entries
    char checksum; // generated of the short file name when the file was created. The short filename can change without changing the long filename in cases where the partition is mounted on a system which does not support long filenames.
    short filename2[6]; // next 6 characters
    char zero_here; // always 0
    short filename3[2]; // last 2 characters of this entry

}

// http://www.c-jump.com/CIS24/Slides/FAT/lecture.html#F01_0030_layout

#endif //FATUM_H