#ifndef GAMEPLAY_MODEL_LIGHTING_H
#define GAMEPLAY_MODEL_LIGHTING_H

#include "types.h"

#include "main/pad_types.h"
#include "main/tmd_types.h"

void Gp_ApplyPadReplay(s32 arg0, PadScratch* arg1);

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
/// both are the process pass's, taken from the element and the object's extra
/// page and CLUT offsets (`gpStreamPrimGt3OffsetLayer`). What a frame adds is the
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
/// extra page and CLUT offsets (`gpStreamPrimGt4OffsetLayer`, whose cursor this
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
/// biases its page and CLUT (`gpStreamPrimGt3ElemColor` steps over the colour word
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

u32* func_8009D388(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2);

u32* func_8009D518(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2);

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
/// for every element of the record by the process pass (`gpStreamPrimG4`), whose
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

/// Handler of a stream's pre-transformed textured-triangle records (`0x31`,
/// `0x39`, `0x3B`, `0x131`, `0x8039`): each element contributes one triangle to
/// the buffer half's first region, with the element's texture words written into
/// it.
///
/// The opcode says the triangle's vertices are already in screen space, so there
/// is no transform or cull for this command to do. It writes the polygon's
/// `u`/`v` fields, and adds the model's texture page and CLUT to the primitive's
/// own, which are stored relative to the model.
u32* gpStreamPrimGt3PreXform(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Handler of a stream's pre-transformed textured-quad records (`0x71`, `0x79`,
/// `0x7B`, `0x171`, `0x8079`): each element contributes one quad to the buffer
/// half's first region, with the element's texture words written into it.
///
/// The opcode says the quad's vertices are already in screen space, so there is
/// no transform or cull for this command to do. It writes the polygon's `u`/`v`
/// fields, and adds the model's texture page and CLUT to the primitive's own,
/// which are stored relative to the model.
u32* gpStreamPrimGt4PreXform(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Handler of a stream's pre-transformed flat-quad records (`0x45`): each
/// element contributes one untextured quad to the buffer half's first region,
/// with the element's colour word written into it.
///
/// Only the packet's fixed fields are written here — its length, its primitive
/// code and the element's colour. A pre-transformed quad's vertices come from the
/// stream's vertex commands, and the draw pass culls the quad and links it into
/// the order table (`gpDrawStreamPrimF4PreXform`), so neither is this command's
/// work.
u32* gpStreamPrimF4PreXform(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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

/// Handler of a stream's textured-triangle records (`0x38`, `0x3A`, `0x8038`,
/// `0x10038`, `0x1003A`, `0x20038`): each element contributes one triangle to
/// the buffer half's second region, with the element's texture words written into
/// it.
///
/// The record is not pre-transformed, so its triangle is built in the region the
/// draw pass transforms; this command writes only the polygon's `u`/`v` fields,
/// and adds the model's texture page and CLUT to the primitive's own, which are
/// stored relative to the model.
u32* gpStreamPrimGt3(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Handler of a stream's textured-quad records (`0x78`, `0x7A`, `0x8078`,
/// `0x10078`, `0x20078`): each element contributes one quad to the buffer half's
/// second region, with the element's texture words written into it.
///
/// The record is not pre-transformed, so its quad is built in the region the
/// draw pass transforms; this command writes only the polygon's `u`/`v` fields,
/// and adds the model's texture page and CLUT to the primitive's own, which are
/// stored relative to the model.
u32* gpStreamPrimGt4(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
u32* gpStreamPrimGt3ElemColor(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
u32* gpStreamPrimGt3CornerColors(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
u32* gpStreamPrimGt4ElemColor(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
u32* gpStreamPrimGt4CornerColors(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
u32* gpStreamPrimGt3OneNormal(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
u32* gpStreamPrimGt4OneNormal(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
u32* gpStreamPrimGt4Unlit(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
u32* gpStreamPrimFt3(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
u32* gpStreamPrimFt4(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Handler of a stream's flat-quad records (`0x44`): each element contributes one
/// untextured quad to the buffer half's second region, with the element's colour
/// word written into it.
///
/// The record is not pre-transformed, so its quad belongs to the region the draw
/// pass transforms: only the packet's fixed fields are written here — its length,
/// its primitive code and the element's colour.
u32* gpStreamPrimF4(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
u32* gpStreamPrimGt3OffsetLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
u32* gpStreamPrimGt3Base(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
u32* gpStreamPrimGt4OffsetLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
u32* gpStreamPrimGt4Base(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Handler of a stream's layered textured-triangle records (`0x4039`): each element
/// contributes two triangles to the buffer half's first region — the base the model
/// is drawn from, and the semi-transparent layer drawn over it.
///
/// The record is pre-transformed, so its triangles are already in screen space and
/// there is no transform or cull for this command to do. `0x4000` asks for two
/// primitives per element: the element's texture words go into the base, with the
/// model's texture page and CLUT added to the primitive's own, and the layer's page
/// and CLUT are written here as fixed values rather than from the object's extra
/// page and CLUT offsets. The pair is drawn by
/// `gpDrawStreamPrimGt3PreXformFixedLayer`, which settles the page the layer is
/// finally drawn from.
u32* gpStreamPrimGt3PreXformFixedLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
u32* gpStreamPrimGt4PreXformLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
u32* gpStreamPrimGt3PreXformOffsetLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
u32* gpStreamPrimGt4PreXformOffsetLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream);

/// Handler of a stream's untextured gouraud-quad records (`0x40`, `0x60`, `0x160`,
/// `0x4040`, `0x4060`, `0x4160`): each element is one `POLY_G4` in the buffer half's
/// second region.
///
/// Nothing of that quad is this command's to write: it is a gouraud primitive, so its
/// colours are lit per corner rather than taken from the element as a flat family's
/// are, and it is untextured, so there are no texture words either. The command's work
/// is to move both cursors on, which it must still do — the half is written by both
/// passes, so a record one of them skipped would put every primitive after it at the
/// wrong address in the other. What fills the room a `0x60` record reserves is the
/// draw pass's (`tmdDrawStreamPrimG4CornerNormals`), one quad per element.
u32* gpStreamPrimG4(TmdStreamWorkspace* ws, s32 flags, u32* stream);

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
u32* gpStreamPrimG3(TmdStreamWorkspace* ws, s32 flags, u32* stream);

#endif // GAMEPLAY_MODEL_LIGHTING_H
