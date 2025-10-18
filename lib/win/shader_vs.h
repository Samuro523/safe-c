
// shader_vs.h : vertex shader

//---------------------------------------------------------------------

const string SHADOW_COMPUTATION =

    "float3 compute_shadow_position (float3 posW, uint instance) " +
    "{ " +
      "float3 position; " +

      "if (shadowMatrix._m33 == 0.0) return float3(-1.0, -1.0, -1.0); " +   // shadows are OFF

      // PosW = world position means relative to camera area
      "position = mul(float4(posW,1.0), shadowMatrix).xyz;  " +

      "static const float res[5] = {256.0, 64.0, 16.0, 4.0, 1.0}; " +

      "position.xy *= res[instance];  " +

      "return position; " +
    "} ";


//---------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
// basic vertex shader
////////////////////////////////////////////////////////////////////////////////

const string VERTEX_SHADER_BASIC_RENDERING =

    SHADOW_COMPUTATION +

    "PixelInputType MyVertexShader (VertexInputType input) " +
    "{  " +
      "PixelInputType output; " +

      // Calculate the position of the vertex against the world, view, and projection matrices.
      "output.posW     = mul(input.position, (float4x3)worldMatrix);  " +
      "output.position = mul(mul(float4(output.posW,1.0), viewMatrix), projectionMatrix);  " +

      // Calculate the normal vector against the world matrix only.
      "output.normal = mul((float3)input.normal, (float3x3)worldMatrix);  " +
      "output.tangent = mul((float3)input.tangent, (float3x3)worldMatrix);  " +

      "output.diffuse = input.diffuse; " +

      // compute the texture coordinates for the pixel shader.
      "output.tex[0] = input.tex[0] * (fact_u_min_1 + 1.0)   +   input.tex[1] * fact_u   +   ofs_u; " +
      "output.tex[1] = input.tex[1] * (fact_v_min_1 + 1.0)   +   input.tex[0] * fact_v   +   ofs_v; " +

      // compute shadow position
      "output.ShadowPos = compute_shadow_position (output.posW, input.instance); " +

      "return output; " +
    "} ";

//---------------------------------------------------------------------

const string VERTEX_SHADER_BASIC_BUILD_SHADOW_MAP =

    SHADOW_COMPUTATION +

    "ShadowVertexOutputType MyVertexShader (ShadowVertexInputType input) " +
    "{  " +
      "ShadowVertexOutputType  output; " +

      // Calculate real world position
      "float3 posW = mul(input.position, (float4x3)worldMatrix);  " +

      // compute shadow position
      "output.position.xyz = compute_shadow_position (posW, input.instance); " +
      "output.position.w = 1.0; " +

      "output.RenderTargetIndex = input.instance; " +

      "return output; " +
    "} ";

//---------------------------------------------------------------------

const string GET_BONE_MATRIX =

  "float4x4 GetBoneMatrix (int bone) " +
  "{" +

  "  if (indexes_supported)  " +
  "  { " +
  "    return g_bone[bone]; " +
  "  } " +
  "  else " +
  "  { " +
      " matrix<float,4,4> m44; " +
      
      " if (bone < 64) " +
      " { " +
        " if (bone < 32) " +
        " { " +
          " if (bone < 16) " +
          " { " +
            " if (bone < 8) " +
            " { " +
              " if (bone < 4) " +
              " { " +
                " if (bone < 2) " +
                " { " +
                  " if (bone == 0)  m44 = g_bone[0]; else m44 = g_bone[1]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 2)  m44 = g_bone[2]; else  m44 = g_bone[3]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 6) " +
                " { " +
                  " if (bone == 4)  m44 = g_bone[4]; else  m44 = g_bone[5]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 6)  m44 = g_bone[6]; else  m44 = g_bone[7]; " +
                " } " +
              " } " +
            " } " +
            " else " +
            " { " +
              " if (bone < 12) " +
              " { " +
                " if (bone < 10) " +
                " { " +
                  " if (bone == 8)  m44 = g_bone[8];   else   m44 = g_bone[9]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 10) m44 = g_bone[10];  else  m44 = g_bone[11]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 14) " +
                " { " +
                  " if (bone == 12) m44 = g_bone[12];  else  m44 = g_bone[13]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 14) m44 = g_bone[14]; else  m44 = g_bone[15]; " +
                " } " +
              " } " +
            " } " +
          " } " +
          " else " +
          " { " +
            " if (bone < 24) " +
            " { " +
              " if (bone < 20) " +
              " { " +
                " if (bone < 18) " +
                " { " +
                  " if (bone == 16) m44 = g_bone[16];  else   m44 = g_bone[17]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 18) m44 = g_bone[18]; else   m44 = g_bone[19]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 22) " +
                " { " +
                  " if (bone == 20) m44 = g_bone[20];  else   m44 = g_bone[21]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 22) m44 = g_bone[22];  else   m44 = g_bone[23]; " +
                " } " +
              " } " +
            " } " +
            " else " +
            " { " +
              " if (bone < 28) " +
              " { " +
                " if (bone < 26) " +
                " { " +
                  " if (bone == 24) m44 = g_bone[24];  else   m44 = g_bone[25]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 26) m44 = g_bone[26];  else   m44 = g_bone[27]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 30) " +
                " { " +
                  " if (bone == 28) m44 = g_bone[28];  else   m44 = g_bone[29]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 30) m44 = g_bone[30];  else   m44 = g_bone[31]; " +
                " } " +
              " } " +
            " } " +
          " } " +
        " } " +
        " else " +
        " { " +
          " if (bone < 48) " +
          " { " +
            " if (bone < 40) " +
            " { " +
              " if (bone < 36) " +
              " { " +
                " if (bone < 34) " +
                " { " +
                  " if (bone == 32) m44 = g_bone[32];  else   m44 = g_bone[33]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 34) m44 = g_bone[34];  else   m44 = g_bone[35]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 38) " +
                " { " +
                  " if (bone == 36) m44 = g_bone[36];  else   m44 = g_bone[37]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 38) m44 = g_bone[38];  else   m44 = g_bone[39]; " +
                " } " +
              " } " +
            " } " +
            " else " +
            " { " +
              " if (bone < 44) " +
              " { " +
                " if (bone < 42) " +
                " { " +
                  " if (bone == 40) m44 = g_bone[40];  else   m44 = g_bone[41]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 42) m44 = g_bone[42];  else   m44 = g_bone[43]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 46) " +
                " { " +
                  " if (bone == 44) m44 = g_bone[44];  else   m44 = g_bone[45]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 46) m44 = g_bone[46];  else  m44 = g_bone[47]; " +
                " } " +
              " } " +
            " } " +
          " } " +
          " else " +
          " { " +
            " if (bone < 56) " +
            " { " +
              " if (bone < 52) " +
              " { " +
                " if (bone < 50) " +
                " { " +
                  " if (bone == 48) m44 = g_bone[48]; else   m44 = g_bone[49]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 50) m44 = g_bone[50];  else   m44 = g_bone[51]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 54) " +
                " { " +
                  " if (bone == 52) m44 = g_bone[52];  else   m44 = g_bone[53]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 54) m44 = g_bone[54];  else   m44 = g_bone[55]; " +
                " } " +
              " } " +
            " } " +
            " else " +
            " { " +
              " if (bone < 60) " +
              " { " +
                " if (bone < 58) " +
                " { " +
                  " if (bone == 56) m44 = g_bone[56];  else  m44 = g_bone[57]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 58) m44 = g_bone[58];  else   m44 = g_bone[59]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 62) " +
                " { " +
                  " if (bone == 60) m44 = g_bone[60];  else   m44 = g_bone[61]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 62) m44 = g_bone[62]; else  m44 = g_bone[63]; " +
                " } " +
              " } " +
            " } " +
          " } " +
        " } " +
      " } " +
      " else if (bone < 128) " +
      " { " +
        " if (bone < 96) " +
        " { " +
          " if (bone < 80) " +
          " { " +
            " if (bone < 72) " +
            " { " +
              " if (bone < 68) " +
              " { " +
                " if (bone < 66) " +
                " { " +
                  " if (bone == 64) m44 = g_bone[64]; else m44 = g_bone[65]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 66) m44 = g_bone[66]; else m44 = g_bone[67]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 70) " +
                " { " +
                  " if (bone == 68) m44 = g_bone[68]; else m44 = g_bone[69]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 70) m44 = g_bone[70]; else m44 = g_bone[71]; " +
                " } " +
              " } " +
            " } " +
            " else " +
            " { " +
              " if (bone < 76) " +
              " { " +
                " if (bone < 74) " +
                " { " +
                  " if (bone == 72) m44 = g_bone[72]; else m44 = g_bone[73]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 74) m44 = g_bone[74]; else m44 = g_bone[75]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 78) " +
                " { " +
                  " if (bone == 76) m44 = g_bone[76]; else m44 = g_bone[77]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 78) m44 = g_bone[78]; else m44 = g_bone[79]; " +
                " } " +
              " } " +
            " } " +
          " } " +
          " else " +
          " { " +
            " if (bone < 88) " +
            " { " +
              " if (bone < 84) " +
              " { " +
                " if (bone < 82) " +
                " { " +
                  " if (bone == 80) m44 = g_bone[80]; else m44 = g_bone[81]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 82) m44 = g_bone[82]; else m44 = g_bone[83]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 86) " +
                " { " +
                  " if (bone == 84) m44 = g_bone[84]; else m44 = g_bone[85]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 86) m44 = g_bone[86]; else m44 = g_bone[87]; " +
                " } " +
              " } " +
            " } " +
            " else " +
            " { " +
              " if (bone < 92) " +
              " { " +
                " if (bone < 90) " +
                " { " +
                  " if (bone == 88) m44 = g_bone[88]; else m44 = g_bone[89]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 90) m44 = g_bone[90]; else m44 = g_bone[91]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 94) " +
                " { " +
                  " if (bone == 92) m44 = g_bone[92]; else m44 = g_bone[93]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 94) m44 = g_bone[94]; else m44 = g_bone[95]; " +
                " } " +
              " } " +
            " } " +
          " } " +
        " } " +
        " else " +
        " { " +
          " if (bone < 112) " +
          " { " +
            " if (bone < 104) " +
            " { " +
              " if (bone < 100) " +
              " { " +
                " if (bone < 98) " +
                " { " +
                  " if (bone == 96)  m44 = g_bone[96]; else  m44 = g_bone[97]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 98)  m44 = g_bone[98];  else  m44 = g_bone[99]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 102) " +
                " { " +
                  " if (bone == 100) m44 = g_bone[100]; else m44 = g_bone[101]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 102) m44 = g_bone[102]; else m44 = g_bone[103]; " +
                " } " +
              " } " +
            " } " +
            " else " +
            " { " +
              " if (bone < 108) " +
              " { " +
                " if (bone < 106) " +
                " { " +
                  " if (bone == 104) m44 = g_bone[104]; else m44 = g_bone[105]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 106) m44 = g_bone[106]; else m44 = g_bone[107]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 110) " +
                " { " +
                  " if (bone == 108) m44 = g_bone[108]; else m44 = g_bone[109]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 110) m44 = g_bone[110]; else m44 = g_bone[111]; " +
                " } " +
              " } " +
            " } " +
          " } " +
          " else " +
          " { " +
            " if (bone < 120) " +
            " { " +
              " if (bone < 116) " +
              " { " +
                " if (bone < 114) " +
                " { " +
                  " if (bone == 112) m44 = g_bone[112]; else m44 = g_bone[113]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 114) m44 = g_bone[114]; else m44 = g_bone[115]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 118) " +
                " { " +
                  " if (bone == 116) m44 = g_bone[116]; else m44 = g_bone[117]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 118) m44 = g_bone[118]; else m44 = g_bone[119]; " +
                " } " +
              " } " +
            " } " +
            " else " +
            " { " +
              " if (bone < 124) " +
              " { " +
                " if (bone < 122) " +
                " { " +
                  " if (bone == 120) m44 = g_bone[120]; else m44 = g_bone[121]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 122) m44 = g_bone[122]; else m44 = g_bone[123]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 126) " +
                " { " +
                  " if (bone == 124) m44 = g_bone[124]; else m44 = g_bone[125]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 126) m44 = g_bone[126]; else m44 = g_bone[127]; " +
                " } " +
              " } " +
            " } " +
          " } " +
        " } " +
      " } " +
      " else " +
      " { " +
        " if (bone < 160) " +
        " { " +
          " if (bone < 144) " +
          " { " +
            " if (bone < 136) " +
            " { " +
              " if (bone < 132) " +
              " { " +
                " if (bone < 130) " +
                " { " +
                  " if (bone == 128) m44 = g_bone[128]; else m44 = g_bone[129]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 130) m44 = g_bone[130];  else m44 = g_bone[131]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 134) " +
                " { " +
                  " if (bone == 132) m44 = g_bone[132]; else m44 = g_bone[133]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 134) m44 = g_bone[134]; else m44 = g_bone[135]; " +
                " } " +
              " } " +
            " } " +
            " else " +
            " { " +
              " if (bone < 140) " +
              " { " +
                " if (bone < 138) " +
                " { " +
                  " if (bone == 136) m44 = g_bone[136]; else m44 = g_bone[137]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 138) m44 = g_bone[138]; else m44 = g_bone[139]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 142) " +
                " { " +
                  " if (bone == 140) m44 = g_bone[140]; else m44 = g_bone[141]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 142) m44 = g_bone[142]; else m44 = g_bone[143]; " +
                " } " +
              " } " +
            " } " +
          " } " +
          " else " +
          " { " +
            " if (bone < 152) " +
            " { " +
              " if (bone < 148) " +
              " { " +
                " if (bone < 146) " +
                " { " +
                  " if (bone == 144) m44 = g_bone[144]; else m44 = g_bone[145]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 146) m44 = g_bone[146]; else m44 = g_bone[147]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 150) " +
                " { " +
                  " if (bone == 148) m44 = g_bone[148]; else m44 = g_bone[149]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 150) m44 = g_bone[150]; else m44 = g_bone[151]; " +
                " } " +
              " } " +
            " } " +
            " else " +
            " { " +
              " if (bone < 156) " +
              " { " +
                " if (bone < 154) " +
                " { " +
                  " if (bone == 152) m44 = g_bone[152]; else m44 = g_bone[153]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 154) m44 = g_bone[154]; else m44 = g_bone[155]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 158) " +
                " { " +
                  " if (bone == 156) m44 = g_bone[156]; else m44 = g_bone[157]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 158) m44 = g_bone[158]; else  m44 = g_bone[159]; " +
                " } " +
              " } " +
            " } " +
          " } " +
        " } " +
        " else " +
        " { " +
          " if (bone < 176) " +
          " { " +
            " if (bone < 168) " +
            " { " +
              " if (bone < 164) " +
              " { " +
                " if (bone < 162) " +
                " { " +
                  " if (bone == 160) m44 = g_bone[160]; else m44 = g_bone[161]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 162) m44 = g_bone[162]; else m44 = g_bone[163]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 166) " +
                " { " +
                  " if (bone == 164) m44 = g_bone[164];  else m44 = g_bone[165]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 166) m44 = g_bone[166]; else m44 = g_bone[167]; " +
                " } " +
              " } " +
            " } " +
            " else " +
            " { " +
              " if (bone < 172) " +
              " { " +
                " if (bone < 170) " +
                " { " +
                  " if (bone == 168) m44 = g_bone[168]; else m44 = g_bone[169]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 170) m44 = g_bone[170]; else m44 = g_bone[171]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 174) " +
                " { " +
                  " if (bone == 172) m44 = g_bone[172]; else  m44 = g_bone[173]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 174) m44 = g_bone[174]; else m44 = g_bone[175]; " +
                " } " +
              " } " +
            " } " +
          " } " +
          " else " +
          " { " +
            " if (bone < 184) " +
            " { " +
              " if (bone < 180) " +
              " { " +
                " if (bone < 178) " +
                " { " +
                  " if (bone == 176) m44 = g_bone[176]; else m44 = g_bone[177]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 178) m44 = g_bone[178]; else m44 = g_bone[179]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 182) " +
                " { " +
                  " if (bone == 180) m44 = g_bone[180];  else  m44 = g_bone[181]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 182) m44 = g_bone[182]; else m44 = g_bone[183]; " +
                " } " +
              " } " +
            " } " +
            " else " +
            " { " +
              " if (bone < 188) " +
              " { " +
                " if (bone < 186) " +
                " { " +
                  " if (bone == 184) m44 = g_bone[184]; else m44 = g_bone[185]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 186) m44 = g_bone[186]; else m44 = g_bone[187]; " +
                " } " +
              " } " +
              " else " +
              " { " +
                " if (bone < 190) " +
                " { " +
                  " if (bone == 188) m44 = g_bone[188]; else m44 = g_bone[189]; " +
                " } " +
                " else " +
                " { " +
                  " if (bone == 190) m44 = g_bone[190]; else m44 = g_bone[191]; " +
                " } " +
              " } " +
            " } " +
          " } " +
        " } " +
      " } " +
      
      " return m44; " +
   " } " +
  "}";

//---------------------------------------------------------------------

#if 0
const string BONES_MAT = 
"Texture2D boneTexture : register(t0); " +

"float4x4 GetBoneMatrix (int boneIndex) " +
"{" +
 " float4 r0 = boneTexture.Load(int3(0, boneIndex, 0)); " +
 " float4 r1 = boneTexture.Load(int3(1, boneIndex, 0)); " +
 " float4 r2 = boneTexture.Load(int3(2, boneIndex, 0)); " +
 " float4 r3 = boneTexture.Load(int3(3, boneIndex, 0)); " +
 " return transpose((float4x4(r0, r1, r2, r3))); " +
"}";
#endif

//---------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
// vertex shader for rigged mesh
////////////////////////////////////////////////////////////////////////////////

const string VERTEX_SHADER_RIGGED_RENDERING =

    SHADOW_COMPUTATION + GET_BONE_MATRIX +

    "PixelInputType MyVertexShader (VertexInputTypeRiggedMesh input)  " +
    "{  " +
      "PixelInputType output;  " +
      "int            i;       " +
      "float3         position, normal, tangent; " +

      "position = float3(0.0, 0.0, 0.0);  " +
      "normal   = float3(0.0, 0.0, 0.0);  " +
      "tangent  = float3(0.0, 0.0, 0.0);  " +

      "for (i=0; i<4; i++) " +
      "{ " +
        "if (input.weight[i] <= 0.0) break; " +

        "int bone = (input.bone >> (8*i)) & 255; " +
        
        "matrix<float,4,4> m44 = GetBoneMatrix (bone);  " +

        "position += (float3)(input.weight[i] * mul(m44, input.position));  " +
        "normal   += input.weight[i] * mul((float3x3)m44, input.normal);  " +
        "tangent  += input.weight[i] * mul((float3x3)m44, input.tangent);  " +
      "}  " +

      // Calculate the position of the vertex against the world, view, and projection matrices.
      "output.posW     = mul(float4(position,1.0), (float4x3)worldMatrix);  " +
      "output.position = mul(mul(float4(output.posW,1.0), viewMatrix), projectionMatrix);  " +

      // Calculate the normal vector against the world matrix only.
      "output.normal = mul(normalize(normal), (float3x3)worldMatrix);  " +
      "output.tangent = mul(normalize(tangent), (float3x3)worldMatrix);  " +

      "output.diffuse = float4(1.0, 1.0, 1.0, 1.0);  " +

      // compute the texture coordinates for the pixel shader.
      "output.tex[0] = input.tex[0] * (fact_u_min_1 + 1.0)   +   input.tex[1] * fact_u   +   ofs_u; " +
      "output.tex[1] = input.tex[1] * (fact_v_min_1 + 1.0)   +   input.tex[0] * fact_v   +   ofs_v; " +

      // compute shadow position
      "output.ShadowPos = compute_shadow_position (output.posW, input.instance); " +

      "return output;  " +
    "} ";


//---------------------------------------------------------------------

const string VERTEX_SHADER_RIGGED_BUILD_SHADOW_MAP =

    SHADOW_COMPUTATION + GET_BONE_MATRIX +

    "ShadowVertexOutputType MyVertexShader (ShadowVertexInputTypeRiggedMesh input)  " +
    "{  " +
      "ShadowVertexOutputType output;  " +
      "int            i;       " +
      "float3         position;  " +

      "position = float3(0.0, 0.0, 0.0);  " +

      "for (i=0; i<4; i++) " +
      "{ " +
        "if (input.weight[i] <= 0.0) break; " +

        "int bone = (input.bone >> (8*i)) & 255; " +
        
        "matrix<float,4,4> m44 = GetBoneMatrix (bone);  " +

        "position += (float3)(input.weight[i] * mul(m44, input.position));  " +
      "}  " +

      // Calculate the position of the vertex against the world, view, and projection matrices.
      "float3 posW = mul(float4(position,1.0), (float4x3)worldMatrix);  " +

      // compute shadow position
      "output.position.xyz = compute_shadow_position (posW, input.instance); " +
      "output.position.w = 1.0; " +

      "output.RenderTargetIndex = input.instance; " +

      "return output;  " +
    "} ";

//---------------------------------------------------------------------
