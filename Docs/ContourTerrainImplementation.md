# Contour Terrain Implementation Notes

## Pipeline Outputs

Run the generator:

`python Tools/TerrainPipeline/generate_terrain_data.py --size 129 --seed 42`

Outputs are written to `Saved/TerrainPipeline/`:

- `heightmap.png`
- `terrain_types.png`
- `facilities.json`

## UE Setup

1. Open the map with `ABattlemap_TestCursorGameMode`.
2. Select the spawned `ATacticalMapGrid` actor.
3. Set `TerrainSurfaceMaterial` to `M_contour` (or an instance `MI_contour`).
4. Ensure the material exposes scalar parameters:
   - `ContourInterval`
   - `ContourLineWidth`
   - `ContourDepthOffset`
   - `ShowCoordinateAxes`
5. Press Play. The grid loads data from `Saved/TerrainPipeline`.

## Runtime Rules Connected To Same Data Source

- Source of truth is `TerrainCellStateMap` in `ATacticalMapGrid`.
- Material parameters are driven by grid-level contour settings.
- Unit movement checks `TerrainCellState.TraversalRules`.
- Facility destruction updates `TerrainCellState` and traversal modifiers.

## Quick Validation Checklist

1. Confirm cursor HUD shows terrain cell info and contour parameters.
2. Confirm land units cannot move into water cells.
3. Confirm road/bridge/railway movement is faster than plains.
4. Confirm Ctrl+RightClick on a facility cell reduces facility HP.
5. Confirm destroyed facility becomes `Rubble` and speed bonus disappears.
