#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include "filesystem.h"
#include <ctime>
#include <string>
#include <vector>

struct SnapshotInfo {
    std::string name;
    time_t creation_time;
    int blocks_used;
};

class SnapshotManager {
  private:
    FileSystem *fs;
    std::string snapshot_dir;

    // Copy data blocks for snapshot
    void copy_blocks(const Inode &src_inode, int dest_inode_num);

    // Copy a directory recursively
    void copy_directory(int src_dir_inode, int dest_dir_inode);

    // Create a snapshot directory if it doesn't exist
    int ensure_snapshot_directory();

    // Calculate blocks used by a directory tree
    int calculate_blocks_used(int dir_inode);

    // Get snapshot info
    std::vector<SnapshotInfo> get_snapshots_info();

  public:
    SnapshotManager(FileSystem *fs);

    // Create a new snapshot
    bool create_snapshot(const std::string &name);

    // Restore from a snapshot
    bool restore_snapshot(const std::string &name);

    // Delete a snapshot
    bool delete_snapshot(const std::string &name);

    // List available snapshots
    std::vector<SnapshotInfo> list_snapshots();

    // Get a specific snapshot's info
    SnapshotInfo get_snapshot_info(const std::string &name);
};

#endif // SNAPSHOT_H
