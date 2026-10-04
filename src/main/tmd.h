#ifndef MAIN_PRIVATE_TMD_H
#define MAIN_PRIVATE_TMD_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

struct Task;

/// Early-image handwritten GTE matrix load (src/main/hasm/Tmd_SetupGteMatrices.s).
void Tmd_SetupGteMatrices(TmdStreamWorkspace* ws, u32 flags, void* stream, TmdObject* node);

/// Walk stream records and jalr each draw handler until `TMD_STREAM_GROUP_END`.
///
/// The caller stops when the word at entry is `TMD_STREAM_END`. This walk does
/// not test that marker, so a terminator in the opcode position is not a stop.
u32* Tmd_DispatchStream(TmdStreamWorkspace* ws, s32 flags, u32* stream);

// The per-frame callback of the task that holds the models' buffers, and the
// states it runs in sequence. The hide and buffer-release states walk the
// current `gTmdList`; drawing and other buffer passes use the same sentinel
// independently of this task.
void Tmd_DispatchTask(struct Task* task);

/// Gives a buffer back to every attached model that has none, then kills the task.
void Tmd_AllocNodeBuffers(struct Task* task);

/// Fallback handler for a stream record the current pass does not consume.
///
/// `stream` addresses the first element word. The caller has already taken the
/// three-word header and stored `ws->elemStride` and `ws->elemCount`: the
/// stride is the element's length in `u32` words, and the count is how many
/// elements follow. The handler returns that cursor advanced by the stride
/// times the count. That address is the word after the payload, so a following
/// group marker or `TMD_STREAM_END` stays for the caller. Construction skips
/// group markers and stops on the terminator without passing it. Drawing
/// returns at a group marker; its outer loop stops on the terminator instead.
/// The handler reads neither the payload nor `flags`, and it writes nothing.
///
/// Packet construction calls it when the opcode has no construction handler.
/// Draw-handler resolution stores it when the opcode has no draw handler, and
/// the draw walk calls the stored slot. Both walks take their next opcode from
/// the returned cursor, so a record with nothing to build or draw is still
/// stepped over. On the draw walk `flags` carries the drawing object's flags;
/// during construction it is zero. This handler selects nothing from them.
u32* tmdSkipStreamRecord(TmdStreamWorkspace* ws, s32 flags, u32* stream);

u32* Tmd_StreamHandler_Prim32(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Culls and links pre-transformed untextured Gouraud triangles as opaque `POLY_G3` packets.
///
/// Draw resolution selects this entry for `0x21` and `0x121`. `elements`
/// starts after the three-word record header; `workspace->elemCount` supplies
/// 0..65535 elements and `elemStride` their stride in u32 words, at least two
/// for a nonempty record. The first two words pack three unsigned u16 depth-cache
/// byte offsets: corners 0/1 in word 0's low/high halves, corner 2 in word 1's
/// low half. Word 1's high half and any further words are ignored. Each offset
/// must be a multiple of four in 0..4092, naming a complete entry in the draw
/// pass's borrowed 1024-entry `workspace->szTable`. Those entries must already
/// hold the corresponding vertices' screen Z (0..65535), with
/// `TMD_VERTEX_DEPTH_INVALID` marking a failed projection.
///
/// The word-aligned `workspace->preXformWrite` must have one complete 28-byte
/// `POLY_G3` slot per element in the selected buffer half's first region.
/// Earlier vertex-projection records must have filled each slot's screen XY
/// and corner RGB. This handler neither projects nor lights: it keeps strictly
/// positive `NCLIP(0,1,2)` winding and rejects any negative cached depth.
/// Every element consumes its packet slot, including rejected triangles;
/// rejection changes neither the packet nor the OT. Accepted packets retain
/// their coordinates and RGB, receive opaque GPU code 0x30 and a six-word
/// DMA length, and prepend to `workspace->ot` using `AVSZ3` depth.
///
/// The GTE's three-depth averaging scale must already be set. The OT bucket is
/// `(((u32)OTZ << (workspace->otDepthShift & 31)) & 0x3FFF) >> 4`, wrapping
/// into 0..1023; normal drawing supplies shifts 0..3. The OT base already
/// includes the object's signed entry displacement, and every resulting bucket
/// must fit the selected table. DMA links retain 24 address bits. Stream, cache,
/// packet and OT capacities are unchecked; all storage is borrowed, and packets
/// and OT storage must remain valid until GPU consumption finishes.
///
/// Returns `elements + elemCount * elemStride`, leaving the next record or
/// marker unconsumed, and advances only `workspace->preXformWrite` among the
/// workspace fields. Counts, saved GTE results and the second-region cursor
/// remain unchanged. An empty record reads no payload and advances neither
/// cursor. `objectFlags` is ignored, including blend and reverse-culling bits;
/// `Tmd_StreamHandler_Prim32` is the separate semi-transparent entry.
u32* tmdDrawStreamPrimG3PreXform(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

u32* Tmd_StreamHandler_Prim3A(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Culls and links pre-transformed untextured Gouraud quads as opaque `POLY_G4` packets.
///
/// Draw resolution selects this entry for `0x61` and `0x161`. `elements`
/// starts after the three-word record header; `workspace->elemCount` supplies
/// 0..65535 elements and `elemStride` their stride in u32 words, at least two
/// for a nonempty record. The first two words pack four unsigned u16 depth-cache
/// byte offsets: corners 0/1 in word 0's low/high halves, corners 2/3 in word 1's
/// low/high halves. Further words are ignored. Each offset must be a multiple
/// of four in 0..4092, naming a complete entry in the draw pass's borrowed
/// 1024-entry `workspace->szTable`. Those entries must already hold the vertices'
/// screen Z (0..65535), with `TMD_VERTEX_DEPTH_INVALID` on failed projections.
///
/// The word-aligned `workspace->preXformWrite` must have one complete 36-byte
/// `POLY_G4` slot per element in the selected buffer half's first region.
/// Earlier projection records must have filled each slot's screen XY and corner
/// RGB. This handler neither projects nor lights: it keeps `NCLIP(0,1,2) > 0`
/// or, if that fails, `NCLIP(1,2,3) < 0`, and rejects any negative cached depth.
/// Every element consumes its packet slot, including rejected quads; rejection
/// changes neither the packet nor the OT. Accepted packets retain XY and RGB,
/// receive opaque GPU code 0x38 and an eight-word DMA length, and prepend to
/// `workspace->ot` using `AVSZ4` depth.
///
/// The GTE's four-depth averaging scale must already be set. The OT bucket is
/// `(((u32)OTZ << (workspace->otDepthShift & 31)) & 0x3FFF) >> 4`, wrapping
/// into 0..1023; normal drawing supplies shifts 0..3. The OT base already
/// includes the object's signed entry displacement, and every resulting bucket
/// must fit the selected table. DMA links retain 24 address bits. Stream, cache,
/// packet and OT capacities are unchecked; all storage is borrowed, and packets
/// and OT storage must remain valid until GPU consumption finishes.
///
/// Returns `elements + elemCount * elemStride`, leaving the next record or
/// marker unconsumed, and updates only `workspace->preXformWrite` among workspace
/// fields. Counts, saved GTE results and the second-region cursor are unchanged.
/// An empty record reads no payload and advances neither cursor. `objectFlags`
/// is ignored, including blend and reverse-culling bits;
/// `Tmd_StreamHandler_Prim3A` is the separate semi-transparent entry.
u32* tmdDrawStreamPrimG4PreXform(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

// Draw-pass handlers, one per record family: the early image's in
// Tmd_StreamHandlers_Ops.s, the gameplay overlay's in that overlay's own units.
// `Tmd_InitSourceStream` patches each into a model's stream for the opcodes it
// answers to, and the draw walk jalrs it. Each is named for the opcode it serves
// or for the command it serves where that has been read.
//
// Gameplay stream handlers are declared in gameplay/model_lighting.h and
// gameplay/model_objects.h, beside their owning module APIs.
/// Projects and draws untextured Gouraud triangles, lighting from one normal per corner.
///
/// Draw resolution selects this handler for `0x20` and `0x22`. `elements`
/// starts after the three-word record header. Each element's first three words
/// contain six u16 byte offsets in order: vertex 0, vertex 1, vertex 2, normal 0,
/// normal 1, normal 2. Word 3 supplies RGB and the GPU command byte, which NCCT
/// preserves in all three lit corner colours. Each offset must address a complete,
/// word-aligned eight-byte vector in the borrowed `workspace->verts` or
/// `workspace->normals` array. Their complete extents are not supplied here.
/// `elemCount` and `elemStride` are decoded unsigned halfwords (0..65535);
/// stride counts u32 words and must be at least four for a nonempty record.
/// The stream must contain count * stride payload words and remain valid for the call.
///
/// The word-aligned `workspace->primWrite` cursor must have count complete
/// `POLY_G3` slots in the selected buffer half's second region. Construction
/// reserves these slots for `0x20` through `modelLightingReserveStreamPrimG3`,
/// but skips `0x22`; drawing still consumes a slot for every element of either
/// opcode. Buffer capacity and later packet cursors must accommodate that walk.
/// Projection errors (GTE FLAG bit 31) and nonpositive screen-space winding
/// reject a triangle; even rejected elements advance the packet cursor by 28 bytes.
/// Accepted packets receive projected coordinates and three independently lit colours.
///
/// The caller supplies the part's GTE transform, light/colour matrices,
/// background colour and depth-average scale. Accepted packets prepend to
/// `workspace->ot` at entry
/// `((OTZ << (workspace->otDepthShift & 31)) & 0x3FFF) >> 4`, with the object's
/// signed OT displacement already in that base. The resulting entry (0..1023)
/// must fit the selected table after displacement. DMA links retain 24 address
/// bits and the tag records six payload words. Packet and OT storage are borrowed
/// and must remain GPU-visible until consumption completes. Capacities are not checked.
///
/// Returns the word after the payload and updates only `workspace->primWrite`;
/// workspace counts and saved GTE results are unchanged. An empty record reads
/// no payload and advances neither cursor. `objectFlags` is ignored, including
/// blend and reverse-culling bits: the command comes from the element, and
/// positive winding is always required.
u32* tmdDrawStreamPrimG3CornerNormals(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Transforms and lights untextured quads using one normal per corner and one material colour.
///
/// Stream resolution selects this handler for `0x60` and `0x62`. `elements`
/// starts after the three-word record header. Words 0 and 1 pack four unsigned
/// u16 vertex byte offsets, in corner order; words 2 and 3 pack the four normal
/// byte offsets in the same order. Word 4 supplies RGB and the GPU command byte,
/// which NCCT/NCCS preserve in all four independently lit corner colours.
/// Each offset must address a complete, word-aligned eight-byte vector in the
/// borrowed `workspace->verts` or `workspace->normals` array. Their complete
/// extents are not supplied here. `elemCount` and `elemStride` are decoded
/// unsigned halfwords (0..65535); stride counts u32 words and must be at least
/// five for a nonempty record. The stream must contain count * stride payload
/// words and remain valid for the call.
///
/// The caller supplies the part's GTE transform, lighting and four-depth
/// averaging scale. Vertex 3 is projected first, followed by vertices 2, 1, 0.
/// When vertex 3 equals the preceding element's vertex 0 offset, its screen XY
/// and depth are reused. A projection with GTE FLAG bit 31 set rejects the
/// quad and invalidates reuse; a facing rejection retains it. Facing keeps
/// `NCLIP(2,1,0) < 0` or, if that fails, `NCLIP(2,1,3) > 0`.
///
/// The word-aligned `workspace->primWrite` cursor must have count complete
/// 36-byte `POLY_G4` slots in the selected buffer half's second region.
/// Construction reserves these slots for `0x60` through
/// `modelLightingReserveStreamPrimG4`, but skips `0x62`; drawing still consumes
/// a slot for every element of either opcode. Buffer capacity and later packet
/// cursors must accommodate that walk. Rejected quads may leave partial
/// coordinate writes. Accepted packets receive all coordinates, four lit
/// RGB/command words and an eight-word DMA length. They prepend to
/// `workspace->ot` at bucket
/// `(((u32)OTZ << (workspace->otDepthShift & 31)) & 0x3FFF) >> 4`, using AVSZ4.
/// Normal drawing supplies shifts 0..3. The OT base already includes the
/// object's signed entry displacement; every resulting bucket (0..1023) must
/// fit the selected table. DMA links retain 24 address bits. Capacities are
/// unchecked; packet and OT storage must remain valid until GPU use finishes.
///
/// Returns `elements + elemCount * elemStride`, leaving the next record or
/// marker unconsumed, and updates only `workspace->primWrite` among workspace
/// fields. Counts, saved GTE results and the first-region cursor are unchanged.
/// An empty record reads no payload and advances neither cursor. `objectFlags`
/// is ignored, including blend and reverse-culling bits; neither opcode forces
/// a GPU command byte, and the facing signs above always apply.
u32* tmdDrawStreamPrimG4CornerNormals(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects vertices and scatters screen coordinates and per-element lit colours for a `0xC0` record.
///
/// `elements` starts after the three-word record header. The caller supplies
/// `workspace->elemCount` (0..65535) and `workspace->elemStride` in u32 words,
/// at least three words per element. Word 0 packs two unsigned u16 byte offsets:
/// vertex, then normal. Word 1 is the material colour `NCCS` multiplies by, with
/// R, G and B in its low three bytes; its high byte is retained in the stored
/// colour word. Word 2 packs the destination byte offsets: screen XY, then lit
/// colour. Geometry offsets name complete eight-byte `SVECTOR` entries in the
/// borrowed arrays; the vertex index (offset / 8) must also fit
/// `workspace->szTable` (1024 entries in normal drawing). Neither geometry
/// array's full extent is supplied here.
///
/// Both destinations are relative to the current `workspace->preXformWrite`
/// and must name complete aligned four-byte words within the selected buffer
/// half's first region. XY is written before colour, even if the destinations
/// coincide. Every element writes both words, including rejected projections.
///
/// The GTE must already hold the part transform, projection settings, light and
/// colour matrices and background colour. Each element loads word 1 over the
/// RGB those left behind. A new vertex is projected with RTPS; its SZ3
/// (0..65535) is cached at `szTable[vertexOffset / 8]`, with
/// `TMD_VERTEX_DEPTH_INVALID` ORed in when the fresh GTE FLAG has
/// `TMD_GTE_ERROR_FLAG`. Consecutive equal vertex offsets reuse SXY2 and that
/// cached depth, including a failed projection, and still light their own
/// normal from their own material word. Later primitives test cached depths
/// before linking.
///
/// Returns `elements + elemCount * elemStride`. The word at that returned
/// address must also be readable: a branch delay slot loads it even for zero
/// elements. The handler leaves all workspace fields and packet cursors
/// unchanged, allocates and links no primitive, and ignores `objectFlags`.
/// It retains no pointer; keep packet storage alive until GPU use finishes.
u32* tmdXformStreamVertsElemColor(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects, lights and links a record's semi-transparent gouraud textured triangles.
///
/// The `0x3A` draw entry always selects GPU code 0x36, regardless of
/// `objectFlags & TMD_OBJECT_SEMI_TRANS`. It shares `tmdDrawStreamGt3`'s
/// geometry, lighting and packet walks: `objectFlags` still carries
/// `TmdObject.flags`, and `TMD_OBJECT_REVERSE_CULLING` keeps negative rather
/// than positive `NCLIP` areas. Zero area and projections reporting
/// `TMD_GTE_ERROR_FLAG` are rejected. Lighting uses one normal per corner
/// with neutral RGB (128,128,128); texture-page blend mode is left unchanged.
///
/// `elements` starts after the three-word header. `workspace->elemCount`
/// supplies 0..65535 elements and `elemStride` their stride in u32 words.
/// Drawing reads the first three words as six u16 byte offsets: vertex 0,
/// vertex 1, vertex 2, normal 0, normal 1, normal 2. Each must name a complete
/// eight-byte `SVECTOR` in its borrowed array. Standard records have at least
/// six words; `tmdBuildStreamGt3` has already copied their texture words into
/// one word-aligned `POLY_GT3` slot per element at `workspace->primWrite`.
/// These slots occupy the selected buffer half's second region. The GTE part
/// transform, light/colour matrices and background colour must already be set.
///
/// Every element consumes its 40-byte slot, including rejected triangles.
/// Accepted packets receive screen coordinates, lit colours and a nine-word
/// DMA length, then prepend to `workspace->ot` at bucket
/// `(((u32)OTZ << workspace->otDepthShift) & 0x3FFF) >> 4`. The displaced OT base and wrapped
/// 0..1023 bucket must fit the backing table; normal draw supplies shifts 0..3.
/// Geometry, stream and packet capacities are not checked here.
///
/// Returns `elements + elemCount * elemStride` and advances `primWrite` past
/// all slots. Workspace counts and saved GTE results are unchanged. All storage
/// is borrowed; no pointer is retained. Keep packets alive until the GPU finishes.
u32* tmdDrawStreamGt3SemiTrans(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects, lights and links a record's semi-transparent gouraud textured quads.
///
/// The `0x7A` draw entry always selects GPU code 0x3E, regardless of
/// `objectFlags & TMD_OBJECT_SEMI_TRANS`. It shares `tmdDrawStreamGt4`'s
/// geometry, lighting and packet walks. `objectFlags` carries `TmdObject.flags`:
/// `TMD_OBJECT_REVERSE_CULLING` still reverses the facing rule. Ordinary facing
/// keeps `NCLIP(2,1,0) < 0` or `NCLIP(2,1,3) > 0`; reversed facing keeps the
/// opposite strict signs. The second test runs only if the first fails to keep
/// the quad. Two zero areas and projections reporting `TMD_GTE_ERROR_FLAG`
/// are rejected. Lighting uses one normal per corner with neutral RGB
/// (128,128,128); the texture-page blend mode is left unchanged.
///
/// `elements` starts after the three-word header. `workspace->elemCount`
/// supplies 0..65535 elements and `elemStride` their stride in u32 words,
/// at least four. Those words pack eight u16 byte offsets: vertices 0..3,
/// then normals 0..3. Each must name a complete, word-aligned eight-byte
/// `SVECTOR` in its borrowed array. Standard records have seven words;
/// `tmdBuildStreamGt4` has already copied their texture words into one
/// word-aligned `POLY_GT4` slot per element at `workspace->primWrite` in the
/// selected buffer half's second region. The GTE part transform, light/colour
/// matrices and background colour must already be set.
///
/// Every element consumes its 52-byte slot, including rejected quads, whose
/// coordinates may be partially written. Accepted packets receive screen
/// coordinates, lit colours and a twelve-word DMA length, then prepend to
/// `workspace->ot` at bucket
/// `(((u32)OTZ << workspace->otDepthShift) & 0x3FFF) >> 4`, using `AVSZ4` depth.
/// The displaced OT base and wrapped 0..1023 bucket must fit the backing table;
/// normal draw supplies shifts 0..3. Geometry, stream and packet capacities
/// are not checked here.
///
/// Returns `elements + elemCount * elemStride` and advances `primWrite` past
/// all slots. Workspace counts and saved GTE results are unchanged. All storage
/// is borrowed; no pointer is retained. Keep packets alive until the GPU finishes.
u32* tmdDrawStreamGt4SemiTrans(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Culls and links pre-transformed Gouraud textured triangles with forced semi-transparency.
///
/// Draw resolution selects this entry for `0x3B`; GPU code 0x36 is used
/// regardless of `objectFlags & TMD_OBJECT_SEMI_TRANS`. It shares
/// `tmdDrawStreamPrimGt3PreXform`'s packet and depth walks. `objectFlags`
/// carries `TmdObject.flags`: `TMD_OBJECT_REVERSE_CULLING` keeps negative
/// rather than positive `NCLIP(0,1,2)` areas. Zero area and any corner marked
/// with `TMD_VERTEX_DEPTH_INVALID` are rejected. Texture-page blend mode
/// is preserved; this entry neither projects nor lights.
///
/// `elements` starts after the three-word record header. `workspace->elemCount`
/// supplies 0..65535 elements and `elemStride` their stride in u32 words,
/// at least two for a nonempty record. The first three u16 values are byte
/// offsets into `workspace->szTable`; word 1's high half and later words
/// are ignored. Each offset must name a complete aligned four-byte cache
/// entry already written by a projection record (0..4092 in normal drawing's
/// 1024-entry cache). Valid entries contain screen Z (0..65535).
///
/// The word-aligned `workspace->preXformWrite` must provide one complete
/// 40-byte `POLY_GT3` slot per element in the selected buffer half's first
/// region. Construction fills the texture words from standard five-word
/// elements; preceding projection records must fill each slot's screen XY
/// and lit RGB. Every element consumes its slot, including rejected triangles,
/// which leave both packet and OT unchanged. Accepted packets retain these
/// words and receive code 0x36 and a nine-word DMA length.
///
/// The caller must set the GTE's three-depth averaging scale. `AVSZ3` depth
/// selects bucket `(((u32)OTZ << (workspace->otDepthShift & 31)) & 0x3FFF) >> 4`
/// in `workspace->ot`, whose base already includes the object's signed entry
/// displacement. Every resulting bucket (0..1023) must fit the backing table;
/// normal drawing supplies shifts 0..3. Accepted packets prepend to that bucket
/// using 24-bit DMA addresses. Stream, cache, packet and OT bounds are unchecked.
///
/// Returns `elements + elemCount * elemStride`, leaving the next record or
/// marker unconsumed, and advances only `workspace->preXformWrite` among
/// workspace fields. Counts and saved GTE results remain unchanged. An empty
/// record reads no payload and advances neither cursor. All storage is borrowed;
/// no pointer is retained. Keep packets and OT storage valid until GPU use ends.
u32* tmdDrawStreamPrimGt3PreXformSemiTrans(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Culls and links pre-transformed Gouraud textured triangles as `POLY_GT3` packets.
///
/// Draw resolution selects this entry for `0x31`, `0x39` and `0x131`.
/// `objectFlags` carries `TmdObject.flags`: `TMD_OBJECT_SEMI_TRANS` selects
/// GPU code 0x36 through `tmdDrawStreamPrimGt3PreXformSemiTrans`, otherwise
/// code 0x34 is used. `TMD_OBJECT_REVERSE_CULLING` keeps negative rather than
/// positive `NCLIP(0,1,2)` areas; zero area is always rejected. The opcode
/// itself is not read, and texture-page blend mode is left unchanged.
///
/// `elements` starts after the three-word header. `workspace->elemCount`
/// supplies 0..65535 elements and `elemStride` their stride in u32 words,
/// at least two for a nonempty record. The first three u16 values are byte
/// offsets into `workspace->szTable`; the high half of word 1 is ignored.
/// Each must name a complete aligned four-byte depth entry written by the
/// projection pre-pass (offsets 0..4092 in normal drawing's 1024-entry cache).
/// Cache entries hold screen Z (0..65535), with `TMD_VERTEX_DEPTH_INVALID`
/// marking a failed projection; any such corner rejects the whole triangle.
///
/// The word-aligned `workspace->preXformWrite` must provide one complete
/// 40-byte packet slot per element in the selected buffer half's first region.
/// `tmdBuildStreamGt3PreXform` builds the texture words from standard five-word
/// elements; earlier projection records scatter screen XY and lit RGB into
/// those slots. This handler preserves those words, changing only accepted
/// packets' command byte and DMA tag. Every element consumes its slot,
/// including rejected triangles.
///
/// Accepted cached depths go into SZ1..SZ3 for AVSZ3 using the caller's GTE
/// ZSF3 scale. Packets receive a nine-word DMA length and prepend to
/// `workspace->ot` at bucket
/// `(((u32)OTZ << (workspace->otDepthShift & 31)) & 0x3FFF) >> 4`.
/// Normal drawing supplies shifts 0..3. The OT base already includes the
/// object's signed entry displacement; every resulting bucket (0..1023)
/// must fit the selected table. DMA links retain 24 address bits. Stream,
/// cache, packet and OT bounds are unchecked; all storage is borrowed.
///
/// Returns `elements + elemCount * elemStride` and advances `preXformWrite`
/// past all slots. Other workspace fields, including counts and saved GTE
/// results, are unchanged. An empty record reads no payload and advances
/// neither cursor. No pointer is retained; keep packets and OT storage valid
/// until GPU use finishes.
u32* tmdDrawStreamPrimGt3PreXform(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Culls and links pre-transformed Gouraud textured quads with forced semi-transparency.
///
/// Draw resolution selects this entry for opcode `0x7B`; it always stamps GPU
/// code 0x3E, independently of `objectFlags & TMD_OBJECT_SEMI_TRANS`.
/// `tmdDrawStreamPrimGt4PreXform` also branches here when that bit is set.
/// `objectFlags` carries `TmdObject.flags`; only `TMD_OBJECT_REVERSE_CULLING`
/// is read here. The shared walks keep `NCLIP(0,1,2) > 0` or, if that fails,
/// `NCLIP(1,2,3) < 0`, reversing both strict signs for reversed culling.
/// This entry neither projects nor lights, and preserves the texture page's
/// blend mode. The record opcode is not read.
///
/// `elements` starts after the three-word record header. The caller supplies
/// `workspace->elemCount` (0..65535) and `elemStride` in u32 words. Drawing
/// reads words 0..1 as four unsigned u16 byte offsets into `workspace->szTable`,
/// in corner order. Each must name a complete aligned four-byte depth entry
/// already written by a projection command (offsets 0..4092 in normal drawing's
/// 1024-entry cache). Valid entries retain screen Z (0..65535); any corner
/// marked `TMD_VERTEX_DEPTH_INVALID` rejects the whole quad before averaging.
/// Drawing needs at least two words per element; `tmdBuildStreamGt4PreXform`
/// constructs texture fields from words 2..4, so complete elements need at
/// least five. The stream must contain count * stride payload words.
///
/// Word-aligned `workspace->preXformWrite` must provide one writable 52-byte
/// `POLY_GT4` slot per element in the selected buffer half's first region.
/// Construction must have initialized texture fields; preceding projection
/// commands must have supplied screen XY in pixels, corner RGB and cached Z.
/// The GTE's ZSF4 must hold the four-depth averaging scale. Accepted packets prepend
/// to `workspace->ot` at AVSZ4 bucket
/// `(((u32)OTZ << (workspace->otDepthShift & 31)) & 0x3FFF) >> 4`.
/// Normal drawing supplies shifts 0..3. The OT base includes the object's
/// signed entry displacement; every wrapped bucket (0..1023) must fit the
/// backing table. DMA links retain 24 address bits and twelve payload words.
/// Only the command byte, DMA tag and selected OT head are written. Rejected
/// quads leave packets and OT unchanged but still consume their reserved slots.
/// Stream, depth-cache, packet and OT bounds are unchecked.
///
/// Returns `elements + elemCount * elemStride`, leaving the next record or
/// marker unconsumed, and advances only `workspace->preXformWrite` among
/// workspace fields. Counts and saved GTE results are unchanged. An empty
/// record reads no payload and advances neither cursor. Workspace, payload and
/// cached depths are borrowed for the call; keep linked packets and OT storage
/// valid until GPU use ends.
u32* tmdDrawStreamPrimGt4PreXformSemiTrans(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Culls and links pre-transformed Gouraud textured quads using cached vertex depths.
///
/// Draw resolution selects this callback for opcodes `0x71`, `0x79` and `0x171`.
/// `objectFlags` contains `TmdObject.flags`: `TMD_OBJECT_SEMI_TRANS` selects
/// GPU code 0x3E instead of 0x3C through `tmdDrawStreamPrimGt4PreXformSemiTrans`.
/// `TMD_OBJECT_REVERSE_CULLING` reverses both facing tests. With corners in
/// packet order, ordinary facing keeps `NCLIP(0,1,2) > 0` or, if that fails,
/// `NCLIP(1,2,3) < 0`; reversed facing keeps the opposite strict signs.
/// Zero in both tests rejects the quad. The record opcode is not read here,
/// and the texture page's blend mode is preserved.
///
/// `elements` starts after the three-word record header. The caller supplies
/// `workspace->elemCount` (0..65535) and `elemStride` in u32 words. Words 0..1
/// pack four unsigned u16 byte offsets into `workspace->szTable`, in corner
/// order. Each must name a complete aligned four-byte depth entry written by
/// a preceding projection command (offsets 0..4092 in normal drawing's
/// 1024-entry cache). Entries retain screen Z (0..65535); any corner marked
/// `TMD_VERTEX_DEPTH_INVALID` rejects the quad before averaging. Drawing reads
/// two words per element, while `tmdBuildStreamGt4PreXform` initializes texture
/// data from words 2..4, so a complete element needs at least five words.
/// The stream must contain count * stride payload words; bounds are unchecked.
///
/// `workspace->preXformWrite` must provide one writable, word-aligned 52-byte
/// `POLY_GT4` slot per element in the selected buffer half's first region.
/// Texture fields and corner colours must already be initialized, and the
/// pre-pass must have supplied screen XY and cached depths. The GTE must hold
/// the four-depth averaging scale; this handler uses existing packet colours.
/// Accepted packets prepend to `workspace->ot` using AVSZ4 depth at bucket
/// `(((u32)OTZ << (workspace->otDepthShift & 31)) & 0x3FFF) >> 4`.
/// Normal drawing supplies shifts 0..3. The OT base includes the object's
/// signed entry displacement; every wrapped bucket (0..1023) must fit the
/// backing table. DMA links retain 24 address bits and twelve payload words.
/// Only the command byte, DMA tag and selected OT head are written; rejection
/// leaves packet and OT data unchanged. Every element consumes its reserved
/// packet slot, including rejected quads.
///
/// Returns `elements + elemCount * elemStride`, leaving the next record or
/// marker unconsumed, and advances only `workspace->preXformWrite` among
/// workspace fields. Counts, saved GTE results and `primWrite` are unchanged.
/// An empty record reads no payload and advances neither cursor. Workspace,
/// stream, cache, packets and OT are borrowed; keep packets and OT storage
/// GPU-visible until consumption finishes.
u32* tmdDrawStreamPrimGt4PreXform(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects and draws opcode-zero triangles, lighting each `POLY_G3` from one face normal.
///
/// `elements` starts after the three-word record header. Each element contains
/// three words: vertex byte offsets 0/1 in the low/high halves of word 0,
/// vertex offset 2 and a normal byte offset in word 1, and RGB plus the GPU
/// command byte in word 2. Offsets must address complete, word-aligned
/// eight-byte vectors in the borrowed `workspace->verts` and `workspace->normals`
/// arrays; their lengths are not supplied. `workspace->elemCount` and
/// `workspace->elemStride` are decoded unsigned halfwords (0..65535); stride
/// counts u32 words and must be at least three for a nonempty record. The
/// stream must contain count * stride payload words and remain valid for the call.
///
/// The word-aligned `workspace->primWrite` cursor must have count complete
/// `POLY_G3` slots in the selected buffer half's second region, as reserved by
/// `modelLightingReserveStreamPrimG3`. Projection errors (GTE FLAG bit 31) and
/// nonpositive screen-space winding reject a triangle; every element still
/// advances the packet cursor by 28 bytes. Accepted triangles receive screen
/// coordinates and the same lit RGB/command word at all three corners.
///
/// The caller supplies the part's GTE transform, lighting and depth-average
/// scale. Accepted packets are prepended to `workspace->ot` at entry
/// `((OTZ << (workspace->otDepthShift & 31)) & 0x3FFF) >> 4`, with the object's
/// signed OT displacement already in that base. The resulting entry (0..1023)
/// must fit the selected table after displacement. DMA links retain 24 address
/// bits and the packet tag records six payload words; GPU-visible packet
/// storage and OT storage are borrowed and must outlive GPU consumption.
///
/// Returns the word after the payload, leaving the following record or marker
/// unconsumed, and updates only `workspace->primWrite`; the workspace count and
/// stride are unchanged. An empty record reads no payload and advances neither
/// cursor. `objectFlags` is ignored, including blend and reverse-culling bits:
/// the command byte comes from the element and positive winding is always required.
u32* tmdDrawStreamPrimG3(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects and draws untextured Gouraud quads, lighting from one face normal.
///
/// Draw resolution selects this handler for `0x40`. `elements` starts after
/// the three-word record header. Words 0 and 1 pack four unsigned u16 vertex
/// byte offsets, in corner order. Word 2 is the face normal's unsigned byte
/// offset: the entire 32-bit word is used, without masking its high half.
/// Word 3 supplies RGB and the GPU command byte, which NCCS preserves in the
/// lit colour repeated at all four corners. Each geometry offset must address
/// a complete eight-byte vector in the borrowed `workspace->verts` or
/// `workspace->normals` array. Their complete extents are not supplied here.
/// `elemCount` and `elemStride` are decoded unsigned halfwords (0..65535);
/// stride counts u32 words and must be at least four for a nonempty record.
/// The stream must contain count * stride payload words and remain valid for the call.
///
/// The caller supplies the part's GTE transform, lighting and four-depth
/// averaging scale. Vertex 3 is projected first, followed by vertices 2, 1, 0.
/// When vertex 3 equals the preceding element's vertex 0 offset, its screen XY
/// and depth are reused. A projection with GTE FLAG bit 31 set rejects the
/// quad and invalidates reuse; a facing rejection retains it. Facing keeps
/// `NCLIP(2,1,0) < 0` or, if that fails, `NCLIP(2,1,3) > 0`.
///
/// The word-aligned `workspace->primWrite` must have count complete 36-byte
/// `POLY_G4` slots in the selected buffer half's second region, as reserved by
/// `modelLightingReserveStreamPrimG4`. Every element consumes one slot,
/// including rejected quads, which may leave partial coordinate writes.
/// Accepted packets receive all coordinates, four equal lit RGB/command words
/// and an eight-word DMA length. They prepend to `workspace->ot` at bucket
/// `(((u32)OTZ << (workspace->otDepthShift & 31)) & 0x3FFF) >> 4`, using AVSZ4.
/// Normal drawing supplies shifts 0..3. The OT base already includes the
/// object's signed entry displacement; every resulting bucket (0..1023) must
/// fit the selected table. DMA links retain 24 address bits. Capacities are
/// unchecked; packet and OT storage must remain valid until GPU use finishes.
///
/// Returns `elements + elemCount * elemStride`, leaving the next record or
/// marker unconsumed, and updates only `workspace->primWrite` among workspace
/// fields. Counts, saved GTE results and the first-region cursor are unchanged.
/// An empty record reads no payload and advances neither cursor. `objectFlags`
/// is ignored, including blend and reverse-culling bits; the command byte
/// comes from the element and the facing signs above always apply.
u32* tmdDrawStreamPrimG4(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects and draws semi-transparent textured triangles, lighting each from one face normal.
///
/// Draw resolution selects this entry for opcode `0x1A`. `elements` starts
/// after the three-word record header. Words 0/1 pack four unsigned u16 byte
/// offsets: vertex 0, vertex 1, vertex 2, then the face normal. Each must address
/// a complete, word-aligned eight-byte vector in the borrowed `workspace->verts`
/// or `workspace->normals` array; neither array's full extent is supplied here.
/// `elemCount` and `elemStride` are decoded unsigned halfwords (0..65535);
/// stride counts u32 words. Drawing reads two words per element, but construction
/// requires at least five: `tmdBuildStreamGt3OneNormal` initializes UVs, texture
/// page and CLUT from words 2..4. The stream must contain count * stride payload
/// words and remain valid for the call.
///
/// The word-aligned `workspace->primWrite` must have count complete 40-byte
/// `POLY_GT3` slots in the selected buffer half's second region, with texture
/// words already initialized. Every element consumes one slot, including
/// triangles rejected for GTE FLAG bit 31 or nonpositive `NCLIP(0,1,2)` winding.
/// Rejection writes neither packet nor OT. Accepted packets receive projected
/// XY and the same lit RGB/command word at all three corners: NCCS lights fixed
/// RGB (128,128,128) with GPU code 0x36. Texture words, including the texture
/// page's blend mode, remain unchanged.
///
/// The caller supplies the part's GTE transform, lighting and three-depth
/// averaging scale. Accepted packets prepend to `workspace->ot` using AVSZ3 at
/// entry `(((u32)OTZ << (workspace->otDepthShift & 31)) & 0x3FFF) >> 4`.
/// Normal drawing supplies shifts 0..3. The OT base already includes the
/// object's signed entry displacement; every resulting bucket (0..1023) must
/// fit the selected table. DMA links retain 24 address bits and the packet tag
/// records nine payload words. Capacities are unchecked; packet and OT storage
/// must remain GPU-visible until consumption completes.
///
/// Returns `elements + elemCount * elemStride`, leaving the next record or
/// marker unconsumed, and updates only `workspace->primWrite` among workspace
/// fields. Counts, saved GTE results and the first-region cursor are unchanged.
/// An empty record reads no payload and advances neither cursor, but still sets
/// GTE RGBC to the fixed colour/code. `objectFlags` is ignored, including blend
/// and reverse-culling bits. This alternate entry shares the rendering body of
/// `tmdDrawStreamPrimGt3OneNormal` and always supplies semi-transparent code 0x36.
u32* tmdDrawStreamPrimGt3OneNormalSemiTrans(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects and draws opaque textured triangles, lighting each from one face normal.
///
/// Draw resolution selects this entry for `0x18`. `elements` starts after the
/// three-word record header. Words 0/1 pack four unsigned u16 byte offsets:
/// vertex 0, vertex 1, vertex 2, then the face normal. Each must address a
/// complete, word-aligned eight-byte vector in the borrowed `workspace->verts`
/// or `workspace->normals` array; neither array's full extent is supplied here.
/// `elemCount` and `elemStride` are decoded unsigned halfwords (0..65535);
/// stride counts u32 words. Drawing reads two words per element, but the
/// complete record requires at least five because `tmdBuildStreamGt3OneNormal`
/// initializes the texture words from words 2..4. The stream must contain
/// count * stride payload words and remain valid for the call.
///
/// The word-aligned `workspace->primWrite` must have count complete 40-byte
/// `POLY_GT3` slots in the selected buffer half's second region, with UVs,
/// texture page and CLUT already initialized by construction. Every element
/// consumes one slot, including triangles rejected for GTE FLAG bit 31 or
/// nonpositive `NCLIP(0,1,2)` winding. Rejection writes neither packet nor OT.
/// Accepted packets receive projected XY and the same lit RGB/command word at
/// all three corners: NCCS lights fixed RGB (128,128,128) with GPU code 0x34.
/// Texture words remain unchanged.
///
/// The caller supplies the part's GTE transform, lighting and three-depth
/// averaging scale. Accepted packets prepend to `workspace->ot` using AVSZ3 at
/// entry `(((u32)OTZ << (workspace->otDepthShift & 31)) & 0x3FFF) >> 4`.
/// Normal drawing supplies shifts 0..3. The OT base already includes the
/// object's signed entry displacement; every resulting bucket (0..1023) must
/// fit the selected table. DMA links retain 24 address bits and the packet tag
/// records nine payload words. Capacities are unchecked; packet and OT storage
/// are borrowed and must remain GPU-visible until consumption completes.
///
/// Returns `elements + elemCount * elemStride`, leaving the next record or
/// marker unconsumed, and updates only `workspace->primWrite` among workspace
/// fields. Counts, saved GTE results and the first-region cursor are unchanged.
/// An empty record reads no payload and advances neither cursor. `objectFlags`
/// is ignored, including blend and reverse-culling bits. Opcode `0x1A` selects
/// `tmdDrawStreamPrimGt3OneNormalSemiTrans`, which shares the body but supplies
/// GPU code 0x36 through its own entry; this entry always supplies opaque 0x34.
u32* tmdDrawStreamPrimGt3OneNormal(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects, lights and links opaque textured quads with one face normal.
///
/// Draw resolution selects this callback for opcode `0x58`. `elements` starts
/// after the three-word record header; `workspace->elemCount` supplies 0..65535
/// elements and `elemStride` their stride in u32 words. Words 0..1 pack four
/// u16 byte offsets into `verts`, in corner order. Word 2 is added whole as a
/// byte offset into `normals`; its upper half must be zero for this record
/// format. Each reference must address a complete eight-byte SVECTOR entry.
/// The draw body reads three words, while construction uses words 3..5 for
/// UV/CLUT/tpage data, so each complete element occupies at least six words.
///
/// `primWrite` addresses prebuilt 52-byte POLY_GT4 slots. Every element consumes
/// one slot, including rejected quads. Corner 3 is projected first, followed
/// by corners (2,1,0); it reuses the preceding element's corner-0 screen XY
/// and depth when their vertex offsets agree. Either projection reporting
/// `TMD_GTE_ERROR_FLAG` rejects the quad and invalidates that reuse key.
/// Facing independently accepts `NCLIP(2,1,0) < 0` or `NCLIP(2,1,3) > 0`.
/// Facing rejection retains the key. Rejected slots can receive screen XY,
/// but their tags and colours are not completed and they are not linked.
/// Accepted slots receive one NCCS result at all four corners, lighting fixed
/// RGB (128,128,128) with GPU code 0x3C. Texture words remain unchanged.
///
/// The caller supplies the part's GTE transform, lighting and four-depth
/// averaging scale. AVSZ4 selects OT bucket
/// `(((u32)OTZ << (workspace->otDepthShift & 31)) & 0x3FFF) >> 4`.
/// Normal drawing supplies shifts 0..3. The OT base already includes the
/// object's signed entry displacement; every bucket (0..1023) must fit the
/// selected table. Packets prepend with 24-bit DMA links and twelve payload
/// words. Capacities are unchecked; stream and geometry storage are borrowed
/// for the call, and packet/OT storage must stay GPU-visible until consumed.
///
/// Returns `elements + elemCount * elemStride`, leaving the next record or
/// marker unconsumed, and updates only `workspace->primWrite` among workspace
/// fields. Counts, saved GTE results and the first-region cursor are unchanged.
/// An empty record reads no payload and advances neither cursor. `objectFlags`
/// is ignored, including blend and reverse-culling bits. Opcode `0x5A` selects
/// `tmdDrawStreamPrimGt4OneNormalSemiTrans`, whose entry shares the body and
/// supplies GPU code 0x3E; this entry always supplies opaque 0x3C.
u32* tmdDrawStreamPrimGt4OneNormal(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects, lights and links semi-transparent textured quads with one face normal.
///
/// Draw resolution selects this callback for opcode `0x5A`. It shares the walk
/// with `tmdDrawStreamPrimGt4OneNormal`, but always supplies GPU code 0x3E.
/// `objectFlags` carries `TmdObject.flags` and is ignored, including blend and
/// reverse-culling bits; the texture page's blend mode is preserved.
///
/// `elements` starts after the three-word record header. The caller supplies
/// `workspace->elemCount` (0..65535) and `elemStride` in u32 words. Words 0..1
/// pack four unsigned u16 vertex byte offsets in corner order. Word 2 is a
/// whole u32 byte offset into `normals`, without halfword masking. Every
/// offset must name a complete, word-aligned eight-byte SVECTOR in the borrowed
/// `verts` or `normals` array. Drawing reads three words per element;
/// construction reads texture words 3..5, so a complete element needs at least
/// six words. Stream and geometry capacities are not checked.
///
/// The GTE must hold the part transform, lighting and four-depth averaging
/// scale. Corner 3 is projected first, followed by (2,1,0). When corner 3's
/// vertex offset equals the preceding element's corner-0 offset, its screen XY
/// and depth are reused. Either projection reporting `TMD_GTE_ERROR_FLAG`
/// rejects the quad and invalidates that key; facing rejection retains it.
/// Facing accepts `NCLIP(2,1,0) < 0` or `NCLIP(2,1,3) > 0`, testing the
/// second triangle only when the first is not accepted. Accepted packets get
/// one NCCS result at all four corners, lighting fixed RGB (128,128,128).
///
/// `workspace->primWrite` must provide one writable, word-aligned 52-byte
/// POLY_GT4 slot per element, with texture words already initialized. Every
/// element consumes a slot, including rejected quads; rejection may write
/// screen XY but does not complete colours/tags or link the packet. Accepted
/// packets prepend to `workspace->ot` using AVSZ4 at bucket
/// `(((u32)OTZ << (workspace->otDepthShift & 31)) & 0x3FFF) >> 4`.
/// Normal drawing supplies shifts 0..3. The OT base includes the object's
/// signed entry displacement, and each resulting bucket (0..1023) must fit
/// the backing table. DMA links retain 24 address bits with twelve payload
/// words. Packet/OT storage must remain GPU-visible until consumption ends.
///
/// Returns `elements + elemCount * elemStride`, leaving the next record or
/// marker unconsumed, and advances only `workspace->primWrite` among workspace
/// fields. Counts, saved GTE results and the first-region cursor are unchanged.
/// An empty record reads no payload and advances neither cursor. Workspace,
/// stream and geometry storage are borrowed for the call.
u32* tmdDrawStreamPrimGt4OneNormalSemiTrans(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects, lights and links Gouraud textured triangles with per-corner material colours.
///
/// Draw resolution selects this callback for opcode `0x130`. `elements` starts
/// after the three-word record header. `workspace->elemCount` supplies 0..65535
/// elements and `elemStride` their stride in u32 words. Words 0..2 pack six u16
/// byte offsets in order: vertex 0, vertex 1, vertex 2, normal 0, normal 1,
/// normal 2. Each must address a complete, word-aligned eight-byte vector in
/// the borrowed `verts` or `normals` array. Words 3..5 supply the respective
/// corners' material RGB and command bytes for independent NCCS lighting.
/// Drawing reads these first six words; the complete record needs at least
/// nine words per element because `tmdBuildStreamGt3CornerColors` reads the
/// texture words at 6..8. Stream and geometry extents are caller obligations.
///
/// `primWrite` must address one writable, word-aligned `POLY_GT3` slot per
/// element in the selected buffer half's second region, with texture fields
/// already initialized by construction. The caller sets the GTE part transform,
/// light/colour matrices, background colour and three-vertex depth-average scale.
/// Projection errors (`TMD_GTE_ERROR_FLAG`) and nonpositive NCLIP areas reject
/// a triangle. `objectFlags` carries `TmdObject.flags`; only
/// `TMD_OBJECT_SEMI_TRANS` is read, selecting GPU code 0x36 instead of 0x34.
/// Reverse-culling and other object bits are ignored; texture-page blend mode
/// is preserved.
///
/// Every element consumes its 40-byte packet slot, even when rejected. Accepted
/// packets receive screen coordinates, three lit colour words, the selected
/// code and a nine-word DMA length. They prepend to `workspace->ot` at bucket
/// `((OTZ << (otDepthShift & 31)) & 0x3FFF) >> 4`. The displaced OT base and
/// wrapped bucket (0..1023) must fit the backing table. Packet links encode
/// 24-bit GPU DMA addresses; keep the packets alive until the GPU finishes.
///
/// Returns `elements + elemCount * elemStride`, leaving the next record or
/// marker unconsumed, and advances `workspace->primWrite` past all slots.
/// Counts, saved GTE results and the pre-transformed cursor remain unchanged.
/// An empty record reads no payload and advances neither cursor. All storage
/// is borrowed for the call; no pointer is retained by the callback.
u32* tmdDrawStreamPrimGt3CornerColors(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects, lights and links textured Gouraud quads with one normal and material colour per corner.
///
/// Draw resolution selects this callback for opcode `0x170`. `elements` starts
/// after the three-word record header. `workspace->elemCount` supplies 0..65535
/// elements and `elemStride` their stride in u32 words. Words 0..3 pack eight
/// unsigned u16 byte offsets: vertices 0..3, then normals 0..3. Each must address
/// a complete, word-aligned eight-byte vector in the borrowed `verts` or
/// `normals` array. Words 4..7 supply the corners' material RGB and command
/// bytes for independent NCCS lighting. Drawing reads these eight words; the
/// complete element needs at least eleven because `tmdBuildStreamGt4CornerColors`
/// initializes the texture fields from words 8..10. Stream and geometry extents
/// are caller obligations.
///
/// `primWrite` must address one writable, word-aligned `POLY_GT4` slot per
/// element in the selected buffer half's second region, with texture fields
/// already initialized. The caller sets the GTE part transform, light/colour
/// matrices, background colour and four-vertex depth-average scale. Vertex 3
/// reuses vertex 0's projected XY and depth from the preceding element when
/// their offsets agree; a failed projection invalidates that reuse. Projections
/// reporting `TMD_GTE_ERROR_FLAG` are rejected. Facing keeps `NCLIP(2,1,0) < 0`
/// or, if that fails, `NCLIP(2,1,3) > 0`. `objectFlags` carries `TmdObject.flags`:
/// only `TMD_OBJECT_SEMI_TRANS` is read, selecting GPU code 0x3E instead of 0x3C.
/// Reverse-culling and other bits are ignored; texture-page blend mode is kept.
///
/// Every element consumes its 52-byte packet slot, including rejected quads,
/// whose coordinates may be partially written. Accepted packets receive screen
/// coordinates, four lit colour words, the selected code and a twelve-word DMA
/// length. They prepend to `workspace->ot` using AVSZ4 depth at bucket
/// `(((u32)OTZ << (workspace->otDepthShift & 31)) & 0x3FFF) >> 4`. The displaced
/// OT base and wrapped bucket (0..1023) must fit the backing table. Links encode
/// 24-bit GPU DMA addresses; keep packets and OT storage alive until GPU use ends.
///
/// Returns `elements + elemCount * elemStride`, leaving the next record or
/// marker unconsumed, and advances only `workspace->primWrite` among workspace
/// fields. Counts, saved GTE results and the pre-transformed cursor stay unchanged.
/// An empty record reads no payload and advances neither cursor. All storage
/// is borrowed for the call; no pointer is retained by the callback.
u32* tmdDrawStreamPrimGt4CornerColors(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

#endif // MAIN_PRIVATE_TMD_H
