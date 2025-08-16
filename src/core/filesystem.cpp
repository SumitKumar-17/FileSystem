#include "core/filesystem.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm> // for std::min

// --- Constructor & Destructor ---
FileSystem::FileSystem(const std::string& name) : disk_name(name), current_dir_inode(0) {}

FileSystem::~FileSystem() {
    if (disk.is_open()) {
        unmount();
    }
}

// --- Private Helper Functions ---
void FileSystem::write_block(int block_num, const char* data) {
    disk.seekp(block_num * BLOCK_SIZE, std::ios::beg);
    disk.write(data, BLOCK_SIZE);
}

void FileSystem::read_block(int block_num, char* data) {
    disk.seekg(block_num * BLOCK_SIZE, std::ios::beg);
    disk.read(data, BLOCK_SIZE);
}

void FileSystem::write_superblock() {
    char buffer[BLOCK_SIZE] = {0};
    memcpy(buffer, &sb, sizeof(Superblock));
    write_block(0, buffer);
}

void FileSystem::read_superblock() {
    char buffer[BLOCK_SIZE];
    read_block(0, buffer);
    memcpy(&sb, buffer, sizeof(Superblock));
}

void FileSystem::write_inodes() {
    char buffer[BLOCK_SIZE];
    int inodes_per_block = BLOCK_SIZE / sizeof(Inode);
    for (int i = 0; i < sb.inode_blocks; ++i) {
        memset(buffer, 0, BLOCK_SIZE);
        memcpy(buffer, &inodes[i * inodes_per_block], inodes_per_block * sizeof(Inode));
        write_block(1 + i, buffer);
    }
}

void FileSystem::read_inodes() {
    inodes.resize(NUM_INODES);
    char buffer[BLOCK_SIZE];
    int inodes_per_block = BLOCK_SIZE / sizeof(Inode);
    for (int i = 0; i < sb.inode_blocks; ++i) {
        read_block(1 + i, buffer);
        memcpy(&inodes[i * inodes_per_block], buffer, inodes_per_block * sizeof(Inode));
    }
}

int FileSystem::allocate_block() {
    if (sb.free_block_list_head == -1) return -1;
    int free_block = sb.free_block_list_head;
    char buffer[BLOCK_SIZE];
    read_block(free_block, buffer);
    memcpy(&sb.free_block_list_head, buffer, sizeof(int));
    write_superblock();
    return free_block;
}

void FileSystem::free_block(int block_num) {
    char buffer[BLOCK_SIZE] = {0};
    memcpy(buffer, &sb.free_block_list_head, sizeof(int));
    write_block(block_num, buffer);
    sb.free_block_list_head = block_num;
    write_superblock();
}

int FileSystem::find_free_inode() {
    for (int i = 0; i < NUM_INODES; ++i) {
        if (inodes[i].mode == 0) return i;
    }
    return -1;
}

std::vector<DirEntry> FileSystem::get_dir_entries(int inode_num) {
    std::vector<DirEntry> entries;
    if (inode_num < 0 || inode_num >= NUM_INODES || inodes[inode_num].mode != 2) {
        return entries;
    }

    char buffer[BLOCK_SIZE];
    Inode& inode = inodes[inode_num];
    for (int i = 0; i < 10 && inode.direct_blocks[i] != 0; ++i) {
        read_block(inode.direct_blocks[i], buffer);
        for (int j = 0; j < BLOCK_SIZE / sizeof(DirEntry); ++j) {
            DirEntry entry;
            memcpy(&entry, buffer + j * sizeof(DirEntry), sizeof(DirEntry));
            if (entry.inode_num != -1) {
                entries.push_back(entry);
            }
        }
    }
    return entries;
}

void FileSystem::add_dir_entry(int dir_inode_num, const std::string& name, int new_inode_num) {
    DirEntry new_entry;
    strncpy(new_entry.name, name.c_str(), MAX_FILENAME_LENGTH - 1);
    new_entry.name[MAX_FILENAME_LENGTH - 1] = '\0';
    new_entry.inode_num = new_inode_num;

    Inode& dir_inode = inodes[dir_inode_num];
    char buffer[BLOCK_SIZE];

    for (int i = 0; i < 10; ++i) {
        if (dir_inode.direct_blocks[i] == 0) {
            dir_inode.direct_blocks[i] = allocate_block();
            if (dir_inode.direct_blocks[i] == -1) return;
            
            for(int k=0; k < BLOCK_SIZE / sizeof(DirEntry); ++k) {
                DirEntry empty_entry;
                empty_entry.inode_num = -1;
                memcpy(buffer + k * sizeof(DirEntry), &empty_entry, sizeof(DirEntry));
            }
        } else {
             read_block(dir_inode.direct_blocks[i], buffer);
        }

        for (int j = 0; j < BLOCK_SIZE / sizeof(DirEntry); ++j) {
            DirEntry entry;
            memcpy(&entry, buffer + j * sizeof(DirEntry), sizeof(DirEntry));
            if (entry.inode_num == -1) {
                memcpy(buffer + j * sizeof(DirEntry), &new_entry, sizeof(DirEntry));
                write_block(dir_inode.direct_blocks[i], buffer);
                dir_inode.size += sizeof(DirEntry);
                return;
            }
        }
    }
}

// --- Public API Functions ---

void FileSystem::format() {
    disk.open(disk_name, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!disk.is_open()) return;

    char zero_block[BLOCK_SIZE] = {0};
    for (int i = 0; i < NUM_BLOCKS; ++i) disk.write(zero_block, BLOCK_SIZE);
    disk.close();

    disk.open(disk_name, std::ios::in | std::ios::out | std::ios::binary);
    
    sb.num_blocks = NUM_BLOCKS;
    sb.num_inodes = NUM_INODES;
    sb.inode_blocks = (NUM_INODES * sizeof(Inode) + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    int first_data_block = 1 + sb.inode_blocks;
    sb.free_block_list_head = first_data_block;
    write_superblock();

    inodes.assign(NUM_INODES, Inode{});
    
    inodes[0].mode = 2; // Root directory
    current_dir_inode = 0;
    add_dir_entry(0, ".", 0);
    add_dir_entry(0, "..", 0);
    write_inodes();

    char buffer[BLOCK_SIZE];
    for (int i = first_data_block; i < NUM_BLOCKS - 1; ++i) {
        int next_block = i + 1;
        memset(buffer, 0, BLOCK_SIZE);
        memcpy(buffer, &next_block, sizeof(int));
        write_block(i, buffer);
    }
    int end_of_list = -1;
    memset(buffer, 0, BLOCK_SIZE);
    memcpy(buffer, &end_of_list, sizeof(int));
    write_block(NUM_BLOCKS - 1, buffer);
}

bool FileSystem::mount() {
    disk.open(disk_name, std::ios::in | std::ios::out | std::ios::binary);
    if (!disk.is_open()) return false;
    read_superblock();
    read_inodes();
    current_dir_inode = 0;
    return true;
}

void FileSystem::unmount() {
    if (disk.is_open()) {
        write_inodes(); // Save any changes to inodes
        disk.close();
    }
}

void FileSystem::mkdir(const std::string& dirname) {
    int free_inode_num = find_free_inode();
    if (free_inode_num == -1) return;

    inodes[free_inode_num].mode = 2;
    add_dir_entry(current_dir_inode, dirname, free_inode_num);
    add_dir_entry(free_inode_num, ".", free_inode_num);
    add_dir_entry(free_inode_num, "..", current_dir_inode);
}

std::vector<DirEntry> FileSystem::ls() {
    return get_dir_entries(current_dir_inode);
}

int FileSystem::find_inode_by_path(const std::string& path) {
    if (path.empty()) return current_dir_inode;
    if (path == "/") return 0;

    std::stringstream ss(path);
    std::string item;
    int current_inode = (path[0] == '/') ? 0 : current_dir_inode;

    std::vector<std::string> parts;
    while (getline(ss, item, '/')) {
        if (!item.empty()) parts.push_back(item);
    }

    for (const auto& part : parts) {
        if (inodes[current_inode].mode != 2) return -1;
        bool found = false;
        for (const auto& entry : get_dir_entries(current_inode)) {
            if (std::string(entry.name) == part) {
                current_inode = entry.inode_num;
                found = true;
                break;
            }
        }
        if (!found) return -1;
    }
    return current_inode;
}

void FileSystem::cd(const std::string& path) {
    if (path == "..") {
        current_dir_inode = find_inode_by_path("..");
        return;
    }
    int inode_num = find_inode_by_path(path);
    if (inode_num != -1 && inodes[inode_num].mode == 2) {
        current_dir_inode = inode_num;
    }
}

void FileSystem::create(const std::string& filename) {
    int free_inode_num = find_free_inode();
    if (free_inode_num == -1) return;

    inodes[free_inode_num].mode = 1;
    add_dir_entry(current_dir_inode, filename, free_inode_num);
}

void FileSystem::write(const std::string& filename, const std::string& data) {
    int inode_num = find_inode_by_path(filename);
    if (inode_num == -1 || inodes[inode_num].mode != 1) return;

    Inode& inode = inodes[inode_num];
    for(int i=0; i<10; ++i) {
        if(inode.direct_blocks[i] != 0) {
            free_block(inode.direct_blocks[i]);
            inode.direct_blocks[i] = 0;
        }
    }

    inode.size = data.size();
    char buffer[BLOCK_SIZE];
    for (size_t i = 0; i < (data.size() + BLOCK_SIZE - 1) / BLOCK_SIZE; ++i) {
        if (i >= 10) break;
        int new_block = allocate_block();
        if (new_block == -1) {
            inode.size = i * BLOCK_SIZE;
            break;
        }
        inode.direct_blocks[i] = new_block;
        
        memset(buffer, 0, BLOCK_SIZE);
        size_t bytes_to_copy = std::min((size_t)BLOCK_SIZE, data.size() - (i * BLOCK_SIZE));
        memcpy(buffer, data.c_str() + i * BLOCK_SIZE, bytes_to_copy);
        write_block(new_block, buffer);
    }
}

std::string FileSystem::read(const std::string& filename) {
    int inode_num = find_inode_by_path(filename);
    if (inode_num == -1 || inodes[inode_num].mode != 1) return "";

    Inode& inode = inodes[inode_num];
    std::string content = "";
    char buffer[BLOCK_SIZE + 1] = {0};

    for (int i = 0; i < 10 && inode.direct_blocks[i] != 0; ++i) {
        read_block(inode.direct_blocks[i], buffer);
        size_t bytes_to_read = std::min((size_t)BLOCK_SIZE, (size_t)inode.size - content.size());
        content.append(buffer, bytes_to_read);
    }
    return content;
}

Inode FileSystem::get_inode(int inode_num) const {
    if (inode_num >= 0 && inode_num < NUM_INODES) {
        return inodes[inode_num];
    }
    return Inode{}; // Return an empty/default inode on error
}
