#ifndef MAIN_PRIVATE_TMD_H
#define MAIN_PRIVATE_TMD_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

struct Task;

/// 0x98-byte scratch for Tmd_SetupDraw (draw path).
typedef struct {
    TmdStreamWorkspace stream; // Shared construction/draw callback layout
    byte               pad_88[0x10];
} TmdScratchDrawBlock;
STATIC_ASSERT_SIZEOF(TmdScratchDrawBlock, 0x98);

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

/// Draw-pass handler of a stream's pre-transformed untextured quad records
/// (`0x61`, `0x161`): each element completes one `POLY_G4` in the buffer half's
/// first region, whose corners are already in screen space, and links it into the
/// ordering table at the depth they average to.
///
/// Nothing is projected or lit here. The pass that projects the stream's vertices
/// (`tmdXformStreamVerts`) has written each corner's screen coordinates and lit
/// colour into the packet, and its depth into the screen-Z table, so an element
/// names its corners in that table rather than in the vertex array. What a frame
/// adds is the quad's filing: the facing comes from the coordinates the packet
/// already carries, the cached depths are averaged for the ordering-table link,
/// and the packet's tag and primitive code are written.
///
/// The facing test is taken first, on the quad's first three corners; where it
/// turns them away, the fourth corner is put through the test as well, and only a
/// quad that both tests reject is left unlinked. The depths are read after that,
/// and an element naming a corner the pre-pass marked as failed is not linked
/// either. Either way the element consumes its packet's room: the first region's
/// cursor advances by one packet per element, which is what keeps the packets in
/// step with the elements that named them.
///
/// The record has no variant for `flags` to select: the primitive code is this
/// entry's own constant, and the body's other entry
/// (`Tmd_StreamHandler_Prim3A`) stamps the blended one, which no opcode resolves
/// to.
u32* tmdDrawStreamPrimG4PreXform(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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

/// Draw handler of a stream's transform pre-pass records that carry a colour of
/// their own (`0xC0`): `tmdXformStreamVerts`'s pass with the element's colour
/// word in place of that one's fixed colour.
///
/// An element names a vertex, a normal, a colour word and the two places in the
/// buffer half its results go. The vertex is projected into screen coordinates,
/// the normal is lit into a colour from the element's own word, and both results
/// are written where the element names — the projection into the slot a packet
/// carries a corner's coordinates in, the colour into the packet colour word
/// beside it.
///
/// The rest is the fixed-colour pass's: the projection reused where consecutive
/// elements name the same vertex, the depth each transform leaves in the
/// per-vertex cache, and the flag a rejected projection sets there. The record
/// has no variant for `flags` to select, so it goes unread.
u32* tmdXformStreamVertsElemColor(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
/// `gpStreamPrimGt4` has already copied their texture words into one
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
u32* tmdDrawStreamPrimGt3PreXformSemiTrans(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
/// length and primitive code are written. An element with any corner depth marked
/// `TMD_VERTEX_DEPTH_INVALID` by the projection pre-pass, or whose triangle turns
/// away, is stepped over rather than drawn, though its packet slot is passed over
/// either way, so the primitives stay aligned with the elements that named them.
///
/// This entry is the whole family's and is the one that chooses between the two
/// primitive codes: it reaches `tmdDrawStreamPrimGt3PreXformSemiTrans` when the
/// drawing object's flags ask for the blended form.
u32* tmdDrawStreamPrimGt3PreXform(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// The draw pass's handler for a stream's pre-transformed textured-quad records
/// that ask for the semi-transparent primitive (`0x7B`): each element's quad is
/// linked into the ordering table under the blended primitive code, at the depth
/// its four corners average to.
///
/// The record is the opaque `0x79` one with the semi-transparency bit set. Its
/// packet is the build pass's (`gpStreamPrimGt4PreXform` lays the texture words
/// down) with the corners the stream's transform records (`0xC8`) have since
/// written into it, so nothing here transforms or lights: the corners go back
/// into the GTE for the facing tests alone. The element's refs are read as the
/// depth cache those records fill, where a vertex whose projection failed is
/// stored with its sign bit set, so a quad that names one of those, or that faces
/// away, is not linked.
///
/// This entry is the same body as `tmdDrawStreamPrimGt4PreXform`, reached
/// directly: the record asks for the blended form by its opcode alone, so the
/// code it stamps is `0x3E` unconditionally, where that entry picks the code from
/// the drawing object's flags. Both read `flags` for one thing besides — the bit
/// a model drawn as a reflection sets, which sends the facing tests the other way
/// round.
u32* tmdDrawStreamPrimGt4PreXformSemiTrans(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Draw-pass handler of a stream's pre-transformed textured-quad records
/// (`0x71`, `0x79`, `0x171`): each element contributes one quad to the buffer
/// half's first region, where its corners are already in screen space, and its
/// packet is linked into the ordering table at the depth those corners measured.
///
/// The record is the pre-transformed form of the `0x78` quads
/// (`tmdDrawStreamGt4`): the build pass (`gpStreamPrimGt4PreXform`) lays each
/// element's texture words, page and CLUT down, and the stream's transform
/// records (`0xC8`, `tmdXformStreamVerts`) project the element's four corners into
/// the same packet, light them and put their depths in the per-vertex screen-Z
/// table, so the element's refs are read as entries of that table rather than as
/// vertex indices. What a frame adds is the quad's filing: the four cached depths
/// are averaged for the ordering-table link, the facing comes from the coordinates
/// the packet already carries, and the packet's length and primitive code are
/// written. An element with any corner depth marked `TMD_VERTEX_DEPTH_INVALID` by
/// the projection pre-pass, or whose quad turns away, is stepped over rather than
/// drawn, though its packet slot is passed over either way, so the primitives stay
/// aligned with the elements that named them.
///
/// This entry is the whole family's and is the one that chooses between the two
/// primitive codes: it reaches `tmdDrawStreamPrimGt4PreXformSemiTrans` when the
/// drawing object's flags ask for the blended form, and stamps the opaque `0x3C`
/// where they do not.
u32* tmdDrawStreamPrimGt4PreXform(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
u32* tmdDrawStreamPrimGt3OneNormalSemiTrans(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
u32* tmdDrawStreamPrimGt3OneNormal(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Draw handler of a stream's one-normal textured-quad records (`0x58`, `0x5A`):
/// each element's quad is transformed and culled, its four corners' screen
/// coordinates and the one colour they are lit from are written into the
/// `POLY_GT4` the record's texture words were laid in, and that packet is linked
/// into the ordering table.
///
/// `tmdProcessStream` fills the polygon's texture words as it builds the record
/// into the buffer half, so what is left here is the half that changes per frame.
/// The element names one normal for the whole quad rather than one per corner, so
/// a single lighting step colours all four corners, and the depth it is filed
/// under is an average of the corners' depths, out of the same transform. The GTE
/// projects three vertices at a time, so the element's fourth corner is projected
/// on its own, ahead of the other three. A corner the GTE reports off screen, or
/// a quad the facing tests reject, is not drawn, though the packet's room is
/// passed over either way, so the primitives stay in step with the elements.
///
/// This entry is the opaque one; the `0x5A` record's entry shares this body and
/// asks for the blended form by its opcode alone, so `flags` selects nothing
/// here either.
u32* tmdDrawStreamPrimGt4OneNormal(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// The draw pass's handler for a stream's one-normal textured-quad records that
/// ask for the semi-transparent primitive (`0x5A`): each element's quad is taken to
/// screen space and lit from the element's one normal, and its packet is filled in
/// with the resulting screen coordinates and colour and linked into the ordering
/// table, unless the transform clipped it or the facing tests turned it away.
///
/// The element is the opaque `0x58` quad's — one normal for the whole quad rather
/// than one per corner, and the same refs and texture words — and the two entries
/// share one body, so the primitive code the packet is built under is the whole of
/// the difference between the two records: `0x3C` for the opaque quad and `0x3E`
/// here, the semi-transparency bit being the difference. The element names no
/// colour, so the quad is lit from a fixed mid-grey, and the same constant carries
/// both, the code in its top byte. The opcode alone selects the variant, so `flags`
/// goes unread.
u32* tmdDrawStreamPrimGt4OneNormalSemiTrans(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
u32* tmdDrawStreamPrimGt3CornerColors(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
u32* tmdDrawStreamPrimGt4CornerColors(TmdStreamWorkspace* ws, s32 flags, u32* stream);

#endif // MAIN_PRIVATE_TMD_H
