//-------------------------------------------------------------------------
// WorldTypes_compilecheck.cpp - Compile-only proof that WorldTypes.h is self-contained
//
// PURPOSE: This TU includes ONLY WorldTypes.h (no other project headers).
//   If it compiles, WorldTypes.h has no missing dependencies.
//
// BUILD: Included in project, compiles to .obj, links to nothing meaningful.
//-------------------------------------------------------------------------

#include "WorldTypes.h"

// Trivial function to produce linkable symbol (prevents "no symbols" warning)
int WorldTypes_compilecheck_dummy() { return 0; }
