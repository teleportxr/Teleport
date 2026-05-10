#!/usr/bin/env bash
# POSIX equivalent of tidy_files.bat: runs HTML Tidy over every *.html file
# under a directory, in-place, via .new sidecar files.

TIDY=$1
DIR=$2

if [ -z "$TIDY" ] || [ -z "$DIR" ]; then
    echo "usage: $0 <tidy-executable> <directory>" >&2
    exit 1
fi

echo "inputs $TIDY $DIR"
echo "TIDY $TIDY"

cd "$DIR" || exit 1

while IFS= read -r -d '' file; do
    target="${file%.html}.new"
    echo "$file"
    echo "call $TIDY $file > $target"
    "$TIDY" "$file" > "$target" || true
done < <(find . -type f -name '*.html' -print0)

while IFS= read -r -d '' new; do
    html="${new%.new}.html"
    cp -f "$new" "$html"
done < <(find . -type f -name '*.new' -print0)

find . -type f -name '*.new' -delete
