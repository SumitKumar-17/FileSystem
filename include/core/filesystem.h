#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <string>
#include <vector>
#include <fstream>
#include <ctime>
#include "journal.h"

// Constants for our file system
const int BLOCK_SIZE = 512;
const int NUM_BLOCKS = 4096;
const int NUM_INODES = 128;
const int MAX_FILENAME_LENGTH = 28;

// Superblock structure
struct Superblock {
    int num_blocks;
    int num_inodes;
    int inode_blocks;
    int free_block_list_head;
};

// Inode structure
struct Inode {
    int mode; // Permissions and file type (e.g., regular file, directory)
    int uid; // User ID
    int gid; // Group ID
    int size;
    int link_count;
    time_t creation_time;
    time_t modification_time;
    time_t access_time;
    int direct_blocks[10];
    int indirect_block;
    int flags; // Additional flags (e.g., for symbolic links)
};

// Directory entry structure
struct DirEntry {
    char name[MAX_FILENAME_LENGTH];
    int inode_num;
};

class FileSystem {
private:
    std::fstream disk;
    std::string disk_name;
    Superblock sb;
    std::vector<Inode> inodes;
    int current_dir_inode;
    Journal* journal;

    void write_block(int block_num, const char* data);
    void read_block(int block_num, char* data);
    void write_superblock();
    void read_superblock();
    void write_inodes();
    void read_inodes();
    int allocate_block();
    void free_block(int block_num);
    int find_free_inode();
    void add_dir_entry(int dir_inode_num, const std::string& name, int new_inode_num);
    void update_inode_times(int inode_num, bool access, bool modify, bool create);

public:
    FileSystem(const std::string& name);
    ~FileSystem();

    void format();
    bool mount();
    void unmount();
    void mkdir(const std::string& dirname);
    std::vector<DirEntry> get_dir_entries(int inode_num);
    std::vector<DirEntry> ls();
    void cd(const std::string& path);
    int find_inode_by_path(const std::string& path);
    void create(const std::string& filename);
    void write(const std::string& filename, const std::string& data);
    std::string read(const std::string& filename);
    void chmod(const std::string& path, int mode);
    void chown(const std::string& path, int uid, int gid);
    void link(const std::string& oldpath, const std::string& newpath);
    void symlink(const std::string& target, const std::string& linkpath);
    void unlink(const std::string& path);
    
    Inode get_inode(int inode_num) const;

    friend class Journal;
};

#endif // FILESYSTEM_H
