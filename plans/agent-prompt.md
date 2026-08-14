# Agent Dispatch Prompt

Use this wrapper when assigning a ticket to another model. Replace bracketed
fields; do not paste several tickets into one task.

```text
Implement ticket [ID] from plans/tasks/[FILE].

First read AGENTS.md and every document/header/evidence item listed by the
ticket. Work only in Allowed Files. Existing code is a partial baseline, not a
claim that the ticket is complete. Do not change public interfaces, Makefile,
registries, profiles, or plans/index.tsv unless the ticket explicitly owns
them.

Run every exact command in Tests and Commands, including negative/refusal
coverage and the stated budget. If evidence or a frozen interface is missing,
stop fail-closed and report the exact missing item instead of guessing.

Handoff with: changed files; evidence IDs; commands and results; deterministic
artifacts; unsupported cases; and integrator-owned changes requested.
```

Give the model the repository checkout, not copied snippets. The ticket paths,
current source, tests, and Git status are part of the task context. For a model
without repository access, include `AGENTS.md`, the ticket, its required docs,
the named public headers, current owned files, and dependency handoffs.

The integrator should reject a handoff that changes files outside ownership,
uses uncited behavior, omits a refusal test, silently skips a command, or marks
its own status done.
