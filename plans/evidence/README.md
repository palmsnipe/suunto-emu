# Phase 3–5 evidence inventory

This ledger is the evidence gate for tickets 300–520. It contains paths,
symbols, hashes, and bounded observations only; it does not contain firmware,
capture bytes, frame pixels, or compatibility payloads.

The inspected `suunto-firmware` source revision is:

```text
540a4a55b0a82dc080111572cc27b0f68de73c92
```

Source-tree paths in the TSV are repository-relative references to that
evidence checkout. The Sapporo firmware package was hashed locally as:

```text
00eba9e7e2a06263894d11030692daf6f3744f8937d84e0f3ff3c078b0516f41
```

`verified` means that the row's narrow observation has an exact source symbol
and, for behavioral evidence, a reproducible lowercase SHA-256 trace. It does
not promote unobserved modes or claim physical-device equivalence. `missing`
means the row records the primary source and any partial trace, but an
acceptance requirement remains absent; the exact gap is written in
`observation` and `negative_evidence`.

The static profile and compatibility-record provenance rows intentionally have
empty trace fields. All other verified behavioral rows have both trace fields
populated. Raw source and capture artifacts remain outside this repository.

Validate the ledger and its positive/refusal checks with:

```sh
sh plans/evidence/check.sh
awk -F '\t' 'NR>1 && $2=="missing"{print $1}' plans/evidence/phase3-5.tsv
```

The second command lists every downstream evidence gate that remains blocked.
After ticket 298 is integrated, the ticket's manifest filter is expected to
run `test_manifest` without changing this ledger.
