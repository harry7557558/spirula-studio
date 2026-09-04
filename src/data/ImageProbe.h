#pragma once

// An image file's pixel size without decoding it -- stb_image's header reader,
// or the EXR probe. Signature matches DatasetParserConfig::probe_image_size,
// which the WebAssembly viewer leaves null because it has neither decoder.

bool probe_image_size(const char* path, int* w, int* h);
