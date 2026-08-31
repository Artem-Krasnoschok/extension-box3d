#include <dmsdk/sdk.h>
#include <box3d/box3d.h>

#include <limits.h>
#include <stdint.h>
#include <string.h>

#define LIB_NAME "Box3DExtension"
#define MODULE_NAME "box3d"

namespace
{
static const char* WORLD_META = "box3d.world";
static const char* BODY_META = "box3d.body";
static const char* SHAPE_META = "box3d.shape";
static const char* UPSTREAM_COMMIT = "47d7f7cc7e091142c08d11dc7d2e493c5d34f536";

struct WorldHandle
{
    b3WorldId id;
};

struct BodyHandle
{
    b3BodyId id;
};

struct ShapeHandle
{
    b3ShapeId id;
};

static b3WorldId g_Worlds[B3_MAX_WORLDS];

static int AbsIndex(lua_State* L, int index)
{
    return index > 0 || index <= LUA_REGISTRYINDEX ? index : lua_gettop(L) + index + 1;
}

static float GetNumberField(lua_State* L, int table_index, const char* name, float fallback)
{
    table_index = AbsIndex(L, table_index);
    lua_getfield(L, table_index, name);
    float value = lua_isnil(L, -1) ? fallback : (float)luaL_checknumber(L, -1);
    if (!b3IsValidFloat(value))
    {
        luaL_error(L, "%s must be a finite number", name);
    }
    lua_pop(L, 1);
    return value;
}

static uint64_t GetUInt64Field(lua_State* L, int table_index, const char* name, uint64_t fallback)
{
    table_index = AbsIndex(L, table_index);
    lua_getfield(L, table_index, name);
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        return fallback;
    }

    lua_Number number = luaL_checknumber(L, -1);
    if (!(number >= 0.0 && number <= 9007199254740991.0))
    {
        luaL_error(L, "%s must be an integer between 0 and 2^53 - 1", name);
    }
    uint64_t value = (uint64_t)number;
    if ((lua_Number)value != number)
    {
        luaL_error(L, "%s must be an integer", name);
    }
    lua_pop(L, 1);
    return value;
}

static int CheckInteger(lua_State* L, int index, const char* name)
{
    lua_Number number = luaL_checknumber(L, index);
    if (!(number >= (lua_Number)INT_MIN && number <= (lua_Number)INT_MAX))
    {
        luaL_error(L, "%s must be an integer in the supported range", name);
    }
    int value = (int)number;
    if ((lua_Number)value != number)
    {
        luaL_error(L, "%s must be an integer", name);
    }
    return value;
}

static int GetIntegerField(lua_State* L, int table_index, const char* name, int fallback)
{
    table_index = AbsIndex(L, table_index);
    lua_getfield(L, table_index, name);
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        return fallback;
    }

    int value = CheckInteger(L, -1, name);
    lua_pop(L, 1);
    return value;
}

static bool GetBooleanField(lua_State* L, int table_index, const char* name, bool fallback)
{
    table_index = AbsIndex(L, table_index);
    lua_getfield(L, table_index, name);
    bool value = lua_isnil(L, -1) ? fallback : lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return value;
}

static b3Vec3 ToB3(const dmVMath::Vector3& value)
{
    b3Vec3 result = { value.getX(), value.getY(), value.getZ() };
    return result;
}

static b3Quat ToB3(const dmVMath::Quat& value)
{
    b3Quat result = { { value.getX(), value.getY(), value.getZ() }, value.getW() };
    return result;
}

static b3Vec3 CheckVector3(lua_State* L, int index)
{
    b3Vec3 value = ToB3(*dmScript::CheckVector3(L, index));
    if (!b3IsValidVec3(value))
    {
        luaL_error(L, "vector3 values must be finite numbers");
    }
    return value;
}

static b3Quat CheckQuat(lua_State* L, int index)
{
    b3Quat value = ToB3(*dmScript::CheckQuat(L, index));
    if (!b3IsValidQuat(value))
    {
        luaL_error(L, "quaternion must contain finite numbers and have unit length");
    }
    return value;
}

static b3Vec3 GetVector3Field(lua_State* L, int table_index, const char* name, b3Vec3 fallback)
{
    table_index = AbsIndex(L, table_index);
    lua_getfield(L, table_index, name);
    b3Vec3 value = lua_isnil(L, -1) ? fallback : CheckVector3(L, -1);
    lua_pop(L, 1);
    return value;
}

static b3Quat GetQuatField(lua_State* L, int table_index, const char* name, b3Quat fallback)
{
    table_index = AbsIndex(L, table_index);
    lua_getfield(L, table_index, name);
    b3Quat value = lua_isnil(L, -1) ? fallback : CheckQuat(L, -1);
    lua_pop(L, 1);
    return value;
}

static void PushVector3(lua_State* L, b3Vec3 value)
{
    dmScript::PushVector3(L, dmVMath::Vector3(value.x, value.y, value.z));
}

static void PushQuat(lua_State* L, b3Quat value)
{
    dmScript::PushQuat(L, dmVMath::Quat(value.v.x, value.v.y, value.v.z, value.s));
}

static void SetNumberField(lua_State* L, const char* name, lua_Number value)
{
    lua_pushnumber(L, value);
    lua_setfield(L, -2, name);
}

static void SetBooleanField(lua_State* L, const char* name, bool value)
{
    lua_pushboolean(L, value);
    lua_setfield(L, -2, name);
}

static void SetVector3Field(lua_State* L, const char* name, b3Vec3 value)
{
    PushVector3(L, value);
    lua_setfield(L, -2, name);
}

static void SetQuatField(lua_State* L, const char* name, b3Quat value)
{
    PushQuat(L, value);
    lua_setfield(L, -2, name);
}

static WorldHandle* RawWorld(lua_State* L, int index)
{
    return (WorldHandle*)luaL_checkudata(L, index, WORLD_META);
}

static BodyHandle* RawBody(lua_State* L, int index)
{
    return (BodyHandle*)luaL_checkudata(L, index, BODY_META);
}

static ShapeHandle* RawShape(lua_State* L, int index)
{
    return (ShapeHandle*)luaL_checkudata(L, index, SHAPE_META);
}

static b3WorldId CheckWorld(lua_State* L, int index)
{
    WorldHandle* handle = RawWorld(L, index);
    if (!b3World_IsValid(handle->id))
    {
        luaL_error(L, "attempt to use a destroyed or invalid Box3D world");
    }
    return handle->id;
}

static b3BodyId CheckBody(lua_State* L, int index)
{
    BodyHandle* handle = RawBody(L, index);
    if (!b3Body_IsValid(handle->id))
    {
        luaL_error(L, "attempt to use a destroyed or invalid Box3D body");
    }
    return handle->id;
}

static b3ShapeId CheckShape(lua_State* L, int index)
{
    ShapeHandle* handle = RawShape(L, index);
    if (!b3Shape_IsValid(handle->id))
    {
        luaL_error(L, "attempt to use a destroyed or invalid Box3D shape");
    }
    return handle->id;
}

static WorldHandle* PushWorld(lua_State* L, b3WorldId id)
{
    WorldHandle* handle = (WorldHandle*)lua_newuserdata(L, sizeof(WorldHandle));
    handle->id = id;
    luaL_getmetatable(L, WORLD_META);
    lua_setmetatable(L, -2);
    return handle;
}

static BodyHandle* PushBody(lua_State* L, b3BodyId id)
{
    BodyHandle* handle = (BodyHandle*)lua_newuserdata(L, sizeof(BodyHandle));
    handle->id = id;
    luaL_getmetatable(L, BODY_META);
    lua_setmetatable(L, -2);
    return handle;
}

static ShapeHandle* PushShape(lua_State* L, b3ShapeId id)
{
    ShapeHandle* handle = (ShapeHandle*)lua_newuserdata(L, sizeof(ShapeHandle));
    handle->id = id;
    luaL_getmetatable(L, SHAPE_META);
    lua_setmetatable(L, -2);
    return handle;
}

static void PushCreatedShape(lua_State* L, b3ShapeId id)
{
    if (!b3Shape_IsValid(id))
    {
        luaL_error(L, "Box3D failed to create a shape");
    }
    PushShape(L, id);
}

static void TrackWorld(lua_State* L, b3WorldId id)
{
    for (int i = 0; i < B3_MAX_WORLDS; ++i)
    {
        if (B3_IS_NULL(g_Worlds[i]))
        {
            g_Worlds[i] = id;
            return;
        }
    }

    b3DestroyWorld(id);
    luaL_error(L, "Box3D world registry is full");
}

static void UntrackWorld(b3WorldId id)
{
    uint32_t stored = b3StoreWorldId(id);
    for (int i = 0; i < B3_MAX_WORLDS; ++i)
    {
        if (B3_IS_NON_NULL(g_Worlds[i]) && b3StoreWorldId(g_Worlds[i]) == stored)
        {
            g_Worlds[i] = b3_nullWorldId;
            return;
        }
    }
}

static int DestroyWorldHandle(lua_State* L)
{
    WorldHandle* handle = RawWorld(L, 1);
    if (b3World_IsValid(handle->id))
    {
        UntrackWorld(handle->id);
        b3DestroyWorld(handle->id);
    }
    handle->id = b3_nullWorldId;
    return 0;
}

static int WorldToString(lua_State* L)
{
    WorldHandle* handle = RawWorld(L, 1);
    lua_pushfstring(L, "box3d.world(%s)", b3World_IsValid(handle->id) ? "valid" : "invalid");
    return 1;
}

static int BodyToString(lua_State* L)
{
    BodyHandle* handle = RawBody(L, 1);
    lua_pushfstring(L, "box3d.body(%s)", b3Body_IsValid(handle->id) ? "valid" : "invalid");
    return 1;
}

static int ShapeToString(lua_State* L)
{
    ShapeHandle* handle = RawShape(L, 1);
    lua_pushfstring(L, "box3d.shape(%s)", b3Shape_IsValid(handle->id) ? "valid" : "invalid");
    return 1;
}

static b3BodyType GetBodyType(lua_State* L, int table_index)
{
    table_index = AbsIndex(L, table_index);
    lua_getfield(L, table_index, "type");
    b3BodyType type = b3_staticBody;
    if (lua_isnumber(L, -1))
    {
        int value = CheckInteger(L, -1, "body type");
        if (value < (int)b3_staticBody || value > (int)b3_dynamicBody)
        {
            luaL_error(L, "body type must be BODY_TYPE_STATIC, BODY_TYPE_KINEMATIC, or BODY_TYPE_DYNAMIC");
        }
        type = (b3BodyType)value;
    }
    else if (lua_isstring(L, -1))
    {
        const char* value = lua_tostring(L, -1);
        if (strcmp(value, "static") == 0) type = b3_staticBody;
        else if (strcmp(value, "kinematic") == 0) type = b3_kinematicBody;
        else if (strcmp(value, "dynamic") == 0) type = b3_dynamicBody;
        else luaL_error(L, "unknown body type '%s'", value);
    }
    else if (!lua_isnil(L, -1))
    {
        luaL_error(L, "body type must be a Box3D constant or string");
    }
    lua_pop(L, 1);
    return type;
}

static b3ShapeDef ReadShapeDef(lua_State* L, int table_index)
{
    b3ShapeDef def = b3DefaultShapeDef();
    def.density = GetNumberField(L, table_index, "density", def.density);
    def.explosionScale = GetNumberField(L, table_index, "explosion_scale", def.explosionScale);
    def.baseMaterial.friction = GetNumberField(L, table_index, "friction", def.baseMaterial.friction);
    def.baseMaterial.restitution = GetNumberField(L, table_index, "restitution", def.baseMaterial.restitution);
    def.baseMaterial.rollingResistance = GetNumberField(L, table_index, "rolling_resistance", def.baseMaterial.rollingResistance);
    def.baseMaterial.tangentVelocity = GetVector3Field(L, table_index, "tangent_velocity", def.baseMaterial.tangentVelocity);
    def.isSensor = GetBooleanField(L, table_index, "is_sensor", def.isSensor);
    def.enableSensorEvents = GetBooleanField(L, table_index, "enable_sensor_events", def.enableSensorEvents);
    def.enableContactEvents = GetBooleanField(L, table_index, "enable_contact_events", def.enableContactEvents);
    def.enableHitEvents = GetBooleanField(L, table_index, "enable_hit_events", def.enableHitEvents);
    def.updateBodyMass = GetBooleanField(L, table_index, "update_body_mass", def.updateBodyMass);
    def.filter.categoryBits = GetUInt64Field(L, table_index, "category_bits", def.filter.categoryBits);
    def.filter.maskBits = GetUInt64Field(L, table_index, "mask_bits", def.filter.maskBits);
    def.filter.groupIndex = GetIntegerField(L, table_index, "group_index", def.filter.groupIndex);
    if (!b3IsValidFloat(def.density) || def.density < 0.0f)
        luaL_error(L, "shape density must be a finite non-negative number");
    if (!b3IsValidFloat(def.baseMaterial.friction) || def.baseMaterial.friction < 0.0f)
        luaL_error(L, "shape friction must be a finite non-negative number");
    if (!b3IsValidFloat(def.baseMaterial.restitution) || def.baseMaterial.restitution < 0.0f)
        luaL_error(L, "shape restitution must be a finite non-negative number");
    if (!b3IsValidFloat(def.baseMaterial.rollingResistance) || def.baseMaterial.rollingResistance < 0.0f)
        luaL_error(L, "shape rolling_resistance must be a finite non-negative number");
    return def;
}

static int GetVersion(lua_State* L)
{
    b3Version version = b3GetVersion();
    lua_newtable(L);
    SetNumberField(L, "major", version.major);
    SetNumberField(L, "minor", version.minor);
    SetNumberField(L, "revision", version.revision);
    lua_pushstring(L, UPSTREAM_COMMIT);
    lua_setfield(L, -2, "upstream_commit");
    return 1;
}

static int CreateWorld(lua_State* L)
{
    b3WorldDef def = b3DefaultWorldDef();
    if (!lua_isnoneornil(L, 1))
    {
        luaL_checktype(L, 1, LUA_TTABLE);
        def.gravity = GetVector3Field(L, 1, "gravity", def.gravity);
        def.restitutionThreshold = GetNumberField(L, 1, "restitution_threshold", def.restitutionThreshold);
        def.hitEventThreshold = GetNumberField(L, 1, "hit_event_threshold", def.hitEventThreshold);
        def.maximumLinearSpeed = GetNumberField(L, 1, "maximum_linear_speed", def.maximumLinearSpeed);
        def.enableSleep = GetBooleanField(L, 1, "enable_sleep", def.enableSleep);
        def.enableContinuous = GetBooleanField(L, 1, "enable_continuous", def.enableContinuous);
        int worker_count = GetIntegerField(L, 1, "worker_count", (int)def.workerCount);
        if (worker_count < 1 || worker_count > B3_MAX_WORKERS)
        {
            return luaL_error(L, "worker_count must be between 1 and %d", B3_MAX_WORKERS);
        }
        def.workerCount = (uint32_t)worker_count;
#if defined(DM_PLATFORM_HTML5)
        if (def.workerCount > 1)
        {
            return luaL_error(L, "wasm-web supports worker_count = 1; use a threaded web target for multithreading");
        }
#endif
    }

    if (!b3IsValidVec3(def.gravity)) return luaL_error(L, "world gravity must contain finite numbers");
    if (def.restitutionThreshold < 0.0f) return luaL_error(L, "restitution_threshold must be non-negative");
    if (def.hitEventThreshold < 0.0f) return luaL_error(L, "hit_event_threshold must be non-negative");
    if (def.maximumLinearSpeed <= 0.0f) return luaL_error(L, "maximum_linear_speed must be positive");

    b3WorldId id = b3CreateWorld(&def);
    if (!b3World_IsValid(id))
    {
        return luaL_error(L, "Box3D failed to create a world");
    }
    TrackWorld(L, id);
    PushWorld(L, id);
    return 1;
}

static int IsWorldValid(lua_State* L)
{
    lua_pushboolean(L, b3World_IsValid(RawWorld(L, 1)->id));
    return 1;
}

static int Step(lua_State* L)
{
    b3WorldId world = CheckWorld(L, 1);
    float dt = (float)luaL_checknumber(L, 2);
    int substeps = 4;
    if (!lua_isnoneornil(L, 3))
    {
        substeps = CheckInteger(L, 3, "substep count");
    }
    if (!b3IsValidFloat(dt) || dt < 0.0f) return luaL_error(L, "time step must be finite and non-negative");
    if (substeps < 1) return luaL_error(L, "substep count must be at least 1");
    b3World_Step(world, dt, substeps);
    return 0;
}

static int SetGravity(lua_State* L)
{
    b3World_SetGravity(CheckWorld(L, 1), CheckVector3(L, 2));
    return 0;
}

static int GetGravity(lua_State* L)
{
    PushVector3(L, b3World_GetGravity(CheckWorld(L, 1)));
    return 1;
}

static int CreateBody(lua_State* L)
{
    b3WorldId world = CheckWorld(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    b3BodyDef def = b3DefaultBodyDef();
    def.type = GetBodyType(L, 2);
    def.position = GetVector3Field(L, 2, "position", def.position);
    def.rotation = GetQuatField(L, 2, "rotation", def.rotation);
    def.linearVelocity = GetVector3Field(L, 2, "linear_velocity", def.linearVelocity);
    def.angularVelocity = GetVector3Field(L, 2, "angular_velocity", def.angularVelocity);
    def.linearDamping = GetNumberField(L, 2, "linear_damping", def.linearDamping);
    def.angularDamping = GetNumberField(L, 2, "angular_damping", def.angularDamping);
    def.gravityScale = GetNumberField(L, 2, "gravity_scale", def.gravityScale);
    def.sleepThreshold = GetNumberField(L, 2, "sleep_threshold", def.sleepThreshold);
    def.enableSleep = GetBooleanField(L, 2, "enable_sleep", def.enableSleep);
    def.isAwake = GetBooleanField(L, 2, "is_awake", def.isAwake);
    def.isBullet = GetBooleanField(L, 2, "is_bullet", def.isBullet);
    def.isEnabled = GetBooleanField(L, 2, "is_enabled", def.isEnabled);
    def.allowFastRotation = GetBooleanField(L, 2, "allow_fast_rotation", def.allowFastRotation);
    def.motionLocks.linearX = GetBooleanField(L, 2, "lock_linear_x", def.motionLocks.linearX);
    def.motionLocks.linearY = GetBooleanField(L, 2, "lock_linear_y", def.motionLocks.linearY);
    def.motionLocks.linearZ = GetBooleanField(L, 2, "lock_linear_z", def.motionLocks.linearZ);
    def.motionLocks.angularX = GetBooleanField(L, 2, "lock_angular_x", def.motionLocks.angularX);
    def.motionLocks.angularY = GetBooleanField(L, 2, "lock_angular_y", def.motionLocks.angularY);
    def.motionLocks.angularZ = GetBooleanField(L, 2, "lock_angular_z", def.motionLocks.angularZ);

    if (!b3IsValidPosition(def.position)) return luaL_error(L, "body position must contain finite numbers");
    if (!b3IsValidQuat(def.rotation)) return luaL_error(L, "body rotation must be a finite unit quaternion");
    if (!b3IsValidVec3(def.linearVelocity)) return luaL_error(L, "linear_velocity must contain finite numbers");
    if (!b3IsValidVec3(def.angularVelocity)) return luaL_error(L, "angular_velocity must contain finite numbers");
    if (def.linearDamping < 0.0f) return luaL_error(L, "linear_damping must be non-negative");
    if (def.angularDamping < 0.0f) return luaL_error(L, "angular_damping must be non-negative");
    if (def.sleepThreshold < 0.0f) return luaL_error(L, "sleep_threshold must be non-negative");

    b3BodyId id = b3CreateBody(world, &def);
    if (!b3Body_IsValid(id)) return luaL_error(L, "Box3D failed to create a body");
    PushBody(L, id);
    return 1;
}

static int DestroyBody(lua_State* L)
{
    BodyHandle* handle = RawBody(L, 1);
    if (b3Body_IsValid(handle->id)) b3DestroyBody(handle->id);
    handle->id = b3_nullBodyId;
    return 0;
}

static int IsBodyValid(lua_State* L)
{
    lua_pushboolean(L, b3Body_IsValid(RawBody(L, 1)->id));
    return 1;
}

static int CreateBox(lua_State* L)
{
    b3BodyId body = CheckBody(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_getfield(L, 2, "half_extents");
    if (lua_isnil(L, -1)) return luaL_error(L, "create_box requires half_extents");
    b3Vec3 half_extents = CheckVector3(L, -1);
    lua_pop(L, 1);
    if (half_extents.x <= 0.0f || half_extents.y <= 0.0f || half_extents.z <= 0.0f)
        return luaL_error(L, "box half_extents must be positive");

    b3ShapeDef def = ReadShapeDef(L, 2);
    b3BoxHull box = b3MakeBoxHull(half_extents.x, half_extents.y, half_extents.z);
    PushCreatedShape(L, b3CreateHullShape(body, &def, &box.base));
    return 1;
}

static int CreateSphere(lua_State* L)
{
    b3BodyId body = CheckBody(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    b3ShapeDef def = ReadShapeDef(L, 2);
    b3Sphere sphere;
    sphere.center = GetVector3Field(L, 2, "center", b3Vec3_zero);
    sphere.radius = GetNumberField(L, 2, "radius", -1.0f);
    if (sphere.radius <= 0.0f) return luaL_error(L, "sphere radius must be positive");
    PushCreatedShape(L, b3CreateSphereShape(body, &def, &sphere));
    return 1;
}

static int CreateCapsule(lua_State* L)
{
    b3BodyId body = CheckBody(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    b3ShapeDef def = ReadShapeDef(L, 2);
    b3Capsule capsule;
    capsule.center1 = GetVector3Field(L, 2, "center1", b3Vec3_zero);
    capsule.center2 = GetVector3Field(L, 2, "center2", b3Vec3_zero);
    capsule.radius = GetNumberField(L, 2, "radius", -1.0f);
    if (capsule.radius <= 0.0f) return luaL_error(L, "capsule radius must be positive");
    PushCreatedShape(L, b3CreateCapsuleShape(body, &def, &capsule));
    return 1;
}

static int CreateCylinder(lua_State* L)
{
    b3BodyId body = CheckBody(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    float height = GetNumberField(L, 2, "height", -1.0f);
    float radius = GetNumberField(L, 2, "radius", -1.0f);
    float y_offset = GetNumberField(L, 2, "y_offset", 0.0f);
    int sides = GetIntegerField(L, 2, "sides", 16);
    if (height <= 0.0f || radius <= 0.0f) return luaL_error(L, "cylinder height and radius must be positive");
    if (sides < 3 || sides > 32) return luaL_error(L, "cylinder sides must be between 3 and 32");

    b3ShapeDef def = ReadShapeDef(L, 2);
    b3HullData* cylinder = b3CreateCylinder(height, radius, y_offset, sides);
    if (cylinder == 0) return luaL_error(L, "Box3D failed to create cylinder geometry");
    b3ShapeId shape = b3CreateHullShape(body, &def, cylinder);
    b3DestroyHull(cylinder);
    PushCreatedShape(L, shape);
    return 1;
}

static int DestroyShape(lua_State* L)
{
    ShapeHandle* handle = RawShape(L, 1);
    bool update_mass = lua_isnoneornil(L, 2) ? true : lua_toboolean(L, 2) != 0;
    if (b3Shape_IsValid(handle->id)) b3DestroyShape(handle->id, update_mass);
    handle->id = b3_nullShapeId;
    return 0;
}

static int IsShapeValid(lua_State* L)
{
    lua_pushboolean(L, b3Shape_IsValid(RawShape(L, 1)->id));
    return 1;
}

static int GetShapeBody(lua_State* L)
{
    PushBody(L, b3Shape_GetBody(CheckShape(L, 1)));
    return 1;
}

static int GetTransform(lua_State* L)
{
    b3WorldTransform transform = b3Body_GetTransform(CheckBody(L, 1));
    PushVector3(L, transform.p);
    PushQuat(L, transform.q);
    return 2;
}

static int SetTransform(lua_State* L)
{
    b3Body_SetTransform(CheckBody(L, 1), CheckVector3(L, 2), CheckQuat(L, 3));
    return 0;
}

static int GetLinearVelocity(lua_State* L)
{
    PushVector3(L, b3Body_GetLinearVelocity(CheckBody(L, 1)));
    return 1;
}

static int SetLinearVelocity(lua_State* L)
{
    b3Body_SetLinearVelocity(CheckBody(L, 1), CheckVector3(L, 2));
    return 0;
}

static int GetAngularVelocity(lua_State* L)
{
    PushVector3(L, b3Body_GetAngularVelocity(CheckBody(L, 1)));
    return 1;
}

static int SetAngularVelocity(lua_State* L)
{
    b3Body_SetAngularVelocity(CheckBody(L, 1), CheckVector3(L, 2));
    return 0;
}

static bool OptionalWake(lua_State* L, int index)
{
    return lua_isnoneornil(L, index) ? true : lua_toboolean(L, index) != 0;
}

static int ApplyForce(lua_State* L)
{
    b3Body_ApplyForce(CheckBody(L, 1), CheckVector3(L, 2), CheckVector3(L, 3), OptionalWake(L, 4));
    return 0;
}

static int ApplyForceToCenter(lua_State* L)
{
    b3Body_ApplyForceToCenter(CheckBody(L, 1), CheckVector3(L, 2), OptionalWake(L, 3));
    return 0;
}

static int ApplyTorque(lua_State* L)
{
    b3Body_ApplyTorque(CheckBody(L, 1), CheckVector3(L, 2), OptionalWake(L, 3));
    return 0;
}

static int ApplyLinearImpulse(lua_State* L)
{
    b3Body_ApplyLinearImpulse(CheckBody(L, 1), CheckVector3(L, 2), CheckVector3(L, 3), OptionalWake(L, 4));
    return 0;
}

static int ApplyLinearImpulseToCenter(lua_State* L)
{
    b3Body_ApplyLinearImpulseToCenter(CheckBody(L, 1), CheckVector3(L, 2), OptionalWake(L, 3));
    return 0;
}

static int ApplyAngularImpulse(lua_State* L)
{
    b3Body_ApplyAngularImpulse(CheckBody(L, 1), CheckVector3(L, 2), OptionalWake(L, 3));
    return 0;
}

static int GetMass(lua_State* L)
{
    lua_pushnumber(L, b3Body_GetMass(CheckBody(L, 1)));
    return 1;
}

static int IsAwake(lua_State* L)
{
    lua_pushboolean(L, b3Body_IsAwake(CheckBody(L, 1)));
    return 1;
}

static int SetAwake(lua_State* L)
{
    b3Body_SetAwake(CheckBody(L, 1), lua_toboolean(L, 2) != 0);
    return 0;
}

static int GetFriction(lua_State* L)
{
    lua_pushnumber(L, b3Shape_GetFriction(CheckShape(L, 1)));
    return 1;
}

static int SetFriction(lua_State* L)
{
    float value = (float)luaL_checknumber(L, 2);
    if (!b3IsValidFloat(value) || value < 0.0f) return luaL_error(L, "friction must be a finite non-negative number");
    b3Shape_SetFriction(CheckShape(L, 1), value);
    return 0;
}

static int GetRestitution(lua_State* L)
{
    lua_pushnumber(L, b3Shape_GetRestitution(CheckShape(L, 1)));
    return 1;
}

static int SetRestitution(lua_State* L)
{
    float value = (float)luaL_checknumber(L, 2);
    if (!b3IsValidFloat(value) || value < 0.0f) return luaL_error(L, "restitution must be a finite non-negative number");
    b3Shape_SetRestitution(CheckShape(L, 1), value);
    return 0;
}

static int Explode(lua_State* L)
{
    b3WorldId world = CheckWorld(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    b3ExplosionDef def = b3DefaultExplosionDef();
    def.position = GetVector3Field(L, 2, "position", def.position);
    def.radius = GetNumberField(L, 2, "radius", def.radius);
    def.falloff = GetNumberField(L, 2, "falloff", def.falloff);
    def.impulsePerArea = GetNumberField(L, 2, "impulse_per_area", def.impulsePerArea);
    def.maskBits = GetUInt64Field(L, 2, "mask_bits", def.maskBits);
    if (!b3IsValidFloat(def.radius) || def.radius < 0.0f) return luaL_error(L, "explosion radius must be finite and non-negative");
    if (!b3IsValidFloat(def.falloff) || def.falloff < 0.0f) return luaL_error(L, "explosion falloff must be finite and non-negative");
    if (!b3IsValidFloat(def.impulsePerArea)) return luaL_error(L, "impulse_per_area must be finite");
    b3World_Explode(world, &def);
    return 0;
}

static int GetBodyEvents(lua_State* L)
{
    b3BodyEvents events = b3World_GetBodyEvents(CheckWorld(L, 1));
    lua_createtable(L, events.moveCount, 0);
    for (int i = 0; i < events.moveCount; ++i)
    {
        const b3BodyMoveEvent& event = events.moveEvents[i];
        lua_createtable(L, 0, 4);
        PushBody(L, event.bodyId);
        lua_setfield(L, -2, "body");
        SetVector3Field(L, "position", event.transform.p);
        SetQuatField(L, "rotation", event.transform.q);
        SetBooleanField(L, "fell_asleep", event.fellAsleep);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static void PushSensorEvent(lua_State* L, b3ShapeId sensor, b3ShapeId visitor)
{
    lua_createtable(L, 0, 2);
    PushShape(L, sensor);
    lua_setfield(L, -2, "sensor_shape");
    PushShape(L, visitor);
    lua_setfield(L, -2, "visitor_shape");
}

static int GetSensorEvents(lua_State* L)
{
    b3SensorEvents events = b3World_GetSensorEvents(CheckWorld(L, 1));
    lua_createtable(L, 0, 2);
    lua_createtable(L, events.beginCount, 0);
    for (int i = 0; i < events.beginCount; ++i)
    {
        PushSensorEvent(L, events.beginEvents[i].sensorShapeId, events.beginEvents[i].visitorShapeId);
        lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, -2, "begin_events");
    lua_createtable(L, events.endCount, 0);
    for (int i = 0; i < events.endCount; ++i)
    {
        PushSensorEvent(L, events.endEvents[i].sensorShapeId, events.endEvents[i].visitorShapeId);
        lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, -2, "end_events");
    return 1;
}

static void PushContactPair(lua_State* L, b3ShapeId a, b3ShapeId b)
{
    lua_createtable(L, 0, 2);
    PushShape(L, a);
    lua_setfield(L, -2, "shape_a");
    PushShape(L, b);
    lua_setfield(L, -2, "shape_b");
}

static int GetContactEvents(lua_State* L)
{
    b3ContactEvents events = b3World_GetContactEvents(CheckWorld(L, 1));
    lua_createtable(L, 0, 3);

    lua_createtable(L, events.beginCount, 0);
    for (int i = 0; i < events.beginCount; ++i)
    {
        PushContactPair(L, events.beginEvents[i].shapeIdA, events.beginEvents[i].shapeIdB);
        lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, -2, "begin_events");

    lua_createtable(L, events.endCount, 0);
    for (int i = 0; i < events.endCount; ++i)
    {
        PushContactPair(L, events.endEvents[i].shapeIdA, events.endEvents[i].shapeIdB);
        lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, -2, "end_events");

    lua_createtable(L, events.hitCount, 0);
    for (int i = 0; i < events.hitCount; ++i)
    {
        const b3ContactHitEvent& event = events.hitEvents[i];
        lua_createtable(L, 0, 5);
        PushShape(L, event.shapeIdA);
        lua_setfield(L, -2, "shape_a");
        PushShape(L, event.shapeIdB);
        lua_setfield(L, -2, "shape_b");
        SetVector3Field(L, "point", event.point);
        SetVector3Field(L, "normal", event.normal);
        SetNumberField(L, "approach_speed", event.approachSpeed);
        lua_rawseti(L, -2, i + 1);
    }
    lua_setfield(L, -2, "hit_events");
    return 1;
}

static const luaL_reg ModuleMethods[] = {
    { "get_version", GetVersion },
    { "create_world", CreateWorld },
    { "destroy_world", DestroyWorldHandle },
    { "is_world_valid", IsWorldValid },
    { "step", Step },
    { "set_gravity", SetGravity },
    { "get_gravity", GetGravity },
    { "create_body", CreateBody },
    { "destroy_body", DestroyBody },
    { "is_body_valid", IsBodyValid },
    { "create_box", CreateBox },
    { "create_sphere", CreateSphere },
    { "create_capsule", CreateCapsule },
    { "create_cylinder", CreateCylinder },
    { "destroy_shape", DestroyShape },
    { "is_shape_valid", IsShapeValid },
    { "get_shape_body", GetShapeBody },
    { "get_transform", GetTransform },
    { "set_transform", SetTransform },
    { "get_linear_velocity", GetLinearVelocity },
    { "set_linear_velocity", SetLinearVelocity },
    { "get_angular_velocity", GetAngularVelocity },
    { "set_angular_velocity", SetAngularVelocity },
    { "apply_force", ApplyForce },
    { "apply_force_to_center", ApplyForceToCenter },
    { "apply_torque", ApplyTorque },
    { "apply_linear_impulse", ApplyLinearImpulse },
    { "apply_linear_impulse_to_center", ApplyLinearImpulseToCenter },
    { "apply_angular_impulse", ApplyAngularImpulse },
    { "get_mass", GetMass },
    { "is_awake", IsAwake },
    { "set_awake", SetAwake },
    { "get_friction", GetFriction },
    { "set_friction", SetFriction },
    { "get_restitution", GetRestitution },
    { "set_restitution", SetRestitution },
    { "explode", Explode },
    { "get_body_events", GetBodyEvents },
    { "get_sensor_events", GetSensorEvents },
    { "get_contact_events", GetContactEvents },
    { 0, 0 }
};

static void RegisterMetatable(lua_State* L, const char* name, lua_CFunction tostring_function, lua_CFunction gc_function)
{
    luaL_newmetatable(L, name);
    lua_pushcfunction(L, tostring_function);
    lua_setfield(L, -2, "__tostring");
    if (gc_function != 0)
    {
        lua_pushcfunction(L, gc_function);
        lua_setfield(L, -2, "__gc");
    }
    lua_pop(L, 1);
}

static void LuaInit(lua_State* L)
{
    RegisterMetatable(L, WORLD_META, WorldToString, DestroyWorldHandle);
    RegisterMetatable(L, BODY_META, BodyToString, 0);
    RegisterMetatable(L, SHAPE_META, ShapeToString, 0);

    luaL_register(L, MODULE_NAME, ModuleMethods);
    lua_pushnumber(L, b3_staticBody);
    lua_setfield(L, -2, "BODY_TYPE_STATIC");
    lua_pushnumber(L, b3_kinematicBody);
    lua_setfield(L, -2, "BODY_TYPE_KINEMATIC");
    lua_pushnumber(L, b3_dynamicBody);
    lua_setfield(L, -2, "BODY_TYPE_DYNAMIC");
    lua_pushstring(L, UPSTREAM_COMMIT);
    lua_setfield(L, -2, "UPSTREAM_COMMIT");
    lua_pop(L, 1);
}
}

static dmExtension::Result AppInitialize(dmExtension::AppParams*)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result Initialize(dmExtension::Params* params)
{
    memset(g_Worlds, 0, sizeof(g_Worlds));
    LuaInit(params->m_L);
    dmLogInfo("Registered %s (Box3D %s)", MODULE_NAME, UPSTREAM_COMMIT);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalize(dmExtension::AppParams*)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result Finalize(dmExtension::Params*)
{
    for (int i = 0; i < B3_MAX_WORLDS; ++i)
    {
        if (b3World_IsValid(g_Worlds[i]))
        {
            b3DestroyWorld(g_Worlds[i]);
        }
        g_Worlds[i] = b3_nullWorldId;
    }
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(Box3DExtension, LIB_NAME, AppInitialize, AppFinalize, Initialize, 0, 0, Finalize)
