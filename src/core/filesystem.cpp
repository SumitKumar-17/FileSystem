#include "core/filesystem.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm> 
FileSystem::FileSystem(const std::string& name) : disk_name(name), current_dir_inode(0), journal(nullptr) {}

FileSystem::~FileSystem() {
    if (disk.is_open()) {
        unmount();
    }
    delete journal;
}

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
    if (dir_inode_num < 0 || dir_inode_num >= NUM_INODES || inodes[dir_inode_num].mode != 2) {
        return;
    }

    Inode& dir_inode = inodes[dir_inode_num];
    DirEntry new_entry;
    strncpy(new_entry.name, name.c_str(), MAX_FILENAME_LENGTH);
    new_entry.name[MAX_FILENAME_LENGTH - 1] = '\0';
    new_entry.inode_num = new_inode_num;

    char buffer[BLOCK_SIZE];
    for (int i = 0; i < 10; ++i) {
        if (dir_inode.direct_blocks[i] == 0) {
            dir_inode.direct_blocks[i] = allocate_block();
            if (dir_inode.direct_blocks[i] == -1) return; 
            memset(buffer, 0, BLOCK_SIZE);
            for(int k=0; k < BLOCK_SIZE / sizeof(DirEntry); ++k) {
                ((DirEntry*)buffer)[k].inode_num = -1;
            }
        } else {
            read_block(dir_inode.direct_blocks[i], buffer);
        }

        for (int j = 0; j < BLOCK_SIZE / sizeof(DirEntry); ++j) {
            DirEntry* entry = (DirEntry*)(buffer + j * sizeof(DirEntry));
            if (entry->inode_num == -1) {
                memcpy(entry, &new_entry, sizeof(DirEntry));
                write_block(dir_inode.direct_blocks[i], buffer);
                dir_inode.size += sizeof(DirEntry);
                return;
            }
        }
    }
}

void FileSystem::format() {
    disk.open(disk_name, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!disk.is_open()) {
        std::cerr << "Error: Could not create disk file." << std::endl;
        return;
    }

    char empty_block[BLOCK_SIZE] = {0};
    for (int i = 0; i < NUM_BLOCKS; ++i) {
        write_block(i, empty_block);
    }

    sb.num_blocks = NUM_BLOCKS;
    sb.num_inodes = NUM_INODES;
    sb.inode_blocks = (NUM_INODES * sizeof(Inode) + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int journal_blocks = 100; // Reserve 100 blocks for the journal
    sb.free_block_list_head = 1 + sb.inode_blocks + journal_blocks;
    write_superblock();

    for (int i = 1 + sb.inode_blocks + journal_blocks; i < NUM_BLOCKS - 1; ++i) {
        int next_block = i + 1;
        char buffer[BLOCK_SIZE] = {0};
        memcpy(buffer, &next_block, sizeof(int));
        write_block(i, buffer);
    }
    int last_block_next = -1;
    char buffer[BLOCK_SIZE] = {0};
    memcpy(buffer, &last_block_next, sizeof(int));
    write_block(NUM_BLOCKS - 1, buffer);

    inodes.assign(NUM_INODES, Inode());
    for(auto& inode : inodes) {
        inode.mode = 0;
    }

    int root_inode_num = find_free_inode();
    inodes[root_inode_num].mode = 2; // Directory
    inodes[root_inode_num].size = 0;
    inodes[root_inode_num].uid = 0; // root user
    inodes[root_inode_num].gid = 0; // root group
    inodes[root_inode_num].link_count = 2; // . and ..
    update_inode_times(root_inode_num, true, true, true);
    current_dir_inode = root_inode_num;

    add_dir_entry(root_inode_num, ".", root_inode_num);
    add_dir_entry(root_inode_num, "..", root_inode_num);

    write_inodes();
    disk.close();
}

bool FileSystem::mount() {
    disk.open(disk_name, std::ios::in | std::ios::out | std::ios::binary);
    if (!disk.is_open()) {
        return false;
    }
    read_superblock();
    read_inodes();
    int journal_start_block = 1 + sb.inode_blocks;
    int journal_num_blocks = 100;
    journal = new Journal(this, journal_start_block, journal_num_blocks);
    journal->recover();
    current_dir_inode = 0; // Root directory
    return true;
}

void FileSystem::unmount() {
    if (disk.is_open()) {
        write_superblock();
        write_inodes();
        disk.close();
    }
}

void FileSystem::mkdir(const std::string& dirname) {
    journal->begin_transaction();
    int new_inode_num = find_free_inode();
    if (new_inode_num == -1) {
        std::cerr << "Error: No free inodes." << std::endl;
        journal->commit_transaction(); // or some rollback mechanism
        return;
    }

    inodes[new_inode_num].mode = 2; // Directory
    inodes[new_inode_num].size = 0;
    inodes[new_inode_num].uid = 0; // Default to root user/group
    inodes[new_inode_num].gid = 0;
    inodes[new_inode_num].link_count = 2; // For . and ..
    update_inode_times(new_inode_num, true, true, true);


    add_dir_entry(current_dir_inode, dirname, new_inode_num);
    add_dir_entry(new_inode_num, ".", new_inode_num);
    add_dir_entry(new_inode_num, "..", current_dir_inode);
    
    char inode_buffer[BLOCK_SIZE];
    int inodes_per_block = BLOCK_SIZE / sizeof(Inode);
    int block_to_update = 1 + (new_inode_num / inodes_per_block);
    memcpy(inode_buffer, &inodes[(new_inode_num / inodes_per_block) * inodes_per_block], inodes_per_block * sizeof(Inode));
    journal->log_metadata_block(block_to_update, inode_buffer);

    journal->commit_transaction();
}

std::vector<DirEntry> FileSystem::ls() {
    return get_dir_entries(current_dir_inode);
}

void FileSystem::cd(const std::string& path) {
    int inode_num = find_inode_by_path(path);
    if (inode_num != -1 && inodes[inode_num].mode == 2) {
        current_dir_inode = inode_num;
    } else {
        std::cerr << "Error: Directory not found." << std::endl;
    }
}

int FileSystem::find_inode_by_path(const std::string& path) {
    if (path.empty()) return -1;

    std::stringstream ss(path);
    std::string segment;
    int start_inode = (path[0] == '/') ? 0 : current_dir_inode;
    
    if (path == "/") return 0;

    while(std::getline(ss, segment, '/')) {
        if (segment.empty()) continue;

        auto entries = get_dir_entries(start_inode);
        bool found = false;
        for (const auto& entry : entries) {
            if (segment == entry.name) {
                start_inode = entry.inode_num;
                found = true;
                break;
            }
        }
        if (!found) return -1;
    }
    return start_inode;
}

void FileSystem::create(const std::string& filename) {
    int new_inode_num = find_free_inode();
    if (new_inode_num == -1) {
        std::cerr << "Error: No free inodes." << std::endl;
        return;
    }

    inodes[new_inode_num].mode = 1; // File
    inodes[new_inode_num].size = 0;
    inodes[new_inode_num].uid = 0; // Default to root user/group
    inodes[new_inode_num].gid = 0;
    inodes[new_inode_num].link_count = 1;
    update_inode_times(new_inode_num, true, true, true);
    for(int i=0; i<10; ++i) inodes[new_inode_num].direct_blocks[i] = 0;
    inodes[new_inode_num].indirect_block = 0;

    add_dir_entry(current_dir_inode, filename, new_inode_num);
}

void FileSystem::write(const std::string& filename, const std::string& data) {
    journal->begin_transaction();
    int inode_num = find_inode_by_path(filename);
    if (inode_num == -1 || inodes[inode_num].mode != 1) {
        std::cerr << "Error: File not found." << std::endl;
        journal->commit_transaction();
        return;
    }
    update_inode_times(inode_num, false, true, false);

    Inode& inode = inodes[inode_num];
    // For simplicity, this overwrites the file completely.
    // First, free existing blocks
    for(int i=0; i<10; ++i) {
        if(inode.direct_blocks[i] != 0) {
            free_block(inode.direct_blocks[i]);
            inode.direct_blocks[i] = 0;
        }
    }
    if(inode.indirect_block != 0) {
        char buffer[BLOCK_SIZE];
        read_block(inode.indirect_block, buffer);
        int* block_pointers = (int*)buffer;
        int pointers_per_block = BLOCK_SIZE / sizeof(int);
        for(int i=0; i<pointers_per_block; ++i) {
            if(block_pointers[i] != 0) {
                free_block(block_pointers[i]);
            }
        }
        free_block(inode.indirect_block);
        inode.indirect_block = 0;
    }
    inode.size = 0;

    // Write new data
    const char* p_data = data.c_str();
    int data_left = data.length();
    int offset = 0;

    // Direct blocks
    for (int i = 0; i < 10 && data_left > 0; ++i) {
        int block_num = allocate_block();
        if (block_num == -1) {
            std::cerr << "Error: Out of space." << std::endl;
            return;
        }
        inode.direct_blocks[i] = block_num;
        int to_write = std::min(data_left, BLOCK_SIZE);
        char buffer[BLOCK_SIZE] = {0};
        memcpy(buffer, p_data + offset, to_write);
        write_block(block_num, buffer);
        data_left -= to_write;
        offset += to_write;
        inode.size += to_write;
    }

    // Indirect blocks
    if (data_left > 0) {
        int indirect_block_num = allocate_block();
        if (indirect_block_num == -1) {
            std::cerr << "Error: Out of space." << std::endl;
            return;
        }
        inode.indirect_block = indirect_block_num;
        char indirect_buffer[BLOCK_SIZE] = {0};
        int* block_pointers = (int*)indirect_buffer;
        int pointers_per_block = BLOCK_SIZE / sizeof(int);

        for (int i = 0; i < pointers_per_block && data_left > 0; ++i) {
            int block_num = allocate_block();
            if (block_num == -1) {
                std::cerr << "Error: Out of space." << std::endl;
                write_block(indirect_block_num, indirect_buffer); // write partial indirect block
                return;
            }
            block_pointers[i] = block_num;
            int to_write = std::min(data_left, BLOCK_SIZE);
            char buffer[BLOCK_SIZE] = {0};
            memcpy(buffer, p_data + offset, to_write);
            write_block(block_num, buffer);
            data_left -= to_write;
            offset += to_write;
            inode.size += to_write;
        }
        write_block(indirect_block_num, indirect_buffer);
        journal->log_data_block(indirect_block_num, indirect_buffer);
    }
    
    char inode_buffer[BLOCK_SIZE];
    int inodes_per_block = BLOCK_SIZE / sizeof(Inode);
    int block_to_update = 1 + (inode_num / inodes_per_block);
    memcpy(inode_buffer, &inodes[(inode_num / inodes_per_block) * inodes_per_block], inodes_per_block * sizeof(Inode));
    journal->log_metadata_block(block_to_update, inode_buffer);

    journal->commit_transaction();
}

std::string FileSystem::read(const std::string& filename) {
    int inode_num = find_inode_by_path(filename);
    if (inode_num == -1 || inodes[inode_num].mode != 1) {
        return "Error: File not found.";
    }
    update_inode_times(inode_num, true, false, false);

    Inode& inode = inodes[inode_num];
    std::string content;
    content.reserve(inode.size);
    char buffer[BLOCK_SIZE];
    int bytes_left = inode.size;

    // Direct blocks
    for (int i = 0; i < 10 && bytes_left > 0; ++i) {
        if (inode.direct_blocks[i] != 0) {
            read_block(inode.direct_blocks[i], buffer);
            int to_read = std::min(bytes_left, BLOCK_SIZE);
            content.append(buffer, to_read);
            bytes_left -= to_read;
        }
    }

    // Indirect blocks
    if (bytes_left > 0 && inode.indirect_block != 0) {
        char indirect_buffer[BLOCK_SIZE];
        read_block(inode.indirect_block, indirect_buffer);
        int* block_pointers = (int*)indirect_buffer;
        int pointers_per_block = BLOCK_SIZE / sizeof(int);

        for (int i = 0; i < pointers_per_block && bytes_left > 0; ++i) {
            if (block_pointers[i] != 0) {
                read_block(block_pointers[i], buffer);
                int to_read = std::min(bytes_left, BLOCK_SIZE);
                content.append(buffer, to_read);
                bytes_left -= to_read;
            }
        }
    }

    return content;
}

void FileSystem::chmod(const std::string& path, int mode) {
    journal->begin_transaction();
    int inode_num = find_inode_by_path(path);
    if (inode_num != -1) {
        inodes[inode_num].mode = (inodes[inode_num].mode & ~0777) | mode;
        update_inode_times(inode_num, false, true, false);
        
        char inode_buffer[BLOCK_SIZE];
        int inodes_per_block = BLOCK_SIZE / sizeof(Inode);
        int block_to_update = 1 + (inode_num / inodes_per_block);
        memcpy(inode_buffer, &inodes[(inode_num / inodes_per_block) * inodes_per_block], inodes_per_block * sizeof(Inode));
        journal->log_metadata_block(block_to_update, inode_buffer);

    } else {
        std::cerr << "Error: File or directory not found." << std::endl;
    }
    journal->commit_transaction();
}

void FileSystem::chown(const std::string& path, int uid, int gid) {
    journal->begin_transaction();
    int inode_num = find_inode_by_path(path);
    if (inode_num != -1) {
        inodes[inode_num].uid = uid;
        inodes[inode_num].gid = gid;
        update_inode_times(inode_num, false, true, false);

        char inode_buffer[BLOCK_SIZE];
        int inodes_per_block = BLOCK_SIZE / sizeof(Inode);
        int block_to_update = 1 + (inode_num / inodes_per_block);
        memcpy(inode_buffer, &inodes[(inode_num / inodes_per_block) * inodes_per_block], inodes_per_block * sizeof(Inode));
        journal->log_metadata_block(block_to_update, inode_buffer);

    } else {
        std::cerr << "Error: File or directory not found." << std::endl;
    }
    journal->commit_transaction();
}

void FileSystem::link(const std::string& oldpath, const std::string& newpath) {
    journal->begin_transaction();
    int inode_num = find_inode_by_path(oldpath);
    if (inode_num == -1) {
        std::cerr << "Error: Source file not found." << std::endl;
        journal->commit_transaction();
        return;
    }
    if (inodes[inode_num].mode == 2) { // is a directory
        std::cerr << "Error: Cannot create hard link to a directory." << std::endl;
        journal->commit_transaction();
        return;
    }

    // For simplicity, assuming newpath is in the current directory
    add_dir_entry(current_dir_inode, newpath, inode_num);
    inodes[inode_num].link_count++;
    update_inode_times(inode_num, false, true, false);

    char inode_buffer[BLOCK_SIZE];
    int inodes_per_block = BLOCK_SIZE / sizeof(Inode);
    int block_to_update = 1 + (inode_num / inodes_per_block);
    memcpy(inode_buffer, &inodes[(inode_num / inodes_per_block) * inodes_per_block], inodes_per_block * sizeof(Inode));
    journal->log_metadata_block(block_to_update, inode_buffer);

    journal->commit_transaction();
}

void FileSystem::symlink(const std::string& target, const std::string& linkpath) {
    journal->begin_transaction();
    int new_inode_num = find_free_inode();
    if (new_inode_num == -1) {
        std::cerr << "Error: No free inodes." << std::endl;
        journal->commit_transaction();
        return;
    }

    inodes[new_inode_num].mode = 3; // Symbolic link type
    inodes[new_inode_num].size = target.length();
    inodes[new_inode_num].uid = 0;
    inodes[new_inode_num].gid = 0;
    inodes[new_inode_num].link_count = 1;
    update_inode_times(new_inode_num, true, true, true);

    // Store target path in a data block
    if (!target.empty()) {
        int block_num = allocate_block();
        if (block_num != -1) {
            inodes[new_inode_num].direct_blocks[0] = block_num;
            char buffer[BLOCK_SIZE] = {0};
            strncpy(buffer, target.c_str(), BLOCK_SIZE);
            write_block(block_num, buffer);
            journal->log_data_block(block_num, buffer);
        }
    }

    add_dir_entry(current_dir_inode, linkpath, new_inode_num);

    char inode_buffer[BLOCK_SIZE];
    int inodes_per_block = BLOCK_SIZE / sizeof(Inode);
    int block_to_update = 1 + (new_inode_num / inodes_per_block);
    memcpy(inode_buffer, &inodes[(new_inode_num / inodes_per_block) * inodes_per_block], inodes_per_block * sizeof(Inode));
    journal->log_metadata_block(block_to_update, inode_buffer);

    journal->commit_transaction();
}

void FileSystem::unlink(const std::string& path) {
    journal->begin_transaction();
    // This is a simplified unlink. It doesn't handle removing directory entries yet.
    int inode_num = find_inode_by_path(path);
    if (inode_num == -1) {
        std::cerr << "Error: File not found." << std::endl;
        journal->commit_transaction();
        return;
    }

    inodes[inode_num].link_count--;
    if (inodes[inode_num].link_count == 0) {
        // Free data blocks
        for (int i = 0; i < 10; ++i) {
            if (inodes[inode_num].direct_blocks[i] != 0) {
                free_block(inodes[inode_num].direct_blocks[i]);
            }
        }
        // Free indirect blocks... (omitted for brevity)
        
        // Free inode
        inodes[inode_num].mode = 0; // Mark as free
    }
    update_inode_times(inode_num, false, true, false);

    char inode_buffer[BLOCK_SIZE];
    int inodes_per_block = BLOCK_SIZE / sizeof(Inode);
    int block_to_update = 1 + (inode_num / inodes_per_block);
    memcpy(inode_buffer, &inodes[(inode_num / inodes_per_block) * inodes_per_block], inodes_per_block * sizeof(Inode));
    journal->log_metadata_block(block_to_update, inode_buffer);

    journal->commit_transaction();
}


Inode FileSystem::get_inode(int inode_num) const {
    if (inode_num >= 0 && inode_num < NUM_INODES) {
        return inodes[inode_num];
    }
    return Inode(); 
}

#include <algorithm>

void FileSystem::update_inode_times(int inode_num, bool access, bool modify, bool create) {
    if (inode_num < 0 || inode_num >= NUM_INODES) return;
    time_t now = time(nullptr);
    if (create) inodes[inode_num].creation_time = now;
    if (access) inodes[inode_num].access_time = now;
    if (modify) inodes[inode_num].modification_time = now;
}
