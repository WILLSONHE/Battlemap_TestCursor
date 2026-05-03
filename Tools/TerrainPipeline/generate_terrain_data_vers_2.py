#!/usr/bin/env python3
"""
Terrain pipeline v2: same generation core as generate_terrain_data.py with explicit
validation_report.json output for acceptance checks (area ratios, neighbor deltas, q80, etc.).
"""

from __future__ import annotations

import argparse
import json
import math
import os
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

# Resolve sibling module when run as `python generate_terrain_data_vers_2.py`
_PIPELINE_DIR = Path(__file__).resolve().parent
if str(_PIPELINE_DIR) not in sys.path:
    sys.path.insert(0, str(_PIPELINE_DIR))

import generate_terrain_data as g


WATER_NAMES = frozenset({"River", "Swamp", "ShallowWater", "OpenWater", "DeepWater"})
LAND_FOR_BEACH = frozenset(
    {
        "Plains",
        "Forest",
        "Hills",
        "Mountains",
        "Urban",
        "Desert",
        "Beach",
        "Road",
        "Railway",
        "Airfield",
        "Port",
        "Bridge",
        "Bunker",
        "Rubble",
    }
)


def _is_water_terrain_name(name: str) -> bool:
    return name in WATER_NAMES


def _find_coastal_port_cell(terrain: np.ndarray, rng: np.random.Generator) -> Optional[Tuple[int, int]]:
    """
    Port must be on a coastal land cell:
    - center cell is land
    - has >=1 OpenWater neighbor
    - has >=1 land neighbor
    """
    h, w = terrain.shape
    candidates: List[Tuple[int, int]] = []
    for y in range(1, h - 1):
        for x in range(1, w - 1):
            center = str(terrain[y, x])
            if _is_water_terrain_name(center):
                continue
            has_openwater = False
            has_land = False
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    if dx == 0 and dy == 0:
                        continue
                    t = str(terrain[y + dy, x + dx])
                    if t == "OpenWater":
                        has_openwater = True
                    if not _is_water_terrain_name(t):
                        has_land = True
            if has_openwater and has_land:
                candidates.append((x, y))
    if not candidates:
        return None
    idx = int(rng.integers(0, len(candidates)))
    return candidates[idx]


def _make_features_v2(terrain: np.ndarray, height01: np.ndarray, seed: int) -> List[g.FacilityRecord]:
    """
    V2 feature placement:
    - Reuse v1 feature generator
    - Reposition Port to coastal-only rule
    """
    facilities = g.make_features(terrain, height01, seed)

    # Remove v1 port placements first.
    facilities = [f for f in facilities if str(getattr(f, "type", "")).lower() != "port"]
    terrain[terrain == "Port"] = "Plains"

    rng = np.random.default_rng(seed + 4040)
    port_cell = _find_coastal_port_cell(terrain, rng)
    if port_cell is not None:
        px, py = port_cell
        terrain[py, px] = "Port"
        size = terrain.shape[0]
        facilities.append(g.FacilityRecord(x=px - size // 2, y=py - size // 2, type="port", hp=200.0))

    return facilities


def _neighbor_delta_stats(
    height_m: np.ndarray,
    terrain: np.ndarray,
    default_delta_m: float,
    hill_mountain_delta_m: float,
) -> Tuple[float, int, int]:
    """Returns (max_abs_delta_m, violations_default_rule_count, edge_checks)."""
    h, w = height_m.shape
    offsets = [(-1, -1), (0, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1)]
    hm_mask = np.isin(terrain, ["Hills", "Mountains"])
    max_delta = 0.0
    violations = 0
    checks = 0
    d0 = float(max(0.0, default_delta_m))
    d1 = float(max(0.0, hill_mountain_delta_m))
    for y in range(h):
        for x in range(w):
            v = float(height_m[y, x])
            for dx, dy in offsets:
                ny, nx = y + dy, x + dx
                if ny < 0 or ny >= h or nx < 0 or nx >= w:
                    continue
                if (ny, nx) < (y, x):
                    continue
                checks += 1
                nv = float(height_m[ny, nx])
                diff = abs(v - nv)
                max_delta = max(max_delta, diff)
                cap = d1 if (hm_mask[y, x] or hm_mask[ny, nx]) else d0
                if diff > cap + 1e-3:
                    violations += 1
    return max_delta, violations, checks


def _terrain_area_fractions(terrain: np.ndarray) -> Dict[str, float]:
    total = float(terrain.size)
    out: Dict[str, float] = {}
    for name in sorted(set(np.unique(terrain).tolist())):
        n = int(np.count_nonzero(terrain == name))
        out[str(name)] = n / total
    return out


def _beach_adjacency_violations(terrain: np.ndarray) -> int:
    """Beach should neighbor OpenWater and at least one non-water land class."""
    h, w = terrain.shape
    p = np.pad(terrain, pad_width=1, mode="edge")
    viol = 0
    for y in range(h):
        for x in range(w):
            if str(terrain[y, x]) != "Beach":
                continue
            has_open = False
            has_land = False
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    if dx == 0 and dy == 0:
                        continue
                    t = str(p[y + 1 + dy, x + 1 + dx])
                    if t == "OpenWater":
                        has_open = True
                    if t in LAND_FOR_BEACH and t not in WATER_NAMES:
                        has_land = True
            if not (has_open and has_land):
                viol += 1
    return viol


def build_validation_report(
    height_m: np.ndarray,
    terrain: np.ndarray,
    args: argparse.Namespace,
    effective_delta_m: float,
    runtime_profile: Dict[str, Any],
) -> Dict[str, Any]:
    h, w = terrain.shape
    pf_mask = np.isin(terrain, ["Plains", "Forest"])
    plain_forest_q80: Optional[float] = None
    if np.any(pf_mask):
        plain_forest_q80 = float(np.quantile(height_m[pf_mask], 0.80))

    max_d, viol_neighbors, edge_checks = _neighbor_delta_stats(
        height_m, terrain, effective_delta_m, 100.0
    )

    water_cells = int(sum(int(np.count_nonzero(terrain == n)) for n in WATER_NAMES))
    swamp_cells = int(np.count_nonzero(terrain == "Swamp"))
    hm_cells = int(np.count_nonzero(np.isin(terrain, ["Hills", "Mountains"])))

    return {
        "version": 2,
        "grid_size": [int(h), int(w)],
        "runtime_profile": runtime_profile,
        "area_fractions_by_terrain": _terrain_area_fractions(terrain),
        "water_cell_fraction": water_cells / float(h * w),
        "swamp_fraction": swamp_cells / float(h * w),
        "hill_mountain_combined_fraction": hm_cells / float(h * w),
        "plain_forest_elevation_q80_m": plain_forest_q80,
        "max_abs_neighbor_delta_m": max_d,
        "neighbor_delta_checks": edge_checks,
        "neighbor_delta_violations_relaxed_cap": viol_neighbors,
        "effective_max_neighbor_delta_m_used": float(effective_delta_m),
        "beach_adjacency_violations": _beach_adjacency_violations(terrain),
    }


def write_validation_report(out_dir: str, report: Dict[str, Any]) -> None:
    path = os.path.join(out_dir, "validation_report.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2, ensure_ascii=False)
    print(f"Wrote validation report: {os.path.abspath(path)}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate Battlemap terrain pipeline data (v2 + validation_report.json).")
    parser.add_argument("--size", type=int, default=129, help="Square map size in cells/pixels.")
    parser.add_argument("--seed", type=int, default=-1, help="Random seed. -1 => random.")
    parser.add_argument("--octaves", type=int, default=5)
    parser.add_argument("--persistence", type=float, default=0.55)
    parser.add_argument("--warp-strength", type=float, default=0.85, help="Domain warp strength for public algorithm.")
    parser.add_argument("--smooth-passes", type=int, default=2, help="Height smoothing passes.")
    parser.add_argument("--elevation-scale-meters", type=float, default=12000.0, help="Elevation scale used by runtime conversion.")
    parser.add_argument("--max-neighbor-delta-meters", type=float, default=5.0, help="Max adjacent-cell elevation difference in meters.")
    parser.add_argument("--water-ratio", type=float, default=0.28, help="Approximate fraction of water cells.")
    parser.add_argument("--land-min-m", type=float, default=-10.0)
    parser.add_argument("--land-max-m", type=float, default=6000.0)
    parser.add_argument("--water-min-m", type=float, default=-400.0)
    parser.add_argument("--water-max-m", type=float, default=2000.0)
    parser.add_argument("--land-mean-m", type=float, default=2200.0)
    parser.add_argument("--land-std-m", type=float, default=1200.0)
    parser.add_argument("--water-mean-m", type=float, default=-140.0)
    parser.add_argument("--water-std-m", type=float, default=120.0)
    parser.add_argument("--land-skew", type=float, default=0.25, help="Skew for land distribution (0 = none).")
    parser.add_argument("--water-skew", type=float, default=-0.15, help="Skew for water distribution (0 = none).")
    parser.add_argument(
        "--target-mode",
        type=str,
        default="flat",
        choices=["flat", "landscape"],
        help="flat: plane + contour material (default); landscape: UE Landscape import workflow.",
    )
    parser.add_argument("--tile-size-meters", type=float, default=10.0, help="Logical cell size for runtime and landscape scale.")
    parser.add_argument("--section-size-quads", type=int, default=63, help="Landscape section size quads.")
    parser.add_argument("--sections-per-component", type=int, default=2, help="Landscape sections per component.")
    parser.add_argument(
        "--output",
        type=str,
        default=os.path.join("Saved", "TerrainPipeline"),
        help="Output directory.",
    )
    args = parser.parse_args()
    if args.size < 3:
        raise ValueError("--size must be >= 3")
    if (args.size % 2) == 0:
        # TacticalMapGrid uses Cell = Pixel - (Width//2); odd size keeps symmetric visible range.
        args.size += 1
        print(f"[warn] even --size adjusted to odd value for symmetric grid coords: {args.size}")

    if args.seed == -1:
        args.seed = int(np.random.default_rng().integers(1, 2**31 - 1))
        print(f"[v2] Random seed chosen: {args.seed}")

    quads_per_component = args.section_size_quads * args.sections_per_component
    component_count = max(1, math.ceil((args.size - 1) / quads_per_component))
    recommended_resolution = component_count * quads_per_component + 1

    quant_step_m = float(args.elevation_scale_meters) / 65535.0
    effective_delta_m = max(0.1, float(args.max_neighbor_delta_meters) - (2.0 * quant_step_m))

    height_m, is_water = g.build_random_heightmap_meters(
        size=args.size,
        seed=args.seed,
        water_ratio=float(args.water_ratio),
        land_min_m=float(args.land_min_m),
        land_max_m=float(args.land_max_m),
        water_min_m=float(args.water_min_m),
        water_max_m=float(args.water_max_m),
        land_mean_m=float(args.land_mean_m),
        land_std_m=float(args.land_std_m),
        water_mean_m=float(args.water_mean_m),
        water_std_m=float(args.water_std_m),
        land_skew=float(args.land_skew),
        water_skew=float(args.water_skew),
        max_neighbor_delta_m=effective_delta_m,
    )

    height01 = np.clip((height_m / float(args.elevation_scale_meters)) + 0.5, 0.0, 1.0).astype(np.float32)

    rng = np.random.default_rng(args.seed + 17)
    terrain = g.build_terrain_from_water_topology(height_m, is_water, rng)

    facilities = _make_features_v2(terrain, height01, seed=args.seed)
    terrain = g.enforce_plain_forest_minimums(terrain, plain_min=0.50, forest_min=0.30)

    height_m = g.compress_land_80pct_band(height_m, terrain, band_m=500.0)
    pf_mask = np.isin(terrain, ["Plains", "Forest"])
    height_m = g.clamp_mask_q80_to_ceiling(height_m, pf_mask, ceiling_m=500.0)

    h, w = terrain.shape
    min_m = np.full((h, w), float(args.land_min_m), dtype=np.float32)
    max_m = np.full((h, w), float(args.land_max_m), dtype=np.float32)
    water_types = np.isin(terrain, ["River", "OpenWater", "ShallowWater", "DeepWater", "Swamp"])
    min_m[water_types] = float(args.water_min_m)
    max_m[water_types] = float(args.water_max_m)
    height_m = g.enforce_neighbor_delta_by_terrain(
        height_m=height_m,
        terrain=terrain,
        default_delta_m=effective_delta_m,
        hill_mountain_delta_m=100.0,
        iterations=96,
        min_m=min_m,
        max_m=max_m,
    )
    height01 = np.clip((height_m / float(args.elevation_scale_meters)) + 0.5, 0.0, 1.0).astype(np.float32)

    runtime_profile: Dict[str, Any] = {
        "target_mode": args.target_mode,
        "tile_size_meters": float(args.tile_size_meters),
        "elevation_scale_meters": float(args.elevation_scale_meters),
        "max_neighbor_delta_meters": float(args.max_neighbor_delta_meters),
        "seed": int(args.seed),
        "land_min_m": float(args.land_min_m),
        "land_max_m": float(args.land_max_m),
        "water_min_m": float(args.water_min_m),
        "water_max_m": float(args.water_max_m),
        "land_mean_m": float(args.land_mean_m),
        "land_std_m": float(args.land_std_m),
        "water_mean_m": float(args.water_mean_m),
        "water_std_m": float(args.water_std_m),
        "land_skew": float(args.land_skew),
        "water_skew": float(args.water_skew),
        "water_ratio": float(args.water_ratio),
        "section_size_quads": int(args.section_size_quads),
        "sections_per_component": int(args.sections_per_component),
        "component_count_x": int(component_count),
        "component_count_y": int(component_count),
        "overall_resolution_x": int(recommended_resolution),
        "overall_resolution_y": int(recommended_resolution),
        "heightmap_resolution_x": int(args.size),
        "heightmap_resolution_y": int(args.size),
        "pipeline_script": "generate_terrain_data_vers_2.py",
    }

    g.write_outputs(args.output, height01, terrain, facilities, runtime_profile)

    report = build_validation_report(height_m, terrain, args, effective_delta_m, runtime_profile)
    write_validation_report(args.output, report)

    if args.target_mode == "landscape" and recommended_resolution != args.size:
        print(
            f"[warn] size={args.size} does not match UE recommended overall resolution {recommended_resolution} "
            f"for section={args.section_size_quads}, sections/component={args.sections_per_component}. "
            "UE Landscape import will resample/crop, which can cause edge flatness or mismatch."
        )
    print(f"Seed: {args.seed}")
    print(f"Height(m) range: min={height_m.min():.2f}, max={height_m.max():.2f}")
    land_vals = height_m[~is_water]
    water_vals = height_m[is_water]
    if land_vals.size:
        print(f"Land Height(m): min={land_vals.min():.2f}, max={land_vals.max():.2f}, mean={land_vals.mean():.2f}, std={land_vals.std():.2f}")
    if water_vals.size:
        print(f"Water Height(m): min={water_vals.min():.2f}, max={water_vals.max():.2f}, mean={water_vals.mean():.2f}, std={water_vals.std():.2f}")
    print(f"Generated terrain outputs at: {os.path.abspath(args.output)}")


if __name__ == "__main__":
    main()
