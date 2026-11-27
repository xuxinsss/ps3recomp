/* D3D12 has no programmable point size. Expand six copies of each point in
 * the VS, after the guest transform. Only selected point-sprite texcoords are
 * replaced; the original fragment program and alpha test remain intact.
 * SV_VertexID is draw-relative in D3D: do NOT subtract StartVertexLocation. */
#ifndef RSX_D3D12_POINT_QUAD_H
#define RSX_D3D12_POINT_QUAD_H
#include "rsx_point_sprite.h"
static inline int rsx_d3d12_point_quad_patch(char* s, size_t cap, unsigned key)
{
 if (!key) return 0;
 if (!s || !cap) return -1;
 if (strstr(s,"_d3d_point_vertex")) return 0;
 char* main=strstr(s,"main(VSInput input)");
 char* row=strstr(s,"    float4 vp_posoffset;\n");
 char* ret=strstr(s,"    return Out;\n");
 if(!main||!row||!ret||row>=main||main>=ret)return -1;
 const char* arg=", uint _d3d_point_vertex : SV_VertexID";
 const char* uniforms="    float4 d3d_point_size;\n    float4 d3d_point_view;\n";
 char code[2048];
 int n=snprintf(code,sizeof code,
  "    const float2 _corner[6]={float2(0,0),float2(1,0),float2(1,1),float2(0,0),float2(1,1),float2(0,1)};\n"
  "    float2 _uv=_corner[_d3d_point_vertex%%6u];\n"
  "    float _sz=d3d_point_size.y!=0.0 ? o[6].x : d3d_point_size.x;\n"
  "    _sz=(_sz==_sz)?_sz:1.0; _sz=clamp(_sz,1.0,d3d_point_size.z);\n"
  "    bool _visible=Out.pos.w>0 && abs(Out.pos.x)<=Out.pos.w && abs(Out.pos.y)<=Out.pos.w && Out.pos.z>=0 && Out.pos.z<=Out.pos.w;\n"
  "    Out.pos.xy+=float2(_uv.x*2-1,1-_uv.y*2)*_sz*d3d_point_view.xy*Out.pos.w;\n"
  "    if(!_visible) Out.pos=float4(0,0,-1,1);\n");
 if(n<0||(size_t)n>=sizeof code)return -1;
 for(unsigned u=0;u<10;u++)if(key&(1u<<u)){
  int a=snprintf(code+n,sizeof code-n,"    Out.t%u=float4(_uv,0,1);\n",u);
  if(a<0||(size_t)a>=sizeof code-n)return -1;n+=a;
 }
 if(strlen(s)+strlen(arg)+strlen(uniforms)+strlen(code)+1>cap)return -1;
 size_t a=(size_t)(main-s)+strlen("main(VSInput input");
 size_t b=(size_t)(row-s)+strlen("    float4 vp_posoffset;\n");
 size_t c=(size_t)(ret-s);
 rsx_point_sprite_insert(s,c,code);
 rsx_point_sprite_insert(s,a,arg);
 rsx_point_sprite_insert(s,b,uniforms);
 return 1;
}
#endif
