/* Prepare the legacy D3D12 exact-source cache from locally supplied Cg
 * programs. No game assets, generated HLSL or DXBC belong in the repository.
 * The runtime remains authoritative: an unseen variant compiles normally.
 * Binary field layout: NVIDIA Cg program metadata (revision 6), also documented
 * by RPCS3's Emu/RSX/Program/CgBinaryProgram.h. This is a separate bounded
 * parser using this SDK's own decompilers; no RPCS3 implementation is linked.
 */
#include "rsx_vp_decompiler.h"
#include "rsx_fp_decompiler.h"
#include "rsx_d3d12_shader_cache.h"
#include "rsx_d3d12_point_quad.h"
#pragma comment(lib,"d3dcompiler.lib")

static unsigned be32(const unsigned char*p)
{ return (unsigned)p[0]<<24 | (unsigned)p[1]<<16 | (unsigned)p[2]<<8 | p[3]; }
static unsigned le32(const unsigned char*p)
{ return (unsigned)p[3]<<24 | (unsigned)p[2]<<16 | (unsigned)p[1]<<8 | p[0]; }
static int range(unsigned offset,unsigned length,unsigned total)
{ return offset<=total && length<=total-offset; }

static unsigned prepared,failed;
static void prepare(const char*text,const char*target)
{
 ID3DBlob*code=NULL,*errors=NULL;
 HRESULT hr=rsx_d3d12_compile_cached(text,strlen(text),"precompile-cg",NULL,NULL,
                                  "main",target,0,0,&code,&errors);
 if(FAILED(hr)){
  ++failed;
  fprintf(stderr,"[precompile] %s failed: %s\n",target,
      errors?(const char*)errors->lpVtbl->GetBufferPointer(errors):"compiler failure");
 }else ++prepared;
 if(code)code->lpVtbl->Release(code);
 if(errors)errors->lpVtbl->Release(errors);
}

int main(int argc,char**argv)
{
 if(argc!=3 && argc!=5){fprintf(stderr,"Usage: d3d12-precompile-cg.exe input.elf cache-directory [worker-index worker-count]\n");return 2;}
 unsigned worker=argc==5?(unsigned)atoi(argv[3]):0;
 unsigned workers=argc==5?(unsigned)atoi(argv[4]):1;
 if(!workers || workers>8 || worker>=workers)return 2;
 if(!CreateDirectoryA(argv[2],NULL) && GetLastError()!=ERROR_ALREADY_EXISTS){
  fprintf(stderr,"Cannot create cache directory\n");return 2;
 }
 DWORD attributes=GetFileAttributesA(argv[2]);
 if(attributes==INVALID_FILE_ATTRIBUTES || !(attributes&FILE_ATTRIBUTE_DIRECTORY))return 2;
 _putenv_s("PS3RECOMP_D3D12_SHADER_CACHE",argv[2]);
 _putenv_s("PS3RECOMP_D3D12_SHADER_CACHE_DISABLE","");
 FILE*f=fopen(argv[1],"rb");if(!f)return 2;
 if(fseek(f,0,SEEK_END)){fclose(f);return 2;}
 long length=ftell(f);
 if(length<32 || length>512*1024*1024){fclose(f);return 2;}
 rewind(f);unsigned char*data=(unsigned char*)malloc((size_t)length);
 if(!data){fclose(f);return 2;}
 size_t got=fread(data,1,(size_t)length,f);fclose(f);
 if(got!=(size_t)length){free(data);return 2;}
 unsigned programs=0,handled=0,skipped=0;
 ULONGLONG start=GetTickCount64();
 static char text[262144];static unsigned char code[65536];
 for(size_t at=0;at+32<=(size_t)length;at+=4){
  const unsigned char*p=data+at;unsigned profile=be32(p);
  if(profile!=7003 && profile!=7004)continue;
  unsigned total=be32(p+8),count=be32(p+12),params=be32(p+16);
  unsigned program=be32(p+20),size=be32(p+24),ucode=be32(p+28);
  if(be32(p+4)!=6 || total<32 || total>1024*1024 || total>(size_t)length-at ||
     !size || size>sizeof code || size%16 || ucode<32 || program<32 ||
     !range(ucode,size,total) || !range(program,24,total) ||
     count>total/48 || !range(params,count*48,total))continue;
  ++programs;
  if((programs-1)%workers!=worker)continue;
  ++handled;
  if(profile==7003){
   unsigned branch_mask=0,branch_ids[32],nb=0;
   for(unsigned i=0;i<size;i+=4){code[i]=p[ucode+i+3];code[i+1]=p[ucode+i+2];code[i+2]=p[ucode+i+1];code[i+3]=p[ucode+i];}
   unsigned instructions=rsx_vp_program_size_instrs(code,size);
   if(!instructions){++skipped;continue;}
   size=instructions*16;
   for(unsigned i=0;i<size;i+=16){
    unsigned op=(le32(code+i+4)>>27)&31;
    if(op==17 || op==18)branch_mask|=1u<<((le32(code+i+12)>>23)&31);
   }
   for(unsigned i=0;i<32;i++)if(branch_mask&(1u<<i))branch_ids[nb++]=i;
   /* Bound exponential work. Such programs still compile on runtime demand. */
   if(nb>8){++skipped;continue;}
   for(unsigned combination=0;combination<(1u<<nb);combination++){
    unsigned bits=0;for(unsigned i=0;i<nb;i++)if(combination&(1u<<i))bits|=1u<<branch_ids[i];
    rsx_vp_set_branch_bits(bits);
    /* Ordinary triangles, fixed points and TC4 point sprites. Other point
     * coordinate masks intentionally use the runtime fallback. */
    const unsigned sprites[]={0,0x40000000u,0x40000010u};
    for(unsigned v=0;v<3;v++){
     if(rsx_vp_decompile(code,size,text,sizeof text)<=0){++failed;continue;}
     if(sprites[v] && rsx_d3d12_point_quad_patch(text,sizeof text,sprites[v])<0){++failed;continue;}
     prepare(text,"vs_5_0");
    }
   }
  }else{
   unsigned cube=0;
   for(unsigned i=0;i<count;i++){
    const unsigned char*q=p+params+i*48;unsigned unit=be32(q+12);
    if(be32(q)==1069 && unit<16)cube|=1u<<unit;
   }
   unsigned ctrl=(p[program+19]?0:0x40)|(p[program+20]?0xe:0);
   /* The parser has validated the complete microcode range. */
   if(rsx_fp_decompile_d3d12(p+ucode,size,ctrl,cube,text,sizeof text)<=0){++failed;continue;}
   prepare(text,"ps_5_0");
  }
  if(handled%5==0){printf("[precompile worker %u] programs=%u prepared=%u failures=%u elapsed=%.1fs\n",worker,handled,prepared,failed,(GetTickCount64()-start)/1000.0);fflush(stdout);}
 }
 printf("[precompile worker %u] complete discovered=%u programs=%u prepared=%u failures=%u skipped=%u seconds=%.3f\n",worker,programs,handled,prepared,failed,skipped,(GetTickCount64()-start)/1000.0);
 free(data);rsx_dxbc_clear();return !programs || failed ? 1 : 0;
}
