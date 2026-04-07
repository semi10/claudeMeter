---
name: commit-message
description: Create a commit message by analyzing staged git changes. Use this skill whenever the user asks to commit, write a commit message, stage and commit, or says things like "commit my changes", "what should I call this commit", or "help me write a commit". Trigger even if they don't use the word "commit" explicitly — context like "save my progress" or "I'm done with this feature" is enough.
---

## Step 1: Gather information

**From git:**

- Current git status: !`git status`
- Current staged diff: !`git diff --staged`

**From the conversation:**

Scan the current conversation for signals about *why* these changes were made — bug reports the user mentioned, features they described, problems they were solving, or design decisions they explained. This context often produces a far richer commit body than the diff alone. If the conversation doesn't reveal motivation, the diff is enough.

## Step 2: Analyze and write commit message(s)

Read the diff and ask: **do these changes tell one story, or several?**

Signs the changes are **one cohesive commit**:
- All changes serve the same purpose (e.g., a bug fix rippling across multiple files)
- You can summarize everything in a single imperative sentence without needing "and also"

Signs the changes are **multiple unrelated concerns**:
- The diff spans visibly different areas (e.g., a new feature, a config tweak, and a docs update)
- The changes fall into clearly different types (feat + fix + chore, etc.)
- You'd need "and" or "also" to describe them all honestly in one line

Each message uses this format:

```
<emoji> <type>: <concise description in imperative mood>

<optional body explaining the motivation or context>
```

| Emoji | Type | When to use |
|-------|------|-------------|
| ✨ | `feat` | New feature or capability |
| 🐛 | `fix` | Bug fix |
| 🔨 | `refactor` | Code restructuring with no behavior change |
| 📝 | `docs` | Documentation only |
| 🎨 | `style` | Formatting, whitespace, naming (no logic change) |
| ✅ | `test` | Adding or updating tests |
| ⚡ | `perf` | Performance improvement |
| 🔧 | `chore` | Build config, dependencies, tooling |

**Description line:** imperative mood, under 72 characters ("add login screen", not "added").

**Body (optional):** include when the *why* isn't obvious from the diff. Prefer paraphrasing the user's own words when available.

## Step 3: Present and commit

**If the changes are cohesive** — show a brief summary of what's staged and your proposed message, then **immediately run `git commit`**. Don't ask "shall I proceed?" — Claude Code's permission system surfaces an Allow/Deny prompt natively.

```bash
git commit -m "<subject line>"
# or, with a body:
git commit -m "<subject line>" -m "<body paragraph>"
```

If the user denies or asks for changes, revise and attempt again.

**Example (cohesive):**

User mentioned: "the search was hammering the API on every keypress"

```
⚡ perf: debounce search input to reduce API calls

Previously every keystroke triggered a fetch. This batches requests
with a 300ms delay after the user stops typing, cutting API load
significantly during fast input.
```

---

**If the changes span multiple unrelated concerns** — don't commit yet. Present one message per concern, each with a short label naming the logical area, then ask the user how they'd like to proceed (commit everything under one message, split into separate commits themselves, etc.).

**Example (multiple concerns):**

```
I found 3 unrelated concerns in the staged changes:

**Auth flow**
✨ feat: add JWT-based login flow

**Build config**
🔧 chore: bump CMake minimum version to 3.20

**Docs**
📝 docs: document hardware pin mapping

How would you like to handle these? I can commit everything under one message,
or you can split the staged files and I'll commit them one at a time.
```

Once the user decides, carry out their choice.
