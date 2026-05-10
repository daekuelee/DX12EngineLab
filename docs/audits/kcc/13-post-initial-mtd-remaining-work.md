# Post Initial MTD Remaining Work

## Current Stop Point

Visible KCC bugs are considered fixed enough to stop for now after adding the
initial-overlap recovery path.

Implemented stop-point contract:

- `startPenetrating` is no longer consumed as a movement, landing, or slide hit.
- Initial overlap recovery is event-scoped and uses `radius + contactOffset`.
- The existing pre/post `Recover` remains actual-radius hard penetration cleanup.
- Trace separates `recoverPush` from `initialRecoverPush`.

Do not continue refactoring just because old plans mention larger KCC work. The
next items below are deferred unless a concrete repro proves they are needed.

## Deferred Work

1. `FindFloorForLanding` / `IsValidLandingSpot`
   - Purpose: separate Falling landing validation from Walking support.
   - Needed only if fake landing, edge landing, or jump-cancel bugs return.
   - Expected policy: descending-only landing, walkable normal, support location,
     floor distance, and edge/perch checks.

2. Floor quality / perch / edge support
   - Purpose: reject floor hits where the capsule is only barely supported by an
     edge or corner.
   - Unreal reference remains useful, but do not copy the full system blindly.
   - Defer until maps include ledges/ramps or a repro shows fake support.

3. Time-budget Walking/Falling continuation
   - Purpose: allow same-tick transitions to consume remaining movement time.
   - Examples: land then continue sliding/walking; leave ledge then continue
     falling.
   - Risk: high. Do after landing/support contracts are stable.

4. Reactive Walking step-up transaction
   - Purpose: attempt stair step only after a real lateral blocker while Walking.
   - Not needed for current visible bug set.
   - Must not reintroduce old pre-lift StepUp semantics.

5. Full MTD / candidate-stream recovery
   - Purpose: improve multi-contact seam/corner recovery if all-contact sum is
     insufficient.
   - PhysX-like future shape: temporal bounds -> touched geometry stream ->
     sequential MTD iterations.
   - Defer unless `initialRecoverPush` still fails to clear seam/corner cases.

6. Debug/trace cleanup
   - Purpose: rename legacy `StepMove`/`StepDown` trace slots once movement
     semantics stop changing.
   - Defer until KCC behavior is stable enough that renaming will not hide bugs.

## Resume Rule

Resume work only from a new repro trace. Pick exactly one lane:

- fake landing / jump cancel -> `FindFloorForLanding`
- edge/perch support -> floor quality
- land-and-continue motion -> time-budget loop
- stair behavior -> reactive Walking step-up
- remaining initial overlap stuck -> full MTD / candidate stream

Do not run multiple lanes in one session.
