#!/bin/bash
# Copyright (c) 2026 Sky
#
# This file is part of the Acer-Nitro-power-tray project and is subject to the
# MIT license included in the LICENSE file in the root directory.

set -e

# Configuration
SOURCE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
REMOTE_URL="git@github.com:Sky-Wright/Acer-Nitro-power-tray.git"

echo "============================================"
echo "Acer-Nitro-power-tray GitHub Backup Script"
echo "============================================"

# Navigate to source
cd "$SOURCE_DIR" || { echo "Error: Could not change to $SOURCE_DIR"; exit 1; }

# Initialize git if not present
if [ ! -d ".git" ]; then
    echo "Initializing new git repository..."
    git init
    git branch -M main
fi

# Force Remote to SSH
if git remote | grep -q "origin"; then
    CURRENT_URL=$(git remote get-url origin)
    if [ "$CURRENT_URL" != "$REMOTE_URL" ]; then
        echo "Updating remote 'origin' to SSH..."
        git remote set-url origin "$REMOTE_URL"
    fi
else
    echo "Adding remote 'origin'..."
    git remote add origin "$REMOTE_URL"
fi

# Show status
echo ""
echo "============================================"
echo "Git Status:"
echo "============================================"
git status --short

# Check if there are changes (including untracked files)
if [ -z "$(git status --porcelain)" ]; then
    echo ""
    echo "No changes to commit."
    exit 0
fi

# Prompt for commit message
if [ "$1" == "push" ] && [ -n "$2" ]; then
    COMMIT_MSG="$2"
    echo "Using provided commit message: $COMMIT_MSG"
elif [ -n "$1" ] && [ "$1" != "push" ]; then
    COMMIT_MSG="$1"
    echo "Using provided commit message: $COMMIT_MSG"
else
    echo ""
    echo "============================================"
    echo "Enter commit message:"
    echo "(Press Enter for default: 'Backup: power-tray - [date]')"
    echo "============================================"
    read -p "> " COMMIT_MSG
fi

# Use default if empty
if [ -z "$COMMIT_MSG" ]; then
    COMMIT_MSG="Backup: power-tray - $(date '+%Y-%m-%d %H:%M:%S')"
    echo "Using default message: $COMMIT_MSG"
fi

# Stage and Commit
echo ""
echo "Staging files..."
git add .

echo "Committing changes..."
git commit -m "$COMMIT_MSG"
echo "Changes committed."

# Confirm push
echo ""
if [ "$1" == "push" ]; then
    CONFIRM_PUSH="Y"
    echo "Auto-push enabled via arguments."
else
    echo "============================================"
    read -p "Push to GitHub? (Y/n): " CONFIRM_PUSH
    CONFIRM_PUSH=${CONFIRM_PUSH:-Y}
fi

if [[ "$CONFIRM_PUSH" =~ ^[Yy]$ ]]; then
    echo "Pushing to GitHub (SSH)..."
    git branch -M main
    git push -u origin main
    echo ""
    echo "============================================"
    echo "Backup Complete!"
    echo "============================================"
else
    echo "Push cancelled. Changes committed locally only."
fi
