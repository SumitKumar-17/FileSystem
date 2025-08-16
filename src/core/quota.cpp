#include "core/quota.h"
#include <algorithm>
#include <ctime>
#include <queue>

QuotaManager::QuotaManager(FileSystem *fs)
    : fs(fs), grace_period(7 * 24 * 60 * 60) { // 7 days default
    // Initialize usage
    update_usage();
}

void QuotaManager::set_grace_period(time_t seconds) {
    grace_period = seconds;
}

void QuotaManager::set_user_quota(int uid, int blocks_soft, int blocks_hard, int inodes_soft,
                                  int inodes_hard) {
    QuotaEntry &quota = user_quotas[uid];

    // Preserve current usage values
    int blocks_used = quota.blocks_used;
    int inodes_used = quota.inodes_used;

    // Update limits
    quota.blocks_soft_limit = blocks_soft;
    quota.blocks_hard_limit = blocks_hard;
    quota.inodes_soft_limit = inodes_soft;
    quota.inodes_hard_limit = inodes_hard;

    // Restore usage values
    quota.blocks_used = blocks_used;
    quota.inodes_used = inodes_used;

    // If previously under quota but now over soft limit, start grace period
    if ((blocks_used > blocks_soft && blocks_soft > 0) ||
        (inodes_used > inodes_soft && inodes_soft > 0)) {
        quota.grace_period_start = time(nullptr);
    }
}

void QuotaManager::set_group_quota(int gid, int blocks_soft, int blocks_hard, int inodes_soft,
                                   int inodes_hard) {
    QuotaEntry &quota = group_quotas[gid];

    // Preserve current usage values
    int blocks_used = quota.blocks_used;
    int inodes_used = quota.inodes_used;

    // Update limits
    quota.blocks_soft_limit = blocks_soft;
    quota.blocks_hard_limit = blocks_hard;
    quota.inodes_soft_limit = inodes_soft;
    quota.inodes_hard_limit = inodes_hard;

    // Restore usage values
    quota.blocks_used = blocks_used;
    quota.inodes_used = inodes_used;

    // If previously under quota but now over soft limit, start grace period
    if ((blocks_used > blocks_soft && blocks_soft > 0) ||
        (inodes_used > inodes_soft && inodes_soft > 0)) {
        quota.grace_period_start = time(nullptr);
    }
}

QuotaEntry QuotaManager::get_user_quota(int uid) {
    auto it = user_quotas.find(uid);
    if (it != user_quotas.end()) {
        return it->second;
    }

    // Return empty quota
    QuotaEntry empty;
    empty.blocks_used = 0;
    empty.blocks_soft_limit = 0;
    empty.blocks_hard_limit = 0;
    empty.inodes_used = 0;
    empty.inodes_soft_limit = 0;
    empty.inodes_hard_limit = 0;
    empty.grace_period_start = 0;
    return empty;
}

QuotaEntry QuotaManager::get_group_quota(int gid) {
    auto it = group_quotas.find(gid);
    if (it != group_quotas.end()) {
        return it->second;
    }

    // Return empty quota
    QuotaEntry empty;
    empty.blocks_used = 0;
    empty.blocks_soft_limit = 0;
    empty.blocks_hard_limit = 0;
    empty.inodes_used = 0;
    empty.inodes_soft_limit = 0;
    empty.inodes_hard_limit = 0;
    empty.grace_period_start = 0;
    return empty;
}

bool QuotaManager::is_over_quota(const QuotaEntry &quota, bool check_blocks, bool check_inodes) {
    time_t now = time(nullptr);

    if (check_blocks) {
        // Check hard limit first
        if (quota.blocks_hard_limit > 0 && quota.blocks_used >= quota.blocks_hard_limit) {
            return true;
        }

        // Check soft limit with grace period
        if (quota.blocks_soft_limit > 0 && quota.blocks_used >= quota.blocks_soft_limit) {
            // If grace period has started and expired
            if (quota.grace_period_start > 0 && now - quota.grace_period_start > grace_period) {
                return true;
            }
        }
    }

    if (check_inodes) {
        // Check hard limit first
        if (quota.inodes_hard_limit > 0 && quota.inodes_used >= quota.inodes_hard_limit) {
            return true;
        }

        // Check soft limit with grace period
        if (quota.inodes_soft_limit > 0 && quota.inodes_used >= quota.inodes_soft_limit) {
            // If grace period has started and expired
            if (quota.grace_period_start > 0 && now - quota.grace_period_start > grace_period) {
                return true;
            }
        }
    }

    return false;
}

bool QuotaManager::would_exceed_quota(int uid, int gid, int blocks_needed, int inodes_needed) {
    // Check user quota
    QuotaEntry user_quota = get_user_quota(uid);
    if (is_over_quota(user_quota, blocks_needed > 0, inodes_needed > 0)) {
        return true;
    }

    // Check projected usage for user
    if (user_quota.blocks_hard_limit > 0 &&
        user_quota.blocks_used + blocks_needed > user_quota.blocks_hard_limit) {
        return true;
    }

    if (user_quota.inodes_hard_limit > 0 &&
        user_quota.inodes_used + inodes_needed > user_quota.inodes_hard_limit) {
        return true;
    }

    // Check group quota
    QuotaEntry group_quota = get_group_quota(gid);
    if (is_over_quota(group_quota, blocks_needed > 0, inodes_needed > 0)) {
        return true;
    }

    // Check projected usage for group
    if (group_quota.blocks_hard_limit > 0 &&
        group_quota.blocks_used + blocks_needed > group_quota.blocks_hard_limit) {
        return true;
    }

    if (group_quota.inodes_hard_limit > 0 &&
        group_quota.inodes_used + inodes_needed > group_quota.inodes_hard_limit) {
        return true;
    }

    return false;
}

void QuotaManager::calculate_user_usage() {
    // Reset all user quotas to zero usage
    for (auto &pair : user_quotas) {
        pair.second.blocks_used = 0;
        pair.second.inodes_used = 0;
    }

    // Iterate through all inodes to count usage
    for (int i = 0; i < NUM_INODES; i++) {
        Inode inode = fs->get_inode(i);

        // Skip free inodes
        if (inode.mode == 0)
            continue;

        // Get user for this inode
        int uid = inode.uid;

        // Ensure quota entry exists for this user
        if (user_quotas.find(uid) == user_quotas.end()) {
            user_quotas[uid] = QuotaEntry();
            user_quotas[uid].blocks_used = 0;
            user_quotas[uid].blocks_soft_limit = 0;
            user_quotas[uid].blocks_hard_limit = 0;
            user_quotas[uid].inodes_used = 0;
            user_quotas[uid].inodes_soft_limit = 0;
            user_quotas[uid].inodes_hard_limit = 0;
            user_quotas[uid].grace_period_start = 0;
        }

        // Count inode
        user_quotas[uid].inodes_used++;

        // Count direct blocks
        for (int j = 0; j < 10; j++) {
            if (inode.direct_blocks[j] != 0) {
                user_quotas[uid].blocks_used++;
            }
        }

        // Count indirect block
        if (inode.indirect_block != 0) {
            user_quotas[uid].blocks_used++;

            // Count blocks referenced by indirect block
            char buffer[BLOCK_SIZE];
            fs->read_block(inode.indirect_block, buffer);
            int *block_pointers = (int *)buffer;
            int pointers_per_block = BLOCK_SIZE / sizeof(int);

            for (int j = 0; j < pointers_per_block; j++) {
                if (block_pointers[j] != 0) {
                    user_quotas[uid].blocks_used++;
                }
            }
        }
    }
}

void QuotaManager::calculate_group_usage() {
    // Reset all group quotas to zero usage
    for (auto &pair : group_quotas) {
        pair.second.blocks_used = 0;
        pair.second.inodes_used = 0;
    }

    // Iterate through all inodes to count usage
    for (int i = 0; i < NUM_INODES; i++) {
        Inode inode = fs->get_inode(i);

        // Skip free inodes
        if (inode.mode == 0)
            continue;

        // Get group for this inode
        int gid = inode.gid;

        // Ensure quota entry exists for this group
        if (group_quotas.find(gid) == group_quotas.end()) {
            group_quotas[gid] = QuotaEntry();
            group_quotas[gid].blocks_used = 0;
            group_quotas[gid].blocks_soft_limit = 0;
            group_quotas[gid].blocks_hard_limit = 0;
            group_quotas[gid].inodes_used = 0;
            group_quotas[gid].inodes_soft_limit = 0;
            group_quotas[gid].inodes_hard_limit = 0;
            group_quotas[gid].grace_period_start = 0;
        }

        // Count inode
        group_quotas[gid].inodes_used++;

        // Count direct blocks
        for (int j = 0; j < 10; j++) {
            if (inode.direct_blocks[j] != 0) {
                group_quotas[gid].blocks_used++;
            }
        }

        // Count indirect block
        if (inode.indirect_block != 0) {
            group_quotas[gid].blocks_used++;

            // Count blocks referenced by indirect block
            char buffer[BLOCK_SIZE];
            fs->read_block(inode.indirect_block, buffer);
            int *block_pointers = (int *)buffer;
            int pointers_per_block = BLOCK_SIZE / sizeof(int);

            for (int j = 0; j < pointers_per_block; j++) {
                if (block_pointers[j] != 0) {
                    group_quotas[gid].blocks_used++;
                }
            }
        }
    }
}

void QuotaManager::update_usage() {
    calculate_user_usage();
    calculate_group_usage();

    // Check for new quota violations and start grace periods
    time_t now = time(nullptr);

    for (auto &pair : user_quotas) {
        QuotaEntry &quota = pair.second;

        // Check blocks soft limit
        if (quota.blocks_soft_limit > 0 && quota.blocks_used > quota.blocks_soft_limit &&
            quota.grace_period_start == 0) {
            quota.grace_period_start = now;
        }

        // Check inodes soft limit
        if (quota.inodes_soft_limit > 0 && quota.inodes_used > quota.inodes_soft_limit &&
            quota.grace_period_start == 0) {
            quota.grace_period_start = now;
        }
    }

    for (auto &pair : group_quotas) {
        QuotaEntry &quota = pair.second;

        // Check blocks soft limit
        if (quota.blocks_soft_limit > 0 && quota.blocks_used > quota.blocks_soft_limit &&
            quota.grace_period_start == 0) {
            quota.grace_period_start = now;
        }

        // Check inodes soft limit
        if (quota.inodes_soft_limit > 0 && quota.inodes_used > quota.inodes_soft_limit &&
            quota.grace_period_start == 0) {
            quota.grace_period_start = now;
        }
    }
}
