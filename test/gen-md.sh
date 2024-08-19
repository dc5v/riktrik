#!/bin/bash

DIR=src

for FILE in "$DIR"/*; do
  if [ -f "$FILE" ]; then
    echo "\`\`\`$FILE"
    cat "$FILE"
    echo "\`\`\`"
    echo
  fi
done

echo "\`\`\`Makefile"
cat "Makefile"
echo "\`\`\`"
echo

echo "\`\`\`config.yml"
cat "config.yml"
echo "\`\`\`"
echo