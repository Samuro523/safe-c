#begin unsafe
/***********************************************************************
Copyright (c) 2006-2011, Skype Limited. All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
- Neither the name of Internet Society, IETF or IETF Trust, nor the 
names of specific contributors, may be used to endorse or promote
products derived from this software without specific prior written
permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS”
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
***********************************************************************/

use opus_types;
use entcode, structs, entdec, tables, nlsf_unpack;


/* Decode side-information parameters from payload */
public
void silk_decode_indices(
    silk_decoder_state          *psDec,                         /* I/O  State                                       */
    ec_dec                      *psRangeDec,                    /* I/O  Compressor data structure                   */
    int                    FrameIndex,                     /* I    Frame number                                */
    int                    decode_LBRR,                    /* I    Flag indicating LBRR data is being decoded  */
    int                    condCoding                      /* I    The type of conditional coding to use       */
)
{
    int   i, k, Ix;
    int   decode_absolute_lagIndex, delta_lagIndex;
    opus_int16 ec_ix[ 16 ];
    byte pred_Q8[ 16 ];

    /*******************************************/
    /* Decode signal type and quantizer offset */
    /*******************************************/
    if( decode_LBRR != 0 || psDec->VAD_flags[ FrameIndex ] != 0 ) {
        Ix = ec_dec_icdf( psRangeDec, &silk_type_offset_VAD_iCDF, 8 ) + 2;
    } else {
        Ix = ec_dec_icdf( psRangeDec, &silk_type_offset_no_VAD_iCDF, 8 );
    }
    psDec->indices.signalType      = (int1)((Ix)>>(1));
    psDec->indices.quantOffsetType = (int1)( Ix & 1 );

    /****************/
    /* Decode gains */
    /****************/
    /* First subframe */
    if( condCoding == 2 ) {
        /* Conditional coding */
        psDec->indices.GainsIndices[ 0 ] = (int1)ec_dec_icdf( psRangeDec, &silk_delta_gain_iCDF, 8 );
    } else {
        /* Independent coding, in two stages: MSB bits followed by 3 LSBs */
        psDec->indices.GainsIndices[ 0 ]  = (int1)((opus_int32)((opus_uint32)(ec_dec_icdf( psRangeDec, &silk_gain_iCDF[ psDec->indices.signalType ], 8 ))<<(3)));
        psDec->indices.GainsIndices[ 0 ] += (int1)ec_dec_icdf( psRangeDec, &silk_uniform8_iCDF, 8 );
    }

    /* Remaining subframes */
    for( i = 1; i < psDec->nb_subfr; i++ ) {
        psDec->indices.GainsIndices[ i ] = (int1)ec_dec_icdf( psRangeDec, &silk_delta_gain_iCDF, 8 );
    }

    /**********************/
    /* Decode LSF Indices */
    /**********************/
    psDec->indices.NLSFIndices[ 0 ] = (int1)ec_dec_icdf( psRangeDec, &psDec->psNLSF_CB->CB1_iCDF[ ( psDec->indices.signalType >> 1 ) * psDec->psNLSF_CB->nVectors ], 8 );
    silk_NLSF_unpack( &ec_ix, &pred_Q8, psDec->psNLSF_CB, psDec->indices.NLSFIndices[ 0 ] );
    ;
    for( i = 0; i < psDec->psNLSF_CB->order; i++ ) {
        Ix = ec_dec_icdf( psRangeDec, &psDec->psNLSF_CB->ec_iCDF[ ec_ix[ i ] ], 8 );
        if( Ix == 0 ) {
            Ix -= ec_dec_icdf( psRangeDec, &silk_NLSF_EXT_iCDF, 8 );
        } else if( Ix == 2 * 4 ) {
            Ix += ec_dec_icdf( psRangeDec, &silk_NLSF_EXT_iCDF, 8 );
        }
        psDec->indices.NLSFIndices[ i+1 ] = (int1)( Ix - 4 );
    }

    /* Decode LSF interpolation factor */
    if( psDec->nb_subfr == 4 ) {
        psDec->indices.NLSFInterpCoef_Q2 = (int1)ec_dec_icdf( psRangeDec, &silk_NLSF_interpolation_factor_iCDF, 8 );
    } else {
        psDec->indices.NLSFInterpCoef_Q2 = 4;
    }

    if( psDec->indices.signalType == 2 )
    {
        /*********************/
        /* Decode pitch lags */
        /*********************/
        /* Get lag index */
        decode_absolute_lagIndex = 1;
        if( condCoding == 2 && psDec->ec_prevSignalType == 2 ) {
            /* Decode Delta index */
            delta_lagIndex = (opus_int16)ec_dec_icdf( psRangeDec, &silk_pitch_delta_iCDF, 8 );
            if( delta_lagIndex > 0 ) {
                delta_lagIndex = delta_lagIndex - 9;
                psDec->indices.lagIndex = (opus_int16)( psDec->ec_prevLagIndex + delta_lagIndex );
                decode_absolute_lagIndex = 0;
            }
        }
        if( decode_absolute_lagIndex != 0 ) {
            /* Absolute decoding */
            psDec->indices.lagIndex  = (opus_int16)((opus_int16)ec_dec_icdf( psRangeDec, &silk_pitch_lag_iCDF, 8 ) * ((psDec->fs_kHz)>>(1)));
            psDec->indices.lagIndex += (opus_int16)ec_dec_icdf( psRangeDec, psDec->pitch_lag_low_bits_iCDF, 8 );
        }
        psDec->ec_prevLagIndex = psDec->indices.lagIndex;

        /* Get countour index */
        psDec->indices.contourIndex = (int1)ec_dec_icdf( psRangeDec, psDec->pitch_contour_iCDF, 8 );

        /********************/
        /* Decode LTP gains */
        /********************/
        /* Decode PERIndex value */
        psDec->indices.PERIndex = (int1)ec_dec_icdf( psRangeDec, &silk_LTP_per_index_iCDF, 8 );

        for( k = 0; k < psDec->nb_subfr; k++ ) {
            psDec->indices.LTPIndex[ k ] = (int1)ec_dec_icdf( psRangeDec, &silk_LTP_gain_iCDF_ptrs[ psDec->indices.PERIndex ], 8 );
        }

        /**********************/
        /* Decode LTP scaling */
        /**********************/
        if( condCoding == 0 ) {
            psDec->indices.LTP_scaleIndex = (int1)ec_dec_icdf( psRangeDec, &silk_LTPscale_iCDF, 8 );
        } else {
            psDec->indices.LTP_scaleIndex = 0;
        }
    }
    psDec->ec_prevSignalType = psDec->indices.signalType;

    /***************/
    /* Decode seed */
    /***************/
    psDec->indices.Seed = (int1)ec_dec_icdf( psRangeDec, &silk_uniform4_iCDF, 8 );
}
#end unsafe
