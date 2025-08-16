#ifndef FSCK_FIXES_H
#define FSCK_FIXES_H

#include "filesystem.h"

// Function to fix invalid block pointers
void fixInvalidBlockPointers(FileSystem* fs);

// Function to fix lost+found directory and move orphaned inodes there
void createLostAndFound(FileSystem* fs);

#endif // FSCK_FIXES_H
