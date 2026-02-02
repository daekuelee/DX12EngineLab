# Evidence Bundle Template

Reusable template for documenting test evidence and debug findings.

---

## Template Structure

An evidence bundle has four sections:

1. **Logs** - Captured debug output with interpretation
2. **Screenshots** - Visual proof with HUD visible
3. **Debug Layer** - D3D12 validation status
4. **Trace Notes** - Code path and observed values

Copy the template below and fill in for each investigation.

---

## Template

```markdown
# Evidence: [Feature/Bug Name]

**Date**: YYYY-MM-DD
**Build**: Debug / Release
**Commit**: [short hash]

---

## 1. Logs

### Configuration
- **Prefixes captured**: [e.g., `[SWEEP]`, `[STEP_UP]`]
- **Duration**: [e.g., 10 seconds of gameplay]
- **Actions performed**: [e.g., walked into cube, attempted step-up]

### Log Excerpt

```
[paste relevant log lines here]
```

### Interpretation

[Explain what the log output means. What was expected? What was observed?]

---

## 2. Screenshots

### File Naming Convention
- Pattern: `dayX_feature_state.png`
- Example: `day3_stepup_success.png`

### Screenshot List

| File | Description | Key HUD Values |
|------|-------------|----------------|
| `filename.png` | [What scenario shows] | [Ctrl=X, StepUp=Y, etc.] |

### Annotations

[Note any values highlighted or circles drawn on screenshots]

---

## 3. Debug Layer

### D3D12 Validation Status

- **Error count**: [expected: 0]
- **Warning count**: [note any warnings]
- **Specific messages**: [paste if relevant]

### Verification Method

[How was debug layer checked? VS Output window filter, PIX capture, etc.]

---

## 4. Trace Notes

### Code Path Traced

| Step | File::Symbol | Value Observed |
|------|--------------|----------------|
| 1 | `WorldState::TickFixed()` | [entry conditions] |
| 2 | `SweepXZ_Capsule()` | TOI=[value] |
| 3 | ... | ... |

### Key Assertions

- [ ] Assertion 1: [expected behavior] - [PASS/FAIL]
- [ ] Assertion 2: [expected behavior] - [PASS/FAIL]

### Values of Interest

| Variable | Expected | Observed | Match |
|----------|----------|----------|-------|
| `posY` | 0.0 | 0.0 | Yes |
| `onGround` | true | true | Yes |

---

## Summary

[One paragraph conclusion: what was proven or discovered?]
```

---

## Example Bundle

```markdown
# Evidence: Capsule Step-Up on 0.3 Height Obstacle

**Date**: 2026-01-31
**Build**: Debug
**Commit**: c17d603

---

## 1. Logs

### Configuration
- **Prefixes captured**: `[STEP_UP]`
- **Duration**: 5 seconds
- **Actions performed**: Walked into T1 fixture (0.3 height cube)

### Log Excerpt

```
[STEP_UP] try=1 ok=1 mask=0x00 h=0.300 cube=5045 pos=(10.00,0.30,15.00)
```

### Interpretation

Step-up succeeded. Pawn climbed 0.3 units onto cube 5045 (T1 fixture). Mask 0x00 indicates no failure conditions hit.

---

## 2. Screenshots

### Screenshot List

| File | Description | Key HUD Values |
|------|-------------|----------------|
| `day3_stepup_t1_success.png` | Pawn standing on T1 | Ctrl=Capsule, StepUp=OK h=0.30 |

### Annotations

HUD StepUp section circled showing "OK" status.

---

## 3. Debug Layer

### D3D12 Validation Status

- **Error count**: 0
- **Warning count**: 0
- **Specific messages**: None

### Verification Method

VS Output window, filtered for "D3D12 ERROR".

---

## 4. Trace Notes

### Code Path Traced

| Step | File::Symbol | Value Observed |
|------|--------------|----------------|
| 1 | `TickFixed()` | Capsule mode, reqDx=0.5 |
| 2 | `SweepXZ_Capsule()` | Hit wall, TOI=0.1 |
| 3 | `TryStepUp_Capsule()` | Up sweep clear, forward clear, settle found |
| 4 | `TickFixed()` | Final posY=0.30 |

### Key Assertions

- [x] Step height <= maxStepHeight (0.3) - PASS
- [x] No penetration after step - PASS
- [x] onGround = true after step - PASS

### Values of Interest

| Variable | Expected | Observed | Match |
|----------|----------|----------|-------|
| `stepSuccess` | true | true | Yes |
| `stepHeightUsed` | 0.3 | 0.3 | Yes |
| `stepFailMask` | 0x00 | 0x00 | Yes |

---

## Summary

Capsule step-up correctly climbs 0.3-height obstacles. The up-forward-down sweep sequence completes without hitting any failure conditions. Ground detection works after step.
```

---

## Checklist

Before finalizing an evidence bundle:

- [ ] Date and commit recorded
- [ ] Build configuration noted (Debug/Release)
- [ ] Log prefixes specified
- [ ] Log excerpt has enough context
- [ ] Interpretation explains meaning
- [ ] Screenshots named following convention
- [ ] HUD visible in screenshots
- [ ] Key values annotated
- [ ] Debug layer error count = 0 (or explained)
- [ ] Code path shows relevant symbols
- [ ] Assertions have clear PASS/FAIL
- [ ] Summary draws conclusion
