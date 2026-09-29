#ifndef MAIN_TMD_TYPES_H
#define MAIN_TMD_TYPES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/coord.h"

/// One entry of the `TmdSource.skeleton` array: a bone's rest transform and the
/// bone it hangs from.
///
/// The array is the model's rest pose, one bone per part. A part's vertices are
/// authored around its own bone, so the skeleton is what puts the parts of a
/// model back in one place: `Tmd_Create` builds the model's per-part coordinate
/// array from these, and the matrices composed from it are what each part is
/// drawn under.
typedef struct {
    MATRIX local;  // Rest transform, in the space of the bone it hangs from
    s32    parent; // Bone it hangs from, as an index into the same array; its own index at the root
} TmdBone;
STATIC_ASSERT_SIZEOF(TmdBone, 0x24);

/// One model as its package ships it: the vertex and normal arrays, the packet
/// stream that draws its parts, and the bone rest pose that places them.
///
/// A package lays a model out as `[vertices][normals][packet stream][record]`
/// with the record last, and every pointer here is an address into that same
/// package, so a model stays whole at whatever address its package loads. A
/// `TmdObject` points here, and the two divide the work between them: the
/// record carries what shipped, while the buffer the model is decoded into and
/// the per-part coordinate array belong to the object.
///
/// The record is not all read-only. The packet stream is resolved to handlers
/// in place the first time the model is used, and `handlersResolved` is how the
/// record says that has happened.
typedef struct {
    s32      handlersResolved; // Zero as shipped, set once the packet stream has been resolved to handlers
    s32      halfSize;         // Size of one half of the model's buffer in bytes; the object allocates both halves together
    s32      firstRegionSize;  // Size of the first of a half's two prim regions, i.e. the offset the second starts at
    s32      partCount;        // Parts the model is divided into; one bone each
    u32*     partVerts;        // Vertex count per part, summing to the vertex array's length
    SVECTOR* verts;            // Vertices, grouped by part
    SVECTOR* normals;          // Normals, indexed independently of the vertices
    TmdBone* skeleton;         // Rest pose: one bone per part, carrying its parent index
    u32*     stream;           // Packet stream, the drawing instructions for the model's parts
} TmdSource;
STATIC_ASSERT_SIZEOF(TmdSource, 0x24);

/// One node of one of the TMD lists: a list's head, or the link every element
/// on it embeds as its first member, from which `PARENT_OF` recovers the
/// element.
///
/// A head is a bare node belonging to no element. Its `next` is the first node
/// and its `prev` the last, which is the head itself while the list is empty,
/// so a walk stops on `next` alone and the head is reachable only through
/// `prev`. Linking and unlinking are written against the pair rather than
/// against whichever type the elements are, so an unlink body differs from the
/// next one only in the head it names.
typedef struct _TmdListHead {
    struct _TmdListHead* next; // Following node, or NULL past the last
    struct _TmdListHead* prev; // Preceding node, or the head at the front
} TmdListHead;
STATIC_ASSERT_SIZEOF(TmdListHead, 0x8);

/// An attached model body: an element of `gTmdList` and the object a
/// spawnType-1 `Task` carries in `Task::extra`.
///
/// The object owns what a model needs at run time — the buffer its packet
/// stream is decoded into, the coordinate array that places its parts, the
/// light and colour matrices it is drawn under — while the `TmdSource` it
/// points at owns what the package shipped. `coords` is part of this object's
/// own allocation, laid out directly after it, so the two are one block.
///
/// The buffer holds two halves and the passes alternate between them, so no
/// pass reads the half it writes: `bufferIndex` selects the half in use and
/// each pass flips it.
typedef struct {
    TmdListHead link;        // Its place on `gTmdList`
    GfxCoord*   coords;      // Per-part coordinate array, part of this object's own block
    u16         flags;       // State bits (0x2 drawn semi-transparent, 0x4 buffer allocated by whoever created it, 0x8 drawn by the flagged pass, 0x10 drawn as a reflection, its faces winding the other way, 0x80 hidden)
    s8          otOffset;    // Ordering-table offset the model's primitives are linked at
    byte        unknown_F;
    TmdSource*  source;      // The model as its package shipped it
    u16         bufferIndex; // Which half of the buffer is in use (0/1); each pass flips it
    u16         halfSize;    // Size of one buffer half, cached from the source
    void*       buffer;      // Both buffer halves, allocated together; NULL while there are none
    MATRIX*     lightMtx;    // Light matrix the model is drawn under
    MATRIX*     colorMtx;    // Colour matrix the model is drawn under
    s8          tpage;       // Texture page the model's primitives are offset by
    s8          clut;        // CLUT the model's primitives are offset by, in 64-entry rows
    u8          tpageOffset; // Further texture page offset the handlers that use one add to a primitive
    u8          clutOffset;  // Further CLUT row offset the handlers that use one add, in 64-entry rows
    byte        unknown_28[0x4];
    s32         lightLevel;  // Lighting, 12.4 fixed point (0x1000 fully lit)
    s32         partCount;   // Parts the model is divided into, cached from the source
} TmdObject;
STATIC_ASSERT_SIZEOF(TmdObject, 0x34);

/// Tmd_Create allocates the object and its partCount coordinates as one block.
/// The coordinate tail has no fixed capacity; its extent comes from the source.
typedef struct {
    TmdObject object;
    GfxCoord  coords[0];
} TmdAllocation;
STATIC_ASSERT_SIZEOF(TmdAllocation, 0x34);
STATIC_ASSERT(OFFSET_OF(TmdAllocation, coords) == 0x34, tmd_allocation_coords_offset);

/// One frame of the scratch a model's packet stream is walked in: what
/// `tmdProcessStream` pushes on `G_SCRATCH_HEAD` and passes to every stream
/// command it runs.
///
/// The frame carries the walk itself — which record is being run, how long its
/// elements are and how many of them there are, and where the next packet goes
/// — and the model state a command reads: the vertex and normal arrays, the
/// texture page and CLUT every textured primitive is offset by, and the object
/// being compiled.
///
/// A buffer half is two regions and the primitive's own opcode picks one: the
/// pre-transformed primitives, which are already in screen space, are built in
/// the first region, and every other primitive in the second.
///
/// The draw pass walks the same stream under `TmdScratchDrawBlock`, a second
/// frame over the same layout, and the commands are declared with one of the
/// two types. The slots only the draw pass fills — the screen-Z table, the
/// ordering table, the two GTE results, the vector scratches and the depth
/// shift — are declared here for that reason, and the run of bytes it reads and
/// this pass does not is left as a pad.
typedef struct {
    u8*        primWrite;     // Write cursor of the half's second region: the primitives the draw pass transforms
    u8*        preXformWrite; // Write cursor of its first region: the pre-transformed primitives, already in screen space
    SVECTOR*   verts;         // Vertex array the commands index
    SVECTOR*   normals;       // Normal array, indexed independently of the vertices
    s32*       szTable;       // Screen Z per vertex, written as the draw pass projects each one and read back by the primitives it does not project again
    u_long*    ot;            // Ordering table the primitives are linked into, at the object's own offset
    s32        elemStride;    // Stride of one element of the record, in words
    s32        elemCount;     // Elements in the record, counted down as the commands build them
    u32        opcode;        // Opcode word of the record, flags included
    s32        gteFlag;       // GTE FLAG as the element's last transform left it
    s32        gteResult;     // What the last GTE step left: a facing, a depth or a vertex's screen Z
    byte       pad_2C[0x44];
    s16        tpage;         // Texture page every textured primitive is offset by
    s16        clut;          // CLUT every textured primitive is offset by, in 64-entry rows
    SVECTOR    elemNormal;    // The element's normal, transformed and lit, held while its texture coordinate is computed
    DVECTOR    texCoord;      // Texture coordinate being computed for the element
    TmdObject* obj;           // The object whose stream is being walked
    s32        otDepthShift;  // Ordering-table depth shift a primitive is linked under
} TmdScratchModelBlock;
STATIC_ASSERT_SIZEOF(TmdScratchModelBlock, 0x88);

#endif // MAIN_TMD_TYPES_H
