#!/bin/bash

# Optionally unset DISPLAY if you do not want to use X11:
# unset DISPLAY
# unset QT_PLUGIN_PATH

# Set up environment variables for EGLFS (direct framebuffer) usage.
# export XDG_RUNTIME_DIR=/run/user/0
# export QT_QPA_PLATFORM=wayland
# export WAYLAND_DISPLAY=/run/wayland-1
# If you need a specific KMS integration plugin on i.MX8, uncomment:
# export QT_QPA_EGLFS_INTEGRATION=eglfs_kms_egldevice
# or
# export QT_QPA_EGLFS_INTEGRATION=eglfs_kms

# Set GStreamer plugin path if needed
# export GST_PLUGIN_PATH=/usr/lib/aarch64-linux-gnu/gstreamer-1.0/


MAX_SIZE_MB=10
BACKUP_DIR="/home/x_user/my_camera_project/old_logs"
FILES=(
    "/home/x_user/my_camera_project/FOLOG.log"
    "/home/x_user/my_camera_project/Outputs.log"
    "/home/x_user/my_camera_project/Errors.log"
    "/home/x_user/my_camera_project/battery_log_book.csv"
)

# Create backup directory if not exists
mkdir -p "$BACKUP_DIR"

# Check each file
for file in "${FILES[@]}"; do
    if [ -f "$file" ]; then
        # Get file size in bytes
        size=$(stat -c %s "$file")
        max_size=$((MAX_SIZE_MB * 1024 * 1024))  # Convert MB to bytes

        if [ "$size" -gt "$max_size" ]; then
            # Create timestamped backup filename
            timestamp=$(date +%Y%m%d-%H%M%S)
            filename=$(basename "$file")
            backup_file="${BACKUP_DIR}/${filename}.${timestamp}"

            # Copy and truncate original file
            cp "$file" "$backup_file"
            truncate -s 0 "$file"
            
            echo "Copied oversized file: $file -> $backup_file"
        fi
    fi
done
# before make clean and make 
# run this find . -exec touch {} \;
# Now launch the Qt application
cd /home/x_user/my_camera_project
./my_camera_project