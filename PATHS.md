# Path support: foundation and map format

The first implementation provides `mdl::Path` and the `readPath` / `writePath`
entity-property codec in TbMdlLib. Paths are game data, not brushes or patches.
No particular entity classname is hardcoded into these APIs.

## Implemented

- Linear, uniform Catmull-Rom, and cubic Bezier evaluation.
- Open and closed paths, normalized sampling, and unit tangents.
- Approximate length, distance sampling, and closest-point queries.
- Auto, Aligned, and Free handle metadata and explicit Bezier handles.
- Affine transformation of positions and handles, including nonuniform scale.
- Versioned entity-property serialization that preserves unrelated properties.
- Tests using the existing `.map` node writer and reader.

This is a library foundation. It does not yet add a viewport path tool, render
curves, recognize path entities through FGD metadata, or connect path transforms
to entity editing. Bevy import, followers, and gameplay consumers also remain to
be implemented in the game repository. EricW compilation has not been tested.

## Version 1 entity properties

```text
{
    "classname" "path"
    "targetname" "train_route"
    "path_version" "1"
    "path_type" "bezier"
    "closed" "0"
    "point_count" "3"
    "point_0" "0 0 64"
    "point_0_out" "32 0 64"
    "point_0_mode" "free"
    "point_1" "128 64 96"
    "point_1_in" "96 64 96"
    "point_1_out" "160 64 96"
    "point_1_mode" "aligned"
    "point_2" "256 0 64"
    "point_2_in" "224 0 64"
    "point_2_mode" "free"
}
```

`path_version`, `path_type`, `closed`, and `point_count` are required. Supported
types are `linear`, `catmull_rom`, and `bezier`. `closed` must be `0` or `1`.
Counts range from 0 to 65536; indices must be contiguous and start at zero.
An empty path is permitted while authoring. Runtime consumers should require at
least two nodes before starting movement.

Positions and handles are **absolute world coordinates**, not offsets from the
entity origin or from the node. Moving a handle therefore means updating its
stored world position. The future entity-transform integration must transform
these properties along with the entity; generic entity transforms currently
leave them alone.

Each position has exactly three finite numbers. Missing positions, duplicate
path properties, out-of-range indices, malformed handles, unknown modes/types,
and unsupported versions are errors. The codec does not guess how to upgrade an
unversioned path. Unrelated properties, including classname and targetname, are
preserved. Rewriting a path removes obsolete numbered points and handles.

`point_N_mode` is optional and defaults to `auto`. In `auto` mode, evaluation
generates handles from neighboring positions and ignores stored explicit
handles. For `aligned` and `free`, explicit handles are used; an omitted handle
coincides with the node. Aligned-mode editing constraints are not implemented
yet, so import preserves the supplied handles without adjusting them.

## Evaluation contract for the game importer

`t` is clamped to [0, 1] and each segment receives an equal interval of parameter
space. An open path with N nodes has N-1 segments; a closed path has N segments,
including the last-to-first segment. A closed path's `sample(1)` equals its
start. Closed paths with fewer than two nodes are constant.

Linear paths interpolate positions directly. For Catmull-Rom, the cubic Bezier
controls for a segment from P[i] to P[j] are:

```text
B0 = P[i]
B1 = P[i] + (P[i+1] - P[i-1]) / 6
B2 = P[j] - (P[j+1] - P[j-1]) / 6
B3 = P[j]
```

Neighbors wrap on closed paths. On open paths, a missing endpoint neighbor is
replaced by the endpoint itself. Auto Bezier handles use the same formula.
This is uniform Catmull-Rom, which can overshoot between unequally spaced nodes;
it is not centripetal Catmull-Rom.

Bezier segments evaluate `(1-u)^3 B0 + 3(1-u)^2 u B1 + 3(1-u) u^2 B2 + u^3 B3`.
Tangents are normalized analytic derivatives, or zero where the derivative is
zero. Empty paths sample to the zero vector; single-node paths sample to that
node. Callers must supply finite positions, handles, and query parameters.

Length, distance sampling, and closest-point queries use a polyline with 64
subdivisions per segment by default. Callers can choose another resolution;
zero requests one subdivision. Distance and closest-point results lie on this
approximation. Distance is clamped to the path length, including for closed
paths; looping and ping-pong behavior belong to the future follower. These
queries currently rebuild their samples, so runtime importers should cache an
arc-length table for repeated per-frame traversal.

## Next editor milestone

Add generic FGD/ENT metadata to identify path-capable entities, then integrate
path decoding with bounds, transforms, rendering, and picking. Point and handle
editing should write entity-property changes through existing map commands so
undo/redo, cloning, deletion, and copy/paste share the normal entity lifecycle.
