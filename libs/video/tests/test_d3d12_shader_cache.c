#include <windows.h>
#include <d3dcompiler.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned compiles;
static HRESULT counted_compile(LPCVOID s,SIZE_T n,LPCSTR name,const D3D_SHADER_MACRO* d,ID3DInclude* i,LPCSTR e,LPCSTR t,UINT f,UINT g,ID3DBlob** b,ID3DBlob** err)
{compiles++;return D3DCompile(s,n,name,d,i,e,t,f,g,b,err);}
#define RSX_D3D12_COMPILE counted_compile
#include "rsx_d3d12_shader_cache.h"
#include "rsx_d3d12_null_srv.h"
#pragma comment(lib,"d3dcompiler.lib")
static const char source[]="float4 main():SV_Target{return float4(1,0,0,1);}";
static void compile_ok(const char* text,UINT flags,const char* target)
{
 ID3DBlob* code=NULL;ID3DBlob* err=NULL;
 HRESULT hr=rsx_d3d12_compile_cached(text,strlen(text),"fixture",NULL,NULL,"main",target,flags,0,&code,&err);
 assert(SUCCEEDED(hr)&&code);assert(!memcmp(code->lpVtbl->GetBufferPointer(code),"DXBC",4));
 if(err)err->lpVtbl->Release(err);code->lpVtbl->Release(code);
}
int main(void)
{
 rsx_d3d12_null_srv view={0};
 assert(!rsx_d3d12_null_srv_reuse(&view,0,28,0x1688,0));
 assert(rsx_d3d12_null_srv_reuse(&view,0,28,0x1688,0));
 assert(!rsx_d3d12_null_srv_reuse(&view,1,28,0x1688,0));
 assert(!rsx_d3d12_null_srv_reuse(&view,0,28,0x1688,0)); /* actual resource overwritten */
 assert(!rsx_d3d12_null_srv_reuse(&view,0,28,0x1689,0));
 assert(!rsx_d3d12_null_srv_reuse(&view,0,28,0x1689,1));
 assert(!rsx_d3d12_null_srv_reuse(&view,0,29,0x1689,1));
 char dir[MAX_PATH];snprintf(dir,sizeof dir,"cache-fixture-%lu",GetCurrentProcessId());
 _putenv_s("PS3RECOMP_D3D12_SHADER_CACHE",dir);_putenv_s("PS3RECOMP_D3D12_SHADER_CACHE_DISABLE","");
 compile_ok(source,0,"ps_5_0");assert(compiles==1);
 compile_ok(source,0,"ps_5_0");assert(compiles==1); /* memory reuse */
 rsx_dxbc_clear();compile_ok(source,0,"ps_5_0");assert(compiles==1); /* process-like cold memory */
 compile_ok(source,D3DCOMPILE_SKIP_OPTIMIZATION,"ps_5_0");assert(compiles==2);
 compile_ok(source,0,"ps_4_0");assert(compiles==3);
 compile_ok("float4 main():SV_Target{return float4(0,1,0,1);}",0,"ps_5_0");assert(compiles==4);
 rsx_dxbc_header h;assert(rsx_dxbc_identity(&h,strlen(source),"main","ps_5_0",0,0));
 HMODULE compiler=GetModuleHandleA("d3dcompiler_47.dll");assert(compiler && compiler!=GetModuleHandleA(NULL));
 const IMAGE_DOS_HEADER* dos=(const IMAGE_DOS_HEADER*)compiler;
 const IMAGE_NT_HEADERS* nt=(const IMAGE_NT_HEADERS*)((const char*)compiler+dos->e_lfanew);
 assert(h.compiler_stamp==nt->FileHeader.TimeDateStamp && h.compiler_size==nt->OptionalHeader.SizeOfImage);
 uint64_t key=rsx_dxbc_hash(&h,sizeof h,UINT64_C(14695981039346656037));key=rsx_dxbc_hash(source,strlen(source),key);
 char path[MAX_PATH];snprintf(path,sizeof path,"%s/dxbc-%016llx.bin",dir,(unsigned long long)key);
 rsx_dxbc_clear();FILE* f=fopen(path,"r+b");assert(f);fseek(f,-1,SEEK_END);int c=fgetc(f);fseek(f,-1,SEEK_END);fputc(c^255,f);fclose(f);
 compile_ok(source,0,"ps_5_0");assert(compiles==5); /* damaged bytecode rejected */
 rsx_dxbc_clear();f=fopen(path,"r+b");assert(f);fseek(f,sizeof h,SEEK_SET);fputc('X',f);fclose(f);
 compile_ok(source,0,"ps_5_0");assert(compiles==6); /* exact source check, even with same filename */
 rsx_dxbc_clear();f=fopen(path,"wb");assert(f);fputs("partial",f);fclose(f);
 compile_ok(source,0,"ps_5_0");assert(compiles==7); /* interrupted file */
 _putenv_s("PS3RECOMP_D3D12_SHADER_CACHE_DISABLE","1");compile_ok(source,0,"ps_5_0");assert(compiles==8);
 _putenv_s("PS3RECOMP_D3D12_SHADER_CACHE_DISABLE","");
 for(int i=0;i<2;i++){ID3DBlob* code=NULL,*err=NULL;assert(FAILED(rsx_d3d12_compile_cached("bad shader",10,"fixture",NULL,NULL,"main","ps_5_0",0,0,&code,&err)));assert(!code);if(err)err->lpVtbl->Release(err);}
 assert(compiles==10);rsx_dxbc_clear();
 puts("PASS exact source/flags/profile, memory/disk reuse, corrupted/truncated cache fallback and compile errors");return 0;
}
