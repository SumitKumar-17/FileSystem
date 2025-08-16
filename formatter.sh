
#!/bin/bash

# This script formats all C/C++ files in the specified directory

# Directory containing the files to format
DIR="./" 

# Function to check the distribution and install clang-format if needed
install_clang_format() {
    if ! command -v clang-format &> /dev/null; then
        echo "clang-format is not installed. Installing it now..."
        
        if [ -f /etc/debian_version ]; then
            # Debian/Ubuntu
            sudo apt-get install -y clang-format
        elif [ -f /etc/arch-release ]; then
            # Arch Linux
            sudo pacman -S clang
        elif [ -f /etc/fedora-release ]; then
            # Fedora
            sudo dnf install clang-tools-extra
        else
            echo "Unsupported distribution. Please install clang-format manually."
            exit 1
        fi
    fi
}

create_style_file() {
    cat > .clang-format << EOF
---
Language: Cpp
BasedOnStyle: LLVM
IndentWidth: 4
UseTab: Never
BreakBeforeBraces: Attach
IndentCaseLabels: true
SpaceBeforeParens: ControlStatements
SpacesInParentheses: false
AlignTrailingComments: true
BreakBeforeBinaryOperators: None
ColumnLimit: 100
IndentPPDirectives: None
SpaceAfterCStyleCast: false
SpaceBeforeAssignmentOperators: true
IndentWrappedFunctionNames: false
AllowShortFunctionsOnASingleLine: None
AllowShortIfStatementsOnASingleLine: false
AllowShortLoopsOnASingleLine: false
AlwaysBreakAfterReturnType: None
ContinuationIndentWidth: 4
PointerAlignment: Right
KeepEmptyLinesAtTheStartOfBlocks: false
MaxEmptyLinesToKeep: 1
EOF
}

main() {
    install_clang_format
    create_style_file
    
    echo "Formatting all C/C++ files in the directory: $DIR"
    
    find "$DIR" -type f \( -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) | while read -r file
    do
        echo "Formatting: $file"
        clang-format -i -style=file "$file" 
    done
    
    echo "Formatting complete!"
}

main
