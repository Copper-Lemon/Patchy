// @name README shot: Object Selection
// @description Selects a vintage car in a busy Havana street with one loose box and the
// @description High Quality model, mutes the rest of the photo through the inverted
// @description selection, and captures the main window.
// @cli --script-arg out=object_select.png --script-arg photo=test-fixtures/readme/havana_vintage_car_cc0.jpg
//
// The box is the same input as one Object Selection drag. The High Quality model is
// downloaded first when missing (a one-time 234 MB download into the app-data folder), and
// the scene refuses to capture with the Standard model. The driver supplies isolated
// settings and 96-DPI geometry on the real windows platform.

var out = patchy.args.out;
var photo = patchy.args.photo;
if (!out) { throw new Error('pass --script-arg out=<output png path>'); }
if (!photo) { throw new Error('pass --script-arg photo=<Havana photo path>'); }

patchy.ui.setWindowSize(1600, 1000);
var doc = app.open(photo);
if (!doc) { throw new Error('open failed: ' + photo); }
app.documents.forEach(function (other) {
  if (other.id !== doc.id && !other.path && !other.modified) { other.close(); }
});
doc.activate();
patchy.ui.setSidePanelWidth(300);
if (!doc.selection.automaticSelectionAvailable) {
  throw new Error('this build has no automatic selection (models or ONNX Runtime missing)');
}
app.downloadObjectSelectionModel();
if (doc.selection.objectSelectionModel !== 'highQuality') {
  throw new Error('the High Quality model is not in use: ' + doc.selection.objectSelectionModel);
}

// The lower street (the photo is 3000 x 1975), so the car is not lost in the frame.
doc.crop(800, 750, 2000, 1225);
doc.activeLayer.name = 'Havana';

// One loose box around the car, in cropped-document pixels.
doc.selection.objectSelect({rect: {x: 620, y: 700, width: 800, height: 400}});
if (!doc.selection.exists) { throw new Error('object selection produced no selection'); }

// Mute everything else so the selected car reads at thumbnail size, then bring the object
// selection back for the marching ants. Both are layers filled through the inverted
// selection, so the photo is untouched.
var photoLayer = doc.activeLayer;
app.runCommand('select.inverse');
var gray = doc.addLayer('Desaturate the rest');
gray.fill('#808080');
gray.blendMode = 'saturation';
var dim = doc.addLayer('Dim the rest');
dim.fill('#000000');
dim.opacity = 45;
doc.activeLayer = photoLayer;
app.runCommand('select.inverse');

app.runCommand('tools.object_select');

// Fit and capture on timer turns so the resize/dock layout settles between steps.
setTimeout(function () {
  patchy.ui.fitOnScreen();
  setTimeout(function () {
    patchy.ui.setStatusMessage('Ready');
    if (!patchy.ui.captureWindow(out)) {
      throw new Error('captureWindow failed: ' + out);
    }
    console.log('captured ' + out);
  }, 250);
}, 250);
