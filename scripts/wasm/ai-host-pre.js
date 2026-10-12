// Injected via --pre-js into the threaded wasm app when it is built with automatic
// selection (cmake/patchy_ai.cmake). The page side of src/ai/segmentation_engine_web.cpp:
// it runs the MobileSAM models in onnxruntime-web, which it imports from the same origin
// on first use and runs in ORT's own proxy worker and heap. docs/ai-selection.md
// ("WebAssembly") describes the protocol.
//
// Rules this file must keep:
// - Never call into wasm: no exports, no ccall, no dynCall (docs/wasm.md: a raw JS-to-wasm
//   call can clobber the Asyncify state of a suspended main loop). The host only reads and
//   writes the shared heap and notifies; C++ workers wait on the slot state words.
// - Re-create heap views at every access: the heap can grow between two awaits.
// - Patent boundary (src/ai/segmentation_engine.hpp): each decode carries exactly the one
//   prompt C++ wrote, and the decoder's mask_input is always zero with has_mask_input 0.
//   Never feed a previous mask or a second prompt to the decoder.
//
// Inert inside pthread workers: only the main thread's C++ calls Module.patchyAiAttach.
(function () {
  // Slot states and word layout, shared with segmentation_engine_web.cpp.
  var FREE = 0, PENDING = 2, CLAIMED = 3, RUNNING = 4, WRITING = 5, DONE = 6, FAILED = 7, ABANDONED = 8;
  var ENCODE = 1, DECODE = 2;
  var W = {
    state: 0, kind: 1, progress: 2, input: 3, inputCount: 4, height: 5, width: 6, coords: 7, labels: 11,
    output: 13, outputCapacity: 14, outputCount: 15, scores: 16, error: 20
  };
  var ERROR_BYTES = 256;
  var EMBEDDING_VALUES = 256 * 64 * 64;
  var MODEL_CACHE = 'patchy-ai-models';

  var host = null;

  function heapBuffer() {
    return (typeof wasmMemory !== 'undefined' && wasmMemory) ? wasmMemory.buffer : HEAPU8.buffer;
  }
  function words() {
    return new Int32Array(heapBuffer());
  }
  function floats() {
    return new Float32Array(heapBuffer());
  }
  function siteUrl(path) {
    return new URL(path, document.baseURI).href;
  }

  function attach(sequenceAddress, slotsAddress, slotCount, slotWords, ortFolder, encoderFile, decoderFile) {
    if (host) {
      return 1;
    }
    if (typeof window === 'undefined' || typeof document === 'undefined' || typeof Atomics === 'undefined' ||
        typeof SharedArrayBuffer === 'undefined' || !self.crossOriginIsolated) {
      return 0;
    }
    host = {
      sequence: sequenceAddress >>> 2,
      slots: slotsAddress >>> 2,
      slotCount: slotCount,
      slotWords: slotWords,
      ortFolder: ortFolder,
      encoderFile: encoderFile,
      decoderFile: decoderFile,
      inFlight: new Set(),
      queue: Promise.resolve(),
      ort: null,
      encoder: null,
      decoder: null,
      loading: null
    };
    watch();
    return 1;
  }

  // Waits on the mailbox sequence (Atomics.waitAsync where the browser has it, a short
  // poll elsewhere) and scans the slots whenever it moves.
  function watch() {
    var seen = NaN;
    function next() {
      var heap = words();
      var now = Atomics.load(heap, host.sequence);
      if (now !== seen) {
        seen = now;
        scan();
      }
      if (typeof Atomics.waitAsync === 'function') {
        var waited = Atomics.waitAsync(heap, host.sequence, seen, 1000);
        (waited.async ? waited.value : Promise.resolve()).then(next);
      } else {
        setTimeout(next, 100);
      }
    }
    next();
  }

  function scan() {
    var heap = words();
    for (var index = 0; index < host.slotCount; ++index) {
      var base = host.slots + index * host.slotWords;
      var state = Atomics.load(heap, base + W.state);
      if (state === PENDING) {
        if (Atomics.compareExchange(heap, base + W.state, PENDING, CLAIMED) === PENDING) {
          start(index, base);
        }
      } else if (state === ABANDONED && !host.inFlight.has(index)) {
        // Cancelled before the host claimed it.
        Atomics.compareExchange(heap, base + W.state, ABANDONED, FREE);
        Atomics.notify(heap, base + W.state);
      }
    }
  }

  // Runs in the same task as the claim: the worker never touches the slot while it is
  // CLAIMED, so the inputs are copied out before the state moves on.
  function start(index, base) {
    var heap = words();
    var all = floats();
    var request = {
      kind: heap[base + W.kind],
      height: heap[base + W.height],
      width: heap[base + W.width],
      coords: all.slice(base + W.coords, base + W.coords + 4),
      labels: all.slice(base + W.labels, base + W.labels + 2),
      input: null
    };
    var inputIndex = heap[base + W.input] >>> 2;
    var inputCount = heap[base + W.inputCount];
    try {
      request.input = all.slice(inputIndex, inputIndex + inputCount);
    } catch (error) {
      complete(base, null, error);
      return;
    }
    host.inFlight.add(index);
    Atomics.store(heap, base + W.state, RUNNING);
    Atomics.notify(heap, base + W.state);
    // One model run at a time: ORT-web sessions are not reentrant.
    host.queue = host.queue.then(function () {
      // Cancelled while queued: skip the run; finish() frees the slot.
      return Atomics.load(words(), base + W.state) === ABANDONED ? null : infer(request, base);
    }).then(function (result) {
      host.inFlight.delete(index);
      finish(base, result, null);
    }, function (error) {
      host.inFlight.delete(index);
      finish(base, null, error);
    });
  }

  function finish(base, result, error) {
    var heap = words();
    if (Atomics.compareExchange(heap, base + W.state, RUNNING, WRITING) !== RUNNING) {
      // Abandoned by a cancel: nobody reads this slot any more.
      Atomics.store(heap, base + W.state, FREE);
      Atomics.notify(heap, base + W.state);
      return;
    }
    complete(base, result, error);
  }

  // Writes the outcome of a slot the host owns (CLAIMED or WRITING) and hands it back.
  function complete(base, result, error) {
    var state = DONE;
    try {
      if (error) {
        throw error;
      }
      var heap = words();
      if (result.data.length > heap[base + W.outputCapacity]) {
        throw new Error('unexpected model output size ' + result.data.length);
      }
      var all = floats();
      all.set(result.data, heap[base + W.output] >>> 2);
      heap[base + W.outputCount] = result.data.length;
      if (result.scores) {
        all.set(result.scores.subarray(0, 4), base + W.scores);
      }
    } catch (failure) {
      state = FAILED;
      writeError(base, failure);
    }
    var words32 = words();
    Atomics.store(words32, base + W.state, state);
    Atomics.notify(words32, base + W.state);
  }

  function writeError(base, failure) {
    var text = String((failure && failure.message) || failure || 'unknown error');
    // ORT-web and the browser word allocation failures differently; C++ recognizes
    // "out of memory" (is_out_of_memory_message) and shows its own message.
    if (/out of memory|\bOOM\b|allocation failed|Memory\.grow|maximum memory/i.test(text)) {
      text = 'out of memory: ' + text;
    }
    var bytes = new TextEncoder().encode(text);
    var target = new Uint8Array(heapBuffer(), (base + W.error) * 4, ERROR_BYTES);
    var length = Math.min(bytes.length, ERROR_BYTES - 1);
    // Stay on a whole UTF-8 sequence when truncating.
    while (length > 0 && length < bytes.length && (bytes[length] & 0xC0) === 0x80) {
      --length;
    }
    target.set(bytes.subarray(0, length));
    target[length] = 0;
  }

  function reportProgress(permille) {
    var heap = words();
    host.inFlight.forEach(function (index) {
      Atomics.store(heap, host.slots + index * host.slotWords + W.progress, permille);
    });
  }

  async function infer(request, base) {
    var ort = await loadSessions();
    if (request.kind === ENCODE) {
      var encoded = await host.encoder.run({
        input_image: new ort.Tensor('float32', request.input, [request.height, request.width, 3])
      }, ['image_embeddings']);
      return { data: encoded.image_embeddings.data, scores: null };
    }
    if (request.kind !== DECODE || request.input.length !== EMBEDDING_VALUES) {
      throw new Error('malformed automatic selection request');
    }
    var decoded = await host.decoder.run({
      image_embeddings: new ort.Tensor('float32', request.input, [1, 256, 64, 64]),
      point_coords: new ort.Tensor('float32', request.coords, [1, 2, 2]),
      point_labels: new ort.Tensor('float32', request.labels, [1, 2]),
      // Always zero with has_mask_input 0: a previous mask is never fed back.
      mask_input: new ort.Tensor('float32', new Float32Array(256 * 256), [1, 1, 256, 256]),
      has_mask_input: new ort.Tensor('float32', new Float32Array([0]), [1]),
      orig_im_size: new ort.Tensor('float32', new Float32Array([request.height, request.width]), [2])
    }, ['masks', 'iou_predictions']);
    var dims = decoded.masks.dims;
    if (dims.length !== 4 || dims[1] !== 4 || dims[2] !== request.height || dims[3] !== request.width) {
      throw new Error('unexpected decoder output shape');
    }
    return { data: decoded.masks.data, scores: decoded.iou_predictions.data };
  }

  function loadSessions() {
    if (!host.loading) {
      host.loading = createSessions().catch(function (error) {
        host.loading = null;  // a later attempt may succeed (the network came back)
        throw error;
      });
    }
    return host.loading;
  }

  async function createSessions() {
    var folder = siteUrl(host.ortFolder + '/');
    var ort;
    try {
      ort = await import(folder + 'ort.wasm.min.mjs');
    } catch (error) {
      throw new Error('could not load ONNX Runtime Web (' + error + ')');
    }
    ort.env.logLevel = 'error';
    ort.env.wasm.wasmPaths = folder;
    // ORT runs in its own worker, so a long encode never blocks the page's main thread.
    ort.env.wasm.proxy = true;
    ort.env.wasm.numThreads = Math.max(1, Math.min(8, navigator.hardwareConcurrency || 4));
    var models = await fetchModels([host.encoderFile, host.decoderFile]);
    var options = { executionProviders: ['wasm'], graphOptimizationLevel: 'all' };
    host.encoder = await ort.InferenceSession.create(models[0], options);
    host.decoder = await ort.InferenceSession.create(models[1], options);
    host.ort = ort;
    return ort;
  }

  // The model files are content-pinned by name, so a cached copy never goes stale; entries
  // for names no longer in use are dropped.
  async function fetchModels(files) {
    var cache = null;
    try {
      cache = await caches.open(MODEL_CACHE);
    } catch (error) {
      cache = null;  // no Cache API (an insecure origin): plain HTTP caching only
    }
    var urls = files.map(function (file) { return siteUrl('ai-models/' + file); });
    var results = new Array(files.length);
    var missing = [];
    for (var index = 0; index < files.length; ++index) {
      var hit = cache ? await cache.match(urls[index]).catch(function () { return undefined; }) : undefined;
      if (hit) {
        results[index] = new Uint8Array(await hit.arrayBuffer());
      } else {
        missing.push(index);
      }
    }
    if (missing.length > 0) {
      var responses = [];
      var total = 0;
      for (var m = 0; m < missing.length; ++m) {
        var response = await fetch(urls[missing[m]]);
        if (!response.ok) {
          throw new Error('could not download the Object Selection model ' + files[missing[m]] +
                          ' (HTTP ' + response.status + ')');
        }
        responses.push(response);
        total += Number(response.headers.get('Content-Length')) || 0;
      }
      var loaded = 0;
      reportProgress(0);
      for (var r = 0; r < responses.length; ++r) {
        var bytes = await readAll(responses[r], function (count) {
          loaded += count;
          if (total > 0) {
            reportProgress(Math.min(999, Math.floor(loaded * 1000 / total)));
          }
        });
        results[missing[r]] = bytes;
        if (cache) {
          await cache.put(urls[missing[r]], new Response(bytes, {
            headers: { 'Content-Type': 'application/octet-stream' }
          })).catch(function () {});
        }
      }
      reportProgress(-1);
    }
    if (cache) {
      cache.keys().then(function (keys) {
        keys.forEach(function (key) {
          if (urls.indexOf(key.url) < 0) {
            cache.delete(key).catch(function () {});
          }
        });
      }).catch(function () {});
    }
    return results;
  }

  async function readAll(response, onBytes) {
    if (!response.body || typeof response.body.getReader !== 'function') {
      var whole = new Uint8Array(await response.arrayBuffer());
      onBytes(whole.length);
      return whole;
    }
    var reader = response.body.getReader();
    var chunks = [];
    var size = 0;
    for (;;) {
      var step = await reader.read();
      if (step.done) {
        break;
      }
      chunks.push(step.value);
      size += step.value.length;
      onBytes(step.value.length);
    }
    var bytes = new Uint8Array(size);
    var offset = 0;
    chunks.forEach(function (chunk) {
      bytes.set(chunk, offset);
      offset += chunk.length;
    });
    return bytes;
  }

  function release() {
    if (!host || host.inFlight.size > 0 || !host.ort) {
      return;
    }
    var encoder = host.encoder;
    var decoder = host.decoder;
    host.encoder = null;
    host.decoder = null;
    host.ort = null;
    host.loading = null;
    [encoder, decoder].forEach(function (session) {
      if (session) {
        session.release().catch(function () {});
      }
    });
  }

  Module['patchyAiAttach'] = attach;
  Module['patchyAiRelease'] = release;
})();
