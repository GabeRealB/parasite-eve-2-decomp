#ifndef MAIN_TMD_TYPES_H
#define MAIN_TMD_TYPES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/coord.h"

/// Initial local transform and parent index for one TMD model part.
///
/// `TmdSource.skeleton` contains `partCount` entries in part order. Creation
/// copies their matrices into the object's mutable coordinates and converts
/// the indices to parent links; animation changes those coordinates rather
/// than the source skeleton. Parent chains must reach a self-parented root,
/// which is initially attached to the view coordinate.
///
/// Matrix coefficients have 12 fractional bits (4096 = 1.0); translations
/// use the same integer model coordinates as the vertices.
typedef struct {
    MATRIX local;       // Initial transform in parent space
    s32    parentIndex; // Parent part index in [0, partCount); own index at a root
} TmdBone;
STATIC_ASSERT_SIZEOF(TmdBone, 0x24);

/// Model geometry, packet stream, and initial per-part transform hierarchy.
///
/// Packaged sources reference arrays in the same image. A `TmdObject` borrows
/// the source geometry and stream and owns its decoded buffers and mutable
/// per-part coordinate array.
///
/// The record is not all read-only. The packet stream is resolved to handlers
/// in place the first time the model is used, and `handlersResolved` is how the
/// record says that has happened.
typedef struct {
    s32            handlersResolved; // Zero as shipped, set once the packet stream has been resolved to handlers
    s32            halfSize;         // Size of one half of the model's buffer in bytes; the object allocates both halves together
    s32            firstRegionSize;  // Size of the first of a half's two prim regions, i.e. the offset the second starts at
    s32            partCount;        // Parts the model is divided into; one bone each
    u32*           partVerts;        // Vertex count per part, summing to the vertex array's length
    SVECTOR*       verts;            // Vertices, grouped by part
    SVECTOR*       normals;          // Normals, indexed independently of the vertices
    const TmdBone* skeleton;         // Initial pose: partCount entries, copied during creation and never changed through this pointer
    u32*           stream;           // Packet stream, the drawing instructions for the model's parts
} TmdSource;
STATIC_ASSERT_SIZEOF(TmdSource, 0x24);

/// Intrusive link for an attached model or 2D-display body, also used as a
/// list's sentinel head.
///
/// `TmdObject` and `GpDisp2d` embed this as their first member. The list being
/// walked determines which container `PARENT_OF` recovers; a sentinel belongs
/// to neither container. Elements must be unlinked before their owner frees
/// them, and their link fields are not cleared by unlinking.
///
/// A sentinel's `next` points to the first element and its `prev` to the last.
/// An empty list has `next == NULL` and `prev` pointing to the sentinel itself.
/// The first element's `prev` points to the sentinel; the last's `next` is
/// `NULL`, so forward walks never visit the sentinel.
typedef struct TmdListNode {
    struct TmdListNode* next; // Next element's link; first element at a head, NULL at the end
    struct TmdListNode* prev; // Previous link; tail at a head, sentinel at the front
} TmdListNode;
STATIC_ASSERT_SIZEOF(TmdListNode, 0x8);

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
    TmdListNode link;        // Its place on `gTmdList`
    GfxCoord*   coords;      // Per-part coordinate array, part of this object's own block
    u16         flags;       // State bits (0x2 semi-transparent primitives, 0x4 skip automatic buffer allocation, 0x8 selected by the flagged draw pass, 0x10 reverse face culling, 0x80 hidden)
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
