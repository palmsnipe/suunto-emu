#!/bin/sh
set -eu

filter=${TEST_FILTER-}
groups=tools/test_groups.tsv
selected=0
available=
group_names=

if [ ! -f "$groups" ]; then
    echo "error: missing test group registry: $groups" >&2
    exit 2
fi

case "$filter" in
    *[!A-Za-z0-9_.-]*)
        echo "error: TEST_FILTER must contain only letters, digits, '.', '_' or '-'" >&2
        exit 2
        ;;
esac

if [ -n "$filter" ]; then
    group_names=$(awk -F '\t' -v wanted="$filter" \
        '$1 == wanted && $1 !~ /^#/ { print $2 }' "$groups")
fi

for source in "$@"; do
    name=$(basename "$source" .c)
    available="$available $name"
    match=0
    if [ -z "$filter" ]; then
        match=1
    elif echo "$name" | grep -F -q -e "$filter"; then
        match=1
    elif grep -E 'test_[A-Za-z0-9_]+|TEST_TAGS:' "$source" |
         grep -F -q -e "$filter"; then
        match=1
    else
        for group_name in $group_names; do
            if [ "$group_name" = "$name" ]; then
                match=1
                break
            fi
        done
    fi
    if [ "$match" -eq 1 ]; then
        echo "$source"
        selected=$((selected + 1))
    fi
done

if [ "$selected" -eq 0 ]; then
    echo "error: TEST_FILTER='$filter' matched no test source, case, or group" >&2
    echo "available test binaries:$available" >&2
    groups_available=$(awk -F '\t' '$1 !~ /^#/ { print $1 }' "$groups" |
        sort -u | tr '\n' ' ')
    echo "available groups: $groups_available" >&2
    exit 2
fi
