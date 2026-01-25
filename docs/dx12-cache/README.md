# DX12 Knowledge Cache

Repo-local DX12 reference to eliminate repeated research and debugging. This cache is the primary source of truth for binding rules, debugging playbooks, and authoritative external references.

---

## Document Map

| Document | Purpose | When to Use |
|----------|---------|-------------|
| [pinned-sources.md](pinned-sources.md) | Curated external URLs with re-fetch policy | Before web searching; after finding useful reference |
| [binding-rules.md](binding-rules.md) | DX12 descriptor/heap/root sig contracts | Before implementing bindings; when debugging GPU crashes |
| [project-abi.md](project-abi.md) | Project-specific root sig ABI | **Before any root signature change**; when adding shaders |
| [glossary.md](glossary.md) | Terms and naming conventions | When encountering unfamiliar DX12/project terminology |
| [debug/README.md](debug/README.md) | Symptom-based debug index | First stop when debugging; links to day packets |
| [patterns/ms-samples-notes.md](patterns/ms-samples-notes.md) | Microsoft samples analysis | When integrating new DX12 features |
| [patterns/triple-buffer-ring.md](patterns/triple-buffer-ring.md) | Frame context pattern | When modifying per-frame resources |

---

## Cache-First Workflow

### Before Implementing
1. Check `project-abi.md` for current bindings
2. Check `binding-rules.md` for DX12 constraints
3. Check `patterns/` for relevant patterns

### Before Debugging
1. Scan `debug/README.md` symptom table
2. Follow link to detailed day packet if exists
3. Check `binding-rules.md` for common GPU crash causes

### Before Web Search
1. Check `pinned-sources.md` for existing reference
2. If not found, search externally
3. **MUST summarize back**: Add URL + note to `pinned-sources.md`

### After Resolving Issue
1. Add one-line entry to `debug/README.md` symptom table
2. Create detailed packet in `docs/contracts/dayX/DayX_Debug_IssuePacket_<Name>.md`

---

## When External Lookup is Allowed

- Cache has no relevant entry
- Need authoritative confirmation of edge case
- **Summarize back immediately** to prevent re-research

---

## Update Triggers

| Document | Update When |
|----------|-------------|
| `pinned-sources.md` | Found new useful reference |
| `binding-rules.md` | Discovered undocumented DX12 behavior |
| `project-abi.md` | **BEFORE** any root signature change |
| `debug/README.md` | After resolving any issue (one-liner + link) |
| `contracts/dayX/*.md` | Detailed issue narrative (day-scoped) |
| `glossary.md` | New term introduced in codebase |
| `patterns/*.md` | New pattern discovered or integrated |

---

## Design Rationale

- **Cache (`dx12-cache/`)**: Persistent, concise reference material
- **Day packets (`contracts/dayX/`)**: Bulky debugging narratives scoped to development phase
- `debug/README.md` is a thin index pointing to day packets, NOT a dumping ground
