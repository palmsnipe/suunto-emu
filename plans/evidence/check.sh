#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
default_ledger="$script_dir/phase3-5.tsv"

fail() {
    printf '%s\n' "phase3-5 evidence: invalid: $*" >&2
    return 1
}

validate_ledger() {
    ledger=$1

    [ -r "$ledger" ] || fail "ledger is not readable: $ledger" || return 1

    awk -F '\t' '
        NR == 1 {
            expected = "id\tstatus\tproduct\tfirmware_sha256\tsource_path\tsource_symbol\ttrace_path\ttrace_sha256\tobservation\tnegative_evidence"
            if ($0 != expected) {
                print "header mismatch" > "/dev/stderr"
                bad = 1
            }
            next
        }
        NF != 10 {
            print "row " NR " does not have 10 TSV fields" > "/dev/stderr"
            bad = 1
            next
        }
        {
            if ($1 == "" || $3 == "" || $4 == "" || $5 == "" ||
                $6 == "" || $9 == "" || $10 == "") {
                print "row " NR " has a blank required field" > "/dev/stderr"
                bad = 1
            }
            if ($2 != "verified" && $2 != "missing") {
                print "row " NR " has unknown status: " $2 > "/dev/stderr"
                bad = 1
            }
            if (length($4) != 64 || $4 !~ /^[0-9a-f]+$/) {
                print "row " NR " has an invalid firmware SHA-256" > "/dev/stderr"
                bad = 1
            }
            if (($7 == "") != ($8 == "")) {
                print "row " NR " has only one trace field" > "/dev/stderr"
                bad = 1
            }
            if ($8 != "" && (length($8) != 64 || $8 !~ /^[0-9a-f]+$/)) {
                print "row " NR " has an invalid trace SHA-256" > "/dev/stderr"
                bad = 1
            }
            if ($2 == "verified" && substr($9, 1, 7) != "static:" && $8 == "") {
                print "verified behavioral row " $1 " has no trace hash" > "/dev/stderr"
                bad = 1
            }
            if ($0 ~ /(^|\t)\/(Users|home)\// || $0 ~ /(^|\t)[A-Za-z]:\\Users\\/) {
                print "row " NR " contains a machine-specific home path" > "/dev/stderr"
                bad = 1
            }
            count++
        }
        END {
            if (count != 35) {
                print "expected 35 evidence rows, found " count > "/dev/stderr"
                bad = 1
            }
            exit bad
        }
    ' "$ledger" || return 1

    expected_ids=$(cat <<'EOF'
E-A4-CLK-001
E-A4-PWR-001
E-A4-RST-001
E-A4-GPIO-001
E-A4-TIMER-001
E-A4-STIMER-001
E-A4-UART-001
E-A4-IOM-001
E-A4-MSPI-001
E-A4-DMA-001
E-A4-MRAM-001
E-SAP-PROFILE-001
E-SAP-FLASH-001
E-SAP-PANEL-001
E-SAP-BUTTONS-001
E-SAP-BACKLIGHT-001
E-SAP-HSPPAD143-001
E-SAP-LSM6DSL-001
E-SAP-TLI493D-001
E-SAP-HAPTIC-001
E-SAP-OPT3007-001
E-SAP-MAX17050-001
E-SAP-CXD5610-001
E-SAP-OHR2-001
E-SAP-COMPAT-PROD-001
E-SAP-COMPAT-GPS-001
E-SAP-COMPAT-OHR-001
E-NEMA-RING-001
E-NEMA-LISTS-001
E-NEMA-TEXTURE-001
E-NEMA-A2LE-001
E-NEMA-PANEL-001
E-SAP-INPUT-REPLAY-001
E-SAP-COMPAT-RESOURCE-001
E-SAP-COMPAT-GPS-004
EOF
)
    actual_ids=$(awk -F '\t' 'NR > 1 { print $1 }' "$ledger")
    [ "$actual_ids" = "$expected_ids" ] || fail "required evidence IDs/order mismatch" || return 1

    duplicates=$(printf '%s\n' "$actual_ids" | sort | uniq -d)
    [ -z "$duplicates" ] || fail "duplicate evidence ID: $duplicates" || return 1
}

expect_refusal() {
    candidate=$1
    label=$2
    if validate_ledger "$candidate" >/dev/null 2>&1; then
        fail "refusal case accepted: $label"
        return 1
    fi
}

run_refusal_cases() {
    ledger=$1
    temp_dir=$(mktemp -d "${TMPDIR:-/tmp}/semu-phase3-5-check.XXXXXX")
    trap 'rm -rf "$temp_dir"' EXIT HUP INT TERM

    awk 'NR == 1 { print; next } NR == 2 { print; print; next } { print }' \
        "$ledger" > "$temp_dir/duplicate.tsv"
    expect_refusal "$temp_dir/duplicate.tsv" "duplicate ID"

    awk -F '\t' 'BEGIN { OFS = "\t" } NR == 2 { $2 = "unknown" } { print }' \
        "$ledger" > "$temp_dir/status.tsv"
    expect_refusal "$temp_dir/status.tsv" "unknown status"

    awk -F '\t' 'BEGIN { OFS = "\t" } NR == 2 { $5 = "" } { print }' \
        "$ledger" > "$temp_dir/source.tsv"
    expect_refusal "$temp_dir/source.tsv" "absent source"

    awk -F '\t' 'BEGIN { OFS = "\t" } NR == 2 { $4 = toupper($4) } { print }' \
        "$ledger" > "$temp_dir/sha.tsv"
    expect_refusal "$temp_dir/sha.tsv" "uppercase SHA-256"

    awk -F '\t' 'BEGIN { OFS = "\t"; changed = 0 }
        NR == 1 { print; next }
        changed == 0 && $2 == "verified" && substr($9, 1, 7) != "static:" {
            $7 = ""
            $8 = ""
            changed = 1
        }
        { print }' "$ledger" > "$temp_dir/trace.tsv"
    expect_refusal "$temp_dir/trace.tsv" "verified row without trace hash"
}

ledger=${1:-$default_ledger}
validate_ledger "$ledger"
run_refusal_cases "$ledger"
printf '%s\n' 'phase3-5 evidence: valid'
