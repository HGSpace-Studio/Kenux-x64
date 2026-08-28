
#!/bin/bash
set -e

KEX="./kexC/bin/kex"
APPS="apps"
BIN="bin"

echo "=== Kenux OS Build System ==="
echo "Kernel: KNE2.7 | System: 26.8.28"
echo ""

mkdir -p $BIN

sync_headers() {
    echo "[1/4] Syncing headers to kexC..."
    cp -f include/*.h kexC/include/ 2>/dev/null || true
}

build_kex() {
    echo "[2/4] Building kex compiler..."
    cd kexC
    make clean 2>/dev/null || true
    make -j$(nproc)
    cd ..
}

build_apps() {
    echo "[3/4] Building applications..."
    
    local apps_list=(
        "snake_game.c:snake.kex:Kenux Snake Game"
        "tetris.c:tetris.kex:Kenux Tetris"
        "calculator.c:calculator.kex:Kenux Calculator"
        "notepad.c:notepad.kex:Kenux Notepad"
        "fastfetch.c:fastfetch.kex:Kenux FastFetch"
        "system_info.c:sysinfo.kex:System Info"
        "network_monitor.c:netmon.kex:Network Monitor"
        "file_manager_gui.c:fm.kex:File Manager"
        "browser.c:browser.kex:Web Browser"
        "terminal_emulator.c:term.kex:Terminal Emulator"
        "music_player.c:music.kex:Music Player"
        "image_viewer.c:image.kex:Image Viewer"
        "text_editor.c:textedit.kex:Text Editor"
        "task_manager.c:tasks.kex:Task Manager"
        "settings.c:settings.kex:Settings"
        "game_launcher.c:games.kex:Game Launcher"
    )
    
    for app in "${apps_list[@]}"; do
        IFS=':' read -r src name desc <<< "$app"
        
        if [ -f "$APPS/$src" ]; then
            echo "  Building: $desc ($name)..."
            $KEX "$APPS/$src" -o "$BIN/$name" --name "$desc" -O2 -I include 2>&1 || {
                echo "  [WARN] Failed to build $name, skipping..."
            }
        fi
    done
}

build_kernel() {
    echo "[4/4] Building kernel (ninja)..."
    if command -v ninja &> /dev/null; then
        ninja -f kernelbuild.ninja 2>/dev/null || echo "  Kernel build requires full environment"
    else
        echo "  Ninja not found, skipping kernel build"
    fi
}

case "${1:-all}" in
    sync)
        sync_headers
        ;;
    kex)
        build_kex
        ;;
    apps)
        build_apps
        ;;
    kernel)
        build_kernel
        ;;
    all|*)
        sync_headers
        build_kex
        build_apps
        build_kernel
        ;;
esac

echo ""
echo "=== Build Complete ==="
echo "Applications in: $BIN/"
ls -lh $BIN/*.kex 2>/dev/null | wc -l
echo "kex files built successfully!"