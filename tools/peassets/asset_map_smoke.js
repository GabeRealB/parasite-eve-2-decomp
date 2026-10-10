// Runs the asset map's script without a browser, under a stub document:
//   node tools/peassets/asset_map_smoke.js local/asset_map.html o.gameplay o.shelter_b3_dumping_hole a.pe2img_282
// Each argument is a subject as the page's address names it: o.<overlay>, a.<asset>, d.<stage>/<folder>, f.<file index> or n. (the proposed names). It prints the subject's
// sections with their tiles and what is flagged. It catches a script that throws and lets the content be read as
// text; it does not check layout.
const fs = require("fs"), path = require("path");
const page = process.argv[2], html = fs.readFileSync(page, "utf8"), js = html.slice(html.indexOf("<script>") + 8, html.lastIndexOf("</script>"));
const els = new Map();
function node(tag) {
  const n = { tag, children: [], attrs: {}, style: { setProperty() {} }, classList: { toggle() {}, add() {}, remove() {} }, hidden: false, value: "", className: "", checked: true, _text: "",
    setAttribute(k, v) { this.attrs[k] = v; if (k === "value" && this.tag !== "option") this.value = v; }, getAttribute(k) { return this.attrs[k]; }, addEventListener(type, f) { (this.on = this.on || {})[type] = f; }, scrollTo() {},
    append(...c) { for (const x of c) { this.children.push(x); if (x && x.tag === "script" && x.src) { try { eval(fs.readFileSync(path.join(path.dirname(page), x.src), "utf8")); } catch (e) { x.onerror && x.onerror(); } } } },
    replaceChildren(...c) { this.children = c; }, querySelectorAll() { return []; }, remove() {},
    getContext() { return { fillRect() {}, strokeRect() {}, clearRect() {}, save() {}, restore() {}, clip() {}, arc() {}, setTransform() {}, drawImage() {}, beginPath() {}, moveTo() {}, lineTo() {}, closePath() {}, fill() {}, stroke() {}, createImageData: (w, h) => ({ data: new Uint8ClampedArray(w * h * 4) }), getImageData: (x, y, w, h) => ({ data: w > 1 ? new Uint8ClampedArray(w * h * 4).fill(255) : [200, 0, 0, 255] }), putImageData() {} }; } };
  Object.defineProperty(n, "textContent", { get() { return this._text + this.children.map(c => typeof c === "string" ? c : c && c.textContent || "").join(""); }, set(v) { this._text = String(v); this.children = []; } });
  Object.defineProperty(n, "lastChild", { get() { return this.children[this.children.length - 1]; } });
  return n;
}
global.document = { getElementById: id => els.get(id) || (els.set(id, node("#" + id)), els.get(id)), createElementNS: (_, t) => node(t), createElement: t => node(t), head: node("head"), documentElement: node("html"), querySelector: () => node("main") };
global.localStorage = { getItem: () => null, setItem() {} }; global.window = global; global.location = { hash: "" }; global.addEventListener = () => {}; global.getComputedStyle = () => ({ getPropertyValue: () => "#f00" }); global.atob = s => Buffer.from(s, "base64").toString("binary");
const walk = (n, f) => { if (!n || typeof n === "string") return; f(n); (n.children || []).forEach(c => walk(c, f)); };
eval(js + `
;(async () => {
  for (const subject of process.argv.slice(3)) {
    const [kind, ...rest] = subject.split("."), name = rest.join(".");
    await (kind === "a" ? showAsset(name) : kind === "f" || kind === "d" ? showFiles(name) : kind === "n" ? showNames() : kind === "g" ? showGroups(name) : showOverlay(name));
    const out = [];
    for (const s of document.getElementById("subject").children) { if (s.tag !== "section") continue; let tiles = 0, flagged = 0, rows = 0, shots = 0; const flags = [];
      walk(s, n => { if (/^tile( |$)/.test(n.className)) { tiles++; if (/miss/.test(n.className)) { flagged++; flags.push(n.textContent.slice(0, 150)); } } if (n.tag === "tr" || n.tag === "li") rows++; if (n.className === "shot") shots++; });
      out.push("  " + s.children[0].textContent + (tiles ? ": " + tiles + " tiles, " + flagged + " flagged" : "") + (shots ? ": " + shots + " shown" : "") + (rows ? ": " + rows + " rows" : "")); flags.slice(0, 3).forEach(f => out.push("      " + f)); }
    console.log(subject + "\\n" + out.join("\\n"));
  }
  navMode = "o"; drawNav(); let links = 0; walk(document.getElementById("list"), n => { if (n.tag === "a") links++; }); console.log("navigator: " + links + " overlay links");
  navMode = "f"; drawNav(); links = 0; walk(document.getElementById("list"), n => { if (n.tag === "a") links++; }); console.log("navigator: " + links + " file links");
  navMode = "a"; drawNav(); links = 0; walk(document.getElementById("list"), n => { if (n.tag === "a") links++; }); console.log("navigator: " + links + " asset links");
})().catch(e => { console.error("FAILED", e); process.exit(1); });`);
