#ifndef JOURNAL_H
#define JOURNAL_H

#include <string>
#include <vector>

class FileSystem; // Forward declaration

const int JOURNAL_MAGIC = 0xDEADBEEF;

enum JournalRecordType {
    TRANSACTION_START,
    METADATA_UPDATE, // For inodes or other metadata blocks
    DATA_UPDATE,     // For data blocks
    TRANSACTION_COMMIT
};

struct JournalRecordHeader {
    JournalRecordType type;
    int block_num; // The original block number in the filesystem
    int size;      // Size of the data in this record
};

struct JournalTransaction {
    int id;
    std::vector<JournalRecordHeader> records;
};

class Journal {
  private:
    FileSystem *fs;
    int start_block;
    int num_blocks;
    int current_block;
    int next_transaction_id;
    bool active_transaction;

    void write_journal_block(int block_offset, const char *data, int size);
    void read_journal_block(int block_offset, char *data, int size);

  public:
    Journal(FileSystem *fs, int start_block, int num_blocks);

    void begin_transaction();
    void log_metadata_block(int block_num, const char *data);
    void log_data_block(int block_num, const char *data);
    void commit_transaction();
    void recover();
};

#endif // JOURNAL_H
