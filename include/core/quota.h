#ifndef QUOTA_H
#define QUOTA_H

#include "filesystem.h"
#include <unordered_map>

struct QuotaEntry {
    int blocks_used;
    int blocks_soft_limit;
    int blocks_hard_limit;
    int inodes_used;
    int inodes_soft_limit;
    int inodes_hard_limit;
    time_t grace_period_start;
};

class QuotaManager {
  private:
    FileSystem *fs;
    std::unordered_map<int, QuotaEntry> user_quotas;  // UID to quota
    std::unordered_map<int, QuotaEntry> group_quotas; // GID to quota
    time_t grace_period;                              // Default grace period in seconds (7 days)

    // Calculate current usage
    void calculate_user_usage();
    void calculate_group_usage();

    // Check if specific user/group is over quota
    bool is_over_quota(const QuotaEntry &quota, bool check_blocks, bool check_inodes);

  public:
    QuotaManager(FileSystem *fs);

    // Set quotas
    void set_user_quota(int uid, int blocks_soft, int blocks_hard, int inodes_soft,
                        int inodes_hard);
    void set_group_quota(int gid, int blocks_soft, int blocks_hard, int inodes_soft,
                         int inodes_hard);

    // Get quota information
    QuotaEntry get_user_quota(int uid);
    QuotaEntry get_group_quota(int gid);

    // Check if operation would exceed quota
    bool would_exceed_quota(int uid, int gid, int blocks_needed, int inodes_needed);

    // Update quota usage after file system operation
    void update_usage();

    // Set grace period
    void set_grace_period(time_t seconds);
};

#endif // QUOTA_H
