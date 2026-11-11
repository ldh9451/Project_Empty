ProjectH C++ / Godot 4.7 GDExtension

Visual Studio:
  1. Open ../ProjectH.sln (Visual Studio 2026, C++ desktop workload).
  2. Select Debug | x64 and Build Solution (Ctrl+Shift+B).
  3. Press F5 to debug the terrain demo. Set breakpoints in voxel_terrain.cpp.

Godot:
  Open project.godot, then press F5 for the Marching Cubes terrain demo.
  Hold left mouse to dig, right mouse to build.
  Q/E: brush radius. Middle mouse drag: orbit. Shift + middle drag: pan.
  Wheel: zoom. F: reset camera. R: reset edits. N: generate next seed.
  The original moving-icon example remains available in cpp_demo.tscn (F6).
  Registered native node types: ProjectHVoxelTerrain and ProjectHCppExample.

Marching Cubes implementation:
  src/voxel_terrain.cpp stores a shared 49 x 33 x 49 density lattice.
  48 x 32 x 48 cells are split into 18 chunks of 16 x 16 x 16 cells.
  Negative density is solid; zero is the isosurface. Cell size is 1 meter.
  The classic 256-case lookup table interpolates edge vertices; density
  gradients provide smooth normals. The table uses Godot's clockwise winding.
  A spherical brush changes density with smooth falloff. Only affected chunks
  (including a one-sample gradient halo) rebuild meshes and collision shapes.
  The shell of the finite volume remains air, keeping the terrain closed.
  scripts/voxel_demo.gd handles camera, physics picking, and brush input.
  Native sculpt(center, radius, amount) uses terrain-local coordinates.
  Positive amount digs; negative amount builds. Use unit terrain scale.

Prototype scope:
  Edits live in memory; R, N, and restarting discard them.
  Fixed volume, synchronous CPU meshing; no streaming, LOD, save/load, or undo.
  Classic Marching Cubes does not resolve every ambiguous saddle case; an
  asymptotic decider or MC33 is a later step for topology guarantees.
  Terrain is generated at runtime, so the editor viewport is initially empty.

Verification (replace godot_console.exe with your Godot console executable):
  godot_console.exe --headless --path . --script tests/voxel_terrain_test.gd
  godot_console.exe --path . --script tests/voxel_capture.gd
  Run these from the project root. The first checks density edits, reset,
  chunk normals, winding, and collision changes after digging/building.
  The second saves before/after images in cpp/.cache and exits.

Third-party table:
  src/marching_cubes_table.h comes from pmneila/PyMCubes (BSD-3-Clause).
  Source: https://github.com/pmneila/PyMCubes/blob/master/mcubes/src/marchingcubes.cpp
  License: third_party/PyMCubes-LICENSE.txt. Retain it when redistributing.

Terminal:
  powershell -ExecutionPolicy Bypass -File .\cpp\build.ps1
  powershell -ExecutionPolicy Bypass -File .\cpp\build.ps1 -Configuration Release

After cloning on a different PC, install Python 3 and Visual Studio C++ first:
  powershell -ExecutionPolicy Bypass -File .\cpp\setup.ps1
  Then build. Dependencies and compiled DLLs are intentionally ignored by Git.
  Change LocalDebuggerCommand in ProjectH.Cpp.vcxproj if Godot is moved.

godot-cpp source snapshot:
  https://github.com/godotengine/godot-cpp
  commit cedbd9e71b24eb2898b28867c5faf6114da9e5e2, api_version=4.7
  Source snapshot includes its upstream LICENSE.md.

Windows SDK:
  The build uses Microsoft's Windows SDK NuGet packages 10.0.26100.9169.
  They are restored to cpp/.cache (no machine-wide SDK installation required).
  The first build after cloning downloads the packages. Internet is required.

build_profile.json restricts generated classes for a faster first build.
When using more Godot classes, add their names to enabled_classes and rebuild,
or remove build_profile=build_profile.json from build.ps1 for the complete API.

The first compile builds godot-cpp too. Later builds usually compile changed C++ only.
