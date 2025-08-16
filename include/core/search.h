#ifndef SEARCH_H
#define SEARCH_H

#include "filesystem.h"
#include <functional>
#include <string>
#include <vector>

enum class SearchCriteriaType {
    NAME,
    SIZE_GREATER_THAN,
    SIZE_LESS_THAN,
    MODIFIED_AFTER,
    MODIFIED_BEFORE,
    FILE_TYPE,
    PERMISSION
};

struct SearchCriteria {
    SearchCriteriaType type;
    std::string stringValue;
    int intValue;
    time_t timeValue;
};

struct SearchResult {
    std::string path;
    int inode_num;
    bool is_dir;
    int size;
    time_t modification_time;
};

class FileSystemSearch {
  private:
    FileSystem *fs;
    std::vector<SearchCriteria> criteria;

    bool match_criteria(const Inode &inode, const std::string &name, const std::string &path);
    void search_directory(int dir_inode, const std::string &current_path,
                          std::vector<SearchResult> &results);

  public:
    FileSystemSearch(FileSystem *fs);

    // Add search criteria
    void add_name_criteria(const std::string &name);
    void add_size_greater_than(int size);
    void add_size_less_than(int size);
    void add_modified_after(time_t time);
    void add_modified_before(time_t time);
    void add_file_type(const std::string &type); // "file", "dir", "symlink"
    void add_permission(int perm);

    // Execute search
    std::vector<SearchResult> search();

    // Clear all criteria
    void clear_criteria();
};

#endif // SEARCH_H
