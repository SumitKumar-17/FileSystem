#include "core/filesystem.h"
#include "core/fsck.h"
#include <iostream>
#include <sys/stat.h>

// Function to fix invalid block pointers
void fixInvalidBlockPointers(FileSystem* fs) {
    // Create a FileSystemCheck object
    FileSystemCheck fsck(fs);
    
    // Check for issues
    std::vector<FsckIssue> issues = fsck.check();
    
    // Fix any fixable issues
    fsck.fix_all_issues();
    
    std::cout << "Fixed " << issues.size() << " filesystem issues." << std::endl;
}

// Function to fix lost+found directory and move orphaned inodes there
void createLostAndFound(FileSystem* fs) {
    // First, check if lost+found exists
    std::vector<DirEntry> root_entries = fs->get_dir_entries(0);
    bool lost_found_exists = false;
    
    for (const auto& entry : root_entries) {
        if (std::string(entry.name) == "lost+found") {
            lost_found_exists = true;
            break;
        }
    }
    
    // If lost+found doesn't exist, create it
    if (!lost_found_exists) {
        // Go to root directory
        fs->cd("/");
        
        // Create lost+found directory
        fs->mkdir("lost+found");
        
        std::cout << "Created lost+found directory" << std::endl;
    }
}
