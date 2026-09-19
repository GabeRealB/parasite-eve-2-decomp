#ifndef TMD_H
#define TMD_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

struct Task;

// Types — TMD model lists (src/main/tmd.c; stage fade/MDEC lives in stage.c)

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

/// One node of one of the TMD lists: the link pair a head and every element on
/// it carry as their first two fields.
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
    TmdListHead*   next;        // Following node of the list, or NULL past the last
    TmdListHead*   prev;        // Preceding node, or the head at the front
    GsCOORDINATE2* coords;      // Per-part coordinate array, part of this object's own block
    u16            flags;       // State bits (0x4 buffer allocated by whoever created it, 0x8 drawn by the flagged pass, 0x80 hidden)
    s8             otOffset;    // Ordering-table offset the model's primitives are linked at
    byte           unknown_F;
    TmdSource*     source;      // The model as its package shipped it
    u16            bufferIndex; // Which half of the buffer is in use (0/1); each pass flips it
    u16            halfSize;    // Size of one buffer half, cached from the source
    void*          buffer;      // Both buffer halves, allocated together; NULL while there are none
    MATRIX*        lightMtx;    // Light matrix the model is drawn under
    MATRIX*        colorMtx;    // Colour matrix the model is drawn under
    u8             tpage;       // Texture page the model's primitives are offset by
    u8             clut;        // CLUT the model's primitives are offset by, in 64-entry rows
    u8             tpageOffset; // Further texture page offset the handlers that use one add to a primitive
    u8             clutOffset;  // Further CLUT row offset the handlers that use one add, in 64-entry rows
    byte           unknown_28[0x4];
    s32            lightLevel;  // Lighting, 12.4 fixed point (0x1000 fully lit)
    s32            partCount;   // Parts the model is divided into, cached from the source
} TmdObject;
STATIC_ASSERT_SIZEOF(TmdObject, 0x34);

/// Head of the model list: the anchor every attached `TmdObject` hangs from.
///
/// A model is linked here when its task attaches it and unlinked when the task
/// releases it, so the list is the model subsystem's whole view of what is
/// currently loaded. The size, buffer, draw and free passes work from it
/// instead of walking the task list.
extern TmdListHead gTmdList;
/// Head of the 2D-display list: the anchor for the coordinate nodes a task
/// attaches in place of a model.
///
/// A node here carries a single coordinate rather than a model with parts, so
/// the entry that follows the link pair is the coordinate rather than the
/// per-part array. The draw pass refreshes that coordinate for every node on
/// the list before it reaches the models and draws nothing from it; the task
/// that attached the node reads the refreshed matrix. The two lists are saved,
/// emptied and restored together.
extern TmdListHead gTmdDisp2dList;
/// Cleared by Tmd_InitLists during system init.
extern s32 D_80071210;

/// One frame of the scratch a model's packet stream is walked in: what
/// `Tmd_ProcessStream` pushes on `G_SCRATCH_HEAD` and passes to every stream
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

/// 0x98-byte scratch for Tmd_SetupDraw (draw path).
typedef struct {
    /* 0x00 */ u8*        field_0;
    /* 0x04 */ u8*        field_4;
    /* 0x08 */ s32        field_8;
    /* 0x0C */ s32        field_C;
    /* 0x10 */ void*      field_10;
    /* 0x14 */ u_long*    field_14;
    /* 0x18 */ byte       pad_18[0x38]; // Dispatch stores 0x18/0x1C/0x20/0x2C/0x30
    /* 0x50 */ MATRIX     mat;
    /* 0x70 */ byte       pad_70[0x10];
    /* 0x80 */ TmdObject* field_80;
    /* 0x84 */ s32        field_84;
    /* 0x88 */ byte       pad_88[0x10];
} TmdScratchDrawBlock;
STATIC_ASSERT_SIZEOF(TmdScratchDrawBlock, 0x98);

/// Draw-path stream command: Tmd_DispatchStream jalr → handler(ws, flags, stream).
typedef u32* (*TmdDrawStreamHandler)(TmdScratchDrawBlock* ws, s32 flags, u32* stream);

// --- APIs ---
void       Tmd_InitLists(void);
TmdObject* Tmd_Create(TmdSource* src, s32 flags);
void       Tmd_ProcessStream(TmdObject* obj);
void       Tmd_SetupDraw(TmdObject* obj);
void       Tmd_AllocMissingBuffers(void);
s32        Tmd_AllocBuffers(TmdObject* obj);
void       Tmd_FreeBuffers(TmdObject* obj);

/// Early-image handwritten GTE matrix load (src/main/hasm/Tmd_SetupGteMatrices.s).
void Tmd_SetupGteMatrices(TmdScratchDrawBlock* ws, u32 flags, void* stream, TmdObject* node);
/// Walk stream records and jalr each draw handler until terminator -2.
u32* Tmd_DispatchStream(TmdScratchDrawBlock* ws, s32 flags, u32* stream);
void Tmd_DrawFlaggedNodes(TmdObject* node);
void Tmd_DrawActiveNodes(TmdObject* node);

// The per-frame callback of the task that holds the models' buffers, and the
// states it runs in sequence. Each state walks `gTmdList` whole and advances
// the task to the next, so the list is only ever worked from a task that owns
// the pass.
void Tmd_DispatchTask(struct Task* task);
/// Marks every attached model as no longer drawn.
void Tmd_FlagAllNodes(struct Task* task);
/// Releases the buffer of every attached model.
void Tmd_FreeNodeBuffers(struct Task* task);
/// Gives a buffer back to every attached model that has none, then kills the task.
void Tmd_AllocNodeBuffers(struct Task* task);
/// Total bytes the attached models hold in their buffers.
s32 Tmd_SumBufferBytes(void);

// Early-image handlers (src/main/hasm/).
// Same ABI for model and draw scratch (shared offsets 0x18/0x1C/…); declared
// as model-side type for Tmd_ProcessStream. Draw path is jalr from hasm only.

/// Handler of a stream record nothing is built from: it steps over the record's
/// elements and returns the cursor that follows them.
///
/// A record is walked past even when nothing is made from it, because the walk
/// reads its next opcode where this handler leaves the cursor. A record whose
/// opcode names no handler of its own is left with this one, which is how both
/// passes over a model's stream step over what they do not build. The record
/// has no variant for `flags` to select, so it goes unread.
u32* tmdSkipStreamRecord(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Prim32(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Prim30(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Prim3A(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Prim38(TmdScratchModelBlock* ws, s32 flags, u32* stream);

// Early-image handlers in Tmd_StreamHandlers_Ops.s (named by stream opcode).
u32* Tmd_StreamHandler_Op20(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op60(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_OpC0(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op3A(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op38(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op7A(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op78(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_OpC8(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op3B(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op39(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op7B(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op79(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op00(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op40(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op1A(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op18(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op58(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op5A(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op130(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op170(TmdScratchModelBlock* ws, s32 flags, u32* stream);

// Overlay stream commands (src/gameplay/gameplay.c), selected by
// Tmd_ProcessStream.

/// Handler of a stream's pre-transformed textured-triangle records (`0x31`,
/// `0x39`, `0x3B`, `0x131`, `0x8039`): each element contributes one triangle to
/// the buffer half's first region, with the element's texture words written into
/// it.
///
/// The opcode says the triangle's vertices are already in screen space, so there
/// is no transform or cull for this command to do. It writes the polygon's
/// `u`/`v` fields, and adds the model's texture page and CLUT to the primitive's
/// own, which are stored relative to the model.
u32* gpStreamPrimGt3PreXform(TmdScratchModelBlock* ws, s32 flags, u32* stream);

/// Handler of a stream's pre-transformed textured-quad records (`0x71`, `0x79`,
/// `0x7B`, `0x171`, `0x8079`): each element contributes one quad to the buffer
/// half's first region, with the element's texture words written into it.
///
/// The opcode says the quad's vertices are already in screen space, so there is
/// no transform or cull for this command to do. It writes the polygon's `u`/`v`
/// fields, and adds the model's texture page and CLUT to the primitive's own,
/// which are stored relative to the model.
u32* gpStreamPrimGt4PreXform(TmdScratchModelBlock* ws, s32 flags, u32* stream);

/// Handler of a stream's pre-transformed flat-quad records (`0x45`): each
/// element contributes one untextured quad to the buffer half's first region,
/// with the element's colour word written into it.
///
/// Only the packet's fixed fields are written here — its length, its primitive
/// code and the element's colour. A pre-transformed quad's vertices come from the
/// stream's vertex commands, and the draw pass culls the quad and links it into
/// the order table, so neither is this command's work.
u32* gpStreamPrimF4PreXform(TmdScratchModelBlock* ws, s32 flags, u32* stream);

/// Handler of a stream's pre-transformed flat-triangle records (`0x5`): each
/// element contributes one untextured triangle to the buffer half's first
/// region, with the element's colour word written into it.
///
/// Only the packet's fixed fields are written here — its length, its primitive
/// code and the element's colour. A pre-transformed triangle's vertices come
/// from the stream's vertex commands, and the draw pass culls the triangle and
/// links it into the order table, so neither is this command's work.
u32* gpStreamPrimF3PreXform(TmdScratchModelBlock* ws, s32 flags, u32* stream);

/// Handler of a stream's textured-triangle records (`0x38`, `0x3A`, `0x8038`,
/// `0x10038`, `0x1003A`, `0x20038`): each element contributes one triangle to
/// the buffer half's second region, with the element's texture words written into
/// it.
///
/// The record is not pre-transformed, so its triangle is built in the region the
/// draw pass transforms; this command writes only the polygon's `u`/`v` fields,
/// and adds the model's texture page and CLUT to the primitive's own, which are
/// stored relative to the model.
u32* gpStreamPrimGt3(TmdScratchModelBlock* ws, s32 flags, u32* stream);

/// Handler of a stream's colour-carrying textured-triangle records (`0x30`):
/// each element contributes one triangle to the buffer half's second region,
/// with the element's texture words written into it.
///
/// The element carries a colour of its own — the colour the triangle's vertices
/// are lit from — so its texture words sit one word further into the element than
/// `gpStreamPrimGt3`'s, whose record is lit from a constant. The record is not
/// pre-transformed, so its triangle is built in the region the draw pass
/// transforms; this command writes only the polygon's `u`/`v` fields, and adds
/// the model's texture page and CLUT to the primitive's own, which are stored
/// relative to the model.
u32* gpStreamPrimGt3ElemColor(TmdScratchModelBlock* ws, s32 flags, u32* stream);
/// Handler of a stream's textured-triangle records whose elements carry one
/// colour word per vertex (`0x130`): each element contributes one triangle to
/// the buffer half's second region, with the element's texture words written
/// into it.
///
/// The record is not pre-transformed, so its triangle is built in the region the
/// draw pass transforms; this command writes only the polygon's `u`/`v` fields,
/// and adds the model's texture page and CLUT to the primitive's own, which are
/// stored relative to the model.
///
/// The colour words are the draw pass's: it lights each vertex from the colour
/// the element carries for that vertex, and this command only steps over them to
/// reach the texture words that follow.
u32* gpStreamPrimGt3VtxColor(TmdScratchModelBlock* ws, s32 flags, u32* stream);
/// Handler of a stream's one-normal textured-triangle records (`0x18`, `0x1A`):
/// each element contributes one triangle to the buffer half's second region, with
/// the element's texture words written into it.
///
/// The element names one normal for the whole triangle rather than one per corner
/// as the `gpStreamPrimGt3` family does, so its texture words begin a word
/// earlier. The record is not pre-transformed, so its triangle is built in the
/// region the draw pass transforms; this command writes only the polygon's `u`/`v`
/// fields, and adds the model's texture page and CLUT to the primitive's own,
/// which are stored relative to the model.
u32* gpStreamPrimGt3OneNormal(TmdScratchModelBlock* ws, s32 flags, u32* stream);

/// Handler of a stream's textured-quad records (`0x78`, `0x7A`, `0x8078`,
/// `0x10078`, `0x20078`): each element contributes one quad to the buffer half's
/// second region, with the element's texture words written into it.
///
/// The record is not pre-transformed, so its quad is built in the region the
/// draw pass transforms; this command writes only the polygon's `u`/`v` fields,
/// and adds the model's texture page and CLUT to the primitive's own, which are
/// stored relative to the model.
u32* gpStreamPrimGt4(TmdScratchModelBlock* ws, s32 flags, u32* stream);

/// Handler of a stream's textured-quad records whose elements carry a colour
/// (`0x70`): each element contributes one quad to the buffer half's second
/// Handler of a stream's textured-quad records that carry a colour per corner
/// (`0x170`): each element contributes one quad to the buffer half's second
/// region, with the element's texture words written into it.
/// Handler of a stream's textured-quad records whose element carries a single
/// normal (`0x58`, `0x5A`): each element contributes one quad to the buffer
/// half's second region, with the element's texture words written into it.
///
/// The record is not pre-transformed, so its quad is built in the region the
/// draw pass transforms; this command writes only the polygon's `u`/`v` fields,
/// and adds the model's texture page and CLUT to the primitive's own, which are
/// stored relative to the model.
///
/// This is `gpStreamPrimGt4`'s family with the opcode's colour bit clear, which
/// is the whole of the difference between them: the element carries an RGB word
/// between its refs and its texture words that the draw pass lights the quad
/// with, where the other family's records carry none and are lit against a fixed
/// colour. That is why the texture words are one word further into the element
/// here.
u32* gpStreamPrimGt4ElemColor(TmdScratchModelBlock* ws, s32 flags, u32* stream);
/// stored relative to the model. Ahead of its texture words the element names a
/// colour for each of the quad's corners — the material the record's transform
/// pass lights into the primitive's own corner colours — so the texture words
/// sit further into the record than `gpStreamPrimGt4`'s do.
u32* gpStreamPrimGt4CornerColors(TmdScratchModelBlock* ws, s32 flags, u32* stream);
/// stored relative to the model. The element carries one normal where the
/// per-corner-normal records carry one per corner, so the whole quad is lit from
/// that one normal and the element is a word shorter than the one
/// `gpStreamPrimGt4` reads.
u32* gpStreamPrimGt4SingleNormal(TmdScratchModelBlock* ws, s32 flags, u32* stream);
/// Handler of a stream's unlit textured-quad records (`0x156`): each element
/// contributes one quad to the buffer half's second region, with the element's
/// own four colours and its texture words written into it.
///
/// The record holds the quad's colours in place of the normals a lit quad's
/// record carries, so nothing lights this quad and the packet is complete once
/// this command has written it: its length and its primitive code, `0x3E` — the
/// semi-transparent gouraud textured quad — are set here rather than at draw
/// time from the model's flags. The element's texture words are written as they
/// are for a lit quad, with the model's texture page and CLUT added to the
/// primitive's own, which are stored relative to the model.
u32* gpStreamPrimGt4Unlit(TmdScratchModelBlock* ws, s32 flags, u32* stream);
/// Handler of a stream's flat textured-triangle records (`0x1C`, `0x1E`): each
/// element contributes one triangle to the buffer half's second region, with the
/// element's texture words written into it.
///
/// The record is not pre-transformed, so its triangle is built in the region the
/// draw pass transforms; this command writes only the polygon's `u`/`v` fields,
/// and adds the model's texture page and CLUT to the primitive's own, which are
/// stored relative to the model. The opcode selects the flat variant of the
/// textured triangle, which takes one colour for the whole primitive rather than
/// one per corner, where `gpStreamPrimGt3` builds the gouraud one.
u32* gpStreamPrimFt3(TmdScratchModelBlock* ws, s32 flags, u32* stream);
/// Handler of a stream's flat-textured-quad records (`0x5C`, `0x5E`): each
/// element contributes one quad to the buffer half's second region, with the
/// element's texture words written into it.
///
/// The opcode selects the flat form of the packet, which carries one colour for
/// the quad where the gouraud form carries one per vertex. The record is not
/// pre-transformed, so its quad is built in the region the draw pass transforms;
/// this command writes only the polygon's `u`/`v` fields, and adds the model's
/// texture page and CLUT to the primitive's own, which are stored relative to
/// the model.
u32* gpStreamPrimFt4(TmdScratchModelBlock* ws, s32 flags, u32* stream);
/// Handler of a stream's flat-quad records (`0x44`): each element contributes one
/// untextured quad to the buffer half's second region, with the element's colour
/// word written into it.
///
/// The record is not pre-transformed, so its quad belongs to the region the draw
/// pass transforms: only the packet's fixed fields are written here — its length,
/// its primitive code and the element's colour.
u32* gpStreamPrimF4(TmdScratchModelBlock* ws, s32 flags, u32* stream);
/// Handler of a stream's flat-triangle records (`0x4`): each element contributes
/// one untextured triangle to the buffer half's second region, with the
/// element's colour word written into it.
///
/// The record is not pre-transformed, so its triangle is built in the region the
/// draw pass transforms; this command writes only the packet's fixed fields — its
/// length, its primitive code and the element's colour.
u32* gpStreamPrimF3(TmdScratchModelBlock* ws, s32 flags, u32* stream);

#endif // TMD_H
