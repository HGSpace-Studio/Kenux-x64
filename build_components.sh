#!/bin/bash

# Kenux OS Component Build Script
# This script builds all the implemented components

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_section() {
    echo -e "\n${BLUE}===========================================${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}===========================================${NC}"
}

# Create build directories
BUILD_DIR="build"
mkdir -p $BUILD_DIR/bin
mkdir -p $BUILD_DIR/lib
mkdir -p $BUILD_DIR/include

print_status "Starting Kenux OS Component Build"

# Phase 1: Foundation Components
print_section "Phase 1: Building Foundation Components"

# Build bash
print_status "Building bash shell..."
gcc -o $BUILD_DIR/bin/bash src/bash/shell.c -I./include -Wall -Wextra -O2
if [ $? -eq 0 ]; then
    print_status "bash built successfully"
else
    print_error "Failed to build bash"
    exit 1
fi

# Build make
print_status "Building make utility..."
gcc -o $BUILD_DIR/bin/make src/make/make.c -I./include -Wall -Wextra -O2
if [ $? -eq 0 ]; then
    print_status "make built successfully"
else
    print_error "Failed to build make"
    exit 1
fi

# Build ld (linker)
print_status "Building ld linker..."
gcc -o $BUILD_DIR/bin/ld src/ld/ld.c -I./include -Wall -Wextra -O2
if [ $? -eq 0 ]; then
    print_status "ld built successfully"
else
    print_error "Failed to build ld"
    exit 1
fi

# Build mkfs
print_status "Building mkfs..."
gcc -o $BUILD_DIR/bin/mkfs src/mkfs/mkfs.c -I./include -Wall -Wextra -O2
if [ $? -eq 0 ]; then
    print_status "mkfs built successfully"
else
    print_error "Failed to build mkfs"
    exit 1
fi

# Build dd
print_status "Building dd..."
gcc -o $BUILD_DIR/bin/dd src/dd/dd.c -I./include -Wall -Wextra -O2
if [ $? -eq 0 ]; then
    print_status "dd built successfully"
else
    print_error "Failed to build dd"
    exit 1
fi

# Build fastfetch
print_status "Building fastfetch..."
gcc -o $BUILD_DIR/bin/fastfetch src/fastfetch/fastfetch.c -I./include -Wall -Wextra -O2
if [ $? -eq 0 ]; then
    print_status "fastfetch built successfully"
else
    print_error "Failed to build fastfetch"
    exit 1
fi

# Phase 2: Development Components (Placeholder)
print_section "Phase 2: Development Components"
print_status "Development tools (gcc, clang) will be implemented in a future update"

# Phase 3: Display and Virtualization (Placeholder)
print_section "Phase 3: Display and Virtualization"
print_status "Display systems (Wayland, Xorg) and virtualization (qemu) will be implemented in future updates"

# Phase 4: Desktop Environments (Placeholder)
print_section "Phase 4: Desktop Environments"
print_status "Desktop environments (KDE Plasma, GNOME) will be implemented in future updates"

# Phase 5: Applications (Placeholder)
print_section "Phase 5: Applications"
print_status "Firefox and other applications will be implemented in future updates"

# Phase 6: System Management (Placeholder)
print_section "Phase 6: System Management"
print_status "systemd will be implemented in future updates"

# Install components
print_section "Installing Components"
print_status "Installing components to system directories"

# Copy binaries to /bin
cp $BUILD_DIR/bin/bash /bin/
cp $BUILD_DIR/bin/make /bin/
cp $BUILD_DIR/bin/ld /bin/
cp $BUILD_DIR/bin/mkfs /bin/
cp $BUILD_DIR/bin/dd /bin/
cp $BUILD_DIR/bin/fastfetch /bin/

print_status "Components installed successfully"

# Create test script
cat > test_components.sh << 'EOF'
#!/bin/bash

# Test script for Kenux OS Components

echo "Testing Kenux OS Components"
echo "============================="

# Test bash
echo -e "\n1. Testing bash..."
if command -v bash >/dev/null 2>&1; then
    echo "✓ bash is available"
    bash --version | head -n 1
else
    echo "✗ bash is not available"
fi

# Test make
echo -e "\n2. Testing make..."
if command -v make >/dev/null 2>&1; then
    echo "✓ make is available"
    make --version | head -n 1
else
    echo "✗ make is not available"
fi

# Test ld
echo -e "\n3. Testing ld..."
if command -v ld >/dev/null 2>&1; then
    echo "✓ ld is available"
    ld --version | head -n 1
else
    echo "✗ ld is not available"
fi

# Test mkfs
echo -e "\n4. Testing mkfs..."
if command -v mkfs >/dev/null 2>&1; then
    echo "✓ mkfs is available"
    mkfs --help | head -n 5
else
    echo "✗ mkfs is not available"
fi

# Test dd
echo -e "\n5. Testing dd..."
if command -v dd >/dev/null 2>&1; then
    echo "✓ dd is available"
    dd --help | head -n 5
else
    echo "✗ dd is not available"
fi

# Test fastfetch
echo -e "\n6. Testing fastfetch..."
if command -v fastfetch >/dev/null 2>&1; then
    echo "✓ fastfetch is available"
    fastfetch --version
else
    echo "✗ fastfetch is not available"
fi

echo -e "\nTesting complete!"
EOF

chmod +x test_components.sh

print_section "Build Complete"
print_status "All foundation components have been successfully built and installed"
print_status "Run './test_components.sh' to test the installed components"
print_status "Run './fastfetch' to see system information"

echo -e "\n${GREEN}Build completed successfully!${NC}"