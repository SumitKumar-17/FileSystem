#include "core/search.h"
#include <queue>
#include <algorithm>
#include <regex>

FileSystemSearch::FileSystemSearch(FileSystem* fs) : fs(fs) {
}

void FileSystemSearch::add_name_criteria(const std::string& name) {
    SearchCriteria criteria;
    criteria.type = SearchCriteriaType::NAME;
    criteria.stringValue = name;
    this->criteria.push_back(criteria);
}

void FileSystemSearch::add_size_greater_than(int size) {
    SearchCriteria criteria;
    criteria.type = SearchCriteriaType::SIZE_GREATER_THAN;
    criteria.intValue = size;
    this->criteria.push_back(criteria);
}

void FileSystemSearch::add_size_less_than(int size) {
    SearchCriteria criteria;
    criteria.type = SearchCriteriaType::SIZE_LESS_THAN;
    criteria.intValue = size;
    this->criteria.push_back(criteria);
}

void FileSystemSearch::add_modified_after(time_t time) {
    SearchCriteria criteria;
    criteria.type = SearchCriteriaType::MODIFIED_AFTER;
    criteria.timeValue = time;
    this->criteria.push_back(criteria);
}

void FileSystemSearch::add_modified_before(time_t time) {
    SearchCriteria criteria;
    criteria.type = SearchCriteriaType::MODIFIED_BEFORE;
    criteria.timeValue = time;
    this->criteria.push_back(criteria);
}

void FileSystemSearch::add_file_type(const std::string& type) {
    SearchCriteria criteria;
    criteria.type = SearchCriteriaType::FILE_TYPE;
    criteria.stringValue = type;
    this->criteria.push_back(criteria);
}

void FileSystemSearch::add_permission(int perm) {
    SearchCriteria criteria;
    criteria.type = SearchCriteriaType::PERMISSION;
    criteria.intValue = perm;
    this->criteria.push_back(criteria);
}

void FileSystemSearch::clear_criteria() {
    criteria.clear();
}

bool FileSystemSearch::match_criteria(const Inode& inode, const std::string& name, const std::string& path) {
    // If no criteria, match everything
    if (criteria.empty()) {
        return true;
    }
    
    // Check all criteria
    for (const auto& criteria : this->criteria) {
        switch (criteria.type) {
            case SearchCriteriaType::NAME: {
                // Use regex for name matching
                std::regex pattern(criteria.stringValue, std::regex_constants::icase);
                if (!std::regex_search(name, pattern)) {
                    return false;
                }
                break;
            }
            
            case SearchCriteriaType::SIZE_GREATER_THAN:
                if (inode.size <= criteria.intValue) {
                    return false;
                }
                break;
                
            case SearchCriteriaType::SIZE_LESS_THAN:
                if (inode.size >= criteria.intValue) {
                    return false;
                }
                break;
                
            case SearchCriteriaType::MODIFIED_AFTER:
                if (inode.modification_time <= criteria.timeValue) {
                    return false;
                }
                break;
                
            case SearchCriteriaType::MODIFIED_BEFORE:
                if (inode.modification_time >= criteria.timeValue) {
                    return false;
                }
                break;
                
            case SearchCriteriaType::FILE_TYPE: {
                bool matches = false;
                if (criteria.stringValue == "file" && inode.mode == 1) {
                    matches = true;
                } else if (criteria.stringValue == "dir" && inode.mode == 2) {
                    matches = true;
                } else if (criteria.stringValue == "symlink" && inode.mode == 3) {
                    matches = true;
                }
                
                if (!matches) {
                    return false;
                }
                break;
            }
                
            case SearchCriteriaType::PERMISSION: {
                // Check if permissions match (assuming mode has UNIX-style perms in lower bits)
                if ((inode.mode & 0777) != criteria.intValue) {
                    return false;
                }
                break;
            }
        }
    }
    
    // All criteria matched
    return true;
}

void FileSystemSearch::search_directory(int dir_inode, const std::string& current_path, std::vector<SearchResult>& results) {
    // Get directory entries
    std::vector<DirEntry> entries = fs->get_dir_entries(dir_inode);
    
    for (const auto& entry : entries) {
        // Skip . and .. entries
        std::string name = entry.name;
        if (name == "." || name == "..") {
            continue;
        }
        
        // Get inode for this entry
        Inode inode = fs->get_inode(entry.inode_num);
        
        // Build path for this entry
        std::string path = current_path.empty() ? name : current_path + "/" + name;
        
        // Check if this entry matches search criteria
        if (match_criteria(inode, name, path)) {
            SearchResult result;
            result.path = path;
            result.inode_num = entry.inode_num;
            results.push_back(result);
        }
        
        // If this is a directory, recurse into it
        if (inode.mode == 2) {
            search_directory(entry.inode_num, path, results);
        }
    }
}

std::vector<SearchResult> FileSystemSearch::search() {
    std::vector<SearchResult> results;
    
    // Start search from root directory
    search_directory(0, "", results);
    
    return results;
}
