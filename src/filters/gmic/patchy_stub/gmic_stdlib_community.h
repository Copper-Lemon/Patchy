// Patchy's replacement for G'MIC's generated gmic_stdlib_community.h.
//
// Upstream embeds the standard library plus the community filters as a
// zlib-compressed array. Patchy ships neither that array nor zlib for it: the
// engine installs the standard library text in gmic::stdlib before a run
// (gmic_engine.cpp), so the interpreter never decompresses this one. gmic.cpp
// still names these two symbols, so they exist, empty.
#pragma once

const unsigned char data_gmic[] = {0};
const unsigned long size_data_gmic = 0UL;
