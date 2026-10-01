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

/// Draw-pass handler of a stream's pre-transformed untextured gouraud-triangle
/// records (`0x21`, `0x121`): each element contributes one `POLY_G3` to the
/// buffer half's first region, where its corners are already in screen space, and
/// links it into the ordering table at the depth those corners average to.
///
/// The record is the untextured gouraud triangle in its pre-transformed form
/// (`tmdDrawStreamPrimG3`, `tmdDrawStreamPrimG3CornerNormals`): the pass that
/// projects the stream's vertices (`tmdXformStreamVerts`) has already written
/// each corner's screen coordinates and lit colour into the packet this handler
/// files, and its depth into the per-vertex screen-Z table, so an element names
/// its three corners in that table rather than in the vertex array, and the
/// normals and colour the element would otherwise carry were consumed there.
/// Nothing in the packet is copied from the element: an untextured `POLY_G3` has
/// no texture word and no material colour the process pass could write, so these
/// records are absent from that pass's table and the whole packet is built per
/// frame. What is left to this handler is the triangle's filing — the three
/// cached depths averaged for the ordering-table link, the facing taken from the
/// coordinates the packet already carries, and the packet's length and primitive
/// code. An element with any corner depth marked `TMD_VERTEX_DEPTH_INVALID` by
/// the projection pre-pass, or whose triangle turns away, is stepped over rather
/// than drawn, though its packet slot is passed over either way, so the primitives
/// stay aligned with the elements that named them.
///
/// The blended primitive is an entry of its own (`Tmd_StreamHandler_Prim32`)
/// rather than a `flags` choice, so `flags` goes unread here.
u32* tmdDrawStreamPrimG3PreXform(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
/// was reserved for every element by the process pass (`modelLightingReserveStreamPrimG3`), whose
/// cursor this one stays in step with.
///
/// The record's `0x22` form resolves to this same body, and the handler reads no
/// `flags`: a semi-transparent variant is not this one's to select, because the
/// packet's code byte is the element's own — carried in its colour word and
/// taken to the packet by the lighting step.
u32* tmdDrawStreamPrimG3CornerNormals(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Draw handler of a stream's untextured gouraud-quad records (`0x60`): each
/// element is one `POLY_G4` in the buffer half's second region, built whole here
/// as the record is transformed.
///
/// The record is the `0x40` quad's with one normal per corner in place of the one
/// it lights the whole face from: the element names a vertex and a normal per
/// corner, and the one colour word the quad is lit from. The corners are
/// projected, and the quad is dropped where either projection raises a GTE error
/// or the facing tests reject it; what survives is lit corner by corner — three
/// corners in one lighting step and the fourth in a step of its own, so each
/// corner of the packet carries the colour its own normal gives — and it is
/// linked into the ordering table at the depth it came out at. A dropped quad
/// still consumes its packet's room, because the room was reserved for every
/// element by the process pass (`modelLightingReserveStreamPrimG4`), whose cursor this one stays in
/// step with.
///
/// The packet's primitive code is the top byte of that same colour word, which is
/// all the record's `0x62` form — the semi-transparent one — differs in. The two
/// share this body, so `flags` has no variant to select and goes unread.
u32* tmdDrawStreamPrimG4CornerNormals(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
u32* tmdDrawStreamGt3SemiTrans(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// The draw pass's handler for a stream's gouraud textured-quad records that ask
/// for the semi-transparent primitive (`0x7A`): each element contributes one quad,
/// taken to screen space and lit corner by corner, and the packet the build pass
/// laid out for it is completed and linked into the ordering table, unless the
/// transform clipped a corner or the facing test turned the quad away.
///
/// The element is the opaque `0x78` quad's — a vertex and a normal per corner, and
/// the same texture words — and the two entries share one body, so the primitive
/// code the packet is built under is the whole of the difference between the two
/// records: `0x3C` for the opaque quad and `0x3E` here, the semi-transparency bit
/// being the difference. The element names no colour, so the quad is lit from a
/// fixed mid-grey, and the same constant carries both, the code in its top byte.
/// The opcode alone settles the variant: this entry does not test the `flags` bit
/// the `0x78` one picks its code from.
u32* tmdDrawStreamGt4SemiTrans(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
/// process pass (`modelLightingReserveStreamPrimG3`), whose cursor this one stays in step with.
/// The record whose corners the vertex pass places instead is
/// `tmdDrawStreamPrimG3PreXform`'s, which files that same packet in the buffer
/// half's first region.
/// The record has no variant for `flags` to select, so it goes unread.
u32* tmdDrawStreamPrimG3(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Draw handler of a stream's untextured gouraud-quad records (`0x40`): each
/// element is one `POLY_G4` in the buffer half's second region, built whole here
/// as the record is transformed.
///
/// The element names the quad's four vertices, the one normal it is lit from and
/// the colour word whose top byte is the packet's primitive code. The fourth
/// corner is projected on its own, because it has to be taken out of the GTE's
/// coordinate stack before the shared transform of the other three overwrites
/// it; an element either transform raises an error on is dropped, its packet's
/// room passed over all the same, since the process pass (`modelLightingReserveStreamPrimG4`)
/// reserved that room for every element and this handler stays in step with its
/// cursor.
///
/// A quad is drawn where either of its halves faces the viewer: the triangle of
/// the first three corners is tested, and where that one turns away the fourth
/// corner is put in the last one's place and tested again. What survives is
/// ordered at the four corners' average depth, and the element's single normal
/// and colour word are lit in one step whose result, the code byte included,
/// colours all four corners alike. The record has no variant for `flags` to
/// select, so it goes unread.
u32* tmdDrawStreamPrimG4(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
