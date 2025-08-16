#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <string>
#include <vector>
#include <fstream>

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
    int mode; 
    int size;
    int direct_blocks[10];
    int indirect_block;
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
    
    Inode get_inode(int inode_num) const;
};

#endif // FILESYSTEM_H
