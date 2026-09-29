#ifndef ROOMS_DRYFIELD_TOILET_H
#define ROOMS_DRYFIELD_TOILET_H

#include "gameplay/area.h"

#include "main/task_types.h"

#include "main/coord.h"

// Retained morph target and snapshot buffers. The descriptor has the same
// four-vector-pointer layout as the dilapidated-house morph controller.
typedef struct {
    SVECTOR* vertices;
    SVECTOR* normals;
    SVECTOR* savedVertices;
    SVECTOR* savedNormals;
    s16 vertexCount;
    s16 normalCount;
    s16 firstVertex;
    s16 blendCount;
} ToiletMorphTarget;
STATIC_ASSERT_SIZEOF(ToiletMorphTarget, 24);

extern ToiletMorphTarget D_dryfield_toilet_801865D0;

void func_dryfield_toilet_8017DEF4(Task* arg0);
void func_dryfield_toilet_8017E64C(Task* arg0);
void func_dryfield_toilet_8017E69C(Task* arg0);
void func_dryfield_toilet_8017EBF4(Task* task);
void func_dryfield_toilet_8017F854(Task* arg0);
extern GpAreaVariant D_dryfield_toilet_80182918[13];

void func_dryfield_toilet_8017DCF0(Task* arg0);

#endif // ROOMS_DRYFIELD_TOILET_H
