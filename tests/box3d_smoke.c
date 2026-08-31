#include <box3d/box3d.h>

#include <stdio.h>

int main(void)
{
    b3WorldDef world_def = b3DefaultWorldDef();
    b3WorldId world = b3CreateWorld(&world_def);
    if (!b3World_IsValid(world))
    {
        fprintf(stderr, "world creation failed\n");
        return 1;
    }

    b3BodyDef ground_def = b3DefaultBodyDef();
    ground_def.position = (b3Vec3){ 0.0f, -0.5f, 0.0f };
    b3BodyId ground = b3CreateBody(world, &ground_def);
    b3BoxHull ground_box = b3MakeBoxHull(10.0f, 0.5f, 10.0f);
    b3ShapeDef ground_shape_def = b3DefaultShapeDef();
    b3CreateHullShape(ground, &ground_shape_def, &ground_box.base);

    b3BodyDef body_def = b3DefaultBodyDef();
    body_def.type = b3_dynamicBody;
    body_def.position = (b3Vec3){ 0.0f, 4.0f, 0.0f };
    b3BodyId body = b3CreateBody(world, &body_def);
    b3Sphere sphere = { { 0.0f, 0.0f, 0.0f }, 0.5f };
    b3ShapeDef shape_def = b3DefaultShapeDef();
    shape_def.density = 1.0f;
    b3CreateSphereShape(body, &shape_def, &sphere);

    for (int i = 0; i < 240; ++i)
    {
        b3World_Step(world, 1.0f / 60.0f, 4);
    }

    b3Pos position = b3Body_GetPosition(body);
    b3DestroyWorld(world);

    if (position.y < 0.45f || position.y > 0.60f)
    {
        fprintf(stderr, "unexpected resting height: %.6f\n", position.y);
        return 2;
    }

    printf("Box3D smoke test passed at y=%.6f\n", position.y);
    return 0;
}
