// Photoshop ground truth for how document commands treat artboards (docs/artboards.md,
// "Feature interactions"). Run through ps-artboard-audit.ps1; prints one line per case.
// Builds its own documents and closes only those.
var sTID = function (s) { return stringIDToTypeID(s); };
var cTID = function (s) { return charIDToTypeID(s); };

function makeArtboard(name, left, top, right, bottom) {
  var desc = new ActionDescriptor();
  var ref = new ActionReference();
  ref.putClass(sTID("artboardSection"));
  desc.putReference(cTID("null"), ref);
  var rect = new ActionDescriptor();
  rect.putDouble(cTID("Top "), top);
  rect.putDouble(cTID("Left"), left);
  rect.putDouble(cTID("Btom"), bottom);
  rect.putDouble(cTID("Rght"), right);
  desc.putObject(sTID("artboardRect"), sTID("classFloatRect"), rect);
  executeAction(cTID("Mk  "), desc, DialogModes.NO);
  app.activeDocument.activeLayer.name = name;
  return app.activeDocument.activeLayer;
}

// A filled pixel layer covering [left, top, right, bottom] inside `parent` (a LayerSet) or
// at the root (parent null).
function addFilledLayer(name, parent, left, top, right, bottom, r, g, b) {
  var doc = app.activeDocument;
  var layer = doc.artLayers.add();
  layer.name = name;
  if (parent !== null) {
    layer.move(parent, ElementPlacement.INSIDE);
  }
  doc.activeLayer = layer;
  doc.selection.select([[left, top], [right, top], [right, bottom], [left, bottom]]);
  var color = new SolidColor();
  color.rgb.red = r; color.rgb.green = g; color.rgb.blue = b;
  doc.selection.fill(color);
  doc.selection.deselect();
  return layer;
}

function artboardRectOf(layer) {
  var ref = new ActionReference();
  ref.putIdentifier(cTID("Lyr "), layer.id);
  var desc = executeActionGet(ref);
  if (!desc.hasKey(sTID("artboardEnabled")) || !desc.getBoolean(sTID("artboardEnabled"))) {
    return null;
  }
  var rect = desc.getObjectValue(sTID("artboard")).getObjectValue(sTID("artboardRect"));
  return [rect.getDouble(cTID("Left")), rect.getDouble(cTID("Top ")), rect.getDouble(cTID("Rght")),
          rect.getDouble(cTID("Btom"))];
}

function boundsOf(layer) {
  var b = layer.bounds;
  return [b[0].as("px"), b[1].as("px"), b[2].as("px"), b[3].as("px")];
}

// One line: canvas size, then every top-level layer (artboard rect or bounds) and every
// child's bounds.
function describe(doc) {
  var parts = ["canvas=" + doc.width.as("px") + "x" + doc.height.as("px")];
  for (var i = doc.layers.length - 1; i >= 0; --i) {
    var layer = doc.layers[i];
    var rect = layer.typename === "LayerSet" ? artboardRectOf(layer) : null;
    if (rect !== null) {
      var children = [];
      for (var c = layer.layers.length - 1; c >= 0; --c) {
        children.push(layer.layers[c].name + boundsOf(layer.layers[c]).join(","));
      }
      parts.push("AB " + layer.name + "[" + rect.join(",") + "]{" + children.join(";") + "}");
    } else {
      parts.push((layer.typename === "LayerSet" ? "GRP " : "LYR ") + layer.name + "[" + boundsOf(layer).join(",") + "]");
    }
  }
  return parts.join(" | ");
}

function buildBase() {
  var doc = app.documents.add(1000, 600, 72, "patchy-artboard-audit", NewDocumentMode.RGB, DocumentFill.TRANSPARENT);
  var a = makeArtboard("A", 0, 0, 300, 300);
  addFilledLayer("A1", a, 20, 20, 120, 120, 0, 0, 255);
  var b = makeArtboard("B", 400, 0, 700, 250);
  addFilledLayer("B1", b, 420, 20, 520, 120, 255, 0, 0);
  // Photoshop refuses to paint outside every artboard ("does not overlap the canvas"), so
  // the loose layer is filled over A and moved out.
  var loose = addFilledLayer("Loose", null, 150, 150, 250, 250, 0, 160, 0);
  loose.translate(650, 250);
  return doc;
}

var lines = [];
function openDocumentNames() {
  var names = {};
  for (var i = 0; i < app.documents.length; ++i) names[app.documents[i].name] = true;
  return names;
}
function run(label, action) {
  var before = openDocumentNames();
  try {
    var doc = buildBase();
    lines.push(label + " BEFORE " + describe(doc));
    var note = action(doc);
    lines.push(label + " AFTER  " + describe(app.activeDocument) + (note ? " | note=" + note : ""));
  } catch (e) {
    lines.push(label + " ERROR " + e + " (line " + e.line + ")");
  }
  // Close only the documents this case opened; never one that was open before it.
  for (var i = app.documents.length - 1; i >= 0; --i) {
    if (!before[app.documents[i].name]) app.documents[i].close(SaveOptions.DONOTSAVECHANGES);
  }
}

app.displayDialogs = DialogModes.NO;
var savedUnits = app.preferences.rulerUnits;
app.preferences.rulerUnits = Units.PIXELS;
function layerNamed(doc, wanted) {
  var found = null;
  var walk = function (container) {
    for (var i = 0; i < container.layers.length && found === null; ++i) {
      var layer = container.layers[i];
      if (layer.name === wanted) found = layer;
      else if (layer.typename === "LayerSet") walk(layer);
    }
  };
  walk(doc);
  return found;
}
function docRef() {
  var r = new ActionReference();
  r.putEnumerated(cTID("Dcmn"), cTID("Ordn"), cTID("Trgt"));
  return r;
}
function where(layer) {
  return layer.name + " in " + layer.parent.name + " [" + boundsOf(layer).join(",") + "]";
}
var CASE_TABLE = {
  probe: function (doc) { return ""; },
  canvas_grow_topleft: function (doc) { doc.resizeCanvas(1200, 800, AnchorPosition.TOPLEFT); return ""; },
  canvas_grow_center: function (doc) { doc.resizeCanvas(1200, 800, AnchorPosition.MIDDLECENTER); return ""; },
  canvas_grow_bottomright: function (doc) { doc.resizeCanvas(1200, 800, AnchorPosition.BOTTOMRIGHT); return ""; },
  canvas_shrink_center: function (doc) { doc.resizeCanvas(800, 400, AnchorPosition.MIDDLECENTER); return ""; },
  image_size_half: function (doc) {
    doc.resizeImage(UnitValue(500, "px"), UnitValue(300, "px"), 72, ResampleMethod.BICUBIC);
    return "";
  },
  crop: function (doc) { doc.crop([100, 50, 600, 400]); return ""; },
  trim: function (doc) { doc.trim(TrimType.TRANSPARENT, true, true, true, true); return ""; },
  rotate_90: function (doc) { doc.rotateCanvas(90); return ""; },
  flip_horizontal: function (doc) {
    var d = new ActionDescriptor();
    d.putReference(cTID("null"), docRef());
    d.putEnumerated(cTID("Axis"), cTID("Ornt"), cTID("Hrzn"));
    executeAction(cTID("Flip"), d, DialogModes.NO);
    return "";
  },
  reveal_all: function (doc) { executeAction(sTID("revealAll"), undefined, DialogModes.NO); return ""; },
  flatten: function (doc) { doc.flatten(); return ""; },
  merge_visible: function (doc) { doc.mergeVisibleLayers(); return ""; },
  new_layer_with_B1_active: function (doc) {
    doc.activeLayer = layerNamed(doc, "B1");
    var d = new ActionDescriptor();
    var r = new ActionReference();
    r.putClass(cTID("Lyr "));
    d.putReference(cTID("null"), r);
    executeAction(cTID("Mk  "), d, DialogModes.NO);
    return "new " + where(doc.activeLayer);
  },
  new_layer_with_artboard_B_active: function (doc) {
    doc.activeLayer = layerNamed(doc, "B");
    var d = new ActionDescriptor();
    var r = new ActionReference();
    r.putClass(cTID("Lyr "));
    d.putReference(cTID("null"), r);
    executeAction(cTID("Mk  "), d, DialogModes.NO);
    return "new " + where(doc.activeLayer);
  },
  select_all_bounds: function (doc) {
    doc.activeLayer = layerNamed(doc, "A1");
    doc.selection.selectAll();
    var b = doc.selection.bounds;
    return "selection [" + b[0].as("px") + "," + b[1].as("px") + "," + b[2].as("px") + "," + b[3].as("px") + "]";
  },
  copy_paste_same_artboard: function (doc) {
    doc.activeLayer = layerNamed(doc, "A1");
    doc.selection.select([[20, 20], [120, 20], [120, 120], [20, 120]]);
    doc.selection.copy();
    doc.selection.deselect();
    doc.paste();
    return "pasted " + where(doc.activeLayer);
  },
  copy_from_A_paste_with_B1_active: function (doc) {
    doc.activeLayer = layerNamed(doc, "A1");
    doc.selection.select([[20, 20], [120, 20], [120, 120], [20, 120]]);
    doc.selection.copy();
    doc.selection.deselect();
    doc.activeLayer = layerNamed(doc, "B1");
    doc.paste();
    return "pasted " + where(doc.activeLayer);
  },
  paste_in_place_with_B1_active: function (doc) {
    doc.activeLayer = layerNamed(doc, "A1");
    doc.selection.select([[20, 20], [120, 20], [120, 120], [20, 120]]);
    doc.selection.copy();
    doc.selection.deselect();
    doc.activeLayer = layerNamed(doc, "B1");
    var d = new ActionDescriptor();
    d.putBoolean(sTID("inPlace"), true);
    d.putEnumerated(cTID("AntA"), cTID("Annt"), cTID("Anno"));
    executeAction(cTID("past"), d, DialogModes.NO);
    return "pasted " + where(doc.activeLayer);
  },
  duplicate_artboard_B: function (doc) {
    doc.activeLayer = layerNamed(doc, "B");
    var d = new ActionDescriptor();
    var r = new ActionReference();
    r.putEnumerated(cTID("Lyr "), cTID("Ordn"), cTID("Trgt"));
    d.putReference(cTID("null"), r);
    executeAction(cTID("Dplc"), d, DialogModes.NO);
    return "duplicate " + doc.activeLayer.name;
  },
  export_png: function (doc) {
    var file = new File(Folder.temp + "/patchy-artboard-audit-export.png");
    doc.saveAs(file, new PNGSaveOptions(), true);
    var opened = app.open(file);
    var size = opened.width.as("px") + "x" + opened.height.as("px");
    opened.close(SaveOptions.DONOTSAVECHANGES);
    file.remove();
    return "png " + size;
  }
};
var cases = typeof AUDIT_CASES !== "undefined" ? AUDIT_CASES : [];
if (cases.length === 0) {
  for (var key in CASE_TABLE) if (key !== "probe") cases.push(key);
}
for (var k = 0; k < cases.length; ++k) {
  var caseName = cases[k];
  if (CASE_TABLE[caseName] !== undefined) run(caseName, CASE_TABLE[caseName]);
  else lines.push(caseName + " UNKNOWN");
}
app.preferences.rulerUnits = savedUnits;
lines.join("\n");
