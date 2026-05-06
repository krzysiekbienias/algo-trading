#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "❌ Usage: ./generate_scaffold.sh <folder> <name>"
  echo "   Example: ./generate_scaffold.sh greedy assign_mice_to_holes"
  exit 1
fi

FOLDER="$1"
NAME="$2"

# Validate simple naming (optional but helpful)
[[ "$FOLDER" =~ ^[A-Za-z0-9_]+$ ]] || { echo "❌ Invalid folder name: $FOLDER"; exit 1; }
[[ "$NAME" =~ ^[A-Za-z0-9_]+$ ]] || { echo "❌ Invalid task name: $NAME"; exit 1; }

# Required existing category folders (DON'T create them)
[[ -d "src/${FOLDER}" ]] || { echo "❌ Missing folder: src/${FOLDER}"; exit 1; }
[[ -d "header/${FOLDER}" ]] || { echo "❌ Missing folder: header/${FOLDER}"; exit 1; }
[[ -d "unit_tests/${FOLDER}" ]] || { echo "❌ Missing folder: unit_tests/${FOLDER}"; exit 1; }

CPP_FILE="src/${FOLDER}/${NAME}.cpp"
HEADER_FILE="header/${FOLDER}/${NAME}.hpp"
TEST_FILE="unit_tests/${FOLDER}/test_${NAME}.cpp"

create_if_missing() {
  local path="$1"
  local content="$2"
  if [[ -e "$path" ]]; then
    echo "⚠️  SKIP (exists): $path"
  else
    printf "%s" "$content" > "$path"
    echo "✅ CREATED: $path"
  fi
}

create_if_missing "$CPP_FILE" \
$'#include <string>\n#include <vector>\n\n'

create_if_missing "$HEADER_FILE" \
$'#pragma once\n\n#include <string>\n#include <vector>\n\n'

create_if_missing "$TEST_FILE" \
"#include \"${FOLDER}/${NAME}.hpp\"
#include <gtest/gtest.h>"

