#ifndef FATUM_H
#define FATUM_H

#include <stdint.h>

// File Attributes Flags
#define FAF_READ_ONLY (char)0x01
#define FAF_HIDDEN_FILE (char)0x02
#define FAF_SYSTEM_FILE (char)0x04
#define FAF_VOL_LABEL (char)0x08
#define FAF_LFN (char)0x0F
#define FAF_DIR (char)0x10
#define FAF_ARCHIVE (char)0x20

// File Entry 1st Byte: unallocated/deleted
#define FEI_UNALLOC (char)0x00
#define FEI_DELETED (char)0xe5

// Datetime masks and shifts
#define DAT_YEAR 0xFE00
#define DAT_MONTH 0x1E0
#define DAT_DAY 0x1F
#define DAT_HOUR 0xF800
#define DAT_MIN 0x7E0
#define DAT_SEC 0x1F

#define SHIFT_YEAR 9
#define SHIFT_MONTH 5
#define SHIFT_HOUR 11
#define SHIFT_MIN 5

// fseek locations
#define LOC_VOLSTART 0
#define LOC_FAT1START LOC_VOLSTART+br.reserved_area_size*br.bytes_per_sector
#define LOC_FAT2START LOC_FAT1START+br.size_of_fat*br.bytes_per_sector
#define LOC_ROOTSTART LOC_FAT1START+br.size_of_fat*br.number_of_fats*br.bytes_per_sector
#define LOC_DATASTART LOC_ROOTSTART+(br.max_files_in_root*sizeof(entry_data_t))
#define LOC_CLUSTER(n) LOC_DATASTART+(n-2)*br.sectors_per_cluster*br.bytes_per_sector

// Cluster offset (from data block start)
#define JMP_CLUSTER(n) (n-2)*br.sectors_per_cluster*br.bytes_per_sector

typedef struct __attribute__ ((__packed__)) boot {
    char assembly_code[3]; // instructions to jump to boot code
    char oem[8]; // in ASCII
    short bytes_per_sector; // 512,1024,2048,4096
    unsigned char sectors_per_cluster; // must be a power of 2 and cluster size must be <=32 KB
    unsigned short reserved_area_size; // in sectors
    unsigned char number_of_fats; // usually 2
    unsigned short max_files_in_root; // 0 for FAT32
    unsigned short sectors_in_fs; // if 2B is not large enough, set to 0 and user 4B value in bytes 32-35
    char media_type; // 0xf0 = removable, 0xf8 = fixed
    unsigned short size_of_fat; // in sectors, 0 for FAT32
    unsigned short sectors_per_track;
    unsigned short heads;
    uint32_t sectors_before_start; // before the start partition
    uint32_t sectors_in_fs_large;
    char drive_number; // BIOS INT 13h (low level disk services) drive number
    char not_used_01;
    char boot_signature; // to validate next three fields (0x29)
    uint32_t vol_serial_number;
    char vol_label[11]; // in ASCII
    char fs_type[8]; // in ASCII
    char not_used_02[448];
    short signature_value; // 0xaa55
} boot_t;

typedef struct __attribute__ ((__packed__)) entry_data {
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
} entry_data_t;

typedef struct __attribute__ ((__packed__)) longfilename {
    char entry_order; // The order of this entry in the sequence of long file name entries. This value helps you to know where in the file's name the characters from this entry should be placed. 
    short filename1[5]; // first 5 characters
    char attributes; // always should equal 0x0F (the long file name attribute)
    char long_entry_type; // 0 for name entries
    char checksum; // generated of the short file name when the file was created. The short filename can change without changing the long filename in cases where the partition is mounted on a system which does not support long filenames.
    short filename2[6]; // next 6 characters
    char zero_here; // always 0
    short filename3[2]; // last 2 characters of this entry
} lfn_t;

typedef struct date {
    short year;
    char month;
    char day;
} filedate_t;

typedef struct time {
    char hrs;
    char min;
    char sec;
} filetime_t;

int load_disk(const char *filename);
void command_prompt();
void prepare_for_exit();
void flush_scan();
void debug_read();
int hidden_in_dir(entry_data_t *entry);
filedate_t get_date(short date);
filetime_t get_time(short time);
void show_dir_content(entry_data_t *first_entry);
int format_filename(const char *filename, char *dst);
entry_data_t *fetch_dir(entry_data_t *dir);
entry_data_t *find_entry(entry_data_t *pwd, const char *filename);

// http://www.c-jump.com/CIS24/Slides/FAT/lecture.html#F01_0030_layout

#endif //FATUM_H