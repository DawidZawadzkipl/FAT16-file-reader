//
// Created by dawid on 23.06.2025.
//

#ifndef PROJEKTFAT_FILE_READER_H
#define PROJEKTFAT_FILE_READER_H

#include <stdint.h>
#include <stdio.h>

#define SECTOR_SIZE 512
#define FAT16_EOC_MIN 0xFFF8
#define FAT16_BAD_CLUSTER 0xFFF7
#define FAT16_FREE_CLUSTER 0x0000

struct fat_super_t {
    uint8_t __jump_code[3];
    char oem_name[8];

    //
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_dir_capacity;
    uint16_t logical_sectors16;
    uint8_t __reserved;
    uint16_t sectors_per_fat;

    uint32_t __reserved2;

    uint32_t hidden_sectors;
    uint32_t logical_sectors32;

    uint16_t __reserved3;
    uint8_t __reserved4;

    uint32_t serial_number;

    char label[11];
    char fsid[8];

    uint8_t __boot_code[448];
    uint16_t magic; // 55 aa
} __attribute__(( packed ));

struct fat_entry_t{
    uint8_t name[8];        //0-7
    uint8_t extension[3];   //8-10
    uint8_t attr;           //11
    uint8_t reserved;       //12
    uint8_t time_ms;        //13
    uint16_t time;          //14-15
    uint16_t date;          //16-17
    uint16_t last_access_date;//18-19
    uint16_t first_cluster_o;    //20-21  reserved for fat12 & fat16
    uint16_t last_mod_time; //22-23
    uint16_t last_mod_date; //24-25
    uint16_t first_cluster_y;    //26-27 younger word, little endian
    uint32_t size;          //28-31
}__attribute__((packed));

struct dir_entry_t{
    char name[13];
    uint32_t size;
    uint8_t is_archived;
    uint8_t is_readonly;
    uint8_t is_system;
    uint8_t is_hidden;
    uint8_t is_directory;
};

struct clusters_chain_t {
    uint16_t *clusters;
    size_t size;
};

struct disk_t {
    FILE *file;
    size_t size;
};
struct disk_t* disk_open_from_file(const char* volume_file_name);
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read);
int disk_close(struct disk_t* pdisk);

struct volume_t {
    struct disk_t *disk;
    uint32_t first_sector;
    struct fat_super_t super_sector;
    uint8_t *fat_table;
    uint32_t fat_size;
    uint32_t root_dir_sectors;
    uint32_t first_data_sector;
    uint32_t total_sectors;
    uint32_t data_sectors;
    uint32_t total_clusters;
};
struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector);
int fat_close(struct volume_t* pvolume);

struct file_t {
    struct volume_t *volume;
    struct fat_entry_t entry;
    uint32_t position;
    struct clusters_chain_t* chain;
};
struct file_t* file_open(struct volume_t* pvolume, const char* file_name);
int file_close(struct file_t* stream);
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream);
int32_t file_seek(struct file_t* stream, int32_t offset, int whence);

struct dir_t {
    struct volume_t *volume;
    uint32_t current_sector;
    uint32_t current_entry;
    uint32_t max_entries;
};
struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path);
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry);
int dir_close(struct dir_t* pdir);

struct clusters_chain_t *get_chain_fat16(const void * const buffer, size_t size, uint16_t first_cluster);
#endif //PROJEKTFAT_FILE_READER_H
