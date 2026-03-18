# NovaLang Build Guide

This guide covers building NovaLang from source using CMake (the recommended approach) with GCC on Linux and Windows.

---

## Table of Contents

1. [Linux (CMake)](#linux-cmake)
2. [Windows (CMake + MinGW)](#windows-cmake--mingw)
3. [Adding Nova to PATH](#adding-nova-to-path)
4. [Usage](#usage)

---

## Linux (CMake)

### Prerequisites

Ensure you have CMake and a C++ compiler installed:

```bash
# Debian/Ubuntu
sudo apt update
sudo apt install cmake g++

# Fedora
sudo dnf install cmake gcc-c++

# Arch Linux
sudo pacman -S cmake gcc

# macOS (with Homebrew)
brew install cmake
```

### Build

```bash
# Navigate to the project directory
cd /path/to/NovaLang

# Create a build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build the project
cmake --build . --config Release
```

The executable will be created at `build/nova`.

### Optimized Build

For better performance, you can use Release mode with optimizations:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

---

## Windows (CMake + MinGW)

### Prerequisites

1. **Install CMake**: Download from [cmake.org](https://cmake.org/download/) or via [Chocolatey](https://chocolatey.org/): `choco install cmake`
2. **Install MinGW-w64**: Download from [MSYS2](https://www.msys2.org/) (recommended)

### Build

Open a MinGW-w64 terminal (or MSYS2 MinGW x64) and run:

```bash
# Navigate to the project directory
cd /c/path/to/NovaLang

# Create a build directory
mkdir build && cd build

# Configure with CMake (using MinGW generator)
cmake -G "MinGW Makefiles" ..

# Build the project
cmake --build . --config Release
```

The executable will be created at `build/nova.exe`.

### Alternative: Visual Studio

If you prefer Visual Studio:

```powershell
# Open Developer Command Prompt for VS
cd C:\path\to\NovaLang
mkdir build
cd build
cmake -G "Visual Studio 17 2022" ..
cmake --build . --config Release
```

---

## Adding Nova to PATH

### Linux

**Option 1: Add to ~/.local/bin (recommended)**

```bash
# Copy the nova binary
cp build/nova ~/.local/bin/
```

**Option 2: Add to /usr/local/bin**

```bash
sudo cp build/nova /usr/local/bin/
```

**Option 3: Add to ~/.bashrc**

```bash
# Add to ~/.bashrc or ~/.zshrc
echo 'export PATH="$PATH:/path/to/NovaLang/build"' >> ~/.bashrc
source ~/.bashrc
```

### Windows

**Option 1: User PATH (recommended)**

1. Press `Win + R`, type `sysdm.cpl`, press Enter
2. Go to **Advanced** → **Environment Variables**
3. Under **User variables**, select **Path** → **Edit**
4. Click **New** and add the directory containing `nova.exe` (e.g., `C:\path\to\NovaLang\build`)
5. Click **OK** on all dialogs

**Option 2: Using PowerShell**

```powershell
# Add for current session
$env:PATH += ";C:\path\to\NovaLang\build"

# Permanent (run as Administrator)
[Environment]::SetEnvironmentVariable(
    "Path",
    $env:Path + ";C:\path\to\NovaLang\build",
    "User"
)
```

---

## Usage

### Running NovaLang Files

Execute a NovaLang source file:

```bash
# Linux
nova path/to/file.nv

# Windows
nova.exe path/to/file.nv
```

NovaLang supports both `.nv` and `.nova` file extensions.

### Inspecting Bytecode

Use the `-bytec` flag to compile a file to bytecode and inspect it:

```bash
# Linux
nova -bytec path/to/file.nv

# Windows
nova.exe -bytec path/to/file.nv
```

This will:
1. Parse and compile the source file to bytecode
2. Saves the bytecode to a `.cnv` file (e.g., `file.cnv`)
3. Print the output path

**Example:**

```bash
# Create a simple NovaLang file
echo 'println("Hello, World!")' > hello.nv

# Run it
nova hello.nv
# Output: Hello, World!

# Inspect bytecode
nova -bytec hello.nv
# Output: Bytecode written to: hello.cnv
```

### REPL Mode

Run NovaLang interactively without specifying a file:

```bash
# Linux
nova

# Windows
nova.exe
```

Available REPL commands:
- `version` or `ver` - Show NovaLang version
- `run <file>` - Execute a file from within REPL
- `exit` or `quit` - Exit the REPL

### Command Summary

| Command | Description |
|---------|-------------|
| `nova file.nv` | Execute a NovaLang file |
| `nova -bytec file.nv` | Compile to bytecode and save to `.cnv` file |
| `nova` | Start interactive REPL |

---

## Troubleshooting

### Linux

**Error: "cmake: command not found"**
```bash
sudo apt install cmake  # Debian/Ubuntu
sudo dnf install cmake  # Fedora
```

**Error: "g++: command not found"**
```bash
sudo apt install g++  # Debian/Ubuntu
```

### Windows

**Error: "cmake: command not found"**
Install CMake or ensure it's in your PATH.

**Error: "cmake generator not found"**
Make sure MinGW-w64 is installed and use the correct terminal (MinGW x64, not MSYS2 bash).

---

## Quick Reference

```bash
# Clone and build on Linux
git clone https://github.com/Novalang-Project/Novalang.git
cd NovaLang
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
cp nova ~/.local/bin/

# Windows (MinGW)
# 1. Install CMake and MinGW-w64
# 2. Run in MinGW terminal:
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
cmake --build .
# 3. Add to PATH (see Windows section)
```
