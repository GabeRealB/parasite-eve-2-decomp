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

/// Shared geometry, command stream and initial hierarchy for a TMD model.
///
/// Objects borrow this record, its geometry and its stream; keep them alive
/// until all referring objects are released. Creation copies the `partCount`
/// skeleton entries into each object's mutable coordinates. Geometry remains
/// writable for morphing, and handler resolution rewrites the stream in place.
///
/// Each object's primitive buffer has two `bufferHalfBytes` halves. Within a
/// half, pre-transformed primitives occupy the first `preXformRegionBytes`
/// bytes and directly transformed primitives follow. These are capacities,
/// not stream lengths; require 0 <= preXformRegionBytes <= bufferHalfBytes <=
/// 65535 because the object caches the half size in a u16.
///
/// `partVertexCounts` describes the part-local vertex groups, but does not
/// establish the complete vertex-array extent. Neither geometry array's total
/// length is stored here. A stream-only source may omit both arrays and the
/// count table; commands must only reference geometry that is present.
///
/// Command headers contain an opcode, a draw-handler slot and a packed word
/// `(elementCount << 16) | elementStrideWords`, followed by the elements.
/// `TMD_STREAM_PART_END` closes a group and `TMD_STREAM_END` ends the stream.
typedef struct {
    s32            handlersResolved;    // Handler slots (0 unresolved, 1 resolved); geometry and opcodes remain writable
    s32            bufferHalfBytes;     // Byte capacity of one primitive-buffer half; both halves are allocated together
    s32            preXformRegionBytes; // Byte capacity of the first region, also the second region's offset within a half
    s32            partCount;           // Number of initial transforms and runtime coordinates; stream may have a final pre-transformed group
    const u32*     partVertexCounts;    // Part-local vertex counts: partCount entries when present, NULL for stream-only sources
    SVECTOR*       verts;               // Writable vertices in integer part-local coordinates
    SVECTOR*       normals;             // Writable normals, indexed independently of the vertices
    const TmdBone* skeleton;            // Initial pose: partCount entries, read only during object creation
    u32*           stream;              // Writable command words: opcode, handler slot, packed count/word stride, then elements
} TmdSource;
STATIC_ASSERT_SIZEOF(TmdSource, 0x24);

/// Word markers in `TmdSource.stream`; the final group may use pre-transformed
/// primitives without another skeleton entry.
enum {
    TMD_STREAM_PART_END = -2, // End of a command group; advance the part coordinate
    TMD_STREAM_END      = -1  // End of the complete stream
};

/// Intrusive link for an attached model or coordinate body, also used as a
/// list's sentinel head.
///
/// `TmdObject` and `ModelObjectCoordBody` embed this as their first member. The list being
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
