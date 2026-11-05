# Light-point osgjs contract

The exporter writes `osgSim::LightPointNode` as `osg.LightPointNode`, retaining
normal node metadata, callbacks and StateSet. `LightPoint` is data, not a child
node. Local-space positions and parent transforms are not baked.

Node fields: `LightPoints` (array), `MinPixelSize`, `MaxPixelSize`,
`MaxVisibleDistance2` (squared distance), `PointSprite` (boolean), and optional
`LightPointSystem`. The default distance is native `FLT_MAX`, emitted as a finite
JSON number with the exporter's normal numeric precision.

Each plain light record contains `On` (boolean), `Position` (three numbers),
`Color` (four numbers), `Intensity`, `Radius`, and
`BlendingMode` (`"ADDITIVE"` or `"BLENDED"`). Optional `Sector` and
`BlinkSequence` are plain objects, not osg type wrappers.

## Sectors

`Sector.Type` selects the following fields; all angles are radians:

| Type | Fields |
| --- | --- |
| `AzimSector` | `MinAzimuth`, `MaxAzimuth`, `AzimuthFadeAngle` |
| `ElevationSector` | `MinElevation`, `MaxElevation`, `ElevationFadeAngle`, `MinFadeElevation`, `MaxFadeElevation` |
| `AzimElevationSector` | All azimuth and elevation fields above |
| `ConeSector` | `Axis` (vec3), `Angle`, `FadeAngle` |
| `DirectionalSector` | `Direction` (vec3), `HorizLobeAngle`, `VertLobeAngle`, `LobeRollAngle`, `FadeAngle`, `HorizFadeAngle`, `VertFadeAngle` |

Evaluate sectors using `v = eyeLocal - Position`. Interpolate **cosine/dot
values**, not angles. Azimuth uses `d = v.x*sin(center)+v.y*cos(center)`,
`center=(min+max)/2`, `half=(max-min)/2`, and `L=length(v.xy)`. Return zero
below `cos(min(PI,half+fade))*L`, one at/above `cos(half)*L`, otherwise
linearly interpolate between those thresholds.

Elevation compares `v.z` with `sin(min/max)*length(v)`. Outside the exported
fade limits return zero; between the lower fade and lower limit or upper limit
and upper fade, interpolate the corresponding sine thresholds; otherwise return
one. `MinFadeElevation`/`MaxFadeElevation` override reconstruction from
`ElevationFadeAngle`: native clipping can make the latter ambiguous.
The combined sector returns **min(azimuth, elevation)**.

Cone uses `d=dot(v,Axis)`, `L=length(v)`, inner `cos(Angle)*L`, outer
`cos(Angle+FadeAngle)*L`. Return one above inner, zero below outer, otherwise
linear interpolation. `Angle` is already the cone half-angle.

Directional sectors use native matrix convention: heading
`atan2(Direction.x,Direction.y)`, pitch
`atan2(Direction.z,length(Direction.xy))`; the native `_local_to_LP` matrix is
`R_y(roll) * R_x(pitch) * R_negativeZ(heading)` in OSG row-vector convention,
then native evaluates `E = matrix * v` (a column-vector multiplication).
Normalize `E.yz`; compare its first component with
`cos(VertLobeAngle/2)` and `cos(VertLobeAngle/2+VertFadeAngle)`.
Normalize `E.xy`; negate it when normalized `E.yz`'s first component is negative;
compare its second component with the corresponding horizontal cosine limits.
Each comparison returns zero below outer, one at/above inner, and linear
interpolation in between. Multiply the horizontal and vertical results.
`HorizFadeAngle`/`VertFadeAngle` override `FadeAngle`, because native clips
these bands independently at PI.

Unknown custom sector types explicitly fail export rather than lose behavior.
No native rendering algorithm is changed.

## Blink and system data

`BlinkSequence` contains `UniqueID`, `PhaseShift`, `PulsePeriod`,
`Pulses: [{Duration, Color: [r,g,b,a]}, ...]`, and optional
`SequenceGroup: {UniqueID, BaseTime}`.
`LightPointSystem` contains `UniqueID`, `Intensity`, and
`AnimationState`: `"ANIMATION_ON"`, `"ANIMATION_OFF"`, or `"ANIMATION_RANDOM"`.
Shared sequences, groups, and systems use `{UniqueID}` references after their
first full definition; these IDs share the usual osgjs object ID namespace.

Blink local time is `(time - BaseTime - PhaseShift) mod PulsePeriod`; omitted
group means base time zero. An empty pulse list produces white. Native samples
the current pulse at interval zero, otherwise averages RGBA over pulse durations
overlapping the frame interval. It uses a strict `>` comparison when advancing
past a pulse boundary. Preserve the stored `PulsePeriod`, not just the sum of
durations (native `setPulse()` does not update the stored period).

System intensity **replaces**, not multiplies, per-point intensity. Native only
blinks for `ANIMATION_ON`; both OFF and RANDOM leave base color unchanged.
Sector multiplies intensity. Reject intensity or alpha at/below `1/256`.
Projected size is multiplied by `sqrt(intensity)`. Blink multiplies all four
base color channels. Beyond the maximum squared distance reject the point;
otherwise alpha is multiplied by `1-(distanceSquared/MaxVisibleDistance2)^2`.
Native bypasses distance fading entirely for `FLT_MAX`. For sizes between one
pixel and the maximum, the min-size clamp additionally scales alpha by
`2/3 + sqrt(originalSize/clampedSize)/3`.

## Preservation and regression fixture

GLES geometry visitors already leave non-geometric leaves intact. The animation
cleaner also treats light points as renderable content, so light-only bone and
skeleton subgraphs are not deleted. The existing osgUtil optimizer explicitly
excludes LightPointNode from both static-transform flattening implementations.
Its GLES `PreTransformVisitor` only reorders geometry vertex accesses.

Build `tests/LightPointExport.cpp` against osg, osgDB, osgSim, osgAnimation and
osgUtil with the same configuration as the plugins. Run
`tests/CheckLightPointExport.ps1 -FixtureExecutable <exe> -OsgjsPlugin <dll>`
from a writable build directory with the matching runtime DLLs and GLES plugin
on the OSG search path. The fixture writes direct/GLES JSON and verifies custom
sector rejection. The script checks JSON types, all five sectors, independent
clipped fades, shared object references, disabled-child export, empty nodes,
and static/bone transforms. Generated fixture outputs belong in the build
directory, not the source tree.
