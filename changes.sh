#!/usr/bin/env bash

if [ -z "$1" ]; then
    echo "usage: ./changes.sh \"description of what changed\""
    exit 1
fi

TIMESTAMP=$(date '+%Y-%m-%d %H:%M')

{
    echo "## $TIMESTAMP"
    echo "$1"
    echo ""
} >> CHANGELOG.md

echo "logged to CHANGELOG.md"
