#ifndef GAMEPLAY_MODEL_LIGHTING_H
#define GAMEPLAY_MODEL_LIGHTING_H

#include "types.h"

#include "main/tmd_types.h"

void Gp_ApplyPadReplay(s32 arg0, u16* arg1);

void func_8009EA50(s32 arg0);

u32* func_8009AF90(TmdStreamWorkspace* ws, s32 arg1, u32* arg2);

/// Handler of a stream's layered transform records (`0x40C8`): each element
/// projects the vertex it names into one corner of both primitives of a layered
/// pair, and lights the normal it names into the colour each of those two
/// corners draws with.
///
/// The record is `tmdXformStreamVerts`'s with the `0x4000` bit set, and the two
/// destinations an element names are what that bit changes: the unlayered pass
/// writes the corner of one primitive, where this one writes that same corner in
/// the base primitive and in the semi-transparent layer drawn over it. The rest
/// is the unlayered pass's — the vertex is projected, its depth cached per
/// vertex, the projection reused where consecutive elements name the same
/// vertex, and a vertex whose transform reported an error stored with its sign
/// bit set — and both corners take the one projection the element produced.
///
/// The colour is where the record's two handlers part company. This one splits
/// the neutral material grey between the pair by the object's lighting level, the
/// layer taking the share the level sets and the base the remainder, so at either
/// end of the level one of the two is left black. It writes no texture coordinate
/// and no colour code of its own, which is what a layer textured from the record
/// needs: this is the handler the record resolves to in the areas whose layered
/// draws take their layer's page and CLUT from the object. The other handler
/// serves a layer that has a page of its own, and derives that layer's texture
/// coordinates from the projected vertices. The projection and the normal the
/// lighting rotates are both kept in the frame's scratch either way, and this
/// handler reads neither.
///
/// The record has no variant for `flags` to select, so it goes unread.
u32* gpXformStreamVertsOffsetLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream);

u32* func_8009B500(TmdStreamWorkspace* ws, s32 arg1, u32* arg2);

/// The draw pass's handler for a stream's layered textured-triangle records
/// (`0x4038`) whose semi-transparent layer is textured from the object: each
/// element's triangle is projected and lit into the pair of packets the process
/// pass laid out for it, and both are linked into the ordering table at the
/// depth it came out at.
///
/// `0x4000` asks for two primitives per element — the base the model is drawn
/// from and the semi-transparent layer drawn over it — and the texture words of
/// both are initialized by `tmdBuildStreamGt3OffsetLayer`: the element's texture
/// words plus the independent layer offsets for the first packet, and plus the
/// model's base offsets for the second. What a frame adds is the
/// rest of each packet: the triangle's screen coordinates, the colours the pair
/// is lit from, their lengths and primitive codes, and the links. The layer is
/// lit from a grey material colour that follows the model's light level and the
/// base from that colour's complement, so the level is what divides the record's
/// brightness between the two of them; the layer is completed under the
/// semi-transparent primitive code with its page's semi-transparency rate set,
/// and the base under the opaque code. An element whose projection the GTE
/// rejects, or whose triangle turns away, is stepped over rather than drawn,
/// though its two packets are passed over either way, so the pair stays in step
/// with the elements that named it.
///
/// The place the session is in picks between this handler and a sibling that
/// textures the layer itself. The walk's `flags` select no variant of the record
/// on top of that, so they go unread.
u32* gpDrawStreamPrimGt3OffsetLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// The draw pass's handler for a stream's layered textured-quad records
/// (`0x4078`) whose semi-transparent layer takes its texture page from the
/// object: each element contributes two quads to the buffer half's second region
/// — the opaque base the model is drawn from and the semi-transparent layer drawn
/// over it — projected and lit into the slots the process pass laid out for them,
/// and linked into the ordering table at the depth their four corners average to.
///
/// The element is the `0x78` quad's — a vertex and a normal per corner, and the
/// element's own texture words — and the `0x4000` bit is the whole of what makes
/// it two primitives rather than one. Both primitives' texture words, page and
/// CLUT are the process pass's: it wrote the element's words into each, and biased
/// their page and CLUT — the base's by the model's, the layer's by the object's
/// extra page and CLUT offsets (`tmdBuildStreamGt4OffsetLayer`, whose cursor this
/// handler stays in step with) — so nothing is textured here. The record's other
/// draw handler is the one that textures the layer itself, from the corners'
/// normals and a page of its own.
///
/// The two primitives are lit from the same normals under complementary greys:
/// the layer at the object's light level scaled to the colour range, the base at
/// that range less the level, so the pair trades brightness between them as the
/// level moves. Three corners are lit in one step and the fourth in a step of its
/// own. A corner the GTE reports off screen, or a quad of which neither half faces
/// the camera, keeps its packets out of the ordering table — the room is consumed
/// either way, since the process pass reserved it for every element of the record.
/// The record has no variant for `flags` to select, so it goes unread.
u32* gpDrawStreamPrimGt4OffsetLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream);

u32* func_8009C414(TmdStreamWorkspace* ws, s32 arg1, u32* arg2);

/// The draw pass's handler for a stream's colour-carrying textured-triangle
/// records (`0x30`): each element's triangle is projected and lit from the three
/// normals it names, and the packet the process pass laid out for it is completed
/// and linked into the ordering table at the depth it came out at.
///
/// The element is the plain textured triangle's (`tmdDrawStreamGt3`) with a colour
/// word of its own between its refs and its texture words — that one word is the
/// whole of the difference between the two records, and it is the colour the
/// triangle's corners are lit from, where the plain record's are lit from a fixed
/// grey. Its extended form is the record that carries a colour and a normal per
/// corner (`tmdDrawStreamPrimGt3CornerColors`). Nothing of the packet's texture is
/// settled here: the process pass copies the element's texture words into it and
/// biases its page and CLUT (`tmdBuildStreamGt3ElemColor` steps over the colour word
/// to reach them), so what this handler writes is the half a frame produces — the
/// projected corner coordinates, the corner colours and the packet's length and
/// primitive code.
///
/// The record has no blended partner opcode, so the primitive code is not the
/// opcode's to settle: the packet is stamped opaque (`0x34`), and blended (`0x36`)
/// where the drawing object's own flags ask for it. That choice is the object's
/// rather than the handler's, so the `flags` argument goes unread. An element whose
/// projection the GTE reports off screen, or whose triangle turns away, is stepped
/// over rather than drawn, though its packet slot is passed over either way, so the
/// primitives stay in step with the elements that named them.
u32* gpDrawStreamPrimGt3ElemColor(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// The draw pass's handler for a stream's colour-carrying textured-quad records
/// (`0x70`): each element contributes one `POLY_GT4` to the buffer half's second
/// region, projected, lit and linked into the ordering table at the depth its
/// corners average to.
///
/// The record is the one whose element carries a colour of its own — the material
/// its quad is lit from — and a normal per corner, so the corners are lit against
/// normals of their own, three of them in one step and the fourth in a step of
/// its own, all from that one colour. Its twin is `tmdDrawStreamGt4`, which
/// completes the same primitive from a record that carries no colour and is lit
/// from a fixed one.
///
/// The GTE projects three vertices at a time, so the element's fourth corner is
/// projected in a step of its own, after the other three; the quad's facing is
/// tested on the first three corners and again on the last three. A corner the
/// GTE reports off screen, or a quad the facing tests reject, is not drawn,
/// though the packet's room is passed over either way, so the primitives stay in
/// step with the elements that named them.
///
/// What it writes is what the transform decides: the projected corners, the four
/// corner colours, the packet's length and primitive code — `0x3C`, or `0x3E`
/// where the object's flags call for the blended form — and the ordering-table
/// link. The primitive itself, texture words included, was written when the
/// stream was compiled into the buffer, so this command completes it in place.
/// `flags` selects nothing: the blended form is the object's to ask for rather
/// than the record's, so the parameter goes unread.
u32* gpDrawStreamPrimGt4ElemColor(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Projects, culls and links opaque raw-texture triangles from TMD stream records.
///
/// Draw resolution selects this callback for `0x1C`. `elements` begins after
/// the three-word record header. Its first three u16 values encode vertex
/// byte offsets; masking with 0xFFF8 discards the low three bits and selects
/// complete eight-byte SVECTOR entries in `workspace->verts`. The meaning of
/// the discarded bits is unproven. The source array's extent is not supplied.
/// Drawing reads two words per element; `modelLightingStreamPrimFt3` needs
/// words 2..4 for persistent texture data, so a complete element needs at least
/// five words. `elemStride` counts u32 words and `elemCount` starts at 0..65535.
/// The payload must cover every full stride; bounds are unchecked.
///
/// The caller supplies the part's GTE transform and ZSF3. GTE FLAG bit 31 or
/// nonpositive NCLIP(0,1,2) rejects a triangle without changing its packet or
/// OT. Accepted triangles receive screen XY in pixels, seven payload words
/// and command 0x25 (opaque FT3, texture RGB bypasses colour modulation).
/// Texture and colour bytes are preserved; raw-texture drawing ignores RGB.
/// `objectFlags` is unused, including blend and reverse-culling flags.
///
/// `primWrite` must address one writable, word-aligned 32-byte POLY_FT3 slot
/// per element in the selected half's second region, with texture initialized.
/// Accepted packets prepend to `workspace->ot` using AVSZ3 at bucket
/// `(((u32)OTZ << gDisplayState.otDepthShift) >> 4) & 1023` (shift 0..3 in
/// normal drawing). The OT base includes the object's signed entry displacement;
/// every resulting bucket must fit its backing table. Links retain 24 address
/// bits. Packet and OT storage must remain GPU-visible until consumption ends.
///
/// Advances `primWrite` by one slot per element, including rejected triangles,
/// and returns `elements + initial elemCount * elemStride`, leaving the next
/// record or marker unconsumed. Consumes `elemCount` to -1 even for an empty
/// record, which reads no payload and advances neither cursor. GTE FLAG/result
/// scratch is updated for nonempty records; `preXformWrite` is unchanged. All
/// input storage is borrowed; no pointer is retained by the callback.
u32* tmdDrawStreamPrimFt3(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Projects, culls and links opaque raw-texture quads from TMD stream records.
///
/// Draw resolution selects this callback for `0x5C`. `elements` begins after
/// the three-word record header. Words 0..1 pack four u16 vertex byte offsets,
/// in corner order. Each is masked with 0xFFF8 and must select a complete
/// eight-byte SVECTOR in `workspace->verts`; discarded low-bit roles and the
/// array's full extent are unproven. Drawing reads two words per element;
/// `modelLightingStreamPrimFt4` initializes texture fields from words 2..4,
/// so full elements need at least five. `elemStride` counts u32 words and
/// `elemCount` starts at 0..65535. Every full stride must be readable.
///
/// The caller supplies the part's GTE transform and ZSF4. Projects corners
/// 0,1,2 together, stores their XY after a successful projection, then projects
/// corner 3. Either projection's GTE FLAG bit 31 rejects the quad. Facing keeps
/// NCLIP(0,1,2) > 0 or, if that fails, NCLIP(1,2,3) < 0. Rejected quads may
/// retain the first three XY writes; they are not linked. Accepted quads receive
/// corner 3's XY, nine payload words and command 0x2D (opaque FT4, texture RGB
/// bypasses colour modulation). Texture and RGB bytes persist; drawing ignores
/// RGB. `objectFlags` is unused, including blend and reverse-culling flags.
///
/// `primWrite` must provide one writable, word-aligned 40-byte POLY_FT4 slot
/// per element in the selected half's second region, with texture initialized.
/// Accepted packets prepend to `workspace->ot` using AVSZ4 at bucket
/// `(((u32)OTZ << gDisplayState.otDepthShift) >> 4) & 1023` (shift 0..3 in
/// normal drawing). The OT base includes the object's signed entry displacement;
/// each bucket must fit the backing table. Links retain 24 address bits. Bounds
/// are unchecked; packet and OT storage must outlive GPU consumption.
///
/// Advances `primWrite` by one slot per element, including rejected quads,
/// and returns `elements + initial elemCount * elemStride`, leaving the next
/// record or marker unconsumed. Consumes `elemCount` to -1 even for an empty
/// record, which reads no payload and advances neither cursor. GTE FLAG/result
/// scratch is updated for nonempty records; `preXformWrite` is unchanged.
/// Workspace, stream and vertices are borrowed; no pointer is retained.
u32* tmdDrawStreamPrimFt4(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

u32* func_8009D718(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2);

u32* func_8009D900(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2);

u32* func_8009DB00(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2);

u32* func_8009DCB8(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2);

u32* func_8009DE48(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2);

u32* func_8009E048(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2);

u32* func_8009E274(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2);

u32* func_8009E4A0(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2);

/// The draw pass's handler for a stream's untextured quad records that name a
/// colour and a normal per corner, in their semi-transparent form (`0x162`):
/// each element is one `POLY_G4` in the buffer half's second region, built whole
/// here as the record is transformed.
///
/// The element is the corner-normals quad's (`tmdDrawStreamPrimG4CornerNormals`,
/// `0x60`/`0x62`) with a colour per corner in place of the one colour word the
/// whole quad is lit from there, so each corner is lit from a normal and a colour
/// of its own. The record's opaque entry is `gpDrawStreamPrimG4CornerColors`
/// (`0x160`), and the semi-transparency bit between the two opcodes is the whole
/// of the difference between the entries: this one stamps the packet `0x3A`
/// where the opaque handler stamps `0x38`.
///
/// The first three corners are taken to screen space in one step and written into
/// the packet; the fourth is projected in a step of its own, and only where those
/// three came out facing the camera, so a quad the first facing test turns away is
/// dropped without its last corner being transformed at all. What survives is lit
/// corner by corner, its depth averaged for the ordering-table link, and linked.
/// A dropped quad still consumes its packet's room, because the room was reserved
/// for every element of the record by the process pass (`modelLightingReserveStreamPrimG4`), whose
/// cursor this handler stays in step with.
///
/// `flags` selects no variant of the record, so it goes unread here.
u32* gpDrawStreamPrimG4CornerColorsSemiTrans(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Handler of a stream's unlit transform pre-pass records (`0xC4`): each element
/// projects the vertex it names into the buffer half, and the record writes no
/// colour and builds no primitive.
///
/// The record is `tmdXformStreamVerts`'s with the lighting dropped, which is the
/// `0x04` bit's meaning in a transform pass: an element names a vertex and the
/// place in the buffer half its projection goes, while the normal the lit passes
/// light a colour from goes unread, so nothing is written into the packet's
/// colour word and no colour is loaded into the GTE. What the record keeps is
/// what the commands drawing from that buffer depend on — the vertex is
/// transformed into screen coordinates, its depth cached in the per-vertex table,
/// the projection reused where consecutive elements name the same vertex, and the
/// cached depth marked failed with the sign bit where the frame's flag word reads
/// as failed. That word is read as the record finds it, not refreshed from the
/// transform it has just run, so the mark describes the last transform that
/// stored one.
///
/// The record has no variant for `flags` to select, so it goes unread.
u32* gpXformStreamVertsUnlit(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Initializes persistent texture fields for a record of pre-transformed Gouraud textured triangles.
///
/// Construction selects this callback for `0x31`, `0x39`, `0x3B`, `0x131` and
/// `0x8039`. `elements` begins after the three-word record header; the caller
/// supplies `workspace->elemCount` (0..65535) and `elemStride` in u32 words,
/// at least five for a nonempty record. The first three u16 values are the
/// draw pass's byte offsets into its depth cache; word 1's high half is ignored.
/// Words 2 and 3 pack unsigned byte U/V texel coordinates with encoded CLUT and
/// texture-page settings. Only word 4's low half supplies U2/V2. Five words is
/// a minimum readable extent, not a fixed element size; bounds are unchecked.
///
/// `preXformWrite` must address one writable, four-byte-aligned `POLY_GT3` slot
/// per element in the selected buffer half's first region. The workspace supplies
/// signed encoded-address displacements: `texturePageOffset` (-128..127) and
/// `encodedClutOffset` (-8192..8128, 64 per palette row). Sums wrap modulo 65536
/// in the packet's u16 fields. Projection records supply screen positions and
/// colours during drawing; the primitive draw handler supplies length/code and
/// links. Construction preserves those fields and the SDK pad fields, including
/// `pad2` beside U2/V2, and leaves the second-region cursor `primWrite` unchanged.
///
/// Advances `preXformWrite` by one 40-byte packet per element and returns
/// `elements` advanced by the original count times the word stride. Consumes
/// `elemCount` to -1 even for an empty record, which reads no payload and advances
/// neither cursor. `objectFlags` is the shared callback argument, passed as zero
/// during construction and ignored here. The stream and packet region must
/// contain all elements and slots. All storage is borrowed; no pointer is retained.
u32* tmdBuildStreamGt3PreXform(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes persistent texture fields for pre-transformed Gouraud textured quads.
///
/// Construction selects this callback for `0x71`, `0x79`, `0x7B`, `0x171`
/// and `0x8079`. `elements` begins after the three-word record header. Words
/// 0..1 contain four depth-cache byte references used during drawing. Words 2
/// and 3 pack U0/V0 with encoded CLUT and U1/V1 with texture-page settings;
/// word 4 packs U2/V2 in its low half and U3/V3 in its high half. UV coordinates
/// are unsigned texel bytes. `elemStride` is in u32 words, at least five for
/// nonempty records; the payload must cover every full stride.
///
/// `preXformWrite` must provide one writable, word-aligned 52-byte POLY_GT4
/// slot per element within the selected half's first region. Signed workspace
/// `texturePageOffset` (-128..127 encoded units) and `encodedClutOffset`
/// (-8192..8128, 64 per palette row) relocate the copied addresses, wrapping
/// to u16. Projection commands supply positions and colours later; drawing
/// sets length/code and links. Construction preserves those fields and all
/// SDK pads, including the halfwords beside U2/V2 and U3/V3.
///
/// The caller supplies `elemCount` (0..65535). Advances `preXformWrite` by
/// one packet per element and returns `elements + initial count * elemStride`,
/// leaving the following marker unconsumed. Consumes the count to -1 even for
/// an empty record, which reads no payload and advances neither cursor.
/// `primWrite` is unchanged. `objectFlags` is passed as zero and ignored.
/// Capacities are unchecked. All storage is borrowed; no pointer is retained.
u32* tmdBuildStreamGt4PreXform(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Handler of a stream's pre-transformed flat-quad records (`0x45`): each
/// element contributes one untextured, unlit quad to the buffer half's first
/// region, with the element's colour word written into it.
///
/// The opcode's `0x01` bit marks the corners as already in screen space. This
/// command writes the packet's fixed fields: its length (5 words after the
/// tag), the element's colour and the opaque flat-quad code (`0x28`). The
/// colour is the element's third word and includes the command byte, so the
/// code is stored after it. The record's draw handler
/// (`tmdDrawStreamPrimF4PreXform`) reads four depth-cache offsets from the
/// element's leading halfwords, culls from the screen coordinates already
/// stored in the packet, and links a quad the cull accepts. That handler
/// leaves this length, code and colour unchanged.
///
/// `preXformWrite` advances by one quad per element. The record has no
/// variant for `flags` to select, and packet construction passes zero, so
/// `flags` goes unread. The returned cursor is the stream advanced by one
/// element stride per element.
u32* modelLightingStreamPrimF4PreXform(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Handler of a stream's pre-transformed flat-triangle records (`0x5`): each
/// element contributes one untextured, unlit triangle to the buffer half's
/// first region, with the element's colour word written into it.
///
/// The opcode's `0x01` bit marks the corners as already in screen space. This
/// command writes the packet's fixed fields: its length (4 words after the
/// tag), the element's colour and the opaque flat-triangle code (`0x20`). The
/// colour is the element's third word and includes the command byte, so the
/// code is stored after it. The record's draw handler reads three depth-cache
/// offsets from the element's leading halfwords, culls from the screen
/// coordinates already stored in the packet, and links a triangle the cull
/// accepts. That handler leaves this length, code and colour unchanged.
///
/// `preXformWrite` advances by one triangle per element. The record has no
/// variant for `flags` to select, and packet construction passes zero, so
/// `flags` goes unread. The returned cursor is the stream advanced by one
/// element stride per element.
u32* modelLightingStreamPrimF3PreXform(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Initializes persistent texture fields for a record of gouraud textured triangles.
///
/// Construction selects this callback for `0x38`, `0x3A`, `0x8038`, `0x10038`,
/// `0x1003A` and `0x20038`. `elements` starts after the three-word record header.
/// The caller supplies `workspace->elemCount` (0..65535) and `elemStride` in
/// u32 words, at least six per element. The first three words pack three vertex
/// and three normal references; words 3 and 4 pack unsigned byte U/V texel
/// coordinates with encoded CLUT and texture-page settings. Word 5's low half
/// packs U2/V2; its high half is ignored.
///
/// `primWrite` must address one writable, four-byte-aligned `POLY_GT3` slot per
/// element in the selected buffer half's second region. Texture page and CLUT
/// sums wrap in their u16 fields after adding the workspace's signed encoded
/// displacements: `texturePageOffset` (-128..127) and `encodedClutOffset`
/// (-8192..8128, 64 per palette row). Drawing supplies positions, colours, packet
/// lengths/codes and links later; construction preserves those fields and `pad2`.
///
/// Advances `primWrite` by one 40-byte packet per element and returns the cursor
/// advanced by `elemCount * elemStride` words. The count is consumed to -1 even
/// for an empty record. `objectFlags` is the shared callback argument, passed as
/// zero during construction and ignored here. Stream and packet capacities are
/// caller obligations; the workspace and storage are borrowed, with no pointer
/// retained by this callback.
u32* tmdBuildStreamGt3(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes persistent texture fields for a record of Gouraud textured quads.
///
/// Construction selects this callback for `0x78`, `0x7A`, `0x8078`, `0x10078`
/// and `0x20078`. `elements` starts after the three-word record header; the
/// caller supplies `workspace->elemCount` (0..65535) and `elemStride` in u32
/// words, at least seven per element. Words 0..3 pack four vertex then four
/// normal u16 references for drawing. Words 4/5 pack unsigned byte U/V texel
/// coordinates with encoded CLUT/texture-page settings; word 6 packs U2/V2
/// in its low half and U3/V3 in its high half. Seven words is a minimum readable
/// extent, not a fixed element size. Construction does not access geometry.
///
/// `primWrite` must address one writable, four-byte-aligned `POLY_GT4` slot per
/// element in the selected buffer half's second region. The workspace supplies
/// signed encoded-address displacements: `texturePageOffset` (-128..127) and
/// `encodedClutOffset` (-8192..8128, 64 per palette row). Sums wrap modulo
/// 65536 in the u16 packet fields. Drawing supplies positions, lit colours,
/// length/code and links later; construction preserves those fields and all
/// packet pad fields.
///
/// Advances `primWrite` by one 52-byte packet per element and returns `elements`
/// advanced by the original count times the word stride, leaving the next
/// record or marker unconsumed. Consumes `elemCount` to -1 even for an empty
/// record, which reads no payload and advances neither cursor. Other workspace
/// fields remain unchanged. `objectFlags` is the shared callback argument,
/// passed as zero during construction and ignored here. Stream and packet
/// capacities are unchecked caller obligations. All storage is borrowed for
/// this call; no pointer is retained by this callback.
u32* tmdBuildStreamGt4(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes persistent texture fields for Gouraud textured triangles with one material colour per element.
///
/// Construction selects this callback for opcode `0x30`. `elements` begins
/// after the three-word record header; `workspace->elemCount` supplies 0..65535
/// elements and `elemStride` their stride in u32 words, at least seven. The
/// first three words pack three vertex then three normal u16 byte offsets.
/// Word 3 supplies the draw pass's material colour. Words 4 and 5 pack unsigned
/// byte U/V texel coordinates with encoded CLUT and texture-page settings;
/// word 6's low half supplies U2/V2 and its high half is ignored.
///
/// `primWrite` must address one writable, four-byte-aligned `POLY_GT3` slot per
/// element in the selected buffer half's second region. The workspace supplies
/// signed encoded-address displacements: `texturePageOffset` (-128..127) and
/// `encodedClutOffset` (-8192..8128, 64 per palette row). Sums wrap in the u16
/// packet fields. Drawing supplies positions, lit colours, length/code and
/// links later; construction preserves those fields and all packet pad fields.
///
/// Advances `primWrite` by one 40-byte packet per element and returns the cursor
/// advanced by the original count times the word stride. Consumes `elemCount`
/// to -1 even for an empty record. `objectFlags` is the shared callback argument,
/// passed as zero during construction and ignored here. Stream and packet
/// capacities are caller obligations; seven words is a minimum readable extent,
/// not a fixed element size. All storage is borrowed; no pointer is retained.
u32* tmdBuildStreamGt3ElemColor(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes persistent texture fields for a record of gouraud textured triangles with per-corner colours.
///
/// Packet construction selects this callback for opcode `0x130`. `elements`
/// starts after the three-word record header; `workspace->elemCount` supplies
/// 0..65535 elements and `elemStride` their stride in u32 words, at least nine.
/// The first three words pack vertex 0..2 then normal 0..2 as u16 byte offsets;
/// words 3..5 carry the corners' material colours for drawing. Words 6 and 7
/// pack unsigned byte U/V texel coordinates with encoded CLUT and texture-page
/// settings; word 8's low half packs U2/V2 and its high half is ignored.
///
/// `primWrite` must address one writable, four-byte-aligned `POLY_GT3` slot per
/// element in the selected buffer half's second region. The workspace supplies
/// signed encoded-address displacements: `texturePageOffset` (-128..127) and
/// `encodedClutOffset` (-8192..8128, 64 per palette row). Sums wrap in the u16
/// packet fields. Drawing supplies positions, lit colours, length/code and
/// links later; construction preserves those fields and the packet's pad fields.
///
/// Advances `primWrite` by one 40-byte packet per element and returns the cursor
/// advanced by the original count times the word stride. Consumes `elemCount`
/// to -1 even for an empty record. `objectFlags` is the shared callback argument,
/// passed as zero during construction and ignored here. Stream and packet
/// capacities are caller obligations. All storage is borrowed for this call;
/// no pointer is retained.
u32* tmdBuildStreamGt3CornerColors(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes persistent texture fields for Gouraud textured quads with one material colour per element.
///
/// Construction selects this callback for opcode `0x70`. `elements` begins
/// after the three-word record header; `workspace->elemCount` supplies 0..65535
/// elements and `elemStride` their stride in u32 words, at least eight. Words
/// 0..3 pack four vertex then four normal u16 byte offsets. Word 4 supplies
/// the draw pass's material colour. Words 5 and 6 pack unsigned byte U/V texel
/// coordinates with encoded CLUT and texture-page settings; word 7 packs U2/V2
/// in its low half and U3/V3 in its high half. Eight words is a minimum readable
/// extent, not a fixed element size. Construction does not access geometry or
/// the colour word.
///
/// `primWrite` must address one writable, four-byte-aligned `POLY_GT4` slot per
/// element in the selected buffer half's second region. The workspace supplies
/// signed encoded-address displacements: `texturePageOffset` (-128..127) and
/// `encodedClutOffset` (-8192..8128, 64 per palette row). Sums wrap modulo
/// 65536 in the u16 packet fields. Drawing supplies positions, lit colours,
/// length/code and links later; construction preserves those fields and all
/// packet pad fields.
///
/// Advances `primWrite` by one 52-byte packet per element and returns `elements`
/// advanced by the original count times the word stride, leaving the next
/// record or marker unconsumed. Consumes `elemCount` to -1 even for an empty
/// record, which reads no payload and advances neither cursor. Other workspace
/// fields remain unchanged. `objectFlags` is the shared callback argument,
/// passed as zero during construction and ignored here. Stream and packet
/// capacities are unchecked caller obligations. All storage is borrowed for
/// this call; no pointer is retained by this callback.
u32* tmdBuildStreamGt4ElemColor(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes persistent texture fields for Gouraud textured quads with one material colour per corner.
///
/// Construction selects this callback for opcode `0x170`. `elements` starts
/// after the three-word record header; `workspace->elemCount` supplies 0..65535
/// elements and `elemStride` their stride in u32 words, at least eleven. Words
/// 0..3 pack four vertex then four normal u16 byte offsets. Words 4..7 carry
/// the corners' material RGB and command bytes for drawing. Words 8 and 9 pack
/// unsigned byte U/V texel coordinates with encoded CLUT and texture-page settings; word 10
/// packs U2/V2 in its low half and U3/V3 in its high half. Eleven words is a
/// minimum readable extent, not a fixed element size. Construction does not
/// access geometry or the colour words.
///
/// `primWrite` must address one writable, four-byte-aligned `POLY_GT4` slot per
/// element in the selected buffer half's second region. The workspace supplies
/// signed encoded-address displacements: `texturePageOffset` (-128..127) and
/// `encodedClutOffset` (-8192..8128, 64 per palette row). Sums wrap modulo
/// 65536 in the u16 packet fields. Drawing supplies positions, lit colours,
/// length/code and links later; construction preserves those fields and all
/// packet pad fields.
///
/// Advances `primWrite` by one 52-byte packet per element and returns `elements`
/// advanced by the original count times the word stride, leaving the next
/// record or marker unconsumed. Consumes `elemCount` to -1 even for an empty
/// record, which reads no payload and advances neither cursor. Other workspace
/// fields remain unchanged. `objectFlags` is the shared callback argument,
/// passed as zero during construction and ignored here. Stream and packet
/// capacities are unchecked caller obligations. All storage is borrowed for
/// this call; no pointer is retained by this callback.
u32* tmdBuildStreamGt4CornerColors(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes persistent texture fields for textured triangles lit from one face normal.
///
/// Construction selects this callback for opcodes `0x18` and `0x1A`. `elements`
/// begins after the three-word record header. The caller supplies
/// `workspace->elemCount` (0..65535) and `elemStride` in u32 words, at least five
/// per element. Words 0/1 pack three vertex u16 byte offsets then one face-normal
/// offset for drawing; construction does not read them or access geometry.
/// Words 2/3 pack unsigned byte U/V texel coordinates with encoded CLUT and
/// texture-page settings. Only word 4's low half supplies U2/V2; its high half
/// is ignored. Five words is a minimum readable extent, not a fixed element size.
///
/// `primWrite` must address one writable, four-byte-aligned, 40-byte `POLY_GT3`
/// slot per element in the selected buffer half's second region. The workspace
/// supplies signed encoded-address displacements: `texturePageOffset`
/// (-128..127) and `encodedClutOffset` (-8192..8128, 64 per palette row). Sums
/// wrap modulo 65536 in the u16 packet fields. Drawing supplies positions, lit
/// colours, length/code and links later; construction preserves those fields
/// and all packet pad fields, including the halfword after U2/V2.
///
/// Advances `primWrite` by one packet per element and returns `elements`
/// advanced by the original count times the word stride, leaving the next
/// record or marker unconsumed. Consumes `elemCount` to -1 even for an empty
/// record, which reads no payload and advances neither cursor. The first-region
/// cursor and other workspace fields remain unchanged. `objectFlags` is the
/// shared callback argument, passed as zero during construction and ignored
/// here. Stream and packet capacities are unchecked caller obligations. All
/// storage is borrowed for the call; no pointer is retained by this callback.
u32* tmdBuildStreamGt3OneNormal(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes persistent texture fields for one-normal Gouraud textured quads.
///
/// `tmdBuildBufferHalf` selects this construction callback for `0x58`/`0x5A`.
/// `elements` starts after the three-word record header. Words 0..1 pack four
/// vertex byte references; the whole of word 2 is the face normal's unsigned
/// byte offset into the normal array. Those references are not read here.
/// Words 3 and 4 pack unsigned byte U/V texel coordinates with encoded
/// CLUT and page settings; word 5 packs U2/V2 and U3/V3 in its low/high halves.
/// `elemStride` counts u32 words and must be at least six for a nonempty record.
/// This minimum readable extent does not establish a fixed element size.
///
/// `primWrite` must provide one writable, word-aligned 52-byte POLY_GT4 slot
/// per element within the selected half's second region. Adds workspace
/// `texturePageOffset` (-128..127 encoded units) and `encodedClutOffset`
/// (-8192..8128, 64 per palette row), wrapping both address sums to u16.
/// Drawing supplies positions, face-normal lighting, length/code and links.
/// Construction preserves those fields and all SDK pads beside UV pairs.
///
/// With initial `elemCount` 0..65535, advances `primWrite` by one packet per
/// element and returns `elements + initial count * elemStride`, leaving the
/// next record or marker unconsumed. Consumes the count to -1 even for an empty
/// record, which reads no payload and advances neither cursor. `preXformWrite`
/// is unchanged. `objectFlags` is passed as zero and ignored. The stream must
/// cover every full stride and the packet region every slot; capacities are
/// unchecked. All storage is borrowed; no pointer is retained.
u32* tmdBuildStreamGt4OneNormal(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes unlit Gouraud textured quads with element colours and fixed semitransparency.
///
/// Construction selects this callback for `0x156`. `elements` begins after
/// the three-word record header. Words 0..1 pack four vertex references for
/// drawing. Words 2..5 copy all four corner RGB/pad words; corner 0's high byte
/// is then replaced with command 0x3E, enabling semitransparent colour-modulated
/// GT4 drawing independently of object flags. The packet length is twelve
/// payload words. Words 6 and 7 pack U0/V0 with CLUT and U1/V1 with page
/// settings; word 8 packs U2/V2 and U3/V3 in its low/high halves. UV coordinates
/// are unsigned texel bytes. No normals or lighting are used.
///
/// `primWrite` must provide one writable, word-aligned 52-byte POLY_GT4 slot
/// per element within the selected half's second region. Copied GPU addresses
/// receive signed workspace `texturePageOffset` (-128..127 encoded units) and
/// `encodedClutOffset` (-8192..8128, 64 per palette row), with u16 wrapping.
/// UV pad halfwords and screen XY persist. Drawing projects/culls the vertices
/// and links accepted packets, leaving the prebuilt colours, length and code.
///
/// The caller supplies `elemCount` (0..65535) and `elemStride` in u32 words,
/// at least nine for a nonempty record. Every full stride must be readable and
/// every packet slot writable; bounds are unchecked. Advances `primWrite` by
/// one packet per element and returns `elements + initial count * elemStride`,
/// leaving the following record/marker unconsumed. Consumes the count to -1
/// even for an empty record, which reads no payload and advances neither cursor.
/// `preXformWrite` is unchanged; `objectFlags` is passed as zero and ignored.
/// All storage is borrowed; no pointer is retained by the callback.
u32* tmdBuildStreamGt4Unlit(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes persistent texture fields for flat textured-triangle stream records.
///
/// `tmdBuildBufferHalf` selects this construction callback for `0x1C` and `0x1E`.
/// Each element reserves one `POLY_FT3` at `workspace->primWrite` in the selected
/// buffer half's second region. Drawing supplies the positions, packet length,
/// raw-texture command and ordering-table link; construction sets no colour.
/// `objectFlags` is the shared callback argument and is ignored here.
///
/// `elements` points past the three-word record header. Element words 2 and 3
/// pack unsigned byte U/V coordinates with encoded CLUT and texture-page words;
/// word 4's low half packs the third U/V pair. The signed encoded-page and CLUT
/// displacements in the workspace are added to the copied u16 fields, wrapping
/// to 16 bits. A CLUT displacement of 64 moves one palette row.
///
/// The initial `elemCount` is 0..65535 and `elemStride` is in u32 words, at least
/// five for nonempty records. The payload must cover every element stride, and
/// the four-byte-aligned destination must have room for `elemCount` packets
/// within its region. Both are borrowed for the call. Advances `primWrite` by
/// that many packets, leaves `elemCount` at -1 even for an empty record, and
/// returns the word after the payload without consuming a terminator.
u32* modelLightingStreamPrimFt3(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes persistent texture fields for flat textured-quad stream records.
///
/// `tmdBuildBufferHalf` selects this construction callback for `0x5C` and `0x5E`.
/// Each element reserves one `POLY_FT4` at `workspace->primWrite` in the selected
/// buffer half's second region. Drawing supplies screen positions, packet
/// length, the raw-texture command (opaque or semi-transparent) and the
/// ordering-table link. Construction sets no colour or packet pad fields.
/// `objectFlags` is the shared callback argument and is ignored here.
///
/// `elements` points past the three-word record header. Its first two words
/// carry four vertex references. Words 2 and 3 pack unsigned byte U/V texel
/// coordinates with encoded CLUT and texture-page settings; word 4 packs the
/// third and fourth U/V pairs in its low and high halves. The workspace's
/// signed encoded-page and CLUT displacements are added to the copied u16
/// fields, wrapping to 16 bits. One CLUT row contributes 64 encoded units.
///
/// The initial `elemCount` is 0..65535; `elemStride` is in u32 words and must be
/// at least five for nonempty records. The payload must cover every element
/// stride, and the four-byte-aligned destination must have room for `elemCount`
/// packets within its region. Workspace, payload and packets are borrowed for
/// the call. Advances `primWrite` by that many packets, leaves `elemCount` at
/// -1 even for an empty record, and returns the word after the payload without
/// consuming a terminator.
u32* modelLightingStreamPrimFt4(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Handler of a stream's flat-quad records (`0x44`): each element contributes
/// one untextured, unlit quad to the buffer half's second region, with the
/// element's colour word written into it.
///
/// The record is not pre-transformed, so its quad is built in the region the
/// draw pass transforms. This command writes the packet's fixed fields: its
/// length (5 words after the tag), the element's colour and the opaque
/// flat-quad code (`0x28`). The colour is the element's third word and
/// includes the command byte, so the code is stored after it. The draw pass
/// reads four vertex references from the element's leading halfwords, and
/// writes the length and code again on each quad it links. The linked quad
/// keeps this colour.
///
/// `primWrite` advances by one quad per element. The record has no variant
/// for `flags` to select, and packet construction passes zero, so `flags` goes
/// unread. The returned cursor is the stream advanced by one element stride per
/// element.
u32* modelLightingStreamPrimF4(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Handler of a stream's flat-triangle records (`0x4`): each element contributes
/// one untextured, unlit triangle to the buffer half's second region, with the
/// element's colour word written into it.
///
/// The record is not pre-transformed, so its triangle is built in the region the
/// draw pass transforms. This command writes the packet's fixed fields: its
/// length (4 words after the tag), the element's colour and the opaque
/// flat-triangle code (`0x20`). The colour is the element's third word and
/// includes the command byte, so the code is stored after it. The draw pass
/// reads three vertex references from the element's leading halfwords, and
/// writes the length and code again on each triangle it links. The linked
/// triangle keeps this colour.
///
/// `primWrite` advances by one triangle per element. The record has no variant
/// for `flags` to select, and packet construction passes zero, so `flags` goes
/// unread. The returned cursor is the stream advanced by one element stride per
/// element.
u32* modelLightingStreamPrimF3(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Initializes both textures in each layered Gouraud triangle packet pair.
///
/// `tmdBuildBufferHalf` selects this construction callback for `0x4038` in stage
/// 2, areas 15 and 16. `elements` starts after the three-word record header.
/// Words 0..2 pack three vertex and three normal references; words 3 and 4
/// pack unsigned byte U/V texel coordinates with encoded CLUT and texture-page
/// settings. Word 5's low half supplies U2/V2; its high half is ignored.
///
/// The first packet receives the element's texture fields plus the object's
/// independent `layerTexturePageOffset` (-128..127 encoded units) and signed
/// `layerClutRowOffset` (-128..127 palette rows, 64 encoded units per row).
/// The relocated page's ABR bit 5 is then set, preserving bit 6 and selecting
/// mode 1 or 3. The second packet instead adds the workspace's base
/// `texturePageOffset` (-128..127) and `encodedClutOffset` (-8192..8128).
/// All address sums wrap to u16; the base offsets are never added to the layer.
/// Drawing supplies the layer's semi-transparent command and the base's opaque
/// command, along with both packets' tags, positions and colours. Construction
/// preserves those fields and the SDK pad fields.
///
/// The caller supplies `elemCount` (0..65535) and `elemStride` in u32 words,
/// at least six for nonempty records, with every full stride readable.
/// `primWrite` must have two writable, four-byte-aligned `POLY_GT3` slots per
/// element within the selected buffer half's second region. Capacities are
/// unchecked. Workspace, object, payload and packet storage are borrowed for
/// the call; no pointer is retained. `objectFlags` is the shared callback
/// argument, passed as zero during construction and ignored here.
///
/// Advances `primWrite` by two 40-byte packets per element and returns
/// `elements + initial elemCount * elemStride`, leaving the next record or
/// terminator unconsumed. The count is consumed to -1 even for an empty record;
/// an empty record reads no element or object fields and advances neither cursor.
u32* tmdBuildStreamGt3OffsetLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes the opaque base texture in each layered gouraud triangle pair.
///
/// Construction selects this callback for `0x4038` outside the areas that use
/// `tmdBuildStreamGt3OffsetLayer`. `elements` starts after the three-word record
/// header. The caller supplies `workspace->elemCount` (0..65535) and `elemStride`
/// in u32 words, at least six per element. The first three words pack three
/// vertex and three normal references; words 3 and 4 pack unsigned byte U/V
/// texel coordinates with encoded CLUT and texture-page settings. Word 5's low
/// half packs U2/V2; its high half is ignored.
///
/// `primWrite` must address two writable, four-byte-aligned `POLY_GT3` slots per
/// element in the selected buffer half's second region. The first slot is left
/// untouched for the draw pass's semi-transparent environment layer. Only the
/// second slot's texture fields are initialized here: page and CLUT sums wrap
/// in their u16 fields after adding the workspace's signed encoded displacements,
/// `texturePageOffset` (-128..127) and `encodedClutOffset` (-8192..8128, 64 per
/// palette row). Its tag, colours/command, positions and pad fields are preserved
/// for drawing.
///
/// Advances `primWrite` by two 40-byte packets per element and returns the cursor
/// advanced by the initial `elemCount * elemStride` words, leaving any terminator
/// unconsumed. The count is consumed to -1 even for an empty record. `objectFlags`
/// is the shared callback argument, passed as zero during construction and ignored
/// here. The caller must provide the full element strides and packet capacity;
/// the workspace and storage are borrowed, with no pointer retained.
u32* tmdBuildStreamGt3LayeredBase(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes both textures in each layered Gouraud quad packet pair.
///
/// `tmdBuildBufferHalf` selects this construction callback for `0x4078` in stage
/// 2, areas 15 and 16. `elements` starts after the three-word record header.
/// Words 0..3 pack four vertex and four normal references; this callback does
/// not read them. Words 4 and 5 pack unsigned byte U/V texel coordinates with
/// encoded CLUT and texture-page settings. Word 6 packs U2/V2 in its low half
/// and U3/V3 in its high half.
///
/// The first packet receives the element's texture fields plus the object's
/// independent `layerTexturePageOffset` (-128..127 encoded units) and signed
/// `layerClutRowOffset` (-128..127 palette rows, 64 encoded units per row).
/// The relocated page's ABR bit 5 is then set, preserving bit 6 and selecting
/// mode 1 or 3. The second packet instead adds the workspace's base
/// `texturePageOffset` (-128..127) and `encodedClutOffset` (-8192..8128).
/// All address sums wrap to u16; the base offsets are never added to the layer.
/// Drawing supplies the layer's semi-transparent command and the base's opaque
/// command, along with both packets' tags, positions and colours. Construction
/// preserves those fields and the SDK pad fields.
///
/// The caller supplies `elemCount` (0..65535) and `elemStride` in u32 words,
/// at least seven for nonempty records, with every full stride readable.
/// `primWrite` must have two writable, four-byte-aligned `POLY_GT4` slots per
/// element within the selected buffer half's second region. Capacities are
/// unchecked. Workspace, object, payload and packet storage are borrowed for
/// the call; no pointer is retained. `objectFlags` is the shared callback
/// argument, passed as zero during construction and ignored here.
///
/// Advances `primWrite` by two 52-byte packets per element and returns
/// `elements + initial elemCount * elemStride`, leaving the next record or
/// terminator unconsumed. The count is consumed to -1 even for an empty record;
/// an empty record reads no element or object fields and advances neither cursor.
u32* tmdBuildStreamGt4OffsetLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes the opaque base texture in each layered Gouraud quad packet pair.
///
/// `tmdBuildBufferHalf` selects this construction callback for `0x4078` outside
/// stage 2, areas 15 and 16, which use `tmdBuildStreamGt4OffsetLayer` instead.
/// `elements` starts after the three-word record header. Words 0..3 pack four
/// vertex and four normal byte-offset references used during drawing; this
/// callback does not read them. Words 4 and 5 pack unsigned byte U/V texel
/// coordinates with encoded CLUT and texture-page settings. Word 6 packs U2/V2
/// in its low half and U3/V3 in its high half.
///
/// `primWrite` addresses two writable, four-byte-aligned `POLY_GT4` slots per
/// element in the selected buffer half's second region. The first slot is left
/// untouched for drawing to supply the semi-transparent environment layer's
/// texture. Only the second slot's texture fields are initialized here: the
/// page and CLUT sums wrap to u16 after adding the workspace's signed encoded
/// displacements, `texturePageOffset` (-128..127) and `encodedClutOffset`
/// (-8192..8128, 64 per palette row). Its tag, colours/command, screen positions
/// and SDK pad fields are preserved for drawing.
///
/// The caller supplies `elemCount` (0..65535), `elemStride` in u32 words (at
/// least seven for nonempty records), the full readable element strides and
/// packet capacity; capacities are unchecked. Workspace, payload and packet
/// storage are borrowed for the call; no storage is allocated. `objectFlags`
/// is the shared callback argument, passed as zero during construction and
/// ignored here.
///
/// Advances `primWrite` by two 52-byte packets per element and returns
/// `elements + initial elemCount * elemStride`, leaving the next record or
/// terminator unconsumed. The count is consumed to -1 even for an empty record;
/// an empty record reads no payload and advances neither cursor.
u32* tmdBuildStreamGt4LayeredBase(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes texture fields in pre-transformed environment/base triangle pairs.
///
/// `tmdBuildBufferHalf` selects this construction callback for `0x4039` outside
/// stage 2, areas 15 and 16. `elements` starts after the three-word record header.
/// Words 0..1 contain three u16 depth-cache references used during drawing;
/// their fourth halfword is ignored. Words 2 and 3 pack unsigned byte U/V texel
/// coordinates with encoded CLUT and texture-page settings. Word 4's low half
/// supplies U2/V2; its high half is ignored. These references are not read here.
///
/// The first packet is the environment layer, seeded with a 4-bit additive page
/// at VRAM word coordinates (960,256) and a CLUT at (256,240). Projection supplies
/// its U/V, positions and colours. `tmdDrawStreamPrimGt3PreXformEnvLayer` replaces
/// the page with a direct-colour environment page, where the retained CLUT is
/// unused. The second packet is the opaque base: its element texture receives
/// `texturePageOffset` (-128..127 encoded units) and `encodedClutOffset`
/// (-8192..8128, 64 per palette row). Address sums wrap to u16. Tags, commands,
/// colours, screen positions and SDK pad fields are preserved for drawing.
///
/// The caller supplies `elemCount` (0..65535) and `elemStride` in u32 words,
/// at least five for nonempty records, with every full stride readable.
/// `preXformWrite` must have two writable, four-byte-aligned `POLY_GT3` slots
/// per element within the selected buffer half's first region. Capacities are
/// unchecked. Workspace, payload and packet storage are borrowed for the call;
/// no pointer is retained. `objectFlags` is the shared callback argument,
/// passed as zero during construction and ignored here.
///
/// Advances `preXformWrite` by two 40-byte packets per element and returns
/// `elements + initial elemCount * elemStride`, leaving the next record or
/// terminator unconsumed. The count is consumed to -1 even for an empty record;
/// an empty record reads no payload and advances neither cursor.
u32* tmdBuildStreamGt3PreXformEnvLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes texture fields in pre-transformed environment/base quad pairs.
///
/// Construction selects this callback for `0x4079` outside stage 2, areas
/// 15 and 16. `elements` starts after the three-word record header. Words 0..1
/// pack four depth-cache byte references for drawing. Words 2 and 3 pack U0/V0
/// with encoded CLUT and U1/V1 with page settings; word 4 packs U2/V2 in its
/// low half and U3/V3 in its high half. UV coordinates are unsigned texel bytes.
/// References are not read here. `elemStride` counts u32 words, at least five
/// for a nonempty record; every full stride must be readable.
///
/// `preXformWrite` must provide two writable, word-aligned 52-byte POLY_GT4
/// slots per element in the selected half's first region. The first is the
/// environment layer: seeds a 4-bit additive page at VRAM word coordinates
/// (960,256) and CLUT at (256,240). Projection supplies its positions, colours
/// and normal-derived UVs later. The environment drawer replaces its page with
/// a direct-colour page, which ignores the retained CLUT. The second is the
/// opaque base: receives element texture data plus workspace `texturePageOffset`
/// (-128..127 encoded units) and `encodedClutOffset` (-8192..8128, 64 per row).
/// Address sums wrap to u16. Tags, length/code, colours, XY and SDK pads persist
/// for projection and drawing; construction performs neither phase.
///
/// With initial `elemCount` 0..65535, advances `preXformWrite` by two packets
/// per element and returns `elements + initial count * elemStride`, leaving
/// the next marker unconsumed. Consumes the count to -1 even for an empty record,
/// which reads no payload and advances neither cursor. `primWrite` is unchanged.
/// `objectFlags` is passed as zero and ignored. Capacities are unchecked. All
/// storage is borrowed for the call; no pointer is retained.
u32* tmdBuildStreamGt4PreXformEnvLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes texture fields in pre-transformed offset-layer/base triangle pairs.
///
/// `tmdBuildBufferHalf` selects this construction callback for opcode `0x4039`
/// in stage 2, areas 15 and 16. `elements` starts after the three-word header.
/// Words 0..1 contain three u16 depth-cache references for drawing and an
/// ignored high half; none are read here. Words 2 and 3 pack unsigned byte
/// U/V texel coordinates with encoded CLUT and texture-page settings. Only
/// word 4's low half supplies U2/V2; its high half is ignored.
///
/// Both packets receive the same element texture. The first is the blended
/// layer: it adds the object's `layerTexturePageOffset` (-128..127 encoded
/// units) and `layerClutRowOffset` (-128..127 palette rows, 64 units per row).
/// Page relocation wraps to u16 before ABR bit 5 is set; bit 6 is preserved,
/// yielding mode 1 or 3. The second is the opaque base: it independently adds
/// `texturePageOffset` (-128..127) and `encodedClutOffset` (-8192..8128).
/// CLUT sums also wrap to u16. Tags, commands, colours, screen positions and
/// SDK pad fields are preserved for the projection and draw passes.
///
/// The caller supplies `elemCount` (0..65535) and `elemStride` in u32 words,
/// at least five for nonempty records, with count * stride words readable.
/// `preXformWrite` must provide two writable, four-byte-aligned `POLY_GT3`
/// slots per element within the selected buffer half's first region; capacity
/// is unchecked. Workspace, live object, payload and packet storage are
/// borrowed for the call; no pointer is retained. `objectFlags` is the shared
/// callback argument, passed as zero during construction and ignored here.
///
/// Advances `preXformWrite` by two 40-byte packets per element and returns
/// `elements + initial elemCount * elemStride`, leaving the following record
/// or terminator unconsumed. Consumes the count to -1 even for an empty record,
/// which reads no payload and advances neither cursor.
u32* tmdBuildStreamGt3PreXformOffsetLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Initializes texture fields in pre-transformed offset-layer/base quad pairs.
///
/// Construction selects this callback for `0x4079` in stage 2, areas 15 and
/// 16. `elements` starts after the three-word record header. Words 0..1 pack
/// four depth-cache byte references for drawing; none are read here. Words 2
/// and 3 pack U0/V0 with CLUT and U1/V1 with page settings; word 4 packs U2/V2
/// and U3/V3 in its low/high halves. UV coordinates are unsigned texel bytes.
/// Both packets receive that texture suffix, with independent GPU addresses.
///
/// The first packet is the blended layer: adds the live object's signed
/// `layerTexturePageOffset` (-128..127 encoded units) and `layerClutRowOffset`
/// (-128..127 palette rows, 64 encoded CLUT units per row). Page relocation
/// wraps to u16 before ABR bit 5 is set; bit 6 is preserved, yielding mode 1 or
/// 3. Drawing supplies the semitransparent command separately. The second is
/// the opaque base: independently adds workspace `texturePageOffset`
/// (-128..127) and `encodedClutOffset` (-8192..8128). CLUT sums wrap to u16.
/// Tags, length/code, colours, screen XY and SDK pads persist for later phases.
///
/// The caller supplies `elemCount` (0..65535) and `elemStride` in u32 words,
/// at least five for a nonempty record, with every full stride readable.
/// `preXformWrite` must provide two writable, word-aligned 52-byte POLY_GT4
/// slots per element in the selected half's first region. Advances that cursor
/// by two packets per element and returns `elements + initial count * elemStride`,
/// leaving the next record/marker unconsumed. Consumes the count to -1 even for
/// an empty record, which reads no payload and advances neither cursor.
/// `primWrite` is unchanged; `objectFlags` is passed as zero and ignored.
/// Capacities are unchecked. Workspace, live object, stream and packet storage
/// are borrowed for the call; no pointer is retained.
u32* tmdBuildStreamGt4PreXformOffsetLayer(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Reserves one untextured Gouraud quad packet per element during TMD buffer construction.
///
/// The construction walk selects this handler for `0x40`, `0x60`, `0x160`,
/// `0x4040`, `0x4060` and `0x4160`. It advances `workspace->primWrite` by one
/// `POLY_G4` per element in the selected buffer half's second region, including
/// opcodes carrying `0x4000`. It reads and writes no payload or packet data;
/// positions, colours and ordering-table links are completed by the draw pass
/// for drawable records. Reserving every element keeps both passes aligned
/// even when drawing rejects a quad.
///
/// `elements` is the first payload word after the three-word record header.
/// The caller decodes `workspace->elemCount` and `workspace->elemStride` from
/// unsigned halfwords (0..65535); the stride counts u32 words. The borrowed
/// stream must contain count * stride payload words, and the word-aligned
/// primitive cursor must have room for count complete `POLY_G4` packets within
/// its region. Returns the word after the payload, leaving any following marker
/// unconsumed. The count ends at -1, including for an empty record. `objectFlags`
/// is unused; construction passes zero to satisfy the shared callback signature.
u32* modelLightingReserveStreamPrimG4(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

/// Reserves one untextured Gouraud triangle packet per element during TMD buffer construction.
///
/// The construction walk selects this handler for `0x0`, `0x20`, `0x120`,
/// `0x4000`, `0x4020` and `0x4120`. It advances `workspace->primWrite` by one
/// `POLY_G3` per element in the selected buffer half's second region. No payload
/// or packet data is read or written: positions, colours and ordering-table
/// links are completed by the draw pass for drawable records.
///
/// `elements` is the first payload word after the three-word record header.
/// The caller decodes `workspace->elemCount` and `workspace->elemStride` from
/// unsigned halfwords (0..65535); the stride counts u32 words. The borrowed
/// stream must contain count * stride payload words, and the word-aligned
/// primitive cursor must have room for count complete `POLY_G3` packets within
/// its region. Returns the word after the payload, leaving any following marker
/// unconsumed. The count ends at -1, including for an empty record; neither
/// cursor advances for that record. `objectFlags` is the shared callback argument,
/// passed as zero during construction and ignored here.
u32* modelLightingReserveStreamPrimG3(TmdStreamWorkspace* workspace, s32 objectFlags, u32* elements);

#endif // GAMEPLAY_MODEL_LIGHTING_H
