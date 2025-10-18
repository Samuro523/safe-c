#begin unsafe
/* Copyright (c) 2007-2008 CSIRO
   Copyright (c) 2007-2009 Xiph.Org Foundation
   Copyright (c) 2008-2009 Gregory Maxwell
   Written by Jean-Marc Valin and Gregory Maxwell */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

use arch;
use modes;
use entcode;
use opus_types;

/** Compute the amplitude (sqrt energy) in each of the bands
 * @param m Mode data
 * @param X Spectrum
 * @param bands Square root of the energy for each band (returned)
 */
void compute_band_energies(      OpusCustomMode *m,       celt_sig *X, celt_ener *bandE, int _end, int C, int M);

/*void compute_noise_energies(      CELTMode *m,       celt_sig *X,       opus_val16 *tonality, celt_ener *bandE);*/

/** Normalise each band of X such that the energy in each band is
    equal to 1
 * @param m Mode data
 * @param X Spectrum (returned normalised)
 * @param bands Square root of the energy for each band
 */
void normalise_bands(      OpusCustomMode *m,       celt_sig *  freq, celt_norm *  X,       celt_ener *bandE, int _end, int C, int M);

/** Denormalise each band of X to restore full amplitude
 * @param m Mode data
 * @param X Spectrum (returned de-normalised)
 * @param bands Square root of the energy for each band
 */
void denormalise_bands(      OpusCustomMode *m,       celt_norm *  X, celt_sig *  freq,       celt_ener *bandE, int _end, int C, int M);

int spreading_decision(      OpusCustomMode *m, celt_norm *X, int *average,
      int last_decision, int *hf_average, int *tapset_decision, int update_hf,
      int _end, int C, int M);

void haar1(celt_norm *X, int N0, int stride);

/** Quantisation/encoding of the residual spectrum
 * @param m Mode data
 * @param X Residual (normalised)
 * @param total_bits Total number of bits that can be used for the frame (including the ones already spent)
 * @param enc Entropy encoder
 */
void quant_all_bands(int encode,       OpusCustomMode *m, int start, int _end,
      celt_norm * X_, celt_norm * Y_,     byte      *collapse_masks,       celt_ener *bandE, int *pulses,
      int shortBlocks, int spread, int dual_stereo, int intensity, int *tf_res,
      opus_int32 total_bits, opus_int32 balance, ec_ctx *ec, int LM, int codedBands, opus_uint32 *seed);

void anti_collapse(      OpusCustomMode *m, celt_norm *X_,     byte      *collapse_masks, int LM, int C, int size,
      int start, int _end, opus_val16 *logE, opus_val16 *prev1logE,
      opus_val16 *prev2logE, int *pulses, opus_uint32 seed);

opus_uint32 celt_lcg_rand(opus_uint32 seed);

#end unsafe
