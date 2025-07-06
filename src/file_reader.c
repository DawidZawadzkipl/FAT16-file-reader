#include "file_reader.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>


struct disk_t* disk_open_from_file(const char* volume_file_name){
    if(volume_file_name == NULL){
        errno = EFAULT;
        return NULL;
    }
    struct disk_t* disk = malloc(sizeof(struct disk_t));
    if(disk == NULL){
        errno = ENOMEM;
        return NULL;
    }
    disk->file = fopen(volume_file_name,"rb");
    if(disk->file==NULL){
        free(disk);
        errno = ENOENT;
        return NULL;
    }
    fseek(disk->file,0,SEEK_END);
    disk->size = ftell(disk->file);
    rewind(disk->file);

    return disk;
}

int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read){
    if(pdisk == NULL || first_sector < 0 || buffer == NULL || sectors_to_read <= 0 ){
        errno = EFAULT;
        return -1;
    }
    if((first_sector+sectors_to_read)*SECTOR_SIZE > (int32_t)pdisk->size){
        errno = ERANGE;
        return -1;
    }

    fseek(pdisk->file,first_sector*SECTOR_SIZE,SEEK_SET);
    int32_t res = fread(buffer,SECTOR_SIZE,sectors_to_read,pdisk->file);
    if(res != sectors_to_read){
        errno = ERANGE;
        return -1;
    }
    return sectors_to_read;
}
int disk_close(struct disk_t* pdisk){
    if(pdisk!=NULL){
        if(pdisk->file != NULL){
            fclose(pdisk->file);
            pdisk->file = NULL;
        }
        free(pdisk);
        pdisk = NULL;
        return 0;
    }
    errno = EFAULT;
    return -1;
}

struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector){
    if(pdisk == NULL ){
        errno = EFAULT;
        return NULL;
    }

    struct volume_t* vol = malloc(sizeof(struct volume_t));
    if(vol == NULL){
        errno = ENOMEM;
        return NULL;
    }
    vol->disk = pdisk;
    vol->first_sector = first_sector;

    if(disk_read(pdisk, (int32_t)first_sector, &vol->super_sector,1)!=1){
        free(vol);
        return NULL;
    }

    if(vol->super_sector.sectors_per_cluster == 0 || vol->super_sector.sectors_per_cluster > 128 ||
            (vol->super_sector.sectors_per_cluster & (vol->super_sector.sectors_per_cluster-1)) != 0){
        free(vol);
        errno = EINVAL;
        return NULL;
    }
    if(vol->super_sector.reserved_sectors == 0 || (vol->super_sector.fat_count != 1 && vol->super_sector.fat_count != 2)){
        free(vol);
        errno = EINVAL;
        return NULL;
    }
    if(vol->super_sector.root_dir_capacity * sizeof(struct fat_entry_t) % SECTOR_SIZE != 0){
        free(vol);
        errno = EINVAL;
        return NULL;
    }
    if((vol->super_sector.logical_sectors16 != 0 && vol->super_sector.logical_sectors32 != 0)||
            vol->super_sector.logical_sectors16 == 0 && vol->super_sector.logical_sectors32 == 0){
        free(vol);
        errno = EINVAL;
        return NULL;
    }
    if(vol->super_sector.logical_sectors16 == 0 && vol->super_sector.logical_sectors32 <= 65535){
        free(vol);
        errno = EINVAL;
        return NULL;
    }
    if(vol->super_sector.sectors_per_fat < 1 || vol->super_sector.magic != 0xAA55){
        free(vol);
        errno = EINVAL;
        return NULL;
    }


    vol->fat_size = vol->super_sector.sectors_per_fat * SECTOR_SIZE;

    vol->fat_table = malloc(vol->fat_size);
    if(vol->fat_table == NULL){
        free(vol);
        errno = ENOMEM;
        return NULL;
    }
    uint32_t fat_start = first_sector + vol->super_sector.reserved_sectors;
    if(disk_read(pdisk, (int32_t)fat_start, vol->fat_table, vol->super_sector.sectors_per_fat) == -1){
        fat_close(vol);
        return NULL;
    }
    if(vol->super_sector.fat_count == 2){
        uint8_t* fat_table_2 = malloc(vol->fat_size);
        if(fat_table_2 == NULL){
            fat_close(vol);
            errno = ENOMEM;
            return NULL;
        }
        if(disk_read(pdisk,(int32_t)(fat_start + vol->super_sector.sectors_per_fat),fat_table_2,vol->super_sector.sectors_per_fat) == -1){
            free(fat_table_2);
            fat_close(vol);
            return NULL;
        }
        if(memcmp(vol->fat_table,fat_table_2,vol->fat_size) != 0){
            free(fat_table_2);
            fat_close(vol);
            errno = EINVAL;
            return NULL;
        }
        free(fat_table_2);
    }
    vol->root_dir_sectors = ((vol->super_sector.root_dir_capacity * 32) + (SECTOR_SIZE - 1)) / SECTOR_SIZE;
    vol->first_data_sector = first_sector + vol->super_sector.reserved_sectors + (vol->super_sector.fat_count * vol->super_sector.sectors_per_fat) + vol->root_dir_sectors;
    if(vol->super_sector.logical_sectors16){
        vol->total_sectors = vol->super_sector.logical_sectors16;
    }
    else{
        vol->total_sectors = vol->super_sector.logical_sectors32;
    }
    vol->data_sectors = vol->total_sectors - (vol->first_data_sector - first_sector);
    vol->total_clusters = vol->data_sectors / vol->super_sector.sectors_per_cluster;
    return vol;
}

int fat_close(struct volume_t* pvolume){
    if(pvolume != NULL){
        if(pvolume->fat_table != NULL){
            free(pvolume->fat_table);
            pvolume->fat_table = NULL;
        }
        free(pvolume);
        pvolume = NULL;
        return 0;
    }
    errno = EFAULT;
    return -1;
}

struct file_t* file_open(struct volume_t* pvolume, const char* file_name){
    if(pvolume == NULL ){
        errno = EFAULT;
        return NULL;
    }
    if(file_name == NULL){
        errno = EISDIR;
        return NULL;
    }

    uint32_t root_size = pvolume->root_dir_sectors * SECTOR_SIZE;
    uint8_t *root_buffer = malloc(root_size);
    if (!root_buffer) {
        errno = ENOMEM;
        return NULL;
    }
    uint32_t root_start = pvolume->first_sector + pvolume->super_sector.reserved_sectors + pvolume->super_sector.fat_count * pvolume->super_sector.sectors_per_fat;

    if (disk_read(pvolume->disk, (int32_t)root_start, root_buffer, pvolume->root_dir_sectors) != (int)pvolume->root_dir_sectors) {
        free(root_buffer);
        return NULL;
    }

    struct fat_entry_t *entries = (struct fat_entry_t*)root_buffer;
    for (uint32_t i = 0; i < pvolume->super_sector.root_dir_capacity; i++) {
        if (entries[i].name[0] == 0x00){
            free(root_buffer);
            return NULL;
        }
        if(entries[i].name[0] == 0xE5 ){
            continue;
        }
        char entry_name[13];

        int j=0;
        int k=0;

        while(j<8 && entries[i].name[j] != ' '){
           entry_name[j] = (char)entries[i].name[j];
            j++;
        }
        if(entries[i].extension[0] != ' '){
            entry_name[j] = '.';
            j++;
            while(k<3 && entries[i].extension[k] != ' '){
                entry_name[j] = (char)entries[i].extension[k];
                j++;
                k++;
            }
        }
        entry_name[j] = '\0';
        if(strcmp(entry_name, file_name) == 0){
            if(entries[i].attr & 0x10 || entries[i].attr & 0x08){
                errno = EISDIR;
                free(root_buffer);
                return NULL;
            }
            struct file_t* f = malloc(sizeof(struct file_t));
            if(f == NULL){
                errno = ENOMEM;
                free(root_buffer);
                return NULL;
            }
            f->volume = pvolume;
            f->entry = entries[i];
            f->position = 0;
            //walidacja i zwoleniania
            f->chain = get_chain_fat16(pvolume->fat_table,pvolume->fat_size,f->entry.first_cluster_y);
            free(root_buffer);
            return f;
        }

    }
    free(root_buffer);
    errno = ENOENT;
    return NULL;
}


int file_close(struct file_t* stream){
    if(stream != NULL){
        if(stream->chain != NULL){
            if(stream->chain->clusters != NULL){
                free(stream->chain->clusters);
            }
            free(stream->chain);
        }
        free(stream);
        return 0;
    }
    errno = EFAULT;
    return -1;
}

size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream){
    if(ptr == NULL || stream == NULL){
        errno = EFAULT;
        return -1;
    }
    if(size == 0 || nmemb == 0){
        return 0;
    }
    if(stream->chain == NULL || stream->chain->size == 0){
        return 0;
    }
    uint32_t offset = 0;
    size_t read_total = size * nmemb;
    size_t bytes_in_file = stream->entry.size - stream->position;
    if(bytes_in_file == 0){
        return 0;
    }
    if(read_total > bytes_in_file){
        read_total = bytes_in_file;
    }
    while(offset < read_total) {

        uint32_t cluster_index = stream->position / (stream->volume->super_sector.sectors_per_cluster * SECTOR_SIZE);
        if(cluster_index >= stream->chain->size) break;
        uint32_t cluster_offset = stream->position % (stream->volume->super_sector.sectors_per_cluster * SECTOR_SIZE);

        uint16_t current_cluster = *(stream->chain->clusters + cluster_index);
        uint32_t first_sector_in_cluster = stream->volume->first_data_sector + ((current_cluster - 2) * stream->volume->super_sector.sectors_per_cluster);

        uint32_t sector_in_cluster = cluster_offset / SECTOR_SIZE;
        uint32_t sector_offset = cluster_offset % SECTOR_SIZE;

        uint8_t sector_buffer[SECTOR_SIZE];
        if(disk_read(stream->volume->disk,(int32_t)first_sector_in_cluster + sector_in_cluster, sector_buffer, 1) !=1){
            if(offset == 0){
                errno = ERANGE;
                return -1;
            }
            break;
        }
        uint32_t bytes_left_in_sector = SECTOR_SIZE - sector_offset;
        size_t num = read_total - offset;
        if(num > bytes_left_in_sector){
            num = bytes_left_in_sector;
        }
        memcpy((uint8_t*)ptr + offset,sector_buffer + sector_offset, num);
        offset += num;
        stream->position += num;
    }
    return offset/size;
}

int32_t file_seek(struct file_t* stream, int32_t offset, int whence){
    if(stream == NULL || stream->volume == NULL){
        errno = EFAULT;
        return -1;
    }
    int32_t pos;
    if(whence == SEEK_SET){
        pos = offset;
    }
    else if(whence == SEEK_END){
        pos = (int32_t)stream->entry.size + offset;

    }
    else if(whence == SEEK_CUR){
        pos = (int32_t)stream->position + offset;
    }
    else{
        errno = EINVAL;
        return -1;
    }
    if(pos < 0 || pos > (int32_t)stream->entry.size){
        errno = ENXIO;
        return -1;
    }
    stream->position = pos;
    return pos;
}

struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path){
    if(pvolume == NULL || dir_path == NULL){
        errno = EFAULT;
        return NULL;
    }

    if(dir_path[0] != '\\'){
        errno = ENOENT;
        return NULL;
    }
    struct dir_t* dir = malloc(sizeof(struct dir_t));
    if(dir == NULL){
        errno = ENOMEM;
        return NULL;
    }
    dir->volume = pvolume;
    dir->current_entry = 0;
    dir->current_sector = pvolume->first_sector + pvolume->super_sector.reserved_sectors + (pvolume->super_sector.fat_count * pvolume->super_sector.sectors_per_fat);
    dir->max_entries = pvolume->super_sector.root_dir_capacity;
    return dir;
}

int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry){
    if(pdir == NULL || pentry == NULL){
        errno = EFAULT;
        return -1;
    }

    uint8_t sector_buffer[SECTOR_SIZE];
    struct fat_entry_t* entries = (struct fat_entry_t*)sector_buffer;

    while(pdir->current_entry < pdir->max_entries){
        uint32_t sector_index = pdir->current_entry / (SECTOR_SIZE / sizeof(struct fat_entry_t));
        uint32_t sector_entry = pdir->current_entry % (SECTOR_SIZE / sizeof(struct fat_entry_t));

        if(sector_entry == 0){
            if(sector_index >= pdir->volume->root_dir_sectors){
                errno = ENXIO;
                return -1;
            }

            if(disk_read(pdir->volume->disk, pdir->current_sector + sector_index, sector_buffer, 1) !=1 ){
                errno = EIO;
                return -1;
            }
        }

        struct fat_entry_t* entry = entries + sector_entry;
        pdir->current_entry++;
        if(*entry->name == 0x00){
            return 1;
        }
        if(*entry->name == 0xE5 || entry->attr & 0x08 || entry->attr == 0x0F){
            continue;
        }
        int i=0;
        int j=0;

        while(i<8 && entry->name[i] != ' '){
            *(pentry->name+i) = (char)entry->name[i];
            i++;
        }
        if(entry->extension[0] != ' '){
            *(pentry->name+i) = '.';
            i++;
            while(j<3 && entry->extension[j] != ' '){
                *(pentry->name+i) = (char)entry->extension[j];
                j++;
                i++;
            }
        }
        *(pentry->name+i) = '\0';
        pentry->size = entry->size;
        //atrybuty
        pentry->is_archived = 0;
        if(entry->attr & 0x20) pentry->is_archived = 1;
        pentry->is_readonly = 0;
        if(entry->attr & 0x01) pentry->is_readonly = 1;
        pentry->is_system = 0;
        if(entry->attr & 0x04) pentry->is_system = 1;
        pentry->is_hidden = 0;
        if(entry->attr & 0x02) pentry->is_hidden = 1;
        pentry->is_directory = 0;
        if(entry->attr & 0x10) pentry->is_directory = 1;
        return 0;
    }
    return 1;
}

int dir_close(struct dir_t* pdir){
    if(pdir != NULL){
        free(pdir);
        pdir = NULL;
        return 0;
    }
    errno = EFAULT;
    return -1;
}


struct clusters_chain_t *get_chain_fat16(const void * const buffer, size_t size, uint16_t first_cluster){
    if(buffer == NULL || first_cluster < 2 || size%2 != 0 || size < 4 ){
        return NULL;
    }
    uint32_t offset = first_cluster * 2;
    if(offset + 2 > size){
        return NULL;
    }

    uint16_t current = *(uint16_t*)((uint8_t*)buffer + offset);
    uint16_t count = 1;
    while(1){
        if(offset + 2 > size){
            return NULL;
        }
        if(current == FAT16_BAD_CLUSTER || current == FAT16_FREE_CLUSTER || current == 0x0001){
            return NULL;
        }
        if(current >= FAT16_EOC_MIN ){
            break;
        }
        offset = current * 2;
        current = *(uint16_t*)((uint8_t*)buffer + offset);
        count++;
    }
    struct clusters_chain_t* cluster_chain = malloc(sizeof(struct clusters_chain_t));
    if(!cluster_chain){
        return NULL;
    }
    cluster_chain->clusters = malloc(count*sizeof(uint16_t));
    if(cluster_chain->clusters == NULL){
        free(cluster_chain);
        return NULL;
    }
    cluster_chain->size = count;
    uint16_t curr = first_cluster;
    for (uint16_t i = 0; i < count; ++i) {
        *(cluster_chain->clusters + i) = curr;
        if(i < count - 1){
            offset = curr * 2;
            curr = *(uint16_t*)((uint8_t*)buffer + offset);
        }
    }
    return cluster_chain;
}
