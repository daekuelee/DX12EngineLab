#pragma once
// Scene/SceneContract.h
// Day4 P0 PR#1 - Self-test declaration

namespace Scene {

// Runs contract self-test (Debug-only, static-once guard)
// Verifies: CellKey ordering, round-trip, size invariants, conflict policy, base primitives
void RunContractSelfTest();

} // namespace Scene
