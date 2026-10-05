#ifndef MAIN_TMD_H
#define MAIN_TMD_H

#include "types.h"

#include "main/tmd_types.h"

/// Resident sentinel for the current attached-model list.
///
/// `next` points to the first `TmdObject.link`, and `prev` to the last. After
/// `Tmd_InitLists`, an empty list has `next == NULL` and `prev == &gTmdList`.
/// The first element points back to this sentinel; the last points forward to
/// `NULL`. Recover model bodies from element links with `PARENT_OF`; the
/// sentinel itself is not a `TmdObject`.
///
/// Successful attachment appends a body; creation alone does not link it.
/// Models excluded from active drawing and models without buffers remain linked,
/// including models awaiting delayed task release. Coordinate refresh visits every model;
/// drawing and buffer management apply their own filters. Unlink a body before
/// freeing it; the resident sentinel is never freed.
///
/// Saving and clearing the endpoints temporarily replaces the working list
/// without moving its bodies. Saved chains retain their back-links to this
/// sentinel and must stay linked and alive until the endpoints are restored.
extern TmdListNode gTmdList;

/// Cleared by Tmd_InitLists during system init.
extern s32 D_80071210;

void Tmd_InitLists(void);

/// Creates a model excluded from active drawing, with a mutable source-skeleton copy.
///
/// The object owns its per-part coordinate array and borrows `src`, which must
/// remain alive until the object is released. The skeleton is read only during
/// creation: each local matrix is copied and each parent index becomes a link,
/// with self-parented roots initially linked to the view coordinate.
///
/// Zero `bufferFlags` allocates and initializes both buffer halves. Nonzero
/// values defer allocation; bit 0 also disables missing-buffer recovery for
/// the object. These flags do not exempt an existing buffer from release.
/// Returns NULL if the object allocation fails. A buffer allocation failure
/// leaves a valid object with a NULL buffer for later allocation.
TmdObject* Tmd_Create(TmdSource* src, s32 bufferFlags);

/// Initializes persistent primitive data in the model's selected buffer half.
///
/// `model` must own a live auxiliary-heap buffer containing two source-sized
/// halves. The source's cached half capacity and region boundary must remain
/// unchanged: pre-transformed packets use the first region, directly transformed
/// packets the second. The writable, word-aligned stream must contain complete
/// three-word headers and count * stride payload words, with stride in u32 words,
/// a group marker after each group and a final `TMD_STREAM_END`. Capacities and
/// callback-specific element layouts are unchecked.
///
/// Selects `nextBufferHalf` (0 first, 1 second), then toggles it before invoking
/// callbacks. Call twice consecutively to refresh both halves after changing
/// texture offsets. Constructors copy UVs and apply signed encoded page/CLUT
/// displacements; some flat-colour commands also initialize colour and GPU code.
/// Drawing later supplies projection, lighting and OT links. The walk selects
/// constructors from opcodes, ignores cached draw slots and passes zero object
/// flags; it neither resolves handlers nor draws. Layered GT3/GT4 records select
/// offset layers in Dryfield by day's parking lot and toilet, independently of
/// draw resolution's toilet-only choice.
///
/// Unsupported construction opcodes skip payloads without advancing packet
/// cursors. This includes drawing's 0x22/0x62/0x122 variants; conversely the
/// 0x4000/0x4020/0x4120 and 0x4040/0x4060/0x4160 cases reserve untextured slots
/// without a resolved draw handler. Both walks must fit the source capacities;
/// construction's final cursors alone do not measure the draw extent.
/// Reserves one uninitialized `TmdStreamWorkspace` on the shared scratch stack
/// for the call and releases it afterwards. Callbacks retain no workspace;
/// keep the buffer alive and avoid changing packets still in use by the GPU.
void tmdBuildBufferHalf(TmdObject* model);

void Tmd_AllocMissingBuffers(void);

/// Allocates and initializes both primitive-buffer halves when the model has none.
///
/// Returns 1 only for a newly allocated buffer, and 0 when a buffer already
/// exists or allocation fails. Existing buffers are untouched. A failed request
/// leaves the buffer NULL and preserves the half selector; success resets it
/// to zero and builds both halves, leaving it zero. Draw flags are unchanged,
/// including `TMD_OBJECT_SKIP_AUTO_BUFFER` and active-pass exclusion.
/// The live source supplies each half's byte capacity (0..65535), which must
/// agree with the model's cache; the auxiliary heap must remain configured.
/// A zero-sized request fails. The model owns the resulting block until release
/// with `tmdFreePrimitiveBuffer`; see `tmdBuildBufferHalf` for stream requirements.
s32 tmdAllocPrimitiveBuffer(TmdObject* model);

/// Releases the model's auxiliary-heap primitive block and clears its buffer pointer.
///
/// A NULL buffer is a no-op. Otherwise it must be a live allocation from the
/// currently configured auxiliary heap, and GPU consumption must have finished.
/// Both halves are released together. The object, coordinates, source, draw
/// flags and half selector remain unchanged, permitting later reallocation.
void tmdFreePrimitiveBuffer(TmdObject* model);

void Tmd_DrawFlaggedNodes(TmdObject* node);

void Tmd_DrawActiveNodes(TmdObject* node);

/// Projects, lights and links a stream record's gouraud textured triangles.
///
/// This is the ordinary `0x38` draw handler. `objectFlags` is `TmdObject.flags`,
/// not the record opcode: `TMD_OBJECT_SEMI_TRANS` selects GPU code 0x36 instead
/// of 0x34, and `TMD_OBJECT_REVERSE_CULLING` keeps negative rather than positive
/// `NCLIP` areas for mirrored geometry. Zero area and a projection reporting
/// `TMD_GTE_ERROR_FLAG` are rejected. The `0x3A` entry,
/// `tmdDrawStreamGt3SemiTrans`, shares the walks but always selects code 0x36.
///
/// `elements` starts after the three-word record header. The caller supplies
/// `workspace->elemCount` (0..65535) and `workspace->elemStride` in u32 words,
/// at least three words per element. Those first three words pack six u16 byte
/// offsets in order: vertex 0, vertex 1, vertex 2, normal 0, normal 1, normal 2.
/// Each offset must name a complete eight-byte `SVECTOR` in the corresponding
/// borrowed array. Standard records also carry three texture words, initialized
/// into the packets by construction; drawing does not read those words.
///
/// The GTE must already hold the part transform, light/colour matrices and
/// background colour. Lighting uses three normals with a fixed RGB (128,128,128).
/// `workspace->primWrite` must address `elemCount` consecutive `POLY_GT3` slots
/// in the selected buffer half's second region, with UVs, page and CLUT already
/// built. Every element consumes one 40-byte slot, including rejected triangles.
/// Accepted packets receive screen coordinates, lit colours and a nine-word DMA
/// length, and are prepended to `workspace->ot` at bucket
/// `(((u32)OTZ << workspace->otDepthShift) & 0x3FFF) >> 4`. This is a wrapped
/// 0..1023 index relative to an OT base already displaced by the model's offset;
/// the resulting entry must fit the backing table. Normal draw supplies shifts
/// 0..3. Geometry, stream and packet capacities are not checked here.
///
/// Returns `elements + elemCount * elemStride` and advances `primWrite` past all
/// packet slots; workspace counts and saved GTE results are unchanged. No pointer
/// is retained. Keep the packet storage alive until the GPU finishes using it.
u32* tmdDrawStreamGt3(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects, lights and links a stream record's gouraud textured quads.
///
/// This is the ordinary `0x78` draw handler. `objectFlags` is `TmdObject.flags`:
/// `TMD_OBJECT_SEMI_TRANS` selects GPU code 0x3E instead of 0x3C, and
/// `TMD_OBJECT_REVERSE_CULLING` reverses facing for mirrored geometry.
/// A projection reporting `TMD_GTE_ERROR_FLAG` is rejected independently of
/// facing. With screen corners numbered as in the packet, ordinary facing keeps
/// `NCLIP(2,1,0) < 0` or `NCLIP(2,1,3) > 0`; reversed facing keeps the opposite
/// strict signs. The second triangle is tested only if the first is not kept;
/// two zero areas are rejected. The `0x7A` entry, `tmdDrawStreamGt4SemiTrans`,
/// shares these walks and always selects code 0x3E.
///
/// `elements` starts after the three-word record header. The caller supplies
/// `workspace->elemCount` (0..65535) and `workspace->elemStride` in u32 words,
/// at least four words per element. Those words pack eight u16 byte offsets:
/// vertices 0..3, then normals 0..3. Each must name a complete eight-byte
/// `SVECTOR` in its borrowed array. Standard records have three more texture
/// words, initialized by `tmdBuildStreamGt4`; drawing does not read them.
///
/// The GTE must already hold the part transform, light/colour matrices and
/// background colour. Lighting uses one normal per corner and fixed RGB
/// (128,128,128), with three normals processed together and the fourth separately.
/// Vertex 3 is projected first, then vertices 2, 1, 0. When vertex 3 matches the
/// preceding element's vertex 0, its screen XY and depth are reused. The ordinary
/// walk invalidates that reuse after a failed projection; the reversed walk
/// retains the comparison key, even on failure.
///
/// `workspace->primWrite` must address `elemCount` consecutive `POLY_GT4` slots
/// in the selected buffer half's second region, with UVs, page and CLUT already
/// built. Every element consumes one 52-byte slot, including rejected quads;
/// rejected slots may contain partial coordinate writes. Accepted packets receive
/// screen coordinates, lit colours and a twelve-word DMA length, and are prepended
/// to `workspace->ot` at bucket
/// `(((u32)OTZ << workspace->otDepthShift) & 0x3FFF) >> 4`, using `AVSZ4` depth.
/// This is a wrapped 0..1023 index relative to an OT base already displaced by the
/// model's offset; the resulting entry must fit the backing table. Normal draw
/// supplies shifts 0..3. Geometry, stream and packet capacities are not checked.
///
/// Returns `elements + elemCount * elemStride` and advances `primWrite` past all
/// packet slots; workspace counts and saved GTE results are unchanged. No pointer
/// is retained. Keep the packet storage alive until the GPU finishes using it.
u32* tmdDrawStreamGt4(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects vertices and scatters screen coordinates and lit colours for a `0xC8` record.
///
/// `elements` starts after the three-word record header. The caller supplies
/// `workspace->elemCount` (0..65535) and `workspace->elemStride` in u32 words,
/// at least two words per element. The first two words pack four u16 byte
/// offsets: vertex, normal, screen-XY destination, colour destination. Geometry
/// offsets name complete eight-byte `SVECTOR` entries in the borrowed arrays;
/// the vertex index (offset / 8) must also fit `workspace->szTable` (1024 entries
/// in normal drawing). Neither geometry array's full extent is supplied here.
///
/// Both destinations are relative to the current `workspace->preXformWrite`
/// and must name complete aligned four-byte words within the selected buffer
/// half's first region. XY is written before colour, even if the destinations
/// coincide. Every element writes both words, including rejected projections.
/// Colour uses fixed RGB (128,128,128); the whole lit RGB2 word is stored, with
/// a zero high byte. A destination at a packet's first colour therefore also
/// clears its GPU command byte, which a later draw handler must supply.
///
/// The GTE must already hold the part transform, projection settings, light and
/// colour matrices and background colour. A new vertex is projected with RTPS;
/// its SZ3 (0..65535) is cached at `szTable[vertexOffset / 8]`, with
/// `TMD_VERTEX_DEPTH_INVALID` ORed in when the fresh GTE FLAG has
/// `TMD_GTE_ERROR_FLAG`. Consecutive equal vertex offsets reuse SXY2 and that
/// cached depth, including a failed projection. Each element still lights its
/// own normal with NCCS. Later primitives test cached depths before linking.
///
/// Returns `elements + elemCount * elemStride`. The word at that returned
/// address must also be readable: a branch delay slot loads it even for zero
/// elements. The handler leaves all workspace fields and packet cursors
/// unchanged, allocates and links no primitive, and ignores `objectFlags`.
/// It retains no pointer; keep packet storage alive until GPU use finishes.
u32* tmdXformStreamVerts(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

#endif // MAIN_TMD_H
