/* -*- c++ -*- */
/*
 * Copyright 2026 M17 Project.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Codec2 constants shared by the M17 coder and decoder blocks.

#ifndef INCLUDED_M17_CODEC2_H
#define INCLUDED_M17_CODEC2_H

#include <stdint.h>

namespace gr
{
	namespace m17
	{
		// One Codec2 3200 frame (20 ms, 8 bytes) of encoded digital silence: what the
		// Codec2 encoder produces for all-zero PCM. Used wherever a voice payload must be
		// filled without real audio. All-zero bytes are NOT silence: they decode as a
		// burst louder than normal speech.
		static constexpr int C2_FRAME_BYTES = 8;
		static constexpr uint8_t C2_SILENCE[C2_FRAME_BYTES] = {0x01, 0x00, 0x09, 0x43, 0x9C, 0xE4, 0x21, 0x08};
	} // namespace m17
} // namespace gr

#endif /* INCLUDED_M17_CODEC2_H */
