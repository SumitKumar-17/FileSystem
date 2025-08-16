#include "core/filesystem.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <dirent.h> // For directory operations
#include <sys/stat.h> // For file stats
#include <QDebug>   // For debug messages
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
    // For external filesystems, we don't use inodes
    bool is_external = (disk_name.find(".fs") == std::string::npos && disk_name.find("/") == 0);
    if (is_external) {
        return -1;  // Not supported for external filesystems
    }

    // Ensure inodes vector is initialized
    if (inodes.empty()) {
        qDebug() << "Warning: Inodes vector is empty in find_free_inode";
        return -1;
    }

    for (size_t i = 0; i < inodes.size(); ++i) {
        if (inodes[i].mode == 0) return i;
    }
    return -1;
}

std::vector<DirEntry> FileSystem::get_dir_entries(int inode_num) {
    std::vector<DirEntry> entries;
    
    // Check if this is an external filesystem (mounted path, not .fs file)
    bool is_external = (disk_name.find(".fs") == std::string::npos && disk_name.find("/") == 0);
    
    if (is_external) {
        // For external filesystems, we'll use system calls to get real directory contents
        // Add . and .. entries
        DirEntry dot_entry;
        strncpy(dot_entry.name, ".", MAX_FILENAME_LENGTH);
        dot_entry.inode_num = current_dir_inode;
        entries.push_back(dot_entry);
        
        DirEntry dotdot_entry;
        strncpy(dotdot_entry.name, "..", MAX_FILENAME_LENGTH);
        dotdot_entry.inode_num = (current_dir_inode == 0) ? 0 : 1; // Root's parent is itself
        entries.push_back(dotdot_entry);
        
        // Get the current path in the external filesystem
        std::string current_path = disk_name;
        // TODO: Track current path relative to mount point
        
        // Use system's dirent to list directory contents
        DIR* dir;
        struct dirent* entry;
        
        if ((dir = opendir(current_path.c_str())) != nullptr) {
            int entry_inode = 2; // Start assigning fake inodes from 2
            
            while ((entry = readdir(dir)) != nullptr) {
                // Skip . and .. as we've already added them
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                    continue;
                }
                
                DirEntry fs_entry;
                strncpy(fs_entry.name, entry->d_name, MAX_FILENAME_LENGTH);
                fs_entry.inode_num = entry_inode++; // Assign a fake inode number
                
                entries.push_back(fs_entry);
            }
            
            closedir(dir);
        }
        
        return entries;
    }
    
    // Original implementation for our virtual filesystem
    if (!is_valid_inode(inode_num) || inodes[inode_num].mode != 2) {
        if (inode_num >= 0) {
            qDebug() << "Warning: Attempted to access invalid directory inode:" << inode_num;
        }
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
    if (!is_valid_inode(dir_inode_num) || inodes[dir_inode_num].mode != 2) {
        if (dir_inode_num >= 0) {
            qDebug() << "Warning: Attempted to add entry to invalid directory inode:" << dir_inode_num;
        }
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
    // Check if this is an external filesystem (mounted path, not .fs file)
    bool is_external = (disk_name.find(".fs") == std::string::npos && disk_name.find("/") == 0);
    
    if (is_external) {
        // For external filesystems, we don't actually open the disk image
        // Instead, we'll use system operations to interact with the real filesystem
        disk.close(); // Just in case it was open before
        
        // Check if the path exists
        std::ifstream test_path(disk_name);
        if (!test_path.good()) {
            std::cerr << "External path doesn't exist or is not accessible: " << disk_name << std::endl;
            return false;
        }
        
        // Initialize dummy superblock and inodes for external filesystem
        sb.num_blocks = NUM_BLOCKS; // Use dummy values
        sb.num_inodes = NUM_INODES;
        sb.inode_blocks = (NUM_INODES * sizeof(Inode) + BLOCK_SIZE - 1) / BLOCK_SIZE;
        
        // Initialize the inodes array with dummy values
        inodes.resize(NUM_INODES);
        
        // Set up root directory
        inodes[0].mode = 040755; // drwxr-xr-x
        inodes[0].size = BLOCK_SIZE;
        inodes[0].uid = 1000; // Default user
        inodes[0].gid = 1000; // Default group
        inodes[0].link_count = 2; // . and ..
        inodes[0].creation_time = inodes[0].modification_time = inodes[0].access_time = time(nullptr);
        
        // No need for journal on external filesystem
        journal = nullptr;
        current_dir_inode = 0; // Root directory
        
        std::cout << "Mounted external filesystem at: " << disk_name << std::endl;
        return true;
    } else {
        // Regular .fs file handling
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
    // Check if this is an external filesystem
    bool is_external = (disk_name.find(".fs") == std::string::npos && disk_name.find("/") == 0);
    
    if (is_external) {
        // For external filesystems, we track a current path relative to the mount point
        std::string full_path;
        
        if (path == "..") {
            // Go up one directory
            size_t last_slash = disk_name.find_last_of('/');
            if (last_slash != std::string::npos && last_slash > 0) {
                full_path = disk_name.substr(0, last_slash);
            } else {
                full_path = disk_name; // Stay at root if already at root
            }
        } else if (path == ".") {
            // Stay in current directory
            full_path = disk_name;
        } else if (path[0] == '/') {
            // Absolute path
            full_path = path;
        } else {
            // Relative path
            full_path = disk_name + "/" + path;
        }
        
        // Check if the directory exists
        DIR* dir = opendir(full_path.c_str());
        if (dir != nullptr) {
            closedir(dir);
            disk_name = full_path;
        } else {
            std::cerr << "Cannot change to directory: " << full_path << std::endl;
        }
        return;
    }
    
    // Original implementation for virtual filesystem
    int inode_num = find_inode_by_path(path);
    if (inode_num != -1 && inodes[inode_num].mode == 2) {
        current_dir_inode = inode_num;
    } else {
        std::cerr << "Error: Directory not found." << std::endl;
    }
}

int FileSystem::find_inode_by_path(const std::string& path) {
    // Check if this is an external filesystem
    bool is_external = (disk_name.find(".fs") == std::string::npos && disk_name.find("/") == 0);
    
    if (is_external) {
        // For external filesystems, verify the path exists
        std::string full_path;
        
        if (path == ".") {
            return 0; // Current directory
        } else if (path == "..") {
            return 1; // Parent directory
        } else if (path[0] == '/') {
            full_path = path; // Absolute path
        } else {
            full_path = disk_name + "/" + path; // Relative path
        }
        
        // Check if file/directory exists
        struct stat path_stat;
        if (stat(full_path.c_str(), &path_stat) == 0) {
            // Return fake inode number
            // We'll use 0 for current dir, 1 for parent, and incrementing for others
            return 2; // Simple implementation for now
        }
        return -1; // Not found
    }
    
    // Original implementation for virtual filesystem
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

    if (!is_valid_inode(new_inode_num)) {
        qDebug() << "Warning: Attempted to create file with invalid inode:" << new_inode_num;
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
    // Create a default inode to return in case of errors
    Inode defaultInode;
    defaultInode.mode = 2; // Default to directory for listings
    
    // Check if this is an external filesystem
    bool is_external = (disk_name.find(".fs") == std::string::npos && disk_name.find("/") == 0);
    
    if (is_external) {
        // For external filesystems, create a fake inode based on the path
        Inode fake_inode;
        
        // Default to directory for simplicity
        fake_inode.mode = 2; // Directory
        
        if (inode_num == 0) {
            // Current directory
            struct stat st;
            if (stat(disk_name.c_str(), &st) == 0) {
                fake_inode.mode = S_ISDIR(st.st_mode) ? 2 : 1; // 2 for dir, 1 for file
                fake_inode.size = st.st_size;
                fake_inode.uid = st.st_uid;
                fake_inode.gid = st.st_gid;
                fake_inode.creation_time = st.st_ctime;
                fake_inode.modification_time = st.st_mtime;
                fake_inode.access_time = st.st_atime;
            }
        }
        
        return fake_inode;
    }
    
    // Basic bounds checks before trying to access the inodes vector
    if (inode_num < 0 || inode_num >= NUM_INODES) {
        if (inode_num >= 0) { // Only log non-negative inode numbers
            qDebug() << "Warning: Inode number out of range:" << inode_num << "(max:" << (NUM_INODES-1) << ")";
        }
        return defaultInode;
    }
    
    // Check if filesystem is mounted
    if (!disk.is_open()) {
        if (inode_num != 0) { // Don't log for root inode
            qDebug() << "Warning: Attempting to get inode" << inode_num << "from unmounted filesystem";
        }
        return defaultInode;
    }
    
    // Check if inodes vector has been initialized
    if (inodes.empty()) {
        qDebug() << "Warning: Inodes vector is empty, cannot access inode" << inode_num;
        return defaultInode;
    }
    
    // Check if within the bounds of the inodes vector
    if (inode_num >= static_cast<int>(inodes.size())) {
        qDebug() << "Warning: Inode number" << inode_num << "exceeds inodes vector size" << inodes.size();
        return defaultInode;
    }
    
    // At this point we've verified it's safe to access the vector
    return inodes[inode_num]; 
}

#include <algorithm>

void FileSystem::update_inode_times(int inode_num, bool access, bool modify, bool create) {
    if (!is_valid_inode(inode_num)) {
        if (inode_num >= 0) {  // Only log valid non-negative inode numbers
            qDebug() << "Warning: Attempted to update times for invalid inode number:" << inode_num;
        }
        return;
    }
    
    time_t now = time(nullptr);
    if (create) inodes[inode_num].creation_time = now;
    if (access) inodes[inode_num].access_time = now;
    if (modify) inodes[inode_num].modification_time = now;
}

bool FileSystem::is_valid_inode(int inode_num) const {
    // Basic bounds checking
    if (inode_num < 0 || inode_num >= NUM_INODES) {
        return false;
    }
    
    // External filesystems are handled differently and don't use the inodes vector
    bool is_external = (disk_name.find(".fs") == std::string::npos && disk_name.find("/") == 0);
    if (is_external) {
        return inode_num == 0;  // Only inode 0 is valid for external filesystems
    }
    
    // Check if filesystem is mounted
    if (!disk.is_open()) {
        return false;  // Filesystem not mounted
    }
    
    // Check if inodes vector is initialized and has enough elements
    if (inodes.empty()) {
        return false;  // No inodes loaded
    }
    
    // Check if the inode number is within the bounds of the inodes vector
    if (inode_num >= static_cast<int>(inodes.size())) {
        return false;
    }
    
    return true;
}

// Fix an invalid block pointer in an inode
void FileSystem::fix_invalid_block_pointer(int inode_num, int block_index) {
    if (!is_valid_inode(inode_num)) {
        std::cerr << "Error: Cannot fix invalid block pointer for invalid inode " << inode_num << std::endl;
        return;
    }
    
    // For direct blocks
    if (block_index >= 0 && block_index < 10) {
        inodes[inode_num].direct_blocks[block_index] = 0;
        
        // If this was the first invalid block, we might need to update the size
        if (block_index == 0) {
            inodes[inode_num].size = 0;
        }
    } 
    // For indirect block
    else if (block_index == 10) {
        inodes[inode_num].indirect_block = 0;
    }
    
    // Update inode times
    update_inode_times(inode_num, false, true, false);
    
    // Write inodes back to disk
    write_inodes();
}

// Fix an orphaned inode by adding it to lost+found
void FileSystem::fix_orphaned_inode(int inode_num, int lost_found_inode) {
    if (!is_valid_inode(inode_num) || !is_valid_inode(lost_found_inode)) {
        std::cerr << "Error: Invalid inode numbers for fix_orphaned_inode" << std::endl;
        return;
    }
    
    // Create a name for the orphaned inode
    std::string name = "#" + std::to_string(inode_num);
    
    // Add entry to lost+found
    add_dir_entry(lost_found_inode, name, inode_num);
    
    // Update link count for the orphaned inode
    inodes[inode_num].link_count++;
    
    // Update inode times
    update_inode_times(inode_num, false, true, false);
    update_inode_times(lost_found_inode, false, true, false);
    
    // Write inodes back to disk
    write_inodes();
}

// Fix incorrect link count for an inode
void FileSystem::fix_inode_link_count(int inode_num, int correct_count) {
    if (!is_valid_inode(inode_num)) {
        std::cerr << "Error: Cannot fix link count for invalid inode " << inode_num << std::endl;
        return;
    }
    
    inodes[inode_num].link_count = correct_count;
    
    // Update inode times
    update_inode_times(inode_num, false, true, false);
    
    // Write inodes back to disk
    write_inodes();
}

// Create lost+found directory if it doesn't exist
int FileSystem::create_lost_found() {
    // Check if lost+found already exists
    int lost_found_inode = find_inode_by_path("/lost+found");
    if (lost_found_inode != -1) {
        return lost_found_inode;
    }
    
    // Save current directory
    int saved_dir = current_dir_inode;
    
    // Go to root directory
    current_dir_inode = 0;
    
    // Create lost+found directory
    mkdir("lost+found");
    
    // Find the inode for the new lost+found directory
    lost_found_inode = find_inode_by_path("/lost+found");
    
    // Restore current directory
    current_dir_inode = saved_dir;
    
    return lost_found_inode;
}
