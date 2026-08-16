#pragma once

// Decode a JPEG file from LittleFS and stream it directly to the TFT.
// No full framebuffer is used; JPEGDEC decodes in chunks (~4 KB working buf).
// Returns true if the file was found and decoded successfully.
bool jpegDisplayFile(const char *path);
