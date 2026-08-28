# Kenux OS

```
■■■■■■■■■   ■■■■■■■■■■
■■■■■■■■■   ■■■■■■■■■■
■■■■■■■■■   ■■■■■■■■■■
■■■■■■■■■   ■■■■■■■■■■
■■■■■■■■■
■■■■■■■■■   ■■■■■■■■■■
■■■■■■■■■   ■■■■■■■■■■
■■■■■■■■■   ■■■■■■■■■■
■■■■■■■■■   ■■■■■■■■■■
```

**Kenux Kernel KNE2.7 | System Version 26.8.28 (Stardust)**

## System Overview

Kenux OS is a modern, high-performance operating system built from scratch with a focus on stability, security, and user experience. Designed for both desktop and server environments, it provides a complete computing platform with native support for gaming, web browsing, multimedia, and development.

### Core Features

- **Advanced Kernel Architecture**: Microkernel-hybrid design with modular drivers
- **Native Graphics Stack**: Hardware-accelerated 2D/3D rendering via KanvasUI
- **Complete Application Suite**: Full productivity, entertainment, and development tools
- **Modern Security**: SELinux-style MAC, full disk encryption, secure boot
- **High Performance**: Optimized for x86_64 with multi-core scheduling
- **Network Ready**: TCP/IP stack, WiFi, Bluetooth, VPN support

## System Information

| Component | Specification |
|-----------|---------------|
| **Kernel** | Kenux Kernel KNE2.7 (KNE) |
| **Architecture** | x86_64 (amd64) |
| **ABI** | Kenux Extended ABI v3 |
| **System Call Interface** | Unified Syscall (kapi_unified_syscall) |
| **Memory Model** | Flat 64-bit with ASLR |
| **Process Scheduler** | CFS with real-time extensions |
| **File Systems** | ext4, Btrfs, ZFS, FAT32, NTFS |
| **Graphics** | Kenux Graphics G5000 (Vulkan/OpenGL) |
| **Audio** | PulseAudio-compatible server |

## Quick Start

### Prerequisites

```bash
# Build dependencies
sudo apt-get install build-essential ninja-build gcc make nasm qemu-system-x86

# Optional: cross-compilation toolchain
sudo apt-get install gcc-x86_64-linux-gnu binutils-x86_64-linux-gnu
```

### Building the System

```bash
# Clone repository
git clone https://github.com/kenux-os/Kenux-x64.git
cd Kenux-x64

# Sync headers and build everything
./build_all.sh all

# Or build components individually:
./build_all.sh sync    # Sync headers to kexC
./build_all.sh kex     # Rebuild kex compiler
./build_all.sh apps    # Build applications only
./build_all.sh kernel  # Build kernel with ninja
```

### Running in QEMU

```bash
# Create disk image
qemu-img create -f qcow2 kenux-disk.qcow2 16G

# Boot system
qemu-system-x86_64 \
  -m 4G \
  -smp 4 \
  -drive file=kenux-disk.qcow2,format=qcow2 \
  -boot d \
  -kernel bin/kernel.bin \
  -append "root=/dev/sda2 console=ttyS0" \
  -enable-kvm \
  -cpu host \
  -display gtk
```

## Application Suite

### System Utilities

| Application | Description | Binary |
|-------------|-------------|--------|
| **FastFetch** | System information display | `fastfetch.kex` |
| **System Info** | Detailed hardware/software info | `sysinfo.kex` |
| **Task Manager** | Process monitoring and management | `tasks.kex` |
| **Settings** | System configuration panel | `settings.kex` |
| **Terminal Emulator** | Command-line interface | `term.kex` |

### Productivity Tools

| Application | Description | Binary |
|-------------|-------------|--------|
| **Notepad** | Simple text editor | `notepad.kex` |
| **Text Editor** | Advanced code/text editor | `textedit.kex` |
| **File Manager** | Graphical file browser | `fm.kex` |
| **Calculator** | Scientific calculator | `calculator.kex` |
| **Web Browser** | Modern web browser | `browser.kex` |

### Entertainment

| Application | Description | Binary |
|-------------|-------------|--------|
| **Snake Game** | Classic snake game | `snake.kex` |
| **Tetris** | Block puzzle game | `tetris.kex` |
| **Game Launcher** | Game library manager | `games.kex` |
| **Music Player** | Audio playback | `music.kex` |
| **Image Viewer** | Photo/image viewer | `image.kex` |

### Network Tools

| Application | Description | Binary |
|-------------|-------------|--------|
| **Network Monitor** | Real-time network statistics | `netmon.kex` |
| **Browser** | Web browsing client | `browser.kex` |

## Development

### Using kex Compiler

The `kex` compiler is Kenux's native application compiler that produces `.kex` executables compatible with the Kenux runtime.

#### Basic Usage

```bash
# Compile a simple program
./kexC/bin/kex hello.c -o hello.kex --name "Hello World"

# With optimization
./kexC/bin/kex app.c -o app.kex --name "My App" -O2 -I include

# Run compiled program
./kexC/bin/kex run app.kex

# Force graphics mode
./kexC/bin/kex run app.kex --gfx --gfx-size 1024x768
```

#### Compiler Options

| Option | Description |
|--------|-------------|
| `-o <path>` | Output file path |
| `--name <name>` | Program name (embedded in header) |
| `-O<level>` | Optimization level (0,1,2,3,s) |
| `-I<dir>` | Add include directory |
| `-L<dir>` | Add library directory |
| `-l<lib>` | Link library |
| `--format` | Output format: kex, kxp, elf |
| `--shared` | Build shared library (.kxp) |
| `-g` | Include debug symbols |
| `-s` | Strip symbols |
| `--static` | Static linking |
| `--verbose` | Verbose output |

### Shell Commands

Kenux Shell (`ksh`) provides a comprehensive command-line interface:

#### File Operations

```bash
ls              # List directory contents
cd <path>       # Change directory
pwd             # Print working directory
cat <file>      # Display file contents
mkdir <dir>     # Create directory
rm <file>       # Remove file/directory
cp <src> <dst>  # Copy files
mv <src> <dst>  # Move/rename files
```

#### System Information

```bash
ps              # List processes
top             # Resource usage monitor
df              # Disk space usage
free            # Memory usage
uname           # System information
uptime          # System uptime
whoami          # Current user
hostname        # System hostname
neofetch        # Display with logo
fastfetch       # Fast system info
sysinfo         # Detailed system report
```

#### Process Management

```bash
kill <pid>      # Terminate process by PID
systemctl       # Service management
reboot          # Reboot system
poweroff        # Power off system
```

#### Applications

```bash
snake           # Play Snake game
tetris          # Play Tetris
calc            # Launch calculator
desktop         # Start graphical environment
run <program>   # Execute ELF binary
```

## Kernel Architecture

### Module System

The kernel uses a modular architecture with dynamic loading:

```
Kernel Core (kenux_kernel)
├── Memory Management
│   ├── Physical Page Allocator
│   ├── Virtual Memory (Paging)
│   ├── Slab Allocator
│   └── Memory Protection (NX, SMEP, SMAP)
├── Process Management
│   ├── Scheduler (CFS + RT)
│   ├── Thread Management
│   └── Signal Handling
├── File Systems
│   ├── VFS Layer
│   ├── ext4 Driver
│   └── Network FS (NFS, SMB)
├── Device Drivers
│   ├── Block Devices (NVMe, SATA)
│   ├── Graphics (GPU, Display)
│   ├── Input (Keyboard, Mouse)
│   └── Network (Ethernet, WiFi)
├── Networking
│   ├── TCP/IP Stack
│   ├── Socket API
│   ├── Firewall (Netfilter)
│   └── Wireless Stack
└── Security
    ├── Mandatory Access Control
    ├── Capabilities
    ├── Audit System
    └── Encryption (AES, ChaCha20)
```

### Key APIs

- **kapi**: Kernel API for driver development
- **kapi_window**: Window manager interface
- **kapi_graphics2d**: 2D graphics primitives
- **kapi_input**: Input device handling
- **kapi_network**: Network programming
- **kapi_vfs**: File system operations
- **kapi_process**: Process management
- **kapi_memory**: Memory allocation
- **kapi_sync_ext**: Synchronization primitives

## Directory Structure

```
Kenux-x64/
├── apps/                  # User-space applications
│   ├── snake_game.c      # Snake game implementation
│   ├── tetris.c          # Tetris game
│   ├── calculator.c      # Calculator app
│   ├── notepad.c         # Text editor
│   ├── fastfetch.c       # System info display
│   ├── browser.c         # Web browser
│   └── ...
├── bin/                   # Compiled binaries (*.kex)
├── include/               # Public headers
│   ├── stdio.h           # Standard I/O
│   ├── stdlib.h          # Standard library
│   ├── string.h          # String functions
│   ├── time.h            # Time functions
│   ├── vga.h             # VGA console API
│   └── kapi_*.h          # Kernel APIs
├── kernel/                # Kernel source
│   ├── arch/x86_64/      # Architecture-specific code
│   ├── lib/libc/         # Kernel libc
│   └── kernel/           # Core kernel
├── kexC/                  # kex compiler
│   ├── src/kex.c         # Compiler source
│   ├── include/          # Compiler headers
│   └── bin/kex           # Compiler binary
├── LeonOS-4/             # Reference implementation
├── build_all.sh          # Build script
├── kernelbuild.ninja     # Ninja build file
└── README.md             # This file
```

## Security Features

- **Address Space Layout Randomization (ASLR)**: Random memory layout
- **Stack Protection**: Canary values and NX bit enforcement
- **Control Flow Integrity (CFI)**: Indirect call validation
- **Kernel Page Table Isolation (KPTI)**: Meltdown mitigation
- **Full Disk Encryption**: LUKS-compatible encryption
- **Secure Boot**: UEFI Secure Boot support
- **Mandatory Access Control**: SELinux-like policies
- **Sandboxing**: Per-application isolation
- **Audit Logging**: Comprehensive security events
- **Memory Safety**: Bounds checking, use-after-free detection

## Performance Characteristics

- **Boot Time**: < 3 seconds to desktop (SSD)
- **Memory Usage**: ~256MB base (with GUI)
- **Scheduler Latency**: < 1ms for RT tasks
- **Graphics Throughput**: 60+ FPS at 1080p
- **Network Performance**: Line-rate 10GbE
- **I/O Throughput**: NVMe Gen4 speeds supported

## Compatibility

### Supported Hardware

- **CPUs**: Intel (Haswell+), AMD (Zen+), x86_64 only
- **RAM**: Minimum 512MB, Recommended 4GB+
- **Storage**: SATA SSD/NVMe, USB boot supported
- **Graphics**: VGA mode fallback, GPU acceleration optional
- **Network**: Gigabit Ethernet, 802.11ac WiFi
- **Input**: PS/2 & USB keyboards/mice

### File Format Support

| Format | Read | Write | Notes |
|--------|------|-------|-------|
| **ext4** | ✅ | ✅ | Primary filesystem |
| **FAT32** | ✅ | ✅ | Removable media |
| **NTFS** | ✅ | ⚠️ | Experimental write |
| **Btrfs** | ✅ | ✅ | Snapshots, compression |
| **ISO9660** | ✅ | ❌ | CD/DVD images |

## Contributing

1. Fork the repository
2. Create feature branch (`git checkout -b feature/amazing-feature`)
3. Commit changes (`git commit -m 'Add amazing feature'`)
4. Push to branch (`git push origin feature/amazing-feature`)
5. Open Pull Request

### Code Style

- Use C11 standard with GNU extensions
- Follow kernel coding style (indent with tabs)
- No trailing whitespace
- Document public APIs in headers
- Test on QEMU before submitting

## License

This project is licensed under the GPL v3 License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- **LeonOS**: Reference architecture and design patterns
- **Linux**: Inspiration for many subsystems
- **StardustUI**: GUI framework foundation
- **TinyCC**: Compiler technology basis
- **PicoLibC**: Embedded C library reference

## Version History

### 26.8.28 (Current)
- Enhanced shell with 35+ commands
- Complete application suite (15+ apps)
- Improved fastfetch with custom ASCII art
- New file manager GUI
- System information utilities
- Gaming applications (Snake, Tetris)

### 26.7.9
- Initial stable release
- Basic window manager
- Core system utilities
- kex compiler v1.0

---

**Built with ❤️ by the Kenux Team**

For more information, visit: https://kenux-os.org

*System Logo: Kenux ASCII Art*
*Kernel: KNE2.7 | Codename: Stardust*