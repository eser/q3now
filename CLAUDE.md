# context-mode — MANDATORY routing rules

You have context-mode MCP tools available. These rules are NOT optional — they protect your context window from flooding. A single unrouted command can dump 56 KB into context and waste the entire session.

## BLOCKED commands — do NOT attempt these

### curl / wget — BLOCKED
Any Bash command containing `curl` or `wget` is intercepted and replaced with an error message. Do NOT retry.
Instead use:
- `ctx_fetch_and_index(url, source)` to fetch and index web pages
- `ctx_execute(language: "javascript", code: "const r = await fetch(...)")` to run HTTP calls in sandbox

### Inline HTTP — BLOCKED
Any Bash command containing `fetch('http`, `requests.get(`, `requests.post(`, `http.get(`, or `http.request(` is intercepted and replaced with an error message. Do NOT retry with Bash.
Instead use:
- `ctx_execute(language, code)` to run HTTP calls in sandbox — only stdout enters context

### WebFetch — BLOCKED
WebFetch calls are denied entirely. The URL is extracted and you are told to use `ctx_fetch_and_index` instead.
Instead use:
- `ctx_fetch_and_index(url, source)` then `ctx_search(queries)` to query the indexed content

## REDIRECTED tools — use sandbox equivalents

### Bash (>20 lines output)
Bash is ONLY for: `git`, `mkdir`, `rm`, `mv`, `cd`, `ls`, `npm install`, `pip install`, and other short-output commands.
For everything else, use:
- `ctx_batch_execute(commands, queries)` — run multiple commands + search in ONE call
- `ctx_execute(language: "shell", code: "...")` — run in sandbox, only stdout enters context

### Read (for analysis)
If you are reading a file to **Edit** it → Read is correct (Edit needs content in context).
If you are reading to **analyze, explore, or summarize** → use `ctx_execute_file(path, language, code)` instead. Only your printed summary enters context. The raw file content stays in the sandbox.

### Grep (large results)
Grep results can flood context. Use `ctx_execute(language: "shell", code: "grep ...")` to run searches in sandbox. Only your printed summary enters context.

## Tool selection hierarchy

1. **GATHER**: `ctx_batch_execute(commands, queries)` — Primary tool. Runs all commands, auto-indexes output, returns search results. ONE call replaces 30+ individual calls.
2. **FOLLOW-UP**: `ctx_search(queries: ["q1", "q2", ...])` — Query indexed content. Pass ALL questions as array in ONE call.
3. **PROCESSING**: `ctx_execute(language, code)` | `ctx_execute_file(path, language, code)` — Sandbox execution. Only stdout enters context.
4. **WEB**: `ctx_fetch_and_index(url, source)` then `ctx_search(queries)` — Fetch, chunk, index, query. Raw HTML never enters context.
5. **INDEX**: `ctx_index(content, source)` — Store content in FTS5 knowledge base for later search.

## Subagent routing

When spawning subagents (Agent/Task tool), the routing block is automatically injected into their prompt. Bash-type subagents are upgraded to general-purpose so they have access to MCP tools. You do NOT need to manually instruct subagents about context-mode.

## Output constraints

- Keep responses under 500 words.
- Write artifacts (code, configs, PRDs) to FILES — never return them as inline text. Return only: file path + 1-line description.
- When indexing content, use descriptive source labels so others can `ctx_search(source: "label")` later.

## ctx commands

| Command | Action |
|---------|--------|
| `ctx stats` | Call the `ctx_stats` MCP tool and display the full output verbatim |
| `ctx doctor` | Call the `ctx_doctor` MCP tool, run the returned shell command, display as checklist |
| `ctx upgrade` | Call the `ctx_upgrade` MCP tool, run the returned shell command, display as checklist |

<!-- noskills:start -->
## noskills orchestrator

This project uses noskills for state-driven orchestration.
Do NOT read `.eser/rules/`, `.eser/specs/`, or concern files directly.
noskills gives you exactly what you need via JSON output.

### Protocol

    npx eser noskills spec <name> next                           # get current instruction
    npx eser noskills spec <name> next --answer="your response"  # submit result and advance

Every noskills command that operates on a spec MUST include `spec <name>`.
Never omit it. Use `npx eser noskills spec list` to see available specs.

### Why noskills calls matter

noskills is not a form to fill out. It is a live state machine that the user
watches in real-time. Every `npx eser noskills spec <name> next --answer` call:

- Updates the spec file on disk (the user sees it change)
- Updates the terminal dashboard if `noskills watch` is running
- Advances the state machine to the next phase
- Records the decision permanently in the project history

When you batch-submit answers or backfill discovery responses yourself,
the user sees nothing happening — then suddenly everything jumps forward.
This defeats the purpose.

Call noskills ONCE per interaction. Ask the user ONE question. Wait for
their answer. Submit it. Ask the next. The user is watching every step.
Do NOT pre-fill answers. Do NOT batch multiple answers. Do NOT answer
discovery questions yourself — the user's input is the data.

### When to call noskills next

You MUST call `npx eser noskills next` in these situations:

1. At the **START** of every conversation (first thing you do)
2. **BEFORE** creating or modifying any file (to verify you have an active task)
3. **AFTER** completing a logical unit of work (to report progress)
4. When you encounter a **DECISION** that affects architecture or scope
5. When you are **UNSURE** what to do next

NEVER proceed with implementation without checking noskills first.
NEVER make architectural decisions independently — noskills routes them to the user.

### Git is read-only

You MUST NOT run git write commands: commit, add, push, checkout, stash,
reset, merge, rebase, cherry-pick. The user controls git. You control files.
You MAY read from git: log, diff, status, show, blame.

### Interactive choices

When noskills output contains `interactiveOptions`, you MUST present them
using the AskUserQuestion tool. NEVER present options in prose.

This is not optional. If you ask a question without AskUserQuestion when
interactiveOptions are present, you are violating protocol.

Pass interactiveOptions as the `options` array in AskUserQuestion.
Use the `commandMap` to resolve the user's selection to a CLI command.

### Convention discovery

When you notice a recurring pattern, receive a correction, or discover
a project quirk during any phase:

1. Ask: "Should this be a permanent rule?"
2. If yes: `npx eser noskills rule add "description" --phases=EXECUTING`
3. If just this spec: note it and move on

Corrections are learnings. Capture them as rules so they compound.
Every session should leave the project's rule set slightly better.
Never write to `.eser/rules/` directly.

### Creating specs

When the user describes what they want to build, create a spec immediately:
`npx eser noskills spec new "user's description here"`

The name is auto-generated from the description. Encourage the user to share
as much context as possible — detailed requirements produce better discovery.
Do NOT ask for a 'short slug' — just take what the user gives you and create
the spec. A one-liner, meeting notes, task list, or full requirements doc all work.

### Pre-discovery research

When starting a new spec, noskills may flag technologies that need
research. Before asking discovery questions, search for current versions
and API status of flagged technologies. Report findings to the user.
Your training data may be outdated — always verify.

### Plan-based discovery

When a spec is created with --from-plan, the first discovery output includes
the plan content. Read it, extract answers for all 6 questions, present them
for user confirmation. The plan is a starting point, not the final answer.

### Discovery modes

noskills offers 5 discovery modes selected at the start of each spec:
- full: Standard 6 questions (default)
- validate: Challenge the user's existing plan
- technical-depth: Focus on architecture and integration
- ship-fast: Minimum viable scope
- explore: Think bigger, find opportunities

### Premise challenge

After mode selection, challenge 2-4 premises from the spec description.
Present each premise for the user to agree/disagree. Submit as JSON.

### Alternatives generation

Before spec draft, propose 2-3 implementation approaches.
User picks one or skips. The selected approach shapes the spec.

### AI slop prevention

When implementing UI, avoid these generic AI-generated patterns:
- 3-column grids with icon circles (the most recognizable AI layout)
- Purple gradients, decorative blobs, wavy dividers
- Everything centered, uniform border-radius everywhere
- Generic hero copy ("Welcome to...", "Unlock the power of...")
- Cookie-cutter section rhythm

Every UI element must feel intentional, not generated. If it looks like
every other AI-built site, iterate until it doesn't.

### Spec splitting

When noskills suggests splitting a spec, present the options and wait for the
user's decision. Never split a spec without explicit user approval.

### Decision principle: Explicit > Clever

You NEVER skip steps, bypass questions, or make assumptions on behalf of the user.
- Discovery questions → ask the user, don't answer yourself
- Classification → ask the user, don't infer
- Spec approval → ask the user, don't auto-approve
- Task refinement → ask the user, don't self-assign
- Rule promotion → ask the user, don't decide
If you think something can be skipped, ASK "would you like to skip this?" — don't skip it.

### Command execution

When told to run a noskills command, execute it IMMEDIATELY. Do not explore,
research, read source code, or plan first. The command output contains all the
context you need. Exploring noskills internals wastes tokens and delays the user.

### JSON output

noskills returns JSON with a `phase` field and phase-specific instructions.
The `meta` block contains resume context - use it to orient yourself,
especially after compaction or at the start of a new session.
Follow the `instruction` field. Use `transition` commands to advance state.

### Phase progress

Every noskills output includes a roadmap showing the current phase.
Display it to the user. At critical transitions (approve, start execution),
noskills shows a gate with an explicit action. Present these prominently.

### Code quality rules

These rules apply during spec execution:
- After every file edit, run type-check and lint before reporting success
- Files over 500 LOC: read in chunks, never assume single read = full file
- If search returns few results, re-run narrower — assume truncation
- Re-read every file before and after editing
- Before refactoring large files, clean dead code first (separate commit)

### Execution commitment

Once a spec enters EXECUTING, complete it. Do not suggest mid-execution
checkpoints, pauses, or "should we stop here?" questions.

noskills encourages small, meaningful specs defined during discovery.
If a spec is well-scoped, there is no reason to stop halfway — half-done
delivers nothing. If it feels too large mid-execution, that means
discovery should have split it. Finish this one, improve the next.
<!-- noskills:end -->
