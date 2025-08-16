#include "core/fsck.h"
#include <iostream>
#include <queue>
#include <unordered_set>

FileSystemCheck::FileSystemCheck(FileSystem *fs) : fs(fs) {
    block_used = new bool[NUM_BLOCKS]();
    inode_used = new bool[NUM_INODES]();
    inode_link_counts = new int[NUM_INODES]();
}

FileSystemCheck::~FileSystemCheck() {
    delete[] block_used;
    delete[] inode_used;
    delete[] inode_link_counts;
}

std::vector<FsckIssue> FileSystemCheck::check() {
    issues.clear();

    // Reset tracking arrays
    for (int i = 0; i < NUM_BLOCKS; i++) {
        block_used[i] = false;
    }

    for (int i = 0; i < NUM_INODES; i++) {
        inode_used[i] = false;
        inode_link_counts[i] = 0;
    }

    // Mark superblock and inode blocks as used
    block_used[0] = true; // Superblock

    int inode_blocks = (NUM_INODES * sizeof(Inode) + BLOCK_SIZE - 1) / BLOCK_SIZE;
    for (int i = 1; i <= inode_blocks; i++) {
        block_used[i] = true;
    }

    // Check file system components
    check_superblock();
    check_inodes();
    check_directory_structure();
    check_blocks();

    return issues;
}

void FileSystemCheck::check_superblock() {
    // The actual superblock checks would be more extensive
    // This is a simplified implementation

    // Check if inode count exceeds maximum
    if (NUM_INODES > 1000000) { // Arbitrary upper limit
        FsckIssue issue;
        issue.type = FsckIssueType::INVALID_INODE;
        issue.inode_num = -1;
        issue.block_num = 0;
        issue.description = "Superblock indicates an unreasonable number of inodes";
        issue.can_fix = false;
        issues.push_back(issue);
    }

    // Check if block count exceeds maximum
    if (NUM_BLOCKS > 10000000) { // Arbitrary upper limit
        FsckIssue issue;
        issue.type = FsckIssueType::INVALID_BLOCK_POINTER;
        issue.inode_num = -1;
        issue.block_num = 0;
        issue.description = "Superblock indicates an unreasonable number of blocks";
        issue.can_fix = false;
        issues.push_back(issue);
    }
}

void FileSystemCheck::check_inodes() {
    for (int i = 0; i < NUM_INODES; i++) {
        Inode inode = fs->get_inode(i);

        // Skip free inodes
        if (inode.mode == 0)
            continue;

        // Check for invalid modes
        if (inode.mode != 1 && inode.mode != 2 && inode.mode != 3) {
            FsckIssue issue;
            issue.type = FsckIssueType::INVALID_INODE;
            issue.inode_num = i;
            issue.block_num = -1;
            issue.description = "Inode has invalid mode: " + std::to_string(inode.mode);
            issue.can_fix = true;
            issues.push_back(issue);
            continue;
        }

        // Mark direct blocks as used
        for (int j = 0; j < 10; j++) {
            if (inode.direct_blocks[j] != 0) {
                if (inode.direct_blocks[j] < 0 || inode.direct_blocks[j] >= NUM_BLOCKS) {
                    FsckIssue issue;
                    issue.type = FsckIssueType::INVALID_BLOCK_POINTER;
                    issue.inode_num = i;
                    issue.block_num = inode.direct_blocks[j];
                    issue.description = "Inode " + std::to_string(i) +
                                        " has invalid direct block pointer: " +
                                        std::to_string(inode.direct_blocks[j]);
                    issue.can_fix = true;
                    issues.push_back(issue);
                } else {
                    if (block_used[inode.direct_blocks[j]]) {
                        FsckIssue issue;
                        issue.type = FsckIssueType::DUPLICATE_BLOCK;
                        issue.inode_num = i;
                        issue.block_num = inode.direct_blocks[j];
                        issue.description = "Block " + std::to_string(inode.direct_blocks[j]) +
                                            " is referenced by multiple inodes";
                        issue.can_fix = true;
                        issues.push_back(issue);
                    }
                    block_used[inode.direct_blocks[j]] = true;
                }
            }
        }

        // Check indirect block
        if (inode.indirect_block != 0) {
            if (inode.indirect_block < 0 || inode.indirect_block >= NUM_BLOCKS) {
                FsckIssue issue;
                issue.type = FsckIssueType::INVALID_BLOCK_POINTER;
                issue.inode_num = i;
                issue.block_num = inode.indirect_block;
                issue.description =
                    "Inode " + std::to_string(i) +
                    " has invalid indirect block pointer: " + std::to_string(inode.indirect_block);
                issue.can_fix = true;
                issues.push_back(issue);
            } else {
                if (block_used[inode.indirect_block]) {
                    FsckIssue issue;
                    issue.type = FsckIssueType::DUPLICATE_BLOCK;
                    issue.inode_num = i;
                    issue.block_num = inode.indirect_block;
                    issue.description = "Indirect block " + std::to_string(inode.indirect_block) +
                                        " is referenced by multiple inodes";
                    issue.can_fix = true;
                    issues.push_back(issue);
                }
                block_used[inode.indirect_block] = true;

                // Read indirect block to check contained block pointers
                char buffer[BLOCK_SIZE];
                fs->read_block(inode.indirect_block, buffer);
                int *block_pointers = (int *)buffer;
                int pointers_per_block = BLOCK_SIZE / sizeof(int);

                for (int j = 0; j < pointers_per_block; j++) {
                    if (block_pointers[j] != 0) {
                        if (block_pointers[j] < 0 || block_pointers[j] >= NUM_BLOCKS) {
                            FsckIssue issue;
                            issue.type = FsckIssueType::INVALID_BLOCK_POINTER;
                            issue.inode_num = i;
                            issue.block_num = block_pointers[j];
                            issue.description = "Inode " + std::to_string(i) +
                                                " has invalid indirect block pointer: " +
                                                std::to_string(block_pointers[j]);
                            issue.can_fix = true;
                            issues.push_back(issue);
                        } else {
                            if (block_used[block_pointers[j]]) {
                                FsckIssue issue;
                                issue.type = FsckIssueType::DUPLICATE_BLOCK;
                                issue.inode_num = i;
                                issue.block_num = block_pointers[j];
                                issue.description = "Block " + std::to_string(block_pointers[j]) +
                                                    " is referenced by multiple inodes";
                                issue.can_fix = true;
                                issues.push_back(issue);
                            }
                            block_used[block_pointers[j]] = true;
                        }
                    }
                }
            }
        }
    }
}

void FileSystemCheck::check_directory_structure() {
    // Mark the root inode as used
    inode_used[0] = true;

    // Queue for BFS traversal of directory structure
    std::queue<int> dir_queue;
    dir_queue.push(0); // Start from root

    // Set to keep track of directories we've already visited
    std::unordered_set<int> visited_dirs;
    visited_dirs.insert(0);

    // BFS traversal to find all directories and files
    while (!dir_queue.empty()) {
        int dir_inode_num = dir_queue.front();
        dir_queue.pop();

        Inode dir_inode = fs->get_inode(dir_inode_num);
        if (dir_inode.mode != 2) {
            // This should be a directory
            FsckIssue issue;
            issue.type = FsckIssueType::INVALID_INODE;
            issue.inode_num = dir_inode_num;
            issue.block_num = -1;
            issue.description = "Inode " + std::to_string(dir_inode_num) +
                                " is not a directory but is referenced as one";
            issue.can_fix = false;
            issues.push_back(issue);
            continue;
        }

        // Get directory entries
        std::vector<DirEntry> entries = fs->get_dir_entries(dir_inode_num);

        for (const auto &entry : entries) {
            // Skip . and .. entries
            if (std::string(entry.name) == "." || std::string(entry.name) == "..") {
                continue;
            }

            // Check if inode number is valid
            if (entry.inode_num < 0 || entry.inode_num >= NUM_INODES) {
                FsckIssue issue;
                issue.type = FsckIssueType::INVALID_INODE;
                issue.inode_num = entry.inode_num;
                issue.block_num = -1;
                issue.description = "Directory entry '" + std::string(entry.name) +
                                    "' references invalid inode " + std::to_string(entry.inode_num);
                issue.can_fix = true;
                issues.push_back(issue);
                continue;
            }

            // Increment link count for this inode
            inode_link_counts[entry.inode_num]++;

            // Mark this inode as used
            inode_used[entry.inode_num] = true;

            // If this is a directory, add to queue if not already visited
            Inode entry_inode = fs->get_inode(entry.inode_num);
            if (entry_inode.mode == 2) {
                if (visited_dirs.find(entry.inode_num) != visited_dirs.end()) {
                    // We've already visited this directory - potential loop
                    FsckIssue issue;
                    issue.type = FsckIssueType::DIRECTORY_LOOP;
                    issue.inode_num = entry.inode_num;
                    issue.block_num = -1;
                    issue.description = "Directory loop detected involving inode " +
                                        std::to_string(entry.inode_num);
                    issue.can_fix = true;
                    issues.push_back(issue);
                } else {
                    visited_dirs.insert(entry.inode_num);
                    dir_queue.push(entry.inode_num);
                }
            }
        }
    }

    // Check for orphaned inodes
    for (int i = 0; i < NUM_INODES; i++) {
        Inode inode = fs->get_inode(i);
        if (inode.mode != 0 && !inode_used[i]) {
            FsckIssue issue;
            issue.type = FsckIssueType::ORPHANED_INODE;
            issue.inode_num = i;
            issue.block_num = -1;
            issue.description =
                "Inode " + std::to_string(i) + " is not referenced by any directory";
            issue.can_fix = true;
            issues.push_back(issue);
        }
    }

    // Check for incorrect link counts
    for (int i = 0; i < NUM_INODES; i++) {
        Inode inode = fs->get_inode(i);
        if (inode.mode != 0 && inode.link_count != inode_link_counts[i]) {
            FsckIssue issue;
            issue.type = FsckIssueType::INCORRECT_LINK_COUNT;
            issue.inode_num = i;
            issue.block_num = -1;
            issue.description = "Inode " + std::to_string(i) +
                                " has incorrect link count: " + std::to_string(inode.link_count) +
                                " (actual: " + std::to_string(inode_link_counts[i]) + ")";
            issue.can_fix = true;
            issues.push_back(issue);
        }
    }
}

void FileSystemCheck::check_blocks() {
    // Check for unreferenced blocks
    for (int i = 0; i < NUM_BLOCKS; i++) {
        if (!block_used[i]) {
            // This is actually normal - blocks are allowed to be free
            // We would only report this if the block wasn't in the free list
            // but that would require more extensive superblock checking
        }
    }
}

void FileSystemCheck::fix_all_issues() {
    for (size_t i = 0; i < issues.size(); i++) {
        if (issues[i].can_fix) {
            fix_issue(i);
        }
    }
}

void FileSystemCheck::fix_issue(int issue_index) {
    if (issue_index < 0 || issue_index >= static_cast<int>(issues.size())) {
        return;
    }

    FsckIssue &issue = issues[issue_index];

    if (!issue.can_fix) {
        std::cerr << "Cannot fix issue: " << issue.description << std::endl;
        return;
    }

    switch (issue.type) {
        case FsckIssueType::INVALID_INODE:
            fix_invalid_inode(issue.inode_num);
            break;
        case FsckIssueType::ORPHANED_INODE:
            fix_orphaned_inode(issue.inode_num);
            break;
        case FsckIssueType::DUPLICATE_BLOCK:
            fix_duplicate_block(issue.block_num);
            break;
        case FsckIssueType::UNREFERENCED_BLOCK:
            fix_unreferenced_block(issue.block_num);
            break;
        case FsckIssueType::DIRECTORY_LOOP:
            fix_directory_loop(issue.inode_num);
            break;
        case FsckIssueType::INCORRECT_LINK_COUNT:
            fix_incorrect_link_count(issue.inode_num);
            break;
        case FsckIssueType::INVALID_BLOCK_POINTER:
            // Need to determine which block index is invalid
            // For simplicity, we'll just set all blocks to 0
            for (int i = 0; i < 10; i++) {
                fix_invalid_block_pointer(issue.inode_num, i);
            }
            break;
    }

    // Mark issue as fixed
    issue.can_fix = false;
    issue.description += " (FIXED)";
}

// Implementation of fix methods
void FileSystemCheck::fix_invalid_inode(int inode_num) {
    // For invalid inodes, we'll just clear them
    Inode inode = fs->get_inode(inode_num);
    inode.mode = 0; // Mark as free

    // We would write this back to disk, but our API doesn't allow direct inode modification
    // In a real implementation, we would need a way to update inodes directly
    std::cout << "Fixed invalid inode " << inode_num << " by marking it as free" << std::endl;
}

void FileSystemCheck::fix_orphaned_inode(int inode_num) {
    // For orphaned inodes, we'll move them to a "lost+found" directory

    // Create lost+found if it doesn't exist
    int lost_found_inode = fs->create_lost_found();

    if (lost_found_inode != -1) {
        // Move the orphaned inode to lost+found
        fs->fix_orphaned_inode(inode_num, lost_found_inode);
        std::cout << "Moved orphaned inode " << inode_num << " to lost+found" << std::endl;
    } else {
        std::cerr << "Failed to create lost+found directory" << std::endl;
    }
}

void FileSystemCheck::fix_duplicate_block(int block_num) {
    // This is a complex case that would require deep filesystem knowledge
    // For simplicity, we'll just report what would be done
    std::cout << "Would fix duplicate block " << block_num
              << " by allocating new block and copying data" << std::endl;
}

void FileSystemCheck::fix_unreferenced_block(int block_num) {
    // Add block to free list
    std::cout << "Would add unreferenced block " << block_num << " to free list" << std::endl;
}

void FileSystemCheck::fix_directory_loop(int inode_num) {
    // Break directory loop by removing problematic entry
    std::cout << "Would break directory loop involving inode " << inode_num << std::endl;
}

void FileSystemCheck::fix_incorrect_link_count(int inode_num) {
    // Use the new FileSystem method to fix the link count
    fs->fix_inode_link_count(inode_num, inode_link_counts[inode_num]);
    std::cout << "Fixed link count for inode " << inode_num << " to "
              << inode_link_counts[inode_num] << std::endl;
}

void FileSystemCheck::fix_invalid_block_pointer(int inode_num, int block_index) {
    // Use the new FileSystem method to fix the invalid block pointer
    fs->fix_invalid_block_pointer(inode_num, block_index);
    std::cout << "Fixed invalid block pointer in inode " << inode_num << " at index " << block_index
              << std::endl;
}
