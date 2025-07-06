#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file_reader.h"

// MBR partition table entry structure
struct partition_entry {
    uint8_t  status;        // Bootable flag (0x80 = active, 0x00 = inactive)
    uint8_t  chs_first[3];  // CHS address of first sector (legacy)
    uint8_t  type;          // Partition type (0x04/0x06 = FAT16)
    uint8_t  chs_last[3];   // CHS address of last sector (legacy)
    uint32_t lba_first;     // LBA of first sector
    uint32_t size;          // Size in sectors
} __attribute__((packed));

// Auto-detect FAT16 partition
uint32_t find_fat16_partition(struct disk_t* disk) {
    uint8_t mbr[512];
    if (disk_read(disk, 0, mbr, 1) != 1) return 0;

    // Check partition table using struct
    struct partition_entry* partitions = (struct partition_entry*)(mbr + 446);
    for (int i = 0; i < 4; i++) {
        struct partition_entry* part = &partitions[i];
        if (part->type == 0x04 || part->type == 0x06) {  // FAT16
            return part->lba_first;   // Start sector
        }
    }
    return 0;  // Default fallback for raw filesystems
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <fat16_image>\n", argv[0]);
        return 1;
    }

    struct disk_t* disk = disk_open_from_file(argv[1]);
    if (!disk) {
        printf("Failed to open disk image\n");
        return 1;
    }

    // Auto-detect partition offset
    uint32_t offset = find_fat16_partition(disk);
    struct volume_t* volume = fat_open(disk, offset);
    if (!volume) {
        printf("Failed to open FAT16 volume\n");
        disk_close(disk);
        return 1;
    }

    printf("FAT16 Reader Demo\n");
    printf("=================\n");

    // Directory listing with first file detection
    printf("Root directory contents:\n");
    char first_file[13] = "";

    struct dir_t* dir = dir_open(volume, "\\");
    if (dir) {
        struct dir_entry_t entry;
        while (dir_read(dir, &entry) == 0) {
            printf("  %-12s %8u bytes", entry.name, entry.size);
            if (entry.is_directory) printf(" [DIR]");
            if (entry.is_readonly) printf(" [RO]");
            if (entry.is_hidden) printf(" [HIDDEN]");
            printf("\n");

            // Save first non-directory file for reading
            if (!entry.is_directory && strlen(first_file) == 0 && entry.size > 0) {
                strcpy(first_file, entry.name);
            }
        }
        dir_close(dir);
    } else {
        printf("Failed to open root directory\n");
    }

    // File reading demonstration
    if (strlen(first_file) > 0) {
        printf("\n\tReading file: %s\n", first_file);

        struct file_t* file = file_open(volume, first_file);
        if (file) {
            printf("File size: %u bytes\n", file->entry.size);

            // Determine how much to read (max 1024 bytes for demo)
            size_t max_read = 1024;
            size_t read_size = file->entry.size > max_read ? max_read : file->entry.size;

            char* buffer = malloc(read_size + 1);
            if (buffer) {
                size_t bytes_read = file_read(buffer, 1, read_size, file);
                buffer[bytes_read] = '\0';

                printf("\nFile content:\n\n");

                // Print content, handling non-printable characters
                for (size_t i = 0; i < bytes_read; i++) {
                    char c = buffer[i];
                    if (c >= 32 && c <= 126) {
                        // Printable ASCII
                        putchar(c);
                    } else if (c == '\n' || c == '\r' || c == '\t') {
                        putchar(c);
                    } else {
                        putchar('.');
                    }
                }

                if (file->entry.size > max_read) {
                    printf("\n... (file truncated, showing first %zu bytes)\n\n\n", max_read);
                }

                printf("\nTesting seek operations:\n");
                int32_t pos = file_seek(file, 0, SEEK_END);
                printf("SEEK_END: position = %d\n", pos);

                pos = file_seek(file, 0, SEEK_SET);
                printf("SEEK_SET(0): position = %d\n", pos);

                if (file->entry.size > 10) {
                    pos = file_seek(file, 10, SEEK_SET);
                    printf("SEEK_SET(10): position = %d\n", pos);
                }

                free(buffer);
            } else {
                printf("Failed to allocate memory for file content\n");
            }

            file_close(file);
            printf("File operations completed successfully\n");
        } else {
            printf("Failed to open file '%s'\n", first_file);
        }
    } else {
        printf("\nNo readable files found in root directory\n");
    }

    // Cleanup
    fat_close(volume);
    disk_close(disk);

    printf("\nDemo completed!\n");
    return 0;
}