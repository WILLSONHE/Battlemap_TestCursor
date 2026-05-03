# Contour Terrain Implementation Notes

## Pipeline Outputs

Run the generator:

`python Tools/TerrainPipeline/generate_terrain_data.py --size 129 --seed -1`

- `--seed -1` means random seed each run (the script prints the seed used).
- Example fixed-seed replay: `--seed 123456`.

Core constraints (current defaults):

- Land bounds: `[-10m, 6000m]`
- Water bounds: `[-400m, 2000m]`
- 8-neighbor delta constraint: `max abs(DeltaH) <= 5m`
- Land and water height distributions are generated separately (normal-like with optional skew).
- Bounds are limits, not targets: generated maps are valid even if they do not reach min/max exactly.
- If `size` is small relative to `max-neighbor-delta-meters`, script prints a feasibility warning but still generates valid data inside bounds.

Outputs are written to `Saved/TerrainPipeline/`:

- `heightmap.png`
- `terrain_types.png`
- `terrain_overlay.png`
- `water_layers.png`
- `facilities.json`

## UE Setup

1. Open the map with `ABattlemap_TestCursorGameMode`.
2. Select the spawned `ATacticalMapGrid` actor.
3. Set `TerrainSurfaceMaterial` to `M_contour` (or an instance `MI_contour`).
4. Ensure the material exposes scalar parameters:
   - `ContourInterval`
   - `ContourLineWidth`
   - `ContourAAWidth`
   - `ContourDepthOffset`
   - `ShowCoordinateAxes`
5. Press Play. The grid loads data from `Saved/TerrainPipeline`.

6. **Map view modes**: Runtime uses keys `1` (Land) / `2` (Ocean) → `SetMapViewMode` → MID parameters `MapViewMode`, `WaterUnifiedColor`, `LandUnifiedColor`. Extend `M_contour` so the base tint uses `TerrainTypesTex` / `WaterLayersTex` and the `IsWater` mask (palette water RGBs matching `TerrainTypeFromPalette`) before your existing contour/axis/label stack.
7. **Debug view switch**: Runtime writes scalar `DebugViewMode` to MID (keys: `3` cycle, `4` back to Final; NumPad `1..5` quick jump to TerrainTypes / WaterLayers / IsWaterMask / BaseTint / ContourMask). In `M_contour`, add a final branch before output:
   - `0 Final`: existing final chain
   - `1 TerrainTypes`: raw `TerrainTypesTex` sample
   - `2 WaterLayers`: raw `WaterLayersTex` sample
   - `3 IsWaterMask`: grayscale mask (0/1)
   - `4 BaseTint`: result before contour/axis/label overlays
   - `5 ContourMask`: contour mask grayscale output before contour color blend

## Contour Runtime Contract (Authoritative Params)

The runtime now treats these as authoritative scalar inputs:

- `ContourInterval`
- `ContourLineWidth`
- `ContourAAWidth`
- `ElevationScaleMeters`
- `DebugViewMode`

Compatibility aliases still exist (`ContourIntervalMeters`, `ContourWidth`, `ElevationScale`) but material graphs should use the authoritative names above.

## Recommended Contour Formula (Material)

Keep contour as a standalone chain:

- `H`: decode normalized height from `HeightmapTex`
- `E = (H - 0.5) * ElevationScaleMeters`
- `P = frac(E / max(ContourInterval, 1))`
- `D = min(P, 1 - P)`
- `W = max(ContourLineWidth * 0.001, 0.001)`
- `Mask = 1 - smoothstep(W, W + ContourAAWidth, D)`
- `Mask = saturate(Mask)`

Then blend safely:

- `ColorAfterContour = lerp(BaseTint, ContourColor, Mask)`

Do not multiply `BaseTint` by contour color/mask, which can drive large dark patches when mask goes unstable.

## Troubleshooting Priority

1. `DebugViewMode=1` (TerrainTypes): verify input texture is stable.
2. `DebugViewMode=5` (ContourMask): verify mask is line-like, not block-like.
3. `DebugViewMode=4` (BaseTint): verify map view blend before contour.
4. `DebugViewMode=0` (Final): verify contour/axis/label composition only.

**v2 generator** (adds `validation_report.json`): `python Tools/TerrainPipeline/generate_terrain_data_vers_2.py`

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

## 并行专项：每周等高线基线（不占主线排期）

- **频率**：每周固定一次短时回归（与 Phase5 主线解耦）。
- **步骤**：在标准测试关卡加载 `M_contour` / `MI_contour`；将 `DebugViewMode` 设为 **ContourMask**（材质标量 `5` 或编辑器 `EMapDebugViewMode::ContourMask`）；对固定相机与缩放截屏存档。
- **对比**：与上一周基线对比 `ContourMask` 是否出现块状发黑、断裂或倍率漂移；若 PR 修改了 `ContourInterval`、`ContourAAWidth`、`ElevationScaleMeters` 或高度图输入，必须更新基线说明（路径或 commit）。
- **快捷键**：运行时 `3` 循环、`4` 回到 Final；小键盘 `1..5` 快速跳转到各 Debug 视图（见上文第 7 点）。
