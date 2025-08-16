cmake_minimum_required(VERSION 3.10)

project(MyFileSystemProject VERSION 1.0 LANGUAGES CXX)

# --- Find Qt libraries ---
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Widgets REQUIRED)

# --- Include Directories ---
include_directories(include)

# --- Add Executable ---
add_executable(FileSystemUI
    src/main.cpp
    src/core/filesystem.cpp
    src/core/journal.cpp
    src/core/fsck.cpp
    src/core/search.cpp
    src/core/quota.cpp
    src/core/snapshot.cpp
    src/ui/mainwindow.cpp
    src/ui/mainwindow.ui # Explicitly add the UI file
    src/ui/filesystem_detector.cpp
    src/ui/filesystem_mount_dialog.cpp
    src/ui/tree_view_manager.cpp
)

# --- Link Libraries ---
target_link_libraries(FileSystemUI PRIVATE Qt::Widgets)

# Add the header with Q_OBJECT to the sources to ensure AUTOMOC processes it.
target_sources(FileSystemUI PRIVATE
    include/ui/mainwindow.h
    include/core/filesystem.h
    include/core/journal.h
    include/core/fsck.h
    include/core/search.h
    include/core/quota.h
    include/core/snapshot.h
    include/ui/filesystem_detector.h
    include/ui/filesystem_mount_dialog.h
    include/ui/tree_view_manager.h
)
