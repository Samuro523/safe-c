
// shader_ps.h

// lerp(x,y,s) returns x + s(y-x)
// saturate() clamps between 0 and 1, it zeroes negative values.
// reflect(i, n) = i - 2 * n * dot(i, n)   result needs not be normalized if input vectors are.
// normalize() divides a vector by its length, making its length 1

//---------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
// basic pixel shader
////////////////////////////////////////////////////////////////////////////////

const string PIXEL_SHADER_SOLID_RENDERING =

 "Texture2D        shaderTexture[3]; " +   // 0:albedo, 1:normal, 2:MRO (R:occlusion, G:roughness, B:metal texture)
 "Texture2DArray   shadowTexture; " +

 "SamplerState           SampleType    : register(s0); " +
 "SamplerComparisonState ShadowSampler : register(s1); " +

//=================================================================================

  // D_GGX : normal distribution creating almost final perfect specular cone, which size varies per roughness.
  // note that we use roughness2 and we don't divide by PI
  // roughness2 = 1 -> returns 1

  " float D_GGX ( float NdotH , float roughness2 ) " +
  " { " +
  "  float f = ( NdotH * roughness2 - NdotH ) * NdotH + 1; " +
  "  return roughness2 / (f * f) ; " +
  " } " +

//=================================================================================

  // Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"
  // creates soft face at light origin,
  // the division by NdotL * NdotV makes the result climb fast and creates a white perimeter at viewer grazing angle.
  // very bright (infinite) at low roughness values.

  " float V_SmithGGXCorrelated (float NdotL, float NdotV, float roughness2) " +
  "  { " +
  // caution : the " NdotL *" and " NdotV *" are explicitely inversed , this is not a mistake.
  "  float Lambda_GGXV = NdotL * sqrt (( - NdotV * roughness2 + NdotV ) * NdotV + roughness2 ); " +
  "  float Lambda_GGXL = NdotV * sqrt (( - NdotL * roughness2 + NdotL ) * NdotL + roughness2 ); " +
    // roughness2=0 => v = 1 / 4*NoL*NoV   => min=1/4, max=+inf
    // roughness2=1 => v = 1 / 2*(NoL+NoV) => min=1/4, max=+inf
  "  return 0.5  / ( Lambda_GGXV + Lambda_GGXL ); " +
  "  } " +

//=================================================================================

  // F : describes how light is reflected from each smooth microfacet.
  // It is a function of incidence angle and wavelength
  // expresses the reflectance of a perfectly smooth, mirrorlike surface.
  // creates fresnel effet : high color value at viewer-light grazing angle.

  " float3 fresnelSchlick (float HdotL, float3 F0, float roughness2) " +
  " { " +
  "   float factor = saturate (3.0 - 4.0 * roughness2); " +    // fresnel activates for roughness in 0.25 .. 0.50
  "   return F0 + factor * (float3(1.0, 1.0, 1.0) - F0) * pow(1.0 - HdotL, 5.0); " +
  " }   " +

//=================================================================================

  // returns PI too much light

  "float3 pbr (float3 F0, float3 albedo, float metallic, float roughness2, float3 V, float3 L0, float3 N, float3 radiance) " +
  "{ " +
  " float3 L = L0; " +
  " float  NdotL = dot(N, L0); " +
  " float  intensity = 1.0; " +

  " if (NdotL <= 0.0)" +
  " {" +
  "   if (translucid <= 0.0) " +
  "     return float3 (0.0, 0.0, 0.0); " +
  "   L = reflect (L, N); " +
  "   NdotL = dot(N, L); " +
  "   intensity = saturate (translucid); " +
  " }" +

  " NdotL = saturate (NdotL); " +
  " float3 specular = float3(0.0, 0.0, 0.0); " +
  " float3 kD       = float3(1.0, 1.0, 1.0); " +  // diffuse fraction

  " if (roughness2 < 1.0) " +    // only for materials not 100% rough
  " {" +
      " float3 H     = normalize (L + V); " +
      " float  NdotH = saturate (dot(N, H)); " +
      " float  D     = D_GGX (NdotH, roughness2); " +
      " float NdotV  = saturate (dot(N, V)) + 1.0e-5; " +  //  avoid artifact
      " float Vis    = V_SmithGGXCorrelated (NdotL, NdotV, roughness2); " +
      " float HdotL  = saturate (dot(H, L)); " +
      " float3 F     = fresnelSchlick (HdotL, F0, roughness2); " +
      " specular  = F * (D * Vis); " +
//      " kD = float3(1.0, 1.0, 1.0) ; " +    // $ - F  reduce diffuse fraction  $ F is not removed in filament ?
  " }" +

  " kD *= 1.0 - metallic; " +                  // no albedo diffuse light for metallic materials
  " return (kD * albedo + specular) * radiance * (intensity * NdotL); " +
  "} " +

//=================================================================================

 "PixelOutputType MyPixelShader(PixelInputType input) " +
 "{ " +
   "PixelOutputType output; " +
   "float3 LightColor; " +
   "float4 finalColor; " +
   "int    i;" +

   //----------------------------------------------------------
   
   // setup variables

   "float3 toEye = normalize (eyePosition - (float3)input.posW);" +

   // noticed a precision problem when specular spot did shake around a sphere, that went better when adding this, but now ok ?
   "input.normal = normalize (input.normal); " +

   //----------------------------------------------------------

   // perlin

   "if (tclock != 0.0) " +   // means perlin is active
   "{" +
     "float2 t1 = float2(input.posW.xz + CameraOffset.xz) " +   // signed +/- 2^23
                " * (32.0 / 65536.0); " +    // integer frequency 32 within area so it's continuous

     "float2 n = perlin_amplitude * float2(noise (float3(t1, 0.37657652 * tclock)), " +
     "                                     noise (float3(t1, 0.39657652 * tclock + 0.23565612121))); " +

     // sin & cos cause a slow translation move so that same texture spot is not always at same place
     "input.tex.xy += (1.0 / 128.0) * n + float2(sin (tclock * 0.001323212), cos (tclock * 0.001671212)); " +

     "float2 distv2 = float2(input.posW.xz - eyePosition.xz); " +
     "float dist2 = dot (distv2, distv2); " +

     "const float NEAR =  64000.0; " +
     "const float FAR  = 256000.0; " +
     "float normalized_dist = saturate ((dist2 - NEAR * NEAR) * (1.0 / ((FAR-NEAR) * (FAR-NEAR)))); " +

     "float fact = lerp (0.125, 0.05, normalized_dist); " +  // 0.05 for near, 0.125 for far

     "input.normal.xz += fact * n; " +

     "input.normal = normalize (input.normal);  " +
   "}" +

   //----------------------------------------------------------

   // normal texture

   "if (has_bump_texture) " +
   "{" +
      "float3 binormal, bumpNormal; " +
      "float4 bumpMap; " +

      "input.tangent = normalize (input.tangent); " +   // need to renormalize after interpolation

      // Gram-Schmidt orthogonalize
      "input.tangent = normalize ((input.tangent - input.normal * dot(input.tangent, input.normal))); " +

      // compute binormal
      "binormal = cross (input.tangent, input.normal); " +

      // get bump value in range (-1, +1)
      "bumpMap = (shaderTexture[1].Sample(SampleType, input.tex) * 2.0) - 1.0; " +

      // parallax effect : bumpMap.z contains height
      "if (parallax_factor != 0.0)" +
      "{" +
        "float3 V = float3(dot(toEye,input.tangent), dot(toEye,binormal), 0.0); " +
        "input.tex -= V.xy * bumpMap.z * parallax_factor; " +
        "bumpMap = (shaderTexture[1].Sample(SampleType, input.tex) * 2.0) - 1.0; " +
        "bumpMap.z = sqrt(1.0 - saturate(bumpMap.x * bumpMap.x + bumpMap.y * bumpMap.y)); " +
      "} " +

      "input.normal = normalize ((bumpMap.x * input.tangent) + (bumpMap.y * binormal) + (bumpMap.z * input.normal)); " +
   "} " +

   //----------------------------------------------------------

   // setup variables

   "float4 albedo = shaderTexture[0].Sample(SampleType, input.tex) * MaterialColor * input.diffuse;" +

   "float metallic, roughness, occlusion; " +

   "if (has_mro_texture) " +
   "{" +
      "float4 tex = shaderTexture[2].Sample(SampleType, input.tex); " +
      "occlusion = tex.r; " +
      "roughness = tex.g; " +
      "metallic  = tex.b; " +
   "}" +
   "else" +
   "{" +
      "occlusion = 1.0; " +
      "roughness = saturate (1.0 - in_smooth); " +
      "metallic  = in_metallic; " +
   "}" +

   " roughness = clamp (roughness, 0.0001, 1.0); " +
   " float roughness2 = roughness * roughness ; " +
   " metallic = saturate (metallic); " +
   " float P00_factor = saturate ((1.0 / 0.05) * (1.0 - roughness2)); " +    // P00 activates from 0.0 to 0.05
   " float3 F00 = 0.04 * P00_factor; " +
   " float3 F0 = lerp (F00, saturate(albedo), metallic); " +

   " float switch_ambiant_sun = 1.0 - (disable_ambiant_sun_underground * (input.posW.y + CameraOffset.y < 0.0)); " +

   //----------------------------------------------------------

   // sun light

   "{ " +

     "float sun_factor; " +   // normally 1.0, otherwise 0.0 if shadows

     "sun_factor = is_sunlight_on * switch_ambiant_sun; " +   // 0 or 1

     "if (input.ShadowPos.z >= 0.0) "  +      // .z equals -1.0 when shadows are OFF
     "{ " +
         "static const float F = 256.0; " +
         "static const float R = 256.0 * (510.0/512.0); " +
         "static const float res[5]  = {     256.0/F,   64.0/F,   16.0/F,   4.0/F, 1.0/F}; " +
         "static const float MAPLIMIT[5] = {  R/256.0,   R/64.0,   R/16.0,   R/4.0,     R}; " +

         "{ " +
             "  float2 xy = input.ShadowPos.xy; " +
             "  float  z = input.ShadowPos.z;   " +      // depth z = 0(far up in sky) to 1(below ground)
             "  float  size = max (abs(xy.x), abs(xy.y)); " +
             "  int    i; " +

             "  for (i=0; i<5 && i<nb_of_shadow_maps; i++) " +
             "  { " +
             "    if (size < MAPLIMIT[i]) " +
             "    { " +
             "      xy = xy * float2(0.5, -0.5) * res[i] + float2(0.5, 0.5); " +

                   "{ " +   // central pixel is in shadow

                     // PCF 3x3
                     "float DarkCells = 0.0;  float3 xyz = float3(xy,i); " +
                     "for (int k = -1; k <= 1; k++) { for (int l = -1; l <= 1; l++) { " +
                     "  DarkCells += shadowTexture.SampleCmpLevelZero(ShadowSampler, xyz, z, int2(k,l)).r; }} " +
                     "sun_factor *= 1.0 - pow(DarkCells * (1.0f/9.0f), 2.2); " +

                   "} " +

             "      break; " +
             "    } " +
             "  } " +
         "}" +
     "} " +

     "float3 radiance = sun_factor * SunLightColor;  " +

     "LightColor = pbr (F0, albedo.rgb, metallic, roughness2, toEye, toSun, input.normal, radiance); " +

   "} " +

   //----------------------------------------------------------

   "float3 ambientLight = AmbiantLightColor * (is_ambiantlight_on * switch_ambiant_sun * occlusion); " +

   "if (is_ambiant_downwards) " +
     "ambientLight *= (0.7 + 0.25*input.normal.y + 0.05*input.normal.x); " +  // byte 0 : ambiant light less intense when not shining downwards

   "if (reflect (-toEye, input.normal).y < 0.0) " +    // specular reflection goes in ground
     "ambientLight *= 1.0 + metallic * (max(0.7, saturate(translucid)) - 1.0); " +

   //----------------------------------------------------------

   // point lights

   "for (i=0; i<NbLights; i++) " +
   "{" +
      "float3 toLight         = Light[i].position - (float3)input.posW;" +
      "float  LightVectorLen2 = dot (toLight, toLight);" +
      "float  LightRange      = Light[i].range; " +
      "if (LightVectorLen2 < LightRange * LightRange && LightVectorLen2 > 0.0) " +
      "{" +
        "float LightVectorLen = sqrt (LightVectorLen2);" +

        "toLight /= LightVectorLen;" +          // normalize light vector

        "float factor = pow((LightRange - LightVectorLen) / LightRange, Light[i].attenuation);  " +

        "if (Light[i].cone > 0.0f)" +
          "factor *= pow(max(dot(-toLight, Light[i].direction), 0.0f), Light[i].cone); " +

        "float3 radiance = factor * Light[i].color;  " +

        "LightColor += pbr (F0, albedo.rgb, metallic, roughness2, toEye, toLight, input.normal, radiance); " +

      "}" +
    "}" +

   //----------------------------------------------------------

   "finalColor.rgb = saturate (LightColor + (ambientLight + EmissiveLightColor) * albedo.rgb); " +
   "finalColor.a = albedo.a; " +

   //----------------------------------------------------------

    // fog
    "if (fogInvRange != 0.0) " +  // static condition
    "{" +
      "float fogFactor = saturate ((length(eyePosition - (float3)input.posW) - fogStart) * fogInvRange); " +
      "float factor = pow (fogFactor, FogInvExponent);" +
      "float3 FogCol = fogColor.rgb * switch_ambiant_sun;" +
      "finalColor.rgb = factor * FogCol + (1.0 - factor) * finalColor.rgb;" +
    "}" +

   //----------------------------------------------------------

    // produce output for 2 targets : screen and picking
    
    "if (test_indexing) " +
    "{" +
      "output.color = float4(0.0, 0.0, 0.0, 1.0); " +
      "output.objectid = finalColor; " +
    "}" +
    "else " +
    "{" +
      "output.color = finalColor; " +
      "output.objectid = 0; " +
      "if (finalColor.a > 0.0 && object_id.a > 0.0)  output.objectid = object_id; " +   // not transparent : store object id
    "}" +
    "return output;" +
   "} ";

   //----------------------------------------------------------

//---------------------------------------------------------------------
