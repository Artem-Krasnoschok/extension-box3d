# Box3D 3D for Defold

A clean-room Defold native extension for the experimental 3D [Box3D](https://github.com/erincatto/box3d) API.

The repository contains a generic Lua binding, pinned upstream headers, a reproducible native-library build, and release packaging for eight Defold targets. It does not contain game-specific destruction rules, gameplay code, or code copied from another Defold project.

> [!IMPORTANT]
> Box3D 3D currently reports version `0.1.0` and its `main` API may change. This extension pins one exact upstream commit so every platform uses the same source.

## Status

- Pinned Box3D commit: [`47d7f7cc7e091142c08d11dc7d2e493c5d34f536`](https://github.com/erincatto/box3d/commit/47d7f7cc7e091142c08d11dc7d2e493c5d34f536)
- Precision: single precision
- Lua API: worlds, bodies, common convex shapes, transforms, velocities, forces, impulses, material values, explosions, and simulation events
- No automatic upstream tracking
- License: MIT

Supported release targets:

| Desktop | Mobile | Web |
| --- | --- | --- |
| `x86_64-win32` | `armv7-android` | `wasm-web` |
| `x86_64-linux` | `arm64-android` | |
| `arm64-linux` | | |
| `x86_64-osx` | | |
| `arm64-osx` | | |

## Installation

Use a release asset, not the source archive from the repository. The release ZIP contains the native libraries that are intentionally not committed to `main`.

Add a released archive URL to **Project > Fetch Libraries** in `game.project`:

```text
https://github.com/Artem-Krasnoschok/extension-box3d/releases/download/v0.1.0/extension-box3d-0.1.0.zip
```

Then fetch libraries and build the project. Replace `v0.1.0` with the release you select.

## Minimal example

```lua
function init(self)
    self.world = box3d.create_world({
        gravity = vmath.vector3(0, -9.81, 0),
    })

    local ground = box3d.create_body(self.world, {
        type = box3d.BODY_TYPE_STATIC,
        position = vmath.vector3(0, -0.5, 0),
    })
    box3d.create_box(ground, {
        half_extents = vmath.vector3(10, 0.5, 10),
        friction = 0.8,
    })

    self.body = box3d.create_body(self.world, {
        type = box3d.BODY_TYPE_DYNAMIC,
        position = vmath.vector3(0, 4, 0),
    })
    box3d.create_box(self.body, {
        half_extents = vmath.vector3(0.5, 0.5, 0.5),
        density = 1,
    })
end

function update(self, dt)
    box3d.step(self.world, dt, 4)
    local position, rotation = box3d.get_transform(self.body)
    go.set_position(position)
    go.set_rotation(rotation)
end

function final(self)
    box3d.destroy_world(self.world)
end
```

See [`example/example.script`](example/example.script) for the runnable library-project example and [`docs/API.md`](docs/API.md) for the API and definition fields.

## Handles and lifetime

Worlds, bodies, and shapes are opaque Lua userdata. This avoids truncating Box3D's 64-bit IDs in Lua numbers.

- Destroy worlds explicitly with `box3d.destroy_world()`. A world also has a garbage-collection fallback.
- Destroy bodies and shapes explicitly when needed.
- Destroying a world invalidates all of its body and shape handles.
- Destroying a body invalidates its shapes.
- End-event shape handles can already be invalid; check them with `box3d.is_shape_valid()` before use.

Invalid-handle use raises a descriptive Lua error instead of calling Box3D with stale IDs.

## Source and release model

`main` contains reviewable source, official Box3D public headers, metadata, build scripts, and tests. It does not accumulate generated `.lib` or `.a` files.

CI performs the following for every platform:

1. Reads `UPSTREAM.json`.
2. Downloads that exact upstream source archive.
3. Verifies its SHA-256 checksum.
4. Builds a static single-precision library.
5. Packages all eight libraries with the extension source and headers.
6. On a `v*` tag, publishes the ZIP and its SHA-256 file to GitHub Releases.

This makes upgrades deliberate: changing upstream requires a reviewed change to both the commit and checksum.

## Scope

The Lua layer intentionally stays close to Box3D. `box3d.explode()` is the upstream geometry-aware operation, and force/impulse functions are direct bindings. Decisions such as damage thresholds, fragment creation, object pooling, scoring, or scene synchronization belong in the consuming game, not in this extension.

The first API surface focuses on the features needed to build useful rigid-body simulations. Meshes, height fields, joints, casts, and debug drawing are not exposed yet. Contributions can add them without embedding application-specific behavior.

## Credits

Box3D was created by [Erin Catto](https://github.com/erincatto) and is developed by the [Box3D contributors](https://github.com/erincatto/box3d/graphs/contributors).

Special thanks to [Ivan Enzhaev](https://github.com/ivan-enzhaev), whose public investigation demonstrated a practical route for running the new 3D Box3D code in Defold:

- [Defold forum discussion](https://forum.defold.com/t/solved-integrating-box3d-into-defold/83038)
- [Ivan's proof-of-concept repository](https://github.com/ivan-enzhaev/pre-built-box3d-for-windows-android-wasm-defold)
- [Upstream Box3D discussion](https://github.com/erincatto/box3d/discussions/102)

This implementation was written independently. No source or binary from Ivan's repository is redistributed. See [`NOTICE.md`](NOTICE.md) and the bundled third-party license.

## License

The extension is licensed under the [MIT License](LICENSE). Box3D is also MIT-licensed; its original license is included at [`third_party/box3d/LICENSE`](third_party/box3d/LICENSE).
