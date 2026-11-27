#include <windows.h>
#include <initguid.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <assert.h>
#include "rsx_d3d12_pipeline_cache.h"
#pragma comment(lib,"d3dcompiler.lib")
static unsigned calls,cached_calls,reject_cached;
static const char driver_data[]="test driver pipeline data";
static HRESULT STDMETHODCALLTYPE cached_blob(ID3D12PipelineState*p,ID3DBlob**out)
{
 (void)p;HRESULT hr=D3DCreateBlob(sizeof driver_data,out);
 if(SUCCEEDED(hr))memcpy((*out)->lpVtbl->GetBufferPointer(*out),driver_data,sizeof driver_data);
 return hr;
}
static ULONG STDMETHODCALLTYPE release_pso(ID3D12PipelineState*p){(void)p;return 1;}
static ID3D12PipelineStateVtbl pso_methods={.Release=release_pso,.GetCachedBlob=cached_blob};
static ID3D12PipelineState fake_pso={&pso_methods};
static HRESULT STDMETHODCALLTYPE create_pso(ID3D12Device*device,const D3D12_GRAPHICS_PIPELINE_STATE_DESC*d,REFIID iid,void**out)
{
 (void)device;(void)iid;calls++;*out=NULL;
 if(d->CachedPSO.pCachedBlob){
  cached_calls++;
  assert(d->CachedPSO.CachedBlobSizeInBytes==sizeof driver_data);
  assert(!memcmp(d->CachedPSO.pCachedBlob,driver_data,sizeof driver_data));
  if(reject_cached)return E_INVALIDARG;
 }
 *out=&fake_pso;return S_OK;
}
static ID3D12DeviceVtbl device_methods={.CreateGraphicsPipelineState=create_pso};
static ID3D12Device fake_device={&device_methods};
int main(void)
{
 _putenv_s("PS3RECOMP_D3D12_PIPELINE_CACHE","cache");
 D3D12_GRAPHICS_PIPELINE_STATE_DESC d={0};
 const unsigned char shader[]={1,2,3,4},root[]={5,6,7,8};
 D3D12_INPUT_ELEMENT_DESC input={"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0};
 d.VS.pShaderBytecode=shader;d.VS.BytecodeLength=sizeof shader;
 d.InputLayout.pInputElementDescs=&input;d.InputLayout.NumElements=1;
 ID3D12PipelineState*p=NULL;
 assert(SUCCEEDED(rsx_d3d12_create_pso_cached(&fake_device,&d,root,sizeof root,&p)));
 assert(calls==1 && cached_calls==0 && p==&fake_pso);
 unsigned char relocated_shader[sizeof shader],relocated_root[sizeof root];
 memcpy(relocated_shader,shader,sizeof shader);memcpy(relocated_root,root,sizeof root);
 char relocated_semantic[]="POSITION";input.SemanticName=relocated_semantic;
 d.VS.pShaderBytecode=relocated_shader;d.pRootSignature=(ID3D12RootSignature*)(uintptr_t)123;
 assert(SUCCEEDED(rsx_d3d12_create_pso_cached(&fake_device,&d,relocated_root,sizeof root,&p)));
 assert(calls==2 && cached_calls==1);
 reject_cached=1;
 assert(SUCCEEDED(rsx_d3d12_create_pso_cached(&fake_device,&d,root,sizeof root,&p)));
 assert(calls==4 && cached_calls==2);reject_cached=0;
 rsx_pso_bytes key=rsx_pso_identity(&d,root,sizeof root);assert(!key.failed);
 char path[MAX_PATH];snprintf(path,sizeof path,"cache/pso-%016llx.bin",(unsigned long long)rsx_pso_hash(key.p,key.n));
 FILE*f=fopen(path,"r+b");assert(f);fseek(f,-1,SEEK_END);fputc(99,f);fclose(f);
 assert(SUCCEEDED(rsx_d3d12_create_pso_cached(&fake_device,&d,root,sizeof root,&p)));
 assert(calls==5 && cached_calls==2); /* corrupt blob never reaches the driver */
 f=fopen(path,"r+b");assert(f);rsx_pso_header h;assert(fread(&h,1,sizeof h,f)==sizeof h);h.blob_size=0xffffffffu;rewind(f);fwrite(&h,1,sizeof h,f);fclose(f);
 assert(SUCCEEDED(rsx_d3d12_create_pso_cached(&fake_device,&d,root,sizeof root,&p)));
 assert(calls==6 && cached_calls==2);
 relocated_shader[0]++;
 assert(SUCCEEDED(rsx_d3d12_create_pso_cached(&fake_device,&d,root,sizeof root,&p)));
 assert(calls==7 && cached_calls==2);
 d.BlendState.RenderTarget[0].RenderTargetWriteMask=15;
 assert(SUCCEEDED(rsx_d3d12_create_pso_cached(&fake_device,&d,root,sizeof root,&p)));
 assert(calls==8 && cached_calls==2);
 relocated_root[0]++;
 assert(SUCCEEDED(rsx_d3d12_create_pso_cached(&fake_device,&d,relocated_root,sizeof root,&p)));
 assert(calls==9 && cached_calls==2);
 free(key.p);
 puts("PASS exact relocated description, root/shader/state identity, driver rejection retry, corrupt blob and oversized cache fallback");
 return 0;
}
