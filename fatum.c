#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "fatum.h"

boot_t br;
char **fats;
entry_data_t *root;
char *data;


int main() {
    FILE *f;
    f = fopen("fat16.bin","rb");
    int ret = fread(&br,sizeof(br),1,f);
    assert(ret==1);

    printf("Assembly code: %02hhx %02hhx %02hhx\n",br.assembly_code[0],br.assembly_code[1],br.assembly_code[2]);
    printf("OEM: %.*s\n",8,br.oem);
    printf("Bytes per sector: %hd\n",br.bytes_per_sector);
    printf("Sectors per cluster: %hhu\n",br.sectors_per_cluster);
    printf("Reserved area size: %hu\n",br.reserved_area_size);
    printf("Number of FATs: %hhu\n",br.number_of_fats);
    printf("Max files in root: %hu\n",br.max_files_in_root);
    printf("Sectors in FS: %hu\n",br.sectors_in_fs);
    printf("Media type: 0x%02hhx\n",br.media_type);
    printf("Size of FAT: %hu\n",br.size_of_fat);
    printf("Sectors per track: %hu\n",br.sectors_per_track);
    printf("Heads: %hu\n",br.heads);
    printf("Sectors before start: %u\n",br.sectors_before_start);
    printf("Sectors in FS (large): %u\n",br.sectors_in_fs_large);
    printf("Drive number: 0x%02hhx\n",br.drive_number);
    printf("Boot signature: 0x%02hhx\n",br.boot_signature);
    printf("Volume serial number: %u\n",br.vol_serial_number);
    printf("Volume label: %.*s\n",11,br.vol_label);
    printf("FS type label: %.*s\n",8,br.fs_type);
    printf("Signature value: 0x%04hx\n",br.signature_value);

    fseek(f,LOC_FAT1START,SEEK_SET);
    fats = malloc(sizeof(char*)*br.number_of_fats);
    assert(fats!=NULL);
    for (int i=0; i<br.number_of_fats; i++) {
        fats[i]=calloc(1,br.size_of_fat*br.bytes_per_sector);
        assert(fats[i]!=NULL);
    }

    for (int i=0; i<br.number_of_fats; i++) {
        ret = fread(fats[i], br.size_of_fat*br.bytes_per_sector, 1, f);
        assert(ret==1);
    }

    fseek(f,LOC_ROOTSTART,SEEK_SET);
    root = calloc(1,sizeof(entry_data_t)*br.max_files_in_root);
    ret = fread(root, sizeof(entry_data_t)*br.max_files_in_root, 1, f);
    assert(ret==1);

    entry_data_t *filep = root;

    printf("Filename: %.*s\n",11,filep->filename);
    printf("Attributes: 0x%02hhx\n",filep->attributes);
    printf("Creation time: 0x%02hhx\n",filep->creation_time_tenths);
    printf("File size: %u\n",filep->file_size);

    char str[100];
    memcpy(str,filep,100);
    printf("\n\n%.*s\n",100,str);

    fclose(f);
    assert(memcmp(fats[0],fats[1],br.size_of_fat*br.bytes_per_sector)==0);
    return 0;
}

