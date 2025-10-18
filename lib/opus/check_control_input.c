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

use control;

/* Check encoder control struct */
public
int check_control_input(
    silk_EncControlStruct        *encControl                    /* I    Control structure                           */
)
{
    ;

    if( ( ( encControl->API_sampleRate            !=  8000 ) &&
          ( encControl->API_sampleRate            != 12000 ) &&
          ( encControl->API_sampleRate            != 16000 ) &&
          ( encControl->API_sampleRate            != 24000 ) &&
          ( encControl->API_sampleRate            != 32000 ) &&
          ( encControl->API_sampleRate            != 44100 ) &&
          ( encControl->API_sampleRate            != 48000 ) ) ||
        ( ( encControl->desiredInternalSampleRate !=  8000 ) &&
          ( encControl->desiredInternalSampleRate != 12000 ) &&
          ( encControl->desiredInternalSampleRate != 16000 ) ) ||
        ( ( encControl->maxInternalSampleRate     !=  8000 ) &&
          ( encControl->maxInternalSampleRate     != 12000 ) &&
          ( encControl->maxInternalSampleRate     != 16000 ) ) ||
        ( ( encControl->minInternalSampleRate     !=  8000 ) &&
          ( encControl->minInternalSampleRate     != 12000 ) &&
          ( encControl->minInternalSampleRate     != 16000 ) ) ||
          ( encControl->minInternalSampleRate > encControl->desiredInternalSampleRate ) ||
          ( encControl->maxInternalSampleRate < encControl->desiredInternalSampleRate ) ||
          ( encControl->minInternalSampleRate > encControl->maxInternalSampleRate ) ) {
        ;
        return -102;
    }
    if( encControl->payloadSize_ms != 10 &&
        encControl->payloadSize_ms != 20 &&
        encControl->payloadSize_ms != 40 &&
        encControl->payloadSize_ms != 60 ) {
        ;
        return -103;
    }
    if( encControl->packetLossPercentage < 0 || encControl->packetLossPercentage > 100 ) {
        ;
        return -105;
    }
    if( encControl->useDTX < 0 || encControl->useDTX > 1 ) {
        ;
        return -108;
    }
    if( encControl->useCBR < 0 || encControl->useCBR > 1 ) {
        ;
        return -109;
    }
    if( encControl->useInBandFEC < 0 || encControl->useInBandFEC > 1 ) {
        ;
        return -107;
    }
    if( encControl->nChannelsAPI < 1 || encControl->nChannelsAPI > 2 ) {
        ;
        return -111;
    }
    if( encControl->nChannelsInternal < 1 || encControl->nChannelsInternal > 2 ) {
        ;
        return -111;
    }
    if( encControl->nChannelsInternal > encControl->nChannelsAPI ) {
        ;
        return -111;
    }
    if( encControl->complexity < 0 || encControl->complexity > 10 ) {
        ;
        return -106;
    }

    return 0;
}
#end unsafe
