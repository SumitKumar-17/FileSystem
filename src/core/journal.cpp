#include "core/journal.h"
#include <cstring>
#include <iostream>

Journal::Journal(FileSystem* fs, int start_block, int num_blocks)
    : fs(fs), start_block(start_block), num_blocks(num_blocks), 
      current_block(0), next_transaction_id(1), active_transaction(false) {}

void Journal::write_journal_block(int block_offset, const char* data, int size) {
    // This is a simplified write, assuming data fits within a single block
    // A real implementation would handle spanning across blocks
    char buffer[BLOCK_SIZE] = {0};
    memcpy(buffer, data, size);
    fs->write_block(start_block + block_offset, buffer);
}

void Journal::read_journal_block(int block_offset, char* data, int size) {
    char buffer[BLOCK_SIZE];
    fs->read_block(start_block + block_offset, buffer);
    memcpy(data, buffer, size);
}

void Journal::begin_transaction() {
    if (active_transaction) {
        std::cerr << "Warning: Transaction already active." << std::endl;
        return;
    }
    JournalRecordHeader start_header;
    start_header.type = TRANSACTION_START;
    start_header.block_num = next_transaction_id;
    start_header.size = 0;
    
    write_journal_block(current_block++, (char*)&start_header, sizeof(JournalRecordHeader));
    active_transaction = true;
}

void Journal::log_metadata_block(int block_num, const char* data) {
    if (!active_transaction) return;
    JournalRecordHeader header;
    header.type = METADATA_UPDATE;
    header.block_num = block_num;
    header.size = BLOCK_SIZE; // Assuming full block writes for simplicity

    write_journal_block(current_block++, (char*)&header, sizeof(JournalRecordHeader));
    write_journal_block(current_block++, data, BLOCK_SIZE);
}

void Journal::log_data_block(int block_num, const char* data) {
    if (!active_transaction) return;
    JournalRecordHeader header;
    header.type = DATA_UPDATE;
    header.block_num = block_num;
    header.size = BLOCK_SIZE;

    write_journal_block(current_block++, (char*)&header, sizeof(JournalRecordHeader));
    write_journal_block(current_block++, data, BLOCK_SIZE);
}

void Journal::commit_transaction() {
    if (!active_transaction) return;
    JournalRecordHeader commit_header;
    commit_header.type = TRANSACTION_COMMIT;
    commit_header.block_num = next_transaction_id;
    commit_header.size = 0;

    write_journal_block(current_block++, (char*)&commit_header, sizeof(JournalRecordHeader));
    
    // This is where the checkpointing happens.
    // The journaled blocks are written to their final locations.
    recover(); 

    // Reset journal for next transaction
    current_block = 0; 
    next_transaction_id++;
    active_transaction = false;
}

void Journal::recover() {
    char header_buffer[sizeof(JournalRecordHeader)];
    JournalRecordHeader header;
    int journal_offset = 0;

    read_journal_block(journal_offset++, header_buffer, sizeof(JournalRecordHeader));
    memcpy(&header, header_buffer, sizeof(JournalRecordHeader));

    if (header.type != TRANSACTION_START) {
        // No valid transaction found
        return;
    }

    std::vector<std::pair<int, std::string>> pending_writes;

    while (journal_offset < num_blocks) {
        read_journal_block(journal_offset++, header_buffer, sizeof(JournalRecordHeader));
        memcpy(&header, header_buffer, sizeof(JournalRecordHeader));

        if (header.type == TRANSACTION_COMMIT) {
            // Found commit record, apply changes
            for(const auto& write : pending_writes) {
                fs->write_block(write.first, write.second.c_str());
            }
            break; // Recovery successful for this transaction
        }
        
        if (header.type == METADATA_UPDATE || header.type == DATA_UPDATE) {
            char data_buffer[BLOCK_SIZE];
            read_journal_block(journal_offset++, data_buffer, BLOCK_SIZE);
            pending_writes.push_back({header.block_num, std::string(data_buffer, BLOCK_SIZE)});
        } else {
            // Incomplete or corrupted transaction, stop recovery
            break;
        }
    }
    // After recovery (or if no commit was found), clear the journal
    char empty_block[BLOCK_SIZE] = {0};
    for(int i=0; i < num_blocks; ++i) {
        write_journal_block(i, empty_block, BLOCK_SIZE);
    }
}
