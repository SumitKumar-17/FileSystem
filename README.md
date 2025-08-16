# FileSys - Filesystem Explorer

A modular Qt/C++ filesystem explorer that allows users to interact with both local (.fs) and external filesystems (USB drives, hard drives, SD cards, optical media).

## Features

### Filesystem Operations
- Create, read, update, and delete files in the virtual filesystem
- Navigate directories with browsing and path support
- Mount both local (.fs) files and external filesystems (USB drives, hard drives, etc.)

### External Device Support
- Automatic detection of USB drives, hard drives, SD cards, and optical media
- Display of both mounted and unmounted external devices
- Ability to mount unmounted partitions directly from the UI
- Support for "UNMOUNTED:" prefix for unmounted devices

### Filesystem Maintenance
- Built-in fsck (filesystem check) to identify and repair filesystem issues
- Automatic detection and repair of:
  - Invalid block pointers
  - Orphaned inodes
  - Incorrect link counts
  - Directory loops
- Lost+found directory for recovering orphaned files

### Search
- Quick search with name filtering
- Advanced search with additional criteria
- Results displayed with paths and navigation

### UI Features
- File browser with hierarchical view
- Status bar with current filesystem information
- Quota management for space allocation
- Clean, modular interface

### Technical Details
- Modular codebase with separation of concerns
- Robust error handling and bounds checking
- Support for Unicode filenames
- Journal-based filesystem operations for crash recovery

## Building
```
mkdir build
cd build
cmake ..
make
```

## Usage
Run the application and use the "File > Open" menu to open an existing filesystem or create a new one. You can also use the "Detect Filesystems" feature to find available filesystems on your system.
