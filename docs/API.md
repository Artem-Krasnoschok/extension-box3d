# Lua API

All vectors are `vmath.vector3` values and rotations are `vmath.quat` values. Definition tables are copied by Box3D during creation unless this document says otherwise.

## World

### `box3d.get_version()`

Returns `{ major, minor, revision, upstream_commit }`.

### `box3d.create_world(options)`

Supported optional fields:

- `gravity`: vector, default from Box3D
- `restitution_threshold`: number
- `hit_event_threshold`: number
- `maximum_linear_speed`: number
- `enable_sleep`: boolean
- `enable_continuous`: boolean
- `worker_count`: integer; `wasm-web` currently requires `1`

Returns a world handle.

### `box3d.destroy_world(world)`

Destroys the world and everything it owns. Safe to call more than once on the same handle.

### `box3d.step(world, dt, substeps)`

Advances the simulation. `substeps` defaults to `4` and must be at least `1`.

### `box3d.set_gravity(world, gravity)` / `box3d.get_gravity(world)`

Sets or gets world gravity.

### `box3d.explode(world, definition)`

Direct binding to `b3World_Explode`. Fields: `position`, `radius`, `falloff`, `impulse_per_area`, and optional `mask_bits`.

## Bodies

### `box3d.create_body(world, definition)`

Supported fields:

- `type`: `BODY_TYPE_STATIC`, `BODY_TYPE_KINEMATIC`, `BODY_TYPE_DYNAMIC`, or the matching string
- `position`, `rotation`
- `linear_velocity`, `angular_velocity`
- `linear_damping`, `angular_damping`, `gravity_scale`, `sleep_threshold`
- `enable_sleep`, `is_awake`, `is_bullet`, `is_enabled`, `allow_fast_rotation`
- `lock_linear_x`, `lock_linear_y`, `lock_linear_z`
- `lock_angular_x`, `lock_angular_y`, `lock_angular_z`

### Lifetime and state

- `box3d.destroy_body(body)`
- `box3d.is_body_valid(body)`
- `box3d.get_transform(body)` returns `position, rotation`
- `box3d.set_transform(body, position, rotation)`
- `box3d.get_linear_velocity(body)` / `box3d.set_linear_velocity(body, velocity)`
- `box3d.get_angular_velocity(body)` / `box3d.set_angular_velocity(body, velocity)`
- `box3d.get_mass(body)`
- `box3d.is_awake(body)` / `box3d.set_awake(body, awake)`

### Forces and impulses

The optional `wake` argument defaults to `true`.

- `box3d.apply_force(body, force, world_point, wake)`
- `box3d.apply_force_to_center(body, force, wake)`
- `box3d.apply_torque(body, torque, wake)`
- `box3d.apply_linear_impulse(body, impulse, world_point, wake)`
- `box3d.apply_linear_impulse_to_center(body, impulse, wake)`
- `box3d.apply_angular_impulse(body, impulse, wake)`

## Shapes

Every create function accepts common shape fields:

- `density`, `friction`, `restitution`, `rolling_resistance`
- `tangent_velocity`, `explosion_scale`
- `is_sensor`, `enable_sensor_events`, `enable_contact_events`, `enable_hit_events`
- `update_body_mass`
- `category_bits`, `mask_bits`, `group_index`

Lua numbers represent integer filter bits exactly up to `2^53 - 1`. Omit a bit field to retain Box3D's full-width default.

Geometry-specific creation:

- `box3d.create_box(body, { half_extents = vector3, ... })`
- `box3d.create_sphere(body, { radius = number, center = vector3, ... })`
- `box3d.create_capsule(body, { center1 = vector3, center2 = vector3, radius = number, ... })`
- `box3d.create_cylinder(body, { height = number, radius = number, y_offset = number, sides = integer, ... })`

Shape operations:

- `box3d.destroy_shape(shape, update_body_mass)`; the second argument defaults to `true`
- `box3d.is_shape_valid(shape)`
- `box3d.get_shape_body(shape)`
- `box3d.get_friction(shape)` / `box3d.set_friction(shape, value)`
- `box3d.get_restitution(shape)` / `box3d.set_restitution(shape, value)`

## Events

Event data is copied into Lua tables immediately after querying.

- `box3d.get_body_events(world)` returns an array of `{ body, position, rotation, fell_asleep }`.
- `box3d.get_sensor_events(world)` returns `{ begin_events, end_events }`; each event contains `sensor_shape` and `visitor_shape`.
- `box3d.get_contact_events(world)` returns `{ begin_events, end_events, hit_events }`. Contact pairs contain `shape_a` and `shape_b`; hit events additionally contain `point`, `normal`, and `approach_speed`.

Read events after `box3d.step()` and before the next step. Box3D can report invalid shapes in end events when destruction caused the event, so call `box3d.is_shape_valid()` where appropriate.
