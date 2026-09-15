#!/bin/sh
set -eu

index=plans/index.tsv
required_sections='Goal
Execution Budget
Required Reading
Current Baseline
Allowed Files
Frozen Interfaces
Evidence Inputs
Implementation
Tests and Commands
Acceptance
Forbidden Scope
Handoff'

normalize_dependencies()
{
    normalized=$(printf '%s' "$1" | tr -d '`\r ')
    if [ "$normalized" = - ]; then
        normalized=none
    fi
    printf '%s\n' "$normalized"
}

if [ ! -f "$index" ]; then
    echo "error: missing task index: $index" >&2
    exit 1
fi

failed=0
count=0
seen='|'
rows_file=${TMPDIR:-/tmp}/semu-task-contracts-$$
trap 'rm -f "$rows_file"' EXIT HUP INT TERM
: > "$rows_file"
tab=$(printf '\t')
exec 3< "$index"
IFS="$tab" read -r header_id header_phase header_title header_status \
    header_dependencies header_ticket header_extra <&3 || failed=1
if [ "${header_id-}" != id ] || [ "${header_ticket-}" != ticket ] ||
   [ -n "${header_extra-}" ]; then
    echo "error: invalid plans/index.tsv header" >&2
    failed=1
fi

while IFS="$tab" read -r id phase title status dependencies ticket extra <&3; do
    if [ -z "$id" ]; then
        continue
    fi
    count=$((count + 1))
    case "$id" in
        [0-9][0-9][0-9]) ;;
        *) echo "error: invalid ticket id in index: $id" >&2; failed=1 ;;
    esac
    case "$seen" in
        *"|$id|"*) echo "error: duplicate ticket id in index: $id" >&2; failed=1 ;;
        *) seen="$seen$id|" ;;
    esac
    case "$status" in
        blocked|ready|in-progress|done|deferred) ;;
        *) echo "error: invalid status for ticket $id: $status" >&2; failed=1 ;;
    esac
    case "$phase" in
        ''|*[!0-9]*) echo "error: invalid phase for ticket $id: $phase" >&2; failed=1 ;;
    esac
    if [ -n "$extra" ] || [ -z "$phase" ] || [ -z "$title" ] ||
       [ -z "$status" ] || [ -z "$dependencies" ] || [ -z "$ticket" ]; then
        echo "error: incomplete index row for ticket $id" >&2
        failed=1
        continue
    fi
    printf '%s\t%s\t%s\n' "$id" "$dependencies" "$status" >> "$rows_file"
    case "$dependencies" in
        -|none)
            if [ "$phase" != 0 ]; then
                echo "error: only phase-0 root tickets may have no dependencies: $id" >&2
                failed=1
            fi
            ;;
    esac
    if [ ! -f "$ticket" ]; then
        echo "error: indexed ticket does not exist: $ticket" >&2
        failed=1
        continue
    fi
    basename_id=$(basename "$ticket" | cut -c 1-3)
    if [ "$basename_id" != "$id" ]; then
        echo "error: ticket basename does not start with id $id: $ticket" >&2
        failed=1
    fi
    if ! grep -q "^# $id " "$ticket"; then
        echo "error: ticket heading does not match id $id: $ticket" >&2
        failed=1
    fi
    ticket_status=$(sed -n 's/^\*\*Status:\*\*[[:space:]]*//p' "$ticket" |
        head -1 | awk '{ gsub(/`/, "", $1); print $1 }')
    if [ "$ticket_status" != "$status" ]; then
        echo "error: ticket status '$ticket_status' does not match index '$status': $ticket" >&2
        failed=1
    fi
    ticket_phase=$(sed -n \
        's/.*\*\*Phase:\*\*[[:space:]]*`\{0,1\}\([0-9][0-9]*\).*/\1/p' \
        "$ticket" | head -1)
    if [ -z "$ticket_phase" ]; then
        echo "error: ticket is missing numeric **Phase:** metadata: $ticket" >&2
        failed=1
    elif [ "$ticket_phase" != "$phase" ]; then
        echo "error: ticket phase '$ticket_phase' does not match index '$phase': $ticket" >&2
        failed=1
    fi
    ticket_dependencies=$(sed -n \
        's/^\*\*Dependencies:\*\*[[:space:]]*//p' "$ticket" | head -1)
    if [ -z "$ticket_dependencies" ]; then
        echo "error: ticket is missing **Dependencies:** metadata: $ticket" >&2
        failed=1
    elif [ "$(normalize_dependencies "$ticket_dependencies")" != \
           "$(normalize_dependencies "$dependencies")" ]; then
        echo "error: ticket dependencies do not match index '$dependencies': $ticket" >&2
        failed=1
    fi
    if [ "$status" = ready ] &&
       grep -E -q '<(id|gap|version|product)>' "$ticket"; then
        echo "error: ready ticket contains unresolved placeholder: $ticket" >&2
        failed=1
    fi
    old_ifs=$IFS
    newline='
'
    IFS=$newline
    for section in $required_sections; do
        if ! grep -F -x -q "## $section" "$ticket"; then
            echo "error: $ticket is missing section: $section" >&2
            failed=1
        fi
    done
    IFS=$old_ifs
done
exec 3<&-

while IFS="$tab" read -r id dependencies row_status; do
    case "$dependencies" in
        -|none) continue ;;
    esac
    old_ifs=$IFS
    IFS=,
    for dependency in $dependencies; do
        IFS=$old_ifs
        if [ "$dependency" = "$id" ]; then
            echo "error: ticket $id depends on itself" >&2
            failed=1
        fi
        case "$dependency" in
            [0-9][0-9][0-9]) ;;
            *) echo "error: invalid dependency '$dependency' for ticket $id" >&2
               failed=1; continue ;;
        esac
        if ! grep -q "^$dependency$tab" "$rows_file"; then
            echo "error: ticket $id has unknown dependency: $dependency" >&2
            failed=1
        elif [ "$row_status" = ready ] || [ "$row_status" = done ]; then
            dependency_status=$(awk -F "$tab" -v wanted="$dependency" \
                '$1 == wanted { print $3; exit }' "$rows_file")
            if [ "$dependency_status" != done ]; then
                echo "error: $row_status ticket $id depends on non-done ticket $dependency" >&2
                failed=1
            fi
        fi
        IFS=,
    done
    IFS=$old_ifs
done < "$rows_file"

file_count=$(find plans/tasks -type f -name '[0-9][0-9][0-9]-*.md' | wc -l |
    tr -d ' ')
if [ "$file_count" -ne "$count" ]; then
    echo "error: index has $count tickets but plans/tasks has $file_count" >&2
    failed=1
fi

if [ "$failed" -ne 0 ]; then
    exit 1
fi
echo "task contracts: $count indexed tickets validated"
