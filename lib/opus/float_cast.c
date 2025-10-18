#begin unsafe

use ../math;
use opus_types;

public opus_int16 FLOAT2INT16 (float x)
{
   *&x = x*32768.0f;
   *&x = ((x) > (-32768.0) ? (x) : (-32768.0));
   *&x = ((x) < (32767.0) ? (x) : (32767.0));
   return (opus_int16)((int)(floor(0.5+x)));
}
#end unsafe