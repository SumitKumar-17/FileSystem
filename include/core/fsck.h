#ifndef FSCK_H
#define FSCK_H

#include "filesystem.h"
#include <string>
#include <vector>

// Enumeration for different types of filesystem issues
enum class FsckIssueType {
    INVALID_INODE,
    ORPHANED_INODE,
    DUPLICATE_BLOCK,
    UNREFERENCED_BLOCK,
    DIRECTORY_LOOP,
    INCORRECT_LINK_COUNT,
    INVALID_BLOCK_POINTER
};

// Structure to store details about filesystem issues
struct FsckIssue {
    FsckIssueType type;
    int inode_num;
    int block_num;
    std::string description;
    bool can_fix;
};

class FileSystemCheck {
  private:
    FileSystem *fs;
    std::vector<FsckIssue> issues;

    // Tracking arrays for block and inode usage
    bool *block_used;
    bool *inode_used;
    int *inode_link_counts;

    // Check for various issues
    void check_inodes();
    void check_directory_structure();
    void check_blocks();
    void check_superblock();

    // Fix issues
    void fix_invalid_inode(int inode_num);
    void fix_orphaned_inode(int inode_num);
    void fix_duplicate_block(int block_num);
    void fix_unreferenced_block(int block_num);
    void fix_directory_loop(int inode_num);
    void fix_incorrect_link_count(int inode_num);
    void fix_invalid_block_pointer(int inode_num, int block_index);

  public:
    FileSystemCheck(FileSystem *fs);
    ~FileSystemCheck();

    // Run fsck and return list of issues
    std::vector<FsckIssue> check();

    // Fix all fixable issues
    void fix_all_issues();

    // Fix specific issue
    void fix_issue(int issue_index);
};

#endif // FSCK_H
