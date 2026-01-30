# Day4 P0 Data Model

## Type Hierarchy

```
Scene/
├── CellKey              # Grid cell identifier
├── CellKeyHash          # Hash functor for unordered_map
├── PrimitiveKind        # Enum: Grid, Floor, KillZone
├── GridPrimitive        # Grid configuration
├── FloorPrimitive       # Floor configuration
├── KillZonePrimitive    # Kill zone configuration
├── StaticObject         # Composition of primitives
├── OverlayOpType        # Enum: Add, Remove, Modify
├── OverlayOp            # Single overlay operation
├── OverlayOps           # Collection with conflict policy
├── InstanceData         # Render instance data
├── CollisionCell        # Collision cell data
├── RenderView           # Render output
├── CollisionView        # Collision output
├── BaseSceneSource      # Base scene container
└── ScopedDisableDebugBreak  # RAII guard for tests
```

## Type Schemas

### CellKey
```cpp
struct CellKey {
    uint16_t ix = 0;
    uint16_t iz = 0;

    uint32_t ToLinearIndex(uint32_t gridSizeX) const;
    static CellKey FromLinearIndex(uint32_t idx, uint32_t gridSizeX);
    bool operator==(const CellKey& other) const;
};
```

### GridPrimitive
```cpp
struct GridPrimitive {
    uint32_t sizeX = 100;           // Default: legacy 100x100
    uint32_t sizeZ = 100;
    float spacing = 2.0f;
    float originX = -100.0f;
    float originZ = -100.0f;
    float renderHalfExtent = 1.0f;
    float collisionHalfExtent = 1.0f;

    uint32_t TotalCells() const;    // Returns sizeX * sizeZ
};
```

### FloorPrimitive
```cpp
struct FloorPrimitive {
    float posY = 0.0f;
    float halfExtentX = 100.0f;
    float halfExtentZ = 100.0f;
};
```

### KillZonePrimitive
```cpp
struct KillZonePrimitive {
    float posY = -50.0f;
};
```

### StaticObject
```cpp
struct StaticObject {
    PrimitiveKind kind = PrimitiveKind::Grid;
    GridPrimitive grid;         // Valid if kind == Grid
    FloorPrimitive floor;       // Valid if kind == Floor
    KillZonePrimitive killZone; // Valid if kind == KillZone
};
```

**Design Rationale**: Uses composition instead of union/variant to avoid UB risk. Payload optimization may come in a future PR once API is stable.

### OverlayOp
```cpp
struct OverlayOp {
    CellKey key;
    OverlayOpType type = OverlayOpType::Add;
    std::string source;  // Source identifier for debugging
};
```

### OverlayOps
```cpp
struct OverlayOps {
    std::unordered_map<CellKey, OverlayOp, CellKeyHash> ops;

    bool TryAdd(const OverlayOp& op);  // Returns false on duplicate
    bool HasKey(const CellKey& key) const;

    inline static bool s_enableDebugBreak = true;
};
```

### RenderView / CollisionView
```cpp
struct RenderView {
    std::vector<InstanceData> instances;
};

struct CollisionView {
    std::vector<CollisionCell> cells;
};
```

### BaseSceneSource
```cpp
struct BaseSceneSource {
    std::vector<StaticObject> objects;

    const GridPrimitive* GetGrid() const;
    const FloorPrimitive* GetFloor() const;
    const KillZonePrimitive* GetKillZone() const;

    bool HasGrid() const;
    bool HasFloor() const;
    bool HasKillZone() const;
};
```

## Factory Functions

### CreateDefaultBaseScene()
```cpp
BaseSceneSource CreateDefaultBaseScene();
```

Creates a default base scene with:
- Grid (100x100, spacing=2.0, origin=(-100,-100))
- Floor (y=0, halfExtent=100x100)
- KillZone (y=-50)
