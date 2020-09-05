#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "fatum.h"

boot_t br;
char **fats;
entry_data_t *root;
char *data;

int load_disk(const char *filename) {
    if (filename==NULL) {
        printf("Error: No filename\n");
        return 1;
    }
    FILE *f;
    f = fopen(filename, "rb");
    if (f==NULL) {
        printf("Error: Wrong filename\n");
        return 1;
    }
    int ret = fread(&br,sizeof(br),1,f);
    if (ret!=1) {
        fclose(f);
        printf("Error: Can't read boot record data\n");
        return 2;
    }

    fseek(f,LOC_FAT1START,SEEK_SET);
    fats = malloc(sizeof(char*)*br.number_of_fats);
    if(fats==NULL) {
        fclose(f);
        printf("Error: Can't allocate memory for FATs\n");
        return 3;
    }

    for (int i=0; i<br.number_of_fats; i++) {
        fats[i]=calloc(1,br.size_of_fat*br.bytes_per_sector);
        if(fats[i]==NULL) {
            for(int j=0; j<i; j++) free(fats[j]);
            fclose(f);
            free(fats);
            printf("Error: Can't allocate memory for FAT %d\n",i);
            return 3;
        }
    }

    for (int i=0; i<br.number_of_fats; i++) {
        ret = fread(fats[i], br.size_of_fat*br.bytes_per_sector, 1, f);
        if(ret!=1) {
            for(int j=0; j<br.number_of_fats; j++) free(fats[j]);
            fclose(f);
            free(fats);
            printf("Error: Can't read FAT %d data\n",i);
            return 2;
        }
    }

    fseek(f,LOC_ROOTSTART,SEEK_SET);
    root = calloc(1,sizeof(entry_data_t)*br.max_files_in_root);
    if(root==NULL) {
        for(int j=0; j<br.number_of_fats; j++) free(fats[j]);
        fclose(f);
        free(fats);
        printf("Error: Can't allocate memory for root dir\n");
        return 3;
    }

    ret = fread(root, sizeof(entry_data_t)*br.max_files_in_root, 1, f);
    if(ret!=1) {
        for(int j=0; j<br.number_of_fats; j++) free(fats[j]);
        fclose(f);
        free(fats);
        free(root);
        printf("Error: Can't read root data\n");
        return 2;
    }

    uint32_t root_sectors = sizeof(entry_data_t)*br.max_files_in_root/br.bytes_per_sector;
    uint32_t sectors_in_fs;
    if(br.sectors_in_fs>br.sectors_in_fs_large) sectors_in_fs=br.sectors_in_fs;
    else sectors_in_fs=br.sectors_in_fs_large;
    uint32_t data_sectors = sectors_in_fs-br.reserved_area_size-br.number_of_fats*br.size_of_fat-root_sectors;
    
    fseek(f,LOC_DATASTART,SEEK_SET);
    data = calloc(1,data_sectors*br.bytes_per_sector);
    if(data==NULL) {
        for(int j=0; j<br.number_of_fats; j++) free(fats[j]);
        fclose(f);
        free(fats);
        free(root);
        printf("Error: Can't allocate memory for data block\n");
        return 3;
    }

    ret = fread(data, data_sectors*br.bytes_per_sector, 1, f);
    if(ret!=1) {
        for(int j=0; j<br.number_of_fats; j++) free(fats[j]);
        fclose(f);
        free(fats);
        free(root);
        free(data);
        printf("Error: Can't read data\n");
        return 2;
    }

    fclose(f);
    return 0;
}

void command_prompt() {
    char buffer[256];
    entry_data_t *current = root;
    while(1) {
        printf("> ");
        scanf("%[^\n]s\n",buffer);
        flush_scan();
        if(!strcmp(buffer,"exit")) {
            prepare_for_exit();
            break;
        }
        if(!strncmp(buffer,"dir",3)) {
            if(buffer[3]=='\0') show_dir_content(current);
            else if(buffer[3]==' ') {
                entry_data_t *dir = find_entry(current,buffer+4);
                if(dir==NULL) {
                    printf("No directory named %s found.\n", buffer+4);
                    continue;
                }
                entry_data_t *fetched = fetch_dir(dir);
                if(fetched==NULL) {
                    printf("%s is not a directory.\n", buffer+4);
                    continue;
                }
                show_dir_content(fetched);                
            }
            else printf("What is a %s? A miserable pile of letters?\n", buffer);
        }
        else printf("What is a %s? A miserable pile of letters?\n", buffer);
    }
    
}

void prepare_for_exit() {
    if (root) free(root);
    if (fats) {
        for (int i=0; i<br.number_of_fats; i++) if(fats[i]) free(fats[i]);
        free(fats);
    }
    if (data) free(data);
}

void show_dir_content(entry_data_t *first_entry) {
    if(first_entry==NULL) return;
    entry_data_t *current = first_entry;
    filedate_t md;
    filetime_t mt;
    short indent;
    char formatted[13];
    short cluster;
    short next_cluster;
    short offset=0;
    int cluster_size;

    if(first_entry==root) {
        cluster=0;
        next_cluster=(short)0xFFFF;
        cluster_size=br.max_files_in_root*sizeof(entry_data_t);
    }
    else {
        entry_data_t *pwd = find_entry(current,".");
        if(pwd==NULL) return;
        cluster = pwd->low_order_address_bytes;
        next_cluster = get_fat_index(cluster,fats[0]);
        cluster_size = br.bytes_per_sector*br.sectors_per_cluster;
    }
    

    while(current->filename[0]!=FEI_UNALLOC) {
        if(hidden_in_dir(current,1)==0) {
            md=get_date(current->modified_date);
            mt=get_time(current->modified_time);
            printf("%02d/%02d/%04d %02d:%02d ",md.day,md.month,md.year,mt.hrs,mt.min);
            
            format_filename(current->filename,formatted);
            indent=13-strlen(formatted);
            printf("%s",formatted);
            for(int i=0; i<indent; i++) printf(" ");

            if(current->attributes==FAF_DIR) printf("<DIR>");
            else printf("%u B",current->file_size);
            printf("\n");           
        }

        offset+=sizeof(entry_data_t);
        if(offset>=cluster_size) {
            if(next_cluster>=(short)0x0002 && next_cluster<=(short)0xFFF6) {
                current=(entry_data_t*)(data+JMP_CLUSTER(next_cluster));
                cluster=next_cluster;
                next_cluster=get_fat_index(cluster,fats[0]);
                offset=0;
            }
            else {
                return;
            }
        }
        else current=(entry_data_t*)((char*)current+sizeof(entry_data_t));
    }
}

entry_data_t *fetch_dir(entry_data_t *dir) {
    if(dir==NULL) return root;
    if(dir->attributes!=FAF_DIR) return NULL;

    return (entry_data_t*)(data+JMP_CLUSTER(dir->low_order_address_bytes));
}

entry_data_t *find_entry(entry_data_t *pwd, const char *filename) {
    if(pwd==NULL || filename==NULL) return NULL;
    int cluster_size;
    if(pwd==root) cluster_size=br.max_files_in_root*sizeof(entry_data_t);
    else cluster_size=br.bytes_per_sector*br.sectors_per_cluster;
    short offset=0;
    entry_data_t *current = pwd;
    char formatted[13];
    while(current->filename[0]!=FEI_UNALLOC) {
        if(hidden_in_dir(current,0)==0) {
            format_filename(current->filename,formatted);
            if(!strcmp(formatted,filename)) return current;
        }
        offset+=sizeof(entry_data_t);
        if(offset>=cluster_size) break;
        current=(entry_data_t*)((char*)current+sizeof(entry_data_t));
    }
    return NULL;
}

int format_filename(const char *filename, char *dst) {
    if(filename==NULL) return -1;
    int pos=0;

    for (int i=0; i<8; i++) {
        if(filename[i]==' ') break;
        dst[pos]=filename[i];
        pos++;
    }

    if(filename[8]==' ') {
        dst[pos]='\0';
        return 0;
    }
    
    dst[pos]='.';
    pos++;
    for (int i=0; i<3; i++) {
        if(filename[8+i]==' ') break;
        dst[pos]=filename[8+i];
        pos++;
    }
    dst[pos]='\0';
    return 0;  
}

int hidden_in_dir(entry_data_t *entry, char hide_dots) {
    if(entry->filename[0]==FEI_DELETED) return 1;
    if(entry->attributes==FAF_LFN) return 1;
    if(hide_dots && *(entry->filename)=='.') return 1;
    return 0;
}

short get_fat_index(unsigned int index, const char* FAT) {
    short* fat_pointer = (short*)FAT;
    for (int i=0; i<index; i++) {
        fat_pointer+=1;
    }
    return *fat_pointer;
}

filedate_t get_date(short date) {
    filedate_t fd;
    fd.year=(date&DAT_YEAR)>>SHIFT_YEAR;
    fd.year+=1980;
    fd.month=(date&DAT_MONTH)>>SHIFT_MONTH;
    fd.day=(date&DAT_DAY);
    return fd;
}

filetime_t get_time(short time) {
    filetime_t ft;
    ft.hrs=(time&DAT_HOUR)>>SHIFT_HOUR;
    ft.min=(time&DAT_MIN)>>SHIFT_MIN;
    ft.sec=(time&DAT_SEC)*2;
    return ft;
}

void flush_scan() {
    char c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

void debug_read() {
    printf("Volume start: %d\n",LOC_VOLSTART);
    printf("Fat 1 start: %d\n",LOC_FAT1START);
    printf("Fat 2 start: %d\n",LOC_FAT2START);
    printf("Root start: %d\n",LOC_ROOTSTART);
    printf("Data start: %ld\n", LOC_DATASTART );
    for (int i=0; i<100; i++) printf("%02hhx ",data[i+JMP_CLUSTER(6)]);
}

int main() {

    load_disk("fat16.bin");

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

    entry_data_t *filep = root;

    printf("Filename: %.*s\n",11,filep->filename);
    printf("Attributes: 0x%02hhx\n",filep->attributes);
    printf("Creation time: 0x%02hhx\n",filep->creation_time_tenths);
    printf("File size: %u\n",filep->file_size);

    assert(memcmp(fats[0],fats[1],br.size_of_fat*br.bytes_per_sector)==0);

    show_dir_content(root);
    int n=5;
    printf("Cluster %d location: %ld\n", n, LOC_CLUSTER(n));
    printf("FAT index: %hu\n", get_fat_index(n,fats[0]));
    printf("%d",get_fat_index(n,fats[0])==(unsigned short)0xFFFF);
    printf("\n");
    command_prompt();
    return 0;
}

