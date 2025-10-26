# CurveMesh

[![Clang Format](https://img.shields.io/github/actions/workflow/status/AxelStrem/Curve3DMesh-plugin/clang-format.yml?branch=main&label=clang-format&logo=github)](https://github.com/AxelStrem/Curve3DMesh-plugin/actions/workflows/clang-format.yml?query=branch%3Amain)
[![Linux x86_64](https://img.shields.io/github/actions/workflow/status/AxelStrem/Curve3DMesh-plugin/build-linux.yml?branch=main&label=linux%20x86_64&logo=linux&style=flat)](https://github.com/AxelStrem/Curve3DMesh-plugin/actions/workflows/build-linux.yml?query=branch%3Amain)
[![macOS Universal](https://img.shields.io/github/actions/workflow/status/AxelStrem/Curve3DMesh-plugin/build-macos.yml?branch=main&label=macOS%20universal&logo=apple&style=flat)](https://github.com/AxelStrem/Curve3DMesh-plugin/actions/workflows/build-macos.yml?query=branch%3Amain)
[![Windows x86_64](https://img.shields.io/github/actions/workflow/status/AxelStrem/Curve3DMesh-plugin/build-windows-x86_64.yml?branch=main&label=windows%20x86_64&logo=windows)](https://github.com/AxelStrem/Curve3DMesh-plugin/actions/workflows/build-windows-x86_64.yml?query=branch%3Amain)
[![Windows x86](https://img.shields.io/github/actions/workflow/status/AxelStrem/Curve3DMesh-plugin/build-windows-x86.yml?branch=main&label=windows%20x86&logo=windows)](https://github.com/AxelStrem/Curve3DMesh-plugin/actions/workflows/build-windows-x86.yml?query=branch%3Amain)

Procedural ribbon/tube mesh generation for Godot 4, driven by a `Curve3D`. The plugin ships the `CurveMesh` primitive mesh resource so you can render smooth wires, trails, and other path-following geometry without hand-authoring meshes.

<video src="https://github.com/user-attachments/assets/0b5239bb-c7e9-4087-aa07-f2b9bb53786d" controls muted loop playsinline style="max-width:100%;height:auto;">
  Your browser does not support the video tag. Watch the demo instead:
  https://github.com/user-attachments/assets/0b5239bb-c7e9-4087-aa07-f2b9bb53786d
</video>

## Features

- `Curve3D`-powered mesh generation with adaptive tessellation or raw control-point polylines.
- Three cross-section profiles (`flat`, `cross`, `tube`) with width curves, segment count, and optional edge extension.
- Orientation controls: follow the curve's parallel transport frame or align to a custom up vector.
- UV scaling by curve length or width, plus optional per-segment tiling for cross profiles.
- Corner handling helpers—angle-based corner detection, smooth/flat shading toggles, vertex interleaving, and overlap filtering.

## Installation

1. Download the ZIP for your platform/architecture from the [Releases](https://github.com/AxelStrem/Curve3DMesh-plugin/releases) page (for example `curvemesh-windows-x86_64.zip`).
2. Extract the archive into your Godot project so it creates `addons/curve_mesh/` with the binaries, `.gdextension`, and icon files.
3. (Re)open the project in Godot and enable the addon under `Project > Project Settings > Plugins`.
4. Create a `MeshInstance3D`, assign a new `CurveMesh` resource, and provide a `Curve3D` to start drawing geometry along the path.

## Building from source

1. Clone the repository with submodules: `git clone --recurse-submodules https://github.com/AxelStrem/Curve3DMesh-plugin.git`.
2. Install build prerequisites:
	- Python 3.8+ and SCons 4 (`python -m pip install scons`).
	- A C++17 toolchain matching your target platform (MSVC, clang, or gcc) and the Godot 4 SDK headers (already supplied via `godot-cpp`).
3. From the repo root, run SCons with the desired platform/target, e.g.:
	- `scons platform=windows target=editor arch=x86_64`
	- `scons platform=linux target=template_release arch=x86_64`
	- `scons platform=macos target=editor arch=universal`
4. Built artifacts land in `build/addons/curve_mesh/`; copy that folder into your Godot project as described above.
