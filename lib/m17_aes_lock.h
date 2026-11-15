/* -*- c++ -*- */
/*
 * Copyright 2026 M17 Project.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// tinier-aes keeps the key size (Nb, Nk, Nr) in global variables, which
// aes_ctr_bytewise_payload_crypt() sets on every call. The coder and the
// decoder run in different threads of the same flowgraph and may use
// different key sizes, so every call into tinier-aes takes this lock.

#ifndef INCLUDED_M17_AES_LOCK_H
#define INCLUDED_M17_AES_LOCK_H

#include <mutex>

namespace gr
{
	namespace m17
	{
		// one mutex for the whole library (an inline function has a single static instance)
		inline std::mutex &aes_mutex()
		{
			static std::mutex m;
			return m;
		}
	} // namespace m17
} // namespace gr

#endif // INCLUDED_M17_AES_LOCK_H
