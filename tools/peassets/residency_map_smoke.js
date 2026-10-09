// Runs the residency map's script without a browser, under a stub document:
//   node tools/peassets/residency_map_smoke.js local/residency_map.html '[["shelter_b3_dumping_hole","view 3","variant 1"]]'
// Each triple is an overlay, a view and a variant; it prints the applied view's tiles and what is flagged.
// It catches a script that throws and lets the pairing be read as text; it does not check layout.
const fs = require("fs"), path = require("path");
const page = process.argv[2], html = fs.readFileSync(page, "utf8"), js = html.slice(html.indexOf("<script>") + 8, html.lastIndexOf("</script>"));
const els = new Map();
function node(tag) {
  const n = { tag, children: [], attrs: {}, style: {}, classList: { toggle() {}, add() {}, remove() {} }, hidden: false, value: "", textContent: "", className: "", checked: true,
    setAttribute(k, v) { this.attrs[k] = v; }, getAttribute(k) { return this.attrs[k]; }, addEventListener() {}, append(...c) { this.children.push(...c); for (const x of c) if (x && x.tag === "script" && x.src) { try { eval(fs.readFileSync(path.join(path.dirname(page), x.src), "utf8")); } catch (e) { x.onerror && x.onerror(); } } },
    replaceChildren(...c) { this.children = c; }, querySelectorAll() { return []; }, remove() {},
    getContext() { return { fillRect() {}, createImageData: (w, h) => ({ data: new Uint8ClampedArray(w * h * 4) }), getImageData: () => ({ data: [200, 0, 0, 255] }), putImageData() {} }; } };
  Object.defineProperty(n, "innerHTML", { set(v) { this.children = (String(v).match(/<(tr|div)\b/g) || []).map(() => node("row")); const m = [...String(v).matchAll(/<option(?: value="[^"]*")?>([^<]*)/g)]; this.options = m.map(x => x[1]); }, get() { return ""; } });
  Object.defineProperty(n, "childNodes", { get() { return this.children; } });
  return n;
}
global.document = { getElementById: id => els.get(id) || (els.set(id, node("#" + id)), els.get(id)), createElementNS: (_, t) => node(t), createElement: t => node(t), head: node("head"), documentElement: node("html") };
global.window = global; global.location = { hash: "" }; global.history = { replaceState() {} }; global.getComputedStyle = () => ({ getPropertyValue: () => "#f00" }); global.atob = s => Buffer.from(s, "base64").toString("binary");
eval(js + `
;(async () => {
  for (const [name, view, variant] of JSON.parse(process.argv[3])) {
    pick(name); document.getElementById("view").value = view; document.getElementById("variant").value = variant; side("applied"); await drawApplied();
    const tiles = document.getElementById("applied").children;
    console.log(name, view, variant, "->", tiles.length, "tiles,", tiles.filter(t => / miss/.test(t.className)).length, "flagged");
    for (const t of tiles.slice(0, 40)) { const txt = t.children.slice(1).map(c => c.textContent); if (/guessed/.test(txt[0]) || txt[3]) console.log("   ", txt.join(" | ").slice(0, 230)); }
  }
  tab("all"); console.log("matrix:", document.getElementById("allfacts").textContent);
})().catch(e => { console.error("FAILED", e); process.exit(1); });`);
