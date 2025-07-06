# 💾 FAT16 File System Reader

A simple FAT16 file system reader written in C that can parse disk images and read files without using standard library file operations. Built to learn how file systems work under the hood.

## ✨ What it does

- 🖼️ Opens disk image files (.dd, .img formats)
- 🔍 Detects FAT16 partitions automatically (handles both MBR and raw images)
- 📋 Lists files in the root directory with attributes
- 📖 Reads and displays file contents
- 🎯 Demonstrates file seek operations

## 📁 Project Structure

```
src/
├── file_reader.h    => API definitions and data structures
├── file_reader.c    => Core implementation of FAT16 parsing
└── main.c          => Demo application showing usage
```

## 🔨 Building

```
gcc -Wall -std=c99 src/file_reader.c src/main.c -o fat16_reader
```

## 🚀 Usage

```bash
# Run with any FAT16 disk image
./fat16_reader disk_image.dd
./fat16_reader filesystem.img

# The program will:
# 1. Auto-detect the FAT16 partition offset
# 2. List all files in root directory  
# 3. Read and display the first file it finds
```

## 📺 Sample Output

### With NIST test image:
```bash
$ ./fat16_reader dfr-01-fat.dd
Found FAT16 partition 0: start=63, size=2088387 sectors
FAT16 Reader Demo
=================
Root directory contents:
  ALCOR.TXT         512 bytes

Reading file: ALCOR.TXT
========================
File size: 512 bytes

File content:
=============
DFR
File Alcor.TXT path root
.+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
=============

Testing seek operations:
SEEK_END: position = 512
SEEK_SET(0): position = 0
SEEK_SET(10): position = 10
File operations completed successfully

Demo completed!
```

### With raw filesystem image:
```bash
$ ./fat16_reader filesystem.img
No MBR partitions found, trying raw filesystem at sector 0
FAT16 Reader Demo
=================
Root directory contents:
  README.TXT        1024 bytes
  DATA.DAT          2048 bytes [RO]

Reading file: README.TXT
========================
[file contents displayed...]
```

## 📚 API

The main API functions in `file_reader.h`:

### 💿 Disk Operations
```
struct disk_t* disk_open_from_file(const char* filename);
int disk_read(struct disk_t* disk, int32_t sector, void* buffer, int32_t count);
int disk_close(struct disk_t* disk);
```

### 📦 Volume Operations
```
struct volume_t* fat_open(struct disk_t* disk, uint32_t sector_offset);
int fat_close(struct volume_t* volume);
```

### 📂 Directory Operations
```
struct dir_t* dir_open(struct volume_t* volume, const char* path);
int dir_read(struct dir_t* dir, struct dir_entry_t* entry);
int dir_close(struct dir_t* dir);
```

### 📄 File Operations
```
struct file_t* file_open(struct volume_t* volume, const char* filename);
size_t file_read(void* buffer, size_t size, size_t count, struct file_t* file);
int32_t file_seek(struct file_t* file, int32_t offset, int whence);
int file_close(struct file_t* file);
```

## ⚙️ How it works

### 🔍 MBR Detection
The program automatically detects whether you're using:
- **Full disk images** (.dd): Reads MBR partition table to find FAT16 partitions
- **Raw filesystem images** (.img): Assumes filesystem starts at sector 0

### 📊 FAT16 Parsing
1. **Boot Sector**: Reads filesystem metadata (cluster size, FAT location, etc.)
2. **File Allocation Table**: Loads the FAT to track which clusters belong to files
3. **Root Directory**: Parses fixed-size directory entries to list files
4. **File Data**: Follows cluster chains to read actual file content

### 🧠 Memory Management
- All allocations are properly freed
- Error handling cleans up resources
- Uses sector-based I/O (512 bytes at a time)

## 🧪 Test Images

### 🏛️ NIST Test Images
```bash
# Download official test image
wget https://cfreds-archive.nist.gov/images/fat-01/dfr-01-fat.dd.bz2
bunzip2 dfr-01-fat.dd.bz2
./fat16_reader dfr-01-fat.dd
```


## ⚠️ Limitations

- **Read-only**: Cannot write or modify files
- **FAT16 only**: Doesn't support FAT32, NTFS, or other filesystems
- **Root directory only**: Cannot navigate into subdirectories
- **Basic error handling**: Could provide more detailed error messages
- **No long filename support**: Only handles 8.3 DOS filenames

## 🎓 What I learned

This project enabled me to further develop my knowledge in:

### 💻 Low-level Programming
- Binary file format parsing in C
- Working with packed structs for on-disk data

### 🗄️ File System Concepts
- How filesystems organize data on disk
- MBR partition tables and boot sectors
- Cluster-based storage allocation
- Directory structures and file metadata

### ⚡ Systems Programming
- Direct disk I/O operations
- Understanding the gap between high-level file APIs and disk storage
- Basic error handling in low-level code

### Could be extended with:
- Support for subdirectories
- Long filename (VFAT) support
- FAT32 compatibility
- Write operations
- Better error messages and recovery

---
## 📜 License
This project is licensed under the MIT License.

Built as a learning project to understand file system internals and low-level C programming. 📚
