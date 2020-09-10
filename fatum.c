#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "fatum.h"

#define DEBUG 1

boot_t br;
char **fats;
entry_data_t *root;
char *data;

char filename[256];
history_t history;

size_t readblock(void* buffer, uint32_t first_block, size_t block_count) {
    if(buffer==NULL || filename==NULL) {
        return 0;
    }
    FILE *f = fopen(filename, "rb");
    if (f==NULL) {
        return 0;
    }
    fseek(f,first_block*512,SEEK_SET);
    int ret = fread(buffer,512,block_count,f);
    fclose(f);
    if(ret!=block_count) {
        return 0;
    }
    return block_count;
}

int load_disk() {
    if (filename==NULL) {
        printf("Error: No filename\n");
        return 1;
    }
    
    int ret = readblock(&br,LOC_VOLSTART,1);
    if(!ret) {
        printf("Reading boot record failed\n");
        return 2;
    }
    fats = malloc(sizeof(char*)*br.number_of_fats);
    if(fats==NULL) {
        printf("Error: Can't allocate memory for FATs\n");
        return 3;
    }

    for (int i=0; i<br.number_of_fats; i++) {
        fats[i]=calloc(1,br.size_of_fat*br.bytes_per_sector);
        if(fats[i]==NULL) {
            for(int j=0; j<i; j++) free(fats[j]);
            free(fats);
            printf("Error: Can't allocate memory for FAT %d\n",i);
            return 3;
        }
    }

    for (int i=0; i<br.number_of_fats; i++) {
        ret = readblock(fats[i], LOC_FAT1START+((br.size_of_fat*br.bytes_per_sector)/512*i), (br.size_of_fat*br.bytes_per_sector)/512);
        if(!ret) {
            for(int j=0; j<br.number_of_fats; j++) free(fats[j]);
            free(fats);
            printf("Error: Can't read FAT %d data\n",i);
            return 2;
        }
    }

    root = calloc(1,sizeof(entry_data_t)*br.max_files_in_root);
    if(root==NULL) {
        for(int j=0; j<br.number_of_fats; j++) free(fats[j]);
        free(fats);
        printf("Error: Can't allocate memory for root dir\n");
        return 3;
    }

    ret = readblock(root, LOC_ROOTSTART, (sizeof(entry_data_t)*br.max_files_in_root)/512);
    if(!ret) {
        for(int j=0; j<br.number_of_fats; j++) free(fats[j]);
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
    
    data = calloc(1,data_sectors*br.bytes_per_sector);
    if(data==NULL) {
        for(int j=0; j<br.number_of_fats; j++) free(fats[j]);
        free(fats);
        free(root);
        printf("Error: Can't allocate memory for data block\n");
        return 3;
    }

    ret = readblock(data, LOC_DATASTART, (data_sectors*br.bytes_per_sector)/512);
    if(!ret) {
        for(int j=0; j<br.number_of_fats; j++) free(fats[j]);
        free(fats);
        free(root);
        free(data);
        printf("Error: Can't read data\n");
        return 2;
    }

    return 0;
}

void command_prompt() {
    printf("Fatum v0.000001\n");
    char buffer[256]="";
    char fn[13];
    entry_data_t *current = root;
    while(1) {
        if(history.size==0) printf("\\");
        else printf("%s",history.dirs[history.size-1]);
        printf("> ");
        scanf("%[^\n]s\n",buffer);
        flush_scan();
        if(!strcmp(buffer,"exit")) {
            prepare_for_exit();
            break;
        }
        else if(!strncmp(buffer,"dir",3)) {
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
        else if(!strncmp(buffer,"cd",2)) {
            if(buffer[2]=='\0') {
                current=root;
                if (history.dirs) {
                    while(history.size>0) {
                        if(history.dirs[history.size-1]) free(history.dirs[history.size-1]);
                        history.size--;
                    }
                }
            }
            else if(buffer[2]==' ') {
                if(buffer[3]=='.' && buffer[4]=='\0') continue;
                entry_data_t *dir = find_entry(current,buffer+3);
                if(dir==NULL) {
                    printf("No directory named %s found.\n", buffer+3);
                    continue;
                }
                strcpy(fn,dir->filename);
                format_filename(fn,fn);
                entry_data_t *fetched = fetch_dir(dir);
                if(fetched==NULL) {
                    printf("%s is not a directory.\n", buffer+3);
                    continue;
                }
                current = fetched;
                if(!strcmp(buffer+3,"..")) {
                    history.size--;
                    free(history.dirs[history.size]);
                    if(history.size==0) {
                        free(history.dirs);
                        history.dirs=NULL;
                    }
                    else {
                        history.dirs=realloc(history.dirs,sizeof(char*)*history.size);
                        if(history.dirs==NULL) {
                            printf("Error: allocation error\n");
                            break;
                    }
                    }
                }
                else {
                    history.size++;
                    history.dirs=realloc(history.dirs,sizeof(char*)*history.size);
                    if(history.dirs==NULL) {
                        printf("Error: allocation error\n");
                        break;
                    }
                    history.dirs[history.size-1]=malloc(sizeof(char)*9);
                    if(history.dirs[history.size-1]==NULL) {
                        printf("Error: allocation error\n");
                        break;
                    }
                    strcpy(history.dirs[history.size-1],fn);
                }
            }
            else printf("What is a %s? A miserable pile of letters?\n", buffer);
        }
        else if (!strcmp(buffer,"pwd")) print_current_dir();
        else if (!strncmp(buffer,"cat",3)) {
            if (buffer[3]=='\0') {
                printf("No filename\n");
                continue;
            }
            if (buffer[3]==' ') {
                entry_data_t *f = find_entry(current,buffer+4);
                if(f==NULL) {
                    printf("No file named %s found.\n",buffer+4);
                    continue;
                }
                int status = print_file_contents(f);
                if(status==-1 || status==-2) {
                    printf("Wrong filename.\n");
                    continue;
                }
                if(status==-3) {
                    printf("%s is a directory.\n",buffer+4);
                    continue;
                }
            }
            else printf("What is a %s? A miserable pile of letters?\n", buffer);
        }
        else if (!strncmp(buffer,"get",3)) {
            if (buffer[3]=='\0') {
                printf("No filename\n");
                continue;
            }
            if (buffer[3]==' ') {
                entry_data_t *f = find_entry(current,buffer+4);
                if(f==NULL) {
                    printf("No file named %s found.\n",buffer+4);
                    continue;
                }
                int status = get_file_contents(f);
                if(status==-1 || status==-2) {
                    printf("Wrong filename.\n");
                    continue;
                }
                if(status==-3) {
                    printf("%s is a directory.\n",buffer+4);
                    continue;
                }
            }
            else printf("What is a %s? A miserable pile of letters?\n", buffer);
        }
        else if (!strncmp(buffer,"zip",3)) {
            if (buffer[3]=='\0') {
                printf("No filename\n");
                continue;
            }
            if (buffer[3]==' ') {
                char *pos = buffer+4;
                entry_data_t *f[3];
                char *next;
                for (int i=0; i<2; i++) {
                    next = strchr(pos,' ');
                    if (next==NULL) {
                        break;
                    }
                    *next='\0';
                    f[i] = find_entry(current,pos);
                    if(f==NULL) {
                        printf("No file named %s found.\n",pos);
                        break;
                    }
                    pos=next+1;   
                }
                next = strchr(pos,' ');
                if(next)*next='\0';
                int status = zip_file_contents(f[0],f[1],pos);
                if(status==-1) printf("zip needs: 2 input files and 1 output file\n");
                else if(status==-2) printf("Wrong filenames\n");
                else if(status==-3) printf("zip doesn't accept directories\n");
                else if(status==-5) printf("Can't open file\n");
            }
            else printf("What is a %s? A miserable pile of letters?\n", buffer);
        }
        else if (!strcmp(buffer,"rootinfo")) {
            print_root_info();
        }
        else if (!strcmp(buffer,"spaceinfo")) {
            print_space_info();
        }
        else if (!strncmp(buffer,"fileinfo",8)) {
            if (buffer[8]=='\0') {
                printf("No filename\n");
                continue;
            }
            if (buffer[8]==' ') {
                entry_data_t *f = find_entry(current,buffer+9);
                if(f==NULL) {
                    printf("No file named %s found.\n",buffer+9);
                    continue;
                }
                print_file_info(f);
            }
            else printf("What is a %s? A miserable pile of letters?\n", buffer);
        }
        else if (!strcmp(buffer,"help")) {
            printf("dir - shows current directory's contents. You can also give a dir name to show its contents.\n");
            printf("     syntax: dir [directory-name]\n");
            printf("cd - changes current directory.\n");
            printf("     syntax: cd directory-name\n");
            printf("pwd - displays current directory's full path.\n");
            printf("cat - shows file's contents.\n");
            printf("     syntax: cat file-name\n");
            printf("get - saves file to the host's folder.\n");
            printf("     syntax: get file-name\n");
            printf("zip - gets 2 files and mixes its contents to a new file.\n");
            printf("     syntax: zip file1-name file2-name output-file-name\n");
            printf("rootinfo - prints root directory info.\n");
            printf("spaceinfo - prints volume information.\n");
            printf("fileinfo - prints file details.\n");
            printf("     syntax: fileinfo file-name\n");
            printf("help - prints this very useful guide\n");
        }
        else if (!strcmp(buffer,"version")) {
            printf("v0.000001\n");
        }
        else if (!strcmp(buffer,"")) continue;
        else printf("What is a %s? A miserable pile of letters?\n", buffer);
    }
    
}

void prepare_for_exit() {
    if (root) free(root);
    if (fats) {
        for (int i=0; i<br.number_of_fats; i++) if(fats[i]) free(fats[i]);
        free(fats);
    }
    if (history.dirs) {
        while(history.size>0) {
            if(history.dirs[history.size-1]) free(history.dirs[history.size-1]);
            history.size--;
        }
        free(history.dirs);
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
    unsigned short cluster;
    unsigned short next_cluster;
    short offset=0;
    int cluster_size;

    if(first_entry==root) {
        cluster=0;
        next_cluster=(unsigned short)0xFFFF;
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
            if(next_cluster>=(unsigned short)0x0002 && next_cluster<=(unsigned short)0xFFF6) {
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
    if(dir->low_order_address_bytes==(unsigned short)0x00) return root;
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
    char fn[13];
    for (int i=0; i<12; i++) fn[i]=filename[i];
    fn[12]='\0';
    for (int i=0; i<strlen(fn); i++) fn[i]=tolower(fn[i]);
    while(current->filename[0]!=FEI_UNALLOC) {
        if(hidden_in_dir(current,0)==0) {
            format_filename(current->filename,formatted);
            for (int i=0; i<strlen(formatted); i++) formatted[i]=tolower(formatted[i]);
            if(!strcmp(formatted,fn)) return current;
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

unsigned short get_fat_index(unsigned int index, const char* FAT) {
    unsigned short* fat_pointer = (unsigned short*)FAT;
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

void print_current_dir() {
    printf("Current working directory: \\");
    for (int i=0; i<history.size; i++) printf("%s\\",history.dirs[i]);
    printf("\n");
}

void flush_scan() {
    char c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

int print_file_contents(entry_data_t *file) {
    if(file==NULL) return -1;
    if(file->filename[0]==FEI_UNALLOC || file->filename[0]==FEI_DELETED) return -2;
    if(file->attributes & FAF_DIR) return -3;
    unsigned short current = file->low_order_address_bytes;
    if(!(current>=(unsigned short)0x0002 && current<=(unsigned short)0xFFF6)) return 0;
    uint32_t wait=file->file_size;
    uint32_t cluster_size=br.bytes_per_sector*br.sectors_per_cluster;
    char *p;
    while(1) {
        if(current>=(unsigned short)0x0002 && current<=(unsigned short)0xFFF6) {
            p=data+JMP_CLUSTER(current);
            if(wait>cluster_size) {
                for(int i=0; i<cluster_size; i++) printf("%c",*(p+i));
                wait-=cluster_size;
            }
            else for(int i=0; i<wait; i++) printf("%c",*(p+i));
        }
        else if(current==(unsigned short)0xFFF7) {
            printf("\nCluster corrupted\n");
            return -4;
        }
        else if(current>=(unsigned short)0xFFF8) break;
        current=get_fat_index(current,fats[0]);
    }
    return 0;
}

int get_file_contents(entry_data_t *file) {
    if(file==NULL) return -1;
    if(file->filename[0]==FEI_UNALLOC || file->filename[0]==FEI_DELETED) return -2;
    if(file->attributes==FAF_DIR) return -3;
    unsigned short current = file->low_order_address_bytes;
    if(!(current>=(unsigned short)0x0002 && current<=(unsigned short)0xFFF6)) return 0;
    FILE *f;
    char formatted[13];
    format_filename(file->filename,formatted);
    f = fopen(formatted,"wb");
    if(f==NULL) {
        return -5;
    }
    uint32_t wait=file->file_size;
    uint32_t cluster_size=br.bytes_per_sector*br.sectors_per_cluster;
    char *p;
    while(1) {
        if(current>=(unsigned short)0x0002 && current<=(unsigned short)0xFFF6) {
            p=data+JMP_CLUSTER(current);
            if(wait>cluster_size) {
                fwrite(p,sizeof(char),cluster_size,f);
                wait-=cluster_size;
            }
            else fwrite(p,sizeof(char),wait,f);
        }
        else if(current==(unsigned short)0xFFF7) {
            printf("\nCluster corrupted\n");
            fclose(f);
            return -4;
        }
        else if(current>=(unsigned short)0xFFF8) break;
        current=get_fat_index(current,fats[0]);
    }
    fclose(f);
    return 0;
}

int zip_file_contents(entry_data_t *file1, entry_data_t *file2, const char *outfile) {
    if(file1==NULL || file2==NULL || outfile==NULL) return -1;
    if(file1->filename[0]==FEI_UNALLOC || file1->filename[0]==FEI_DELETED || file2->filename[0]==FEI_UNALLOC || file2->filename[0]==FEI_DELETED) return -2;
    if(file1->attributes==FAF_DIR || file2->attributes==FAF_DIR) return -3;
    unsigned short current[2];
    current[0]=file1->low_order_address_bytes;
    current[1]=file2->low_order_address_bytes;
    if(!(current[0]>=(unsigned short)0x0002 && current[0]<=(unsigned short)0xFFF6)) return 0;
    if(!(current[1]>=(unsigned short)0x0002 && current[1]<=(unsigned short)0xFFF6)) return 0;
    FILE *f;
    f = fopen(outfile,"wb");
    if(f==NULL) return -5;
    uint32_t wait[2];
    wait[0]=file1->file_size;
    wait[1]=file2->file_size;
    uint32_t cluster_wait[2];
    uint32_t cluster_size=br.bytes_per_sector*br.sectors_per_cluster;
    char *pos[2]={NULL};
    char *next[2];
    size_t len;
    char skip[2]={0};
    while(1) {
        for (int i=0; i<2; i++) {
            if(wait[i]>0 && !skip[i]) {
                if(current[i]>=(unsigned short)0x0002 && current[i]<=(unsigned short)0xFFF6) {
                    if(pos[i]==NULL) {
                        pos[i]=data+JMP_CLUSTER(current[i]);
                        cluster_wait[i]=cluster_size;
                    }
                    next[i]=strchr(pos[i],'\n');
                    if(next[i]) {
                        *next[i]='\0';
                        len=strlen(pos[i]);
                    }
                    if(next[i]==NULL || len>cluster_wait[i]) {
                        if(next[i]) *next[i]='\n';
                        if(wait[i]>cluster_wait[i]) {
                            fwrite(pos[i],sizeof(char),cluster_wait[i],f);
                            wait[i]-=cluster_wait[i];
                        }
                        else {
                            fwrite(pos[i],sizeof(char),wait[i],f);
                            wait[i]=0;
                        }
                        current[i]=get_fat_index(current[i],fats[0]);
                        pos[i]=NULL;
                        if(i==0) skip[1]=1;
                        else skip[0]=1;
                    }
                    else {
                        fwrite(pos[i],sizeof(char),len,f);
                        fwrite("\n",sizeof(char),1,f);
                        wait[i]-=(len+1);
                        cluster_wait[i]-=(len+1);
                        pos[i]=next[i]+1;
                        *next[i]='\n';
                        if(i==0) skip[1]=0;
                        else skip[0]=0;
                    }
                }
                else if(current[i]==(unsigned short)0xFFF7) {
                    printf("\nCluster corrupted\n");
                    fclose(f);
                    return -4;
                }
                else if(current[i]>=(unsigned short)0xFFF8) wait[i]=0; 
            }
        }
        if(wait[0]<=0 && wait[1]<=0) break;
    }
    fclose(f);
    return 0;
}

void print_root_info() {
    entry_data_t *current = root;
    int entries=0;
    while(current->filename[0]!=FEI_UNALLOC) {
        if(hidden_in_dir(current,1)==0) {
            entries++;
        }
        current=(entry_data_t*)((char*)current+sizeof(entry_data_t));
    }
    printf("Entries in the root directory: %d\n", entries);
    printf("Max entries: %hu\n",br.max_files_in_root);
    printf("Used percentage: %d%%\n",(entries/br.max_files_in_root)*100);
}

void print_space_info() {
    uint32_t fat_entries=(br.size_of_fat*br.bytes_per_sector)/4;
    unsigned short *entry=(unsigned short *)fats[0];

    int used_entries=0;
    int free_entries=0;
    int crpt_entries=0;
    int last_entries=0;

    for (int i=0; i<fat_entries; i++) {
        if(*entry>=(unsigned short)0x0002 && *entry<=(unsigned short)0xFFF6) used_entries++;
        else if (*entry==(unsigned short)0x0000) free_entries++;
        else if (*entry==(unsigned short)0xFFF7) crpt_entries++;
        else if (*entry>=(unsigned short)0xFFF8) last_entries++;
        entry++;
    }

    printf("Used clusters: %d\n",used_entries);
    printf("Free clusters: %d\n",free_entries);
    printf("Corrupted clusters: %d\n",crpt_entries);
    printf("Ending clusters: %d\n",last_entries);
    printf("Cluster size in bytes: %d\n",br.sectors_per_cluster*br.bytes_per_sector);
    printf("Cluster size in sectors: %d\n",br.sectors_per_cluster);
}

void print_file_info(entry_data_t *f) {
    if(f==NULL) return;
    printf("File path: \\");
    for (int i=0; i<history.size; i++) printf("%s\\",history.dirs[i]);
    char formatted[13];
    format_filename(f->filename,formatted);
    printf("%s\n",formatted);
    filedate_t cd = get_date(f->creation_date);
    filedate_t ad = get_date(f->access_date);
    filedate_t md = get_date(f->modified_date);
    filetime_t ct = get_time(f->creation_time);
    filetime_t mt = get_time(f->modified_time);
    
    printf("Attributes: ");
    (f->attributes & FAF_ARCHIVE)?printf("A+ "):printf("A- ");
    (f->attributes & FAF_READ_ONLY)?printf("R+ "):printf("R- ");
    (f->attributes & FAF_SYSTEM_FILE)?printf("S+ "):printf("S- ");
    (f->attributes & FAF_HIDDEN_FILE)?printf("H+ "):printf("H- ");
    (f->attributes & FAF_DIR)?printf("D+ "):printf("D- ");
    (f->attributes & FAF_VOL_LABEL)?printf("V+ "):printf("V- ");

    printf("\nFile size: %u\n",f->file_size);
    printf("Last modified: %02d/%02d/%04d %02d:%02d\n",md.day,md.month,md.year,mt.hrs,mt.min);
    printf("Last access: %02d/%02d/%04d\n",ad.day,ad.month,ad.year);
    printf("Created: %02d/%02d/%04d %02d:%02d\n",cd.day,cd.month,cd.year,ct.hrs,ct.min);
    printf("Clusters chain: ");
    unsigned short cluster = f->low_order_address_bytes;
    int clusters=0;
    while(cluster>=(unsigned short)0x0002 && cluster<=(unsigned short)0xFFF6) {
        printf("%hu ",cluster);
        cluster=get_fat_index(cluster,fats[0]);
        clusters++;
    }
    printf("\nClusters count: %d\n",clusters);
}

int main() {

    strcpy(filename,"fat16.bin");
    int ret = load_disk();
    if(ret) return ret;
    history.dirs=NULL;
    history.size=0;
    if(memcmp(fats[0],fats[1],br.size_of_fat*br.bytes_per_sector)) {
        prepare_for_exit();
        return -1;
    }

    command_prompt();
    return 0;
}

