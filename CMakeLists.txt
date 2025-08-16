cmake_minimum_required(VERSION 3.10)

project(MyFileSystemProject VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Widgets REQUIRED)

include_directories(include)

add_executable(FileSystemUI
    src/main.cpp
    src/core/filesystem.cpp
    src/core/journal.cpp
    src/core/fsck.cpp
    src/core/fsck_fixes.cpp
    src/core/search.cpp
    src/core/quota.cpp
    src/core/snapshot.cpp
    src/ui/mainwindow.cpp
    src/ui/mainwindow.ui 
    src/ui/filesystem_detector.cpp
    src/ui/filesystem_local_detector.cpp
    src/ui/filesystem_external_detector.cpp
    src/ui/mainwindow_file_ops.cpp
    src/ui/mainwindow_dialogs.cpp
    src/ui/filesystem_mount_dialog.cpp
    src/ui/tree_view_manager.cpp
)

target_link_libraries(FileSystemUI PRIVATE Qt::Widgets)

target_sources(FileSystemUI PRIVATE
    include/ui/mainwindow.h
    include/core/filesystem.h
    include/core/journal.h
    include/core/fsck.h
    include/core/fsck_fixes.h
    include/core/search.h
    include/core/quota.h
    include/core/snapshot.h
    include/ui/filesystem_detector.h
    include/ui/filesystem_local_detector.h
    include/ui/filesystem_external_detector.h
    include/ui/mainwindow_file_ops.h
    include/ui/mainwindow_dialogs.h
    include/ui/filesystem_mount_dialog.h
    include/ui/tree_view_manager.h
)
