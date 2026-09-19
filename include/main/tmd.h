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
    u16            flags;       // State bits (0x2 drawn semi-transparent, 0x4 buffer allocated by whoever created it, 0x8 drawn by the flagged pass, 0x10 drawn as a reflection, 0x80 hidden)
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

/// Builds a model's primitives into one half of its buffer: it walks the model's
/// packet stream and runs the handler the record's opcode selects, and the
/// handler lays the record's packets out and fills in what the record carries.
///
/// A packet is not all per-frame. Its screen coordinates, its lit colours and its
/// link into the ordering table are the draw pass's to write as it transforms the
/// record, and what is left — a textured primitive's texture coordinates, and the
/// model's texture page and CLUT row added to the primitive's own — is settled
/// here, once. So the walk runs wherever a model's buffer is filled, and again
/// wherever the page or CLUT the model draws with changes.
///
/// It works on the half the model is not drawing from and flips
/// `TmdObject.bufferIndex` onto it, so a caller that needs both halves to carry
/// the change calls it twice in a row. Its scratch frame is pushed on
/// `G_SCRATCH_HEAD` for the length of the walk, and the place the session is in
/// picks between the two handlers a record asking for a semi-transparent layer
/// has.
void tmdProcessStream(TmdObject* obj);

void Tmd_SetupDraw(TmdObject* obj);
void Tmd_AllocMissingBuffers(void);
s32  Tmd_AllocBuffers(TmdObject* obj);
void Tmd_FreeBuffers(TmdObject* obj);

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

// Early-image handlers (src/main/hasm/): the draw pass's handlers for the record
// opcodes they cover. Tmd_InitSourceStream resolves each record's opcode to its
// handler and the draw walk (Tmd_DispatchStream, reached from Tmd_SetupDraw)
// jalrs it — the draw path reaches a handler only by jalr; the pass that builds
// the primitives has a family of its own instead, the `gpStreamPrim*` handlers in
// the gameplay overlay. A handler runs on the scratch frame of whichever pass
// dispatched it, and the two frames lay their slots out alike, so the model-side
// type names every slot a handler touches and is the type they all take: the type
// Tmd_InitSourceStream resolves a record's handler into and `tmdProcessStream`
// passes the handlers it runs.

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

// Early-image draw handlers in Tmd_StreamHandlers_Ops.s, one per record family:
// `Tmd_InitSourceStream` patches each into a model's stream for the opcodes it
// answers to, and the draw walk jalrs it. Each is named for the opcode it serves
// or for the command it serves where that has been read.
/// Draw-pass handler of a stream's untextured triangle records that name a
/// normal per corner (`0x20`, `0x22`): each element is one `POLY_G3` in the
/// buffer half's second region, built whole here as the record is transformed.
///
/// The record is the `0x0` triangle's with one normal per corner in place of the
/// one it lights the face from: the element names the triangle's three vertices,
/// three normals and the colour word whose top byte is the packet's primitive
/// code. The vertices are projected, and the triangle is dropped where that
/// transform raises a GTE error or the triangle faces away; what survives is lit
/// from its three normals in one step, each corner taking its colour from its
/// own normal under the model's light, so the packet's three colour words carry
/// a result each where `tmdDrawStreamPrimG3` writes the same lit colour to all of
/// them. The packet is linked into the ordering table at the depth it came out
/// at, and a dropped triangle still consumes its packet's room, because the room
/// was reserved for every element by the process pass (`gpStreamPrimG3`), whose
/// cursor this one stays in step with.
///
/// The record's `0x22` form resolves to this same body, and the handler reads no
/// `flags`: a semi-transparent variant is not this one's to select, because the
/// packet's code byte is the element's own — carried in its colour word and
/// taken to the packet by the lighting step.
u32* tmdDrawStreamPrimG3CornerNormals(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op60(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_OpC0(TmdScratchModelBlock* ws, s32 flags, u32* stream);
/// The draw pass's handler for a stream's gouraud textured-triangle records that
/// ask for the semi-transparent primitive (`0x3A`): each element's three corners
/// are projected and lit from the three normals it names, and the packet the
/// build pass laid out for it is completed with those coordinates and colours and
/// linked into the ordering table where its depth puts it.
///
/// The element is the opaque `0x38` record's — three vertices and one normal per
/// corner — and the two records share one body, so the constant the triangle is
/// lit from is the whole of the difference between them: a fixed mid-grey whose
/// top byte is the packet's primitive code, `0x34` for the opaque triangle and
/// `0x36` here, the semi-transparency bit between the two. The opcode settles
/// that choice on its own, so this entry draws the semi-transparent form
/// unconditionally, where the `0x38` entry is the one that asks `flags` for it.
u32* tmdDrawStreamGt3SemiTrans(TmdScratchModelBlock* ws, s32 flags, u32* stream);

/// Draw-pass handler of a stream's gouraud-shaded textured-triangle records
/// (`0x38`, `0x3A`): each element contributes one triangle to the buffer half's
/// second region, with its corners projected and lit and the primitive linked
/// into the ordering table at the model's own offset.
///
/// The record's texture words belong to the pass that builds the primitive
/// (`gpStreamPrimGt3` writes them), so what is filled here is the triangle's
/// three corners and the colours they are lit from, which come from the three
/// normals the element names. An element whose projection the GTE rejects, or
/// whose triangle turns away, is stepped over rather than drawn, though its
/// packet slot is passed over either way, so the primitives stay aligned with
/// the elements that named them.
///
/// The record's `0x3A` form — `tmdDrawStreamGt3SemiTrans` — is the same body
/// reached with the semi-transparent shading constant, and this entry is the one
/// that picks the shading from `flags`. One further bit of `flags` picks between
/// the body's two copies of the walk, which keep opposite signs of the facing
/// result: a model drawn as a reflection asks for it, because a mirroring
/// transform reverses the model's faces.
u32* tmdDrawStreamGt3(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op7A(TmdScratchModelBlock* ws, s32 flags, u32* stream);

/// Draw handler of a stream's gouraud textured-quad records (`0x78`): each
/// element contributes one quad, projected and lit into the buffer slot the build
/// pass laid out for it, and linked into the ordering table at its own depth.
///
/// The element names a vertex and a normal per corner, so the quad is lit corner
/// by corner: three corners in one lighting step and the fourth in a step of its
/// own, each corner's colour being what its own normal gives under the model's
/// light. A corner the GTE reports off screen, or a quad the facing tests reject,
/// is not drawn — the packet is left out of the ordering table and the walk moves
/// on to the next element.
///
/// The packet's texture words, page and CLUT are the build pass's
/// (`gpStreamPrimGt4`); this pass writes the half a frame produces — the corner
/// coordinates, the corner colours, and the packet's length and primitive code.
/// That code is the semi-transparent one where the drawing object's flags ask for
/// it, which is what this handler reads `flags` for: the `0x7A` record's handler
/// shares this body and asks unconditionally.
u32* tmdDrawStreamGt4(TmdScratchModelBlock* ws, s32 flags, u32* stream);
/// Handler of a stream's transform pre-pass records (`0xC8`): each element
/// contributes one transformed vertex to the buffer half, and the record builds
/// no primitive of its own.
///
/// An element names a vertex, a normal, and the two places in the buffer half
/// its results go: the vertex is projected into screen coordinates, the normal
/// is lit into a colour, and both are written where the element names. The
/// elements carry no colour word, so the lighting uses a fixed colour instead.
///
/// The projection is reused where consecutive elements name the same vertex, and
/// each vertex's depth goes to the per-vertex cache: the commands that build a
/// primitive from screen coordinates already in the buffer average those depths
/// to order it, and a vertex whose transform reported an error is stored with
/// its sign bit set, which is how those commands know the primitive cannot be
/// drawn. The record has no variant for `flags` to select, so it goes unread.
u32* tmdXformStreamVerts(TmdScratchModelBlock* ws, s32 flags, u32* stream);
/// The draw pass's handler for a stream's pre-transformed textured-triangle
/// records that ask for the semi-transparent primitive (`0x3B`): each element's
/// triangle is linked into the ordering table under the blended primitive code,
/// at the depth its three corners average to.
///
/// The record is the opaque `0x39` one with the semi-transparency bit set, and
/// its packet was built by the other pass over the same stream, so nothing here
/// transforms or lights: the corners are in the packet already, put there by the
/// model's transform records (`0xC8`) together with the colours they are lit
/// from, and they go back into the GTE for the facing test alone. The element's
/// refs are read as the depth cache those records fill, where a vertex whose
/// projection failed is stored with its sign bit set, so a triangle that names
/// one of those, or that faces away, is not linked.
///
/// This entry is the same body as `tmdDrawStreamPrimGt3PreXform`, reached
/// directly: the record asks for the blended form by its opcode alone, where
/// that entry picks it from the drawing object's flags. Both read `flags` for
/// one thing besides — the bit a model drawn as a reflection sets, which sends
/// the facing test the other way round.
u32* tmdDrawStreamPrimGt3PreXformSemiTrans(TmdScratchModelBlock* ws, s32 flags, u32* stream);
/// Draw-pass handler of a stream's pre-transformed gouraud textured-triangle
/// records (`0x31`, `0x39`, `0x131`): each element contributes one triangle to
/// the buffer half's first region, where its corners are already in screen space,
/// and links it into the ordering table at the model's own offset.
///
/// The record is the pre-transformed form of the `0x38` triangles
/// (`tmdDrawStreamGt3`): the pass that projects the stream's vertices
/// (`tmdXformStreamVerts`) has already written each corner's screen coordinates
/// and lit colour into the packet this handler files, and its depth into the
/// per-vertex screen-Z table, so an element names its three corners in that table
/// rather than in the vertex array. What a frame adds is the triangle's filing:
/// the three cached depths are averaged for the ordering-table link, the facing
/// comes from the coordinates the packet already carries, and the packet's
/// length and primitive code are written. An element whose cached depth is marked
/// off screen, or whose triangle turns away, is stepped over rather than drawn,
/// though its packet slot is passed over either way, so the primitives stay
/// aligned with the elements that named them.
///
/// This entry is the whole family's and is the one that chooses between the two
/// primitive codes: it reaches `tmdDrawStreamPrimGt3PreXformSemiTrans` when the
/// drawing object's flags ask for the blended form.
u32* tmdDrawStreamPrimGt3PreXform(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op7B(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op79(TmdScratchModelBlock* ws, s32 flags, u32* stream);
/// Handler of a stream's untextured gouraud-triangle records (`0x0`): each
/// element is one `POLY_G3` in the buffer half's second region, built whole here
/// as the record is transformed.
///
/// The element names the triangle's three vertices, the one normal it is lit
/// from and the colour word whose top byte is the packet's primitive code. The
/// vertices are projected, and the triangle is dropped where that transform
/// raises a GTE error or the triangle faces away; what survives has the
/// element's colour — lit from that normal, which is why the three corners
/// share it — written to all of them, and the packet linked into the ordering
/// table at the depth it came out at. A dropped triangle still consumes its
/// packet's room, because the room was reserved for every element by the
/// process pass (`gpStreamPrimG3`), whose cursor this one stays in step with.
/// The record has no variant for `flags` to select, so it goes unread.
u32* tmdDrawStreamPrimG3(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op40(TmdScratchModelBlock* ws, s32 flags, u32* stream);
/// The draw pass's handler for a stream's one-normal textured-triangle records
/// that ask for the semi-transparent primitive (`0x1A`): each element's triangle
/// is taken to screen space and lit from the element's one normal, and its packet
/// is filled in with the resulting screen coordinates and colours and linked into
/// the ordering table, unless the transform clipped the triangle or the facing
/// test turned it away.
///
/// The element is the opaque `0x18` triangle's — one normal for the whole triangle
/// rather than one per corner, and the same refs and texture words — and the two
/// entries share one body, so the primitive code the packet is built under is the
/// whole of the difference between the two records: `0x34` for the opaque triangle
/// and `0x36` here, the semi-transparency bit being the difference. The element
/// names no colour, so the triangle is lit from a fixed mid-grey, and the same
/// constant carries both, the code in its top byte. The opcode alone selects the
/// variant, so `flags` goes unread.
u32* tmdDrawStreamPrimGt3OneNormalSemiTrans(TmdScratchModelBlock* ws, s32 flags, u32* stream);

/// Draw handler of a stream's one-normal textured-triangle records (`0x18`,
/// `0x1A`): each element's triangle is transformed and culled, its screen
/// coordinates and lit colour are written into the `POLY_GT3` the record's texture
/// words were laid in, and that packet is linked into the ordering table.
///
/// `tmdProcessStream` fills the polygon's texture words as it builds the record
/// into the buffer half, so what is left here is the half that changes per frame.
/// The element names one normal for the whole triangle rather than one per corner,
/// so a single lighting step colours all three corners; the depth the packet is
/// filed under is the three vertices' average, out of the same transform.
///
/// The `0x1A` entry shares this body and differs only in the primitive code the
/// polygon is drawn with, which this family carries in a fixed material colour
/// instead of reading one from the element: `0x34` opaque, `0x36` blended. Which
/// of the two is drawn is settled by the opcode alone, so `flags` selects nothing.
u32* tmdDrawStreamPrimGt3OneNormal(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op58(TmdScratchModelBlock* ws, s32 flags, u32* stream);
u32* Tmd_StreamHandler_Op5A(TmdScratchModelBlock* ws, s32 flags, u32* stream);
/// Handler of a stream's textured-triangle records that carry a colour per
/// corner (`0x130`): each element contributes one triangle, projected, shaded
/// and linked into the ordering table.
///
/// The element names a vertex, a normal and a colour for each of the triangle's
/// three corners, so every corner is shaded from a pair of its own where the
/// record families that carry one colour for the whole element shade all three
/// from that one colour. An element whose vertices fall behind the camera, or
/// whose triangle faces away, contributes nothing. The model's flags choose
/// between the opaque and the semi-transparent form of the primitive.
///
/// The element's texture words are not this handler's: the other pass over the
/// same stream copies them into the model's buffer when the model is created,
/// and this one leaves them where they lie.
u32* tmdDrawStreamPrimGt3CornerColors(TmdScratchModelBlock* ws, s32 flags, u32* stream);

/// Handler of a stream's textured-quad records that name one colour per corner
/// (`0x170`): each element contributes one quad to the buffer half's second
/// region, projected, lit per corner and linked into the ordering table.
///
/// The element names a colour and a normal for each corner, so the four corners
/// are lit independently and may differ. A quad whose projection overflows, or
/// whose corners wind the wrong way, is dropped rather than drawn, and one that
/// survives is left translucent where the object's flags call for it.
///
/// What it writes is what the transform decides: the projected corners, the
/// corner colours, the primitive code and the ordering-table link. The primitive
/// itself, texture words included, was written when the stream was compiled into
/// the buffer, so this command completes it in place.
u32* tmdDrawStreamPrimGt4CornerColors(TmdScratchModelBlock* ws, s32 flags, u32* stream);

// Overlay stream commands (src/gameplay/gameplay.c), selected by
// tmdProcessStream.

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

/// Handler of a stream's textured-triangle records that carry a colour per corner
/// (`0x130`): each element contributes one triangle to the buffer half's second
/// region, with the element's texture words written into it.
///
/// The record is not pre-transformed, so its triangle is built in the region the
/// draw pass transforms; this command writes only the polygon's `u`/`v` fields,
/// and adds the model's texture page and CLUT to the primitive's own, which are
/// stored relative to the model. Ahead of its texture words the element names a
/// colour for each of the triangle's corners — the material the record's
/// transform pass lights into the primitive's own corner colours — so the texture
/// words sit further into the record than `gpStreamPrimGt3ElemColor`'s do.
///
/// The corner colours are the transform pass's: it lights each corner from the
/// colour the element carries for it, and this command only steps over them to
/// reach the texture words that follow.
u32* gpStreamPrimGt3CornerColors(TmdScratchModelBlock* ws, s32 flags, u32* stream);

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
/// region, with the element's texture words written into it.
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

/// Handler of a stream's textured-quad records that carry a colour per corner
/// (`0x170`): each element contributes one quad to the buffer half's second
/// region, with the element's texture words written into it.
///
/// The record is not pre-transformed, so its quad is built in the region the
/// draw pass transforms; this command writes only the polygon's `u`/`v` fields,
/// and adds the model's texture page and CLUT to the primitive's own, which are
/// stored relative to the model. Ahead of its texture words the element names a
/// colour for each of the quad's corners — the material the record's transform
/// pass lights into the primitive's own corner colours — so the texture words
/// sit further into the record than `gpStreamPrimGt4`'s do.
u32* gpStreamPrimGt4CornerColors(TmdScratchModelBlock* ws, s32 flags, u32* stream);

/// Handler of a stream's one-normal textured-quad records (`0x58`, `0x5A`): each
/// element contributes one quad to the buffer half's second region, with the
/// element's texture words written into it.
///
/// The record is not pre-transformed, so its quad is built in the region the
/// draw pass transforms; this command writes only the polygon's `u`/`v` fields,
/// and adds the model's texture page and CLUT to the primitive's own, which are
/// stored relative to the model. The element carries one normal where the
/// per-corner-normal records carry one per corner, so the whole quad is lit from
/// that one normal and the element is a word shorter than the one
/// `gpStreamPrimGt4` reads.
u32* gpStreamPrimGt4OneNormal(TmdScratchModelBlock* ws, s32 flags, u32* stream);

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

/// Handler of a stream's untextured gouraud-quad records (`0x40`, `0x60`, `0x160`,
/// `0x4040`, `0x4060`, `0x4160`): each element is one `POLY_G4` in the buffer half's
/// second region.
///
/// Nothing of that quad is this command's to write: it is a gouraud primitive, so its
/// colours are lit per corner rather than taken from the element as a flat family's
/// are, and it is untextured, so there are no texture words either. The command's work
/// is to move both cursors on, which it must still do — the half is written by both
/// passes, so a record one of them skipped would put every primitive after it at the
/// wrong address in the other.
u32* gpStreamPrimG4(TmdScratchModelBlock* ws, s32 flags, u32* stream);

/// Handler of a stream's layered textured-triangle records (`0x4038`) whose
/// semi-transparent layer is textured from the object: each element contributes two
/// triangles to the buffer half's second region, with the element's texture words
/// written into both.
///
/// The record is not pre-transformed, so its triangles are built in the region the
/// draw pass transforms. `0x4000` asks for two primitives per element — the base the
/// model is drawn from, and the semi-transparent layer drawn over it — and that layer
/// is normally the transform pass's to texture, from a page of its own. The walk
/// takes this handler where the layer is textured from the record instead: the same
/// `u`/`v` fields go into both primitives, the base takes the model's texture page
/// and CLUT, and the layer takes those plus the object's extra page and CLUT offsets,
/// along with the semi-transparency rate it blends at.
u32* gpStreamPrimGt3OffsetLayer(TmdScratchModelBlock* ws, s32 flags, u32* stream);

/// Handler of a stream's layered textured-triangle records (`0x4038`) whose layer
/// is the transform pass's to texture: each element contributes two triangles to
/// the buffer half's second region, with the element's texture words written into
/// the base.
///
/// The record is not pre-transformed, so its triangles are built in the region the
/// draw pass transforms. `0x4000` asks for two primitives per element — the base the
/// model is drawn from, and the semi-transparent layer drawn over it — and this
/// command writes the base alone: it takes the element's texture words and adds the
/// model's own texture page and CLUT, which are stored relative to the model. The
/// layer's texture words are the transform pass's, worked out from the triangle it
/// draws. `gpStreamPrimGt3OffsetLayer` is the walk's other choice, taken where the
/// layer is textured from the element as well.
u32* gpStreamPrimGt3Base(TmdScratchModelBlock* ws, s32 flags, u32* stream);

/// Handler of a stream's layered textured-quad records (`0x4078`) whose
/// semi-transparent layer takes its texture page from the object: each element
/// contributes two quads to the buffer half's second region, with the element's
/// texture words written into both.
///
/// The record is not pre-transformed, so its quads are built in the region the
/// draw pass transforms. `0x4000` asks for two primitives per element — the base
/// the model is drawn from, and the semi-transparent layer drawn over it — and
/// that layer is normally the draw pass's to texture, from a page of its own. The
/// walk takes this handler where both quads are written from the record instead:
/// the same `u`/`v` fields go into each, the base takes the model's texture page
/// and CLUT, and the layer takes the object's extra page and CLUT offsets, along
/// with the semi-transparency rate it blends at.
u32* gpStreamPrimGt4OffsetLayer(TmdScratchModelBlock* ws, s32 flags, u32* stream);

/// Handler of a stream's layered textured-quad records (`0x4078`): each element
/// contributes the quad the model is drawn from to the buffer half's second region,
/// with the model's texture page and CLUT added to the element's own texture words.
///
/// The record is not pre-transformed, so its quad is built in the region the draw
/// pass transforms; this command writes only the polygon's `u`/`v` fields. `0x4000`
/// asks for two primitives per element — the quad written here, and the
/// semi-transparent layer drawn over it — so an element advances the write cursor
/// past both, and the layer is left for the transform pass, which draws it from a
/// page of its own. Where the layer's `u`/`v` are to come from the record as well,
/// the walk takes a sibling handler instead.
u32* gpStreamPrimGt4Base(TmdScratchModelBlock* ws, s32 flags, u32* stream);

/// Handler of a stream's layered textured-triangle records (`0x4039`): each element
/// contributes two triangles to the buffer half's first region — the base the model
/// is drawn from, and the semi-transparent layer drawn over it.
///
/// The record is pre-transformed, so its triangles are already in screen space and
/// there is no transform or cull for this command to do. `0x4000` asks for two
/// primitives per element: the element's texture words go into the base, with the
/// model's texture page and CLUT added to the primitive's own, and the layer's page
/// and CLUT are written here as fixed values rather than from the object's extra
/// page and CLUT offsets.
u32* gpStreamPrimGt3PreXformFixedLayer(TmdScratchModelBlock* ws, s32 flags, u32* stream);

/// Handler of a stream's layered pre-transformed textured-quad records (`0x4079`):
/// each element contributes two quads to the buffer half's first region — the base
/// the model is drawn from, and the semi-transparent layer drawn over it.
///
/// The `0x4000` bit is the whole of the difference between these records and
/// `gpStreamPrimGt4PreXform`'s, which contribute the base quad alone. The base is
/// textured as it is there, from the element's texture words with the model's
/// texture page and CLUT added. The layer is the draw pass's to texture: the
/// record's vertex commands write its coordinates there and give it texture
/// coordinates derived from the vertices' normals, and the page it draws from is
/// chosen from what those commands leave behind. Of the layer's own words this
/// command sets only its page and CLUT, at a fixed pair (`0x3F`, `0x3C10`).
///
/// The opcode says the quad's vertices are already in screen space, so there is no
/// transform or cull for this command to do. Where the layer is textured from the
/// object's page and CLUT offsets instead, the walk takes the record's other
/// handler.
u32* gpStreamPrimGt4PreXformLayer(TmdScratchModelBlock* ws, s32 flags, u32* stream);

/// Handler of a stream's pre-transformed layered textured-triangle records
/// (`0x4039`) whose semi-transparent layer is textured from the object: each
/// element contributes two triangles to the buffer half's first region, with the
/// element's texture words written into both.
///
/// The record is pre-transformed, so its triangles are already in screen space and
/// no transform pass builds either of them. `0x4000` asks for two primitives per
/// element — the base the model is drawn from, and the semi-transparent layer drawn
/// over it — and this command fills both: the base takes the model's texture page
/// and CLUT, and the layer takes the element's own plus the object's extra page and
/// CLUT offsets, along with the semi-transparency rate it blends at.
///
/// The walk picks between this handler and one that textures the layer from a fixed
/// page of its own: it takes this one in the areas whose layered draws are textured
/// from the object.
u32* gpStreamPrimGt3PreXformOffsetLayer(TmdScratchModelBlock* ws, s32 flags, u32* stream);

/// Handler of a stream's layered pre-transformed textured-quad records (`0x4079`)
/// whose semi-transparent layer is textured from the object: each element
/// contributes two quads to the buffer half's first region, with the element's
/// texture words written into both.
///
/// The opcode says the quads' vertices are already in screen space, so there is no
/// transform for this command to do. `0x4000` asks for two primitives per element —
/// the base the model is drawn from, and the semi-transparent layer drawn over it —
/// and that layer is normally the transform pass's to texture, from a page of its
/// own. The walk takes this handler where the layer is textured from the record
/// instead: the same `u`/`v` fields go into both primitives, the base takes the
/// model's texture page and CLUT added to the record's own, and the layer takes the
/// object's extra page and CLUT offsets in their place, along with the
/// semi-transparency rate it blends at.
u32* gpStreamPrimGt4PreXformOffsetLayer(TmdScratchModelBlock* ws, s32 flags, u32* stream);

/// Handler of a stream's untextured gouraud-triangle records (`0x0`, `0x20`,
/// `0x120`, `0x4000`, `0x4020`, `0x4120`): each element reserves one `POLY_G3`'s
/// worth of the buffer half's second region, and nothing is written into it.
///
/// The packet has no field this pass could fill. It carries no texture, and its
/// colours are lit from the element's material rather than copied from it, so the
/// draw pass writes the packet whole — colours, screen coordinates and link into
/// the ordering table — as it transforms the record. What is left here is the
/// packet's room: stepping the primitive cursor past it is what keeps the records
/// that follow building where the draw pass will look for them, and the `0x0`
/// records are built there by `tmdDrawStreamPrimG3`, the `0x20` ones — the same
/// packet, lit from a normal per corner — by `tmdDrawStreamPrimG3CornerNormals`.
u32* gpStreamPrimG3(TmdScratchModelBlock* ws, s32 flags, u32* stream);

#endif // TMD_H
