/* Optional exact-description driver PSO cache. A cached blob is only a hint:
 * the original description is always supplied, and any rejection retries
 * without cached data (including driver/adapter changes). */
#ifndef RSX_D3D12_PIPELINE_CACHE_H
#define RSX_D3D12_PIPELINE_CACHE_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define RSX_PSO_LIMIT (64u*1024u*1024u)
typedef struct { unsigned char* p; size_t n; int failed; } rsx_pso_bytes;
static void rsx_pso_append(rsx_pso_bytes* b,const void*p,size_t n)
{
 if(b->failed)return;
 if((n && !p) || n>RSX_PSO_LIMIT || b->n>RSX_PSO_LIMIT-n){b->failed=1;return;}
 if(!n)return;
 void*q=realloc(b->p,b->n+n);if(!q){b->failed=1;return;}
 b->p=(unsigned char*)q;memcpy(b->p+b->n,p,n);b->n+=n;
}
static uint64_t rsx_pso_hash(const void*p,size_t n)
{
 const unsigned char*s=(const unsigned char*)p;uint64_t h=UINT64_C(14695981039346656037);
 while(n--)h=(h^*s++)*UINT64_C(1099511628211);return h;
}
static rsx_pso_bytes rsx_pso_identity(const D3D12_GRAPHICS_PIPELINE_STATE_DESC*d,
                                    const void*root,size_t root_size)
{
 rsx_pso_bytes b={0};
 if(!root || !root_size || root_size>65536 || d->InputLayout.NumElements>32 ||
    (d->InputLayout.NumElements && !d->InputLayout.pInputElementDescs) ||
    d->StreamOutput.NumEntries || d->StreamOutput.NumStrides){b.failed=1;return b;}
 D3D12_GRAPHICS_PIPELINE_STATE_DESC c=*d;
 c.pRootSignature=NULL;c.VS.pShaderBytecode=NULL;c.PS.pShaderBytecode=NULL;
 c.DS.pShaderBytecode=NULL;c.HS.pShaderBytecode=NULL;c.GS.pShaderBytecode=NULL;
 c.InputLayout.pInputElementDescs=NULL;
 c.StreamOutput.pSODeclaration=NULL;c.StreamOutput.pBufferStrides=NULL;
 memset(&c.CachedPSO,0,sizeof c.CachedPSO);
 rsx_pso_append(&b,&c,sizeof c);
 uint32_t rn=(uint32_t)root_size;rsx_pso_append(&b,&rn,sizeof rn);rsx_pso_append(&b,root,root_size);
 const D3D12_SHADER_BYTECODE stages[]={d->VS,d->PS,d->DS,d->HS,d->GS};
 for(unsigned i=0;i<5;i++)rsx_pso_append(&b,stages[i].pShaderBytecode,stages[i].BytecodeLength);
 for(unsigned i=0;i<d->InputLayout.NumElements;i++){
  const D3D12_INPUT_ELEMENT_DESC*e=&d->InputLayout.pInputElementDescs[i];
  if(!e->SemanticName || strlen(e->SemanticName)>255){b.failed=1;break;}
  D3D12_INPUT_ELEMENT_DESC v=*e;v.SemanticName=NULL;rsx_pso_append(&b,&v,sizeof v);
  rsx_pso_append(&b,e->SemanticName,strlen(e->SemanticName)+1);
 }
 return b;
}
typedef struct { uint32_t magic,version,key_size,blob_size;uint64_t blob_hash; } rsx_pso_header;
static void* rsx_pso_read(const char*path,const rsx_pso_bytes*key,size_t*size)
{
 *size=0;FILE*f=fopen(path,"rb");if(!f)return NULL;
 rsx_pso_header h;void*saved=NULL,*blob=NULL;
 if(fread(&h,1,sizeof h,f)!=sizeof h || h.magic!=0x314F5350 || h.version!=1 ||
    h.key_size!=key->n || !h.blob_size || h.blob_size>RSX_PSO_LIMIT)goto done;
 saved=malloc(h.key_size);if(!saved)goto done;
 if(fread(saved,1,h.key_size,f)!=h.key_size || memcmp(saved,key->p,key->n))goto done;
 blob=malloc(h.blob_size);if(!blob)goto done;
 if(fread(blob,1,h.blob_size,f)!=h.blob_size || fgetc(f)!=EOF || rsx_pso_hash(blob,h.blob_size)!=h.blob_hash){free(blob);blob=NULL;goto done;}
 *size=h.blob_size;
done:free(saved);fclose(f);return blob;
}
static void rsx_pso_write(const char*path,const rsx_pso_bytes*key,ID3D12PipelineState*pso)
{
 ID3DBlob*blob=NULL;
 HRESULT cached_result=pso->lpVtbl->GetCachedBlob(pso,&blob);
 if(FAILED(cached_result) || !blob){if(blob)blob->lpVtbl->Release(blob);return;}
 size_t n=blob->lpVtbl->GetBufferSize(blob);const void*data=blob->lpVtbl->GetBufferPointer(blob);
 if(n && n<=RSX_PSO_LIMIT){
  rsx_pso_header h={0x314F5350,1,(uint32_t)key->n,(uint32_t)n,rsx_pso_hash(data,n)};
  char temp[MAX_PATH];int len=snprintf(temp,sizeof temp,"%s.%lu.%lu.tmp",path,GetCurrentProcessId(),GetCurrentThreadId());
  if(len>0 && len<(int)sizeof temp){
   FILE*f=fopen(temp,"wb");if(f){
    int ok=fwrite(&h,1,sizeof h,f)==sizeof h && fwrite(key->p,1,key->n,f)==key->n && fwrite(data,1,n,f)==n;
    if(fclose(f))ok=0;
    if(ok)MoveFileExA(temp,path,MOVEFILE_REPLACE_EXISTING);
    DeleteFileA(temp);
   }
  }
 }
 blob->lpVtbl->Release(blob);
}
static HRESULT rsx_d3d12_create_pso_cached(ID3D12Device*device,
 const D3D12_GRAPHICS_PIPELINE_STATE_DESC*d,const void*root,size_t root_size,
 ID3D12PipelineState**result)
{
 const char*dir=getenv("PS3RECOMP_D3D12_PIPELINE_CACHE");
 if(!dir || !*dir || d->CachedPSO.pCachedBlob || d->CachedPSO.CachedBlobSizeInBytes)
  return device->lpVtbl->CreateGraphicsPipelineState(device,d,&IID_ID3D12PipelineState,(void**)result);
 rsx_pso_bytes key=rsx_pso_identity(d,root,root_size);
 if(key.failed){free(key.p);return device->lpVtbl->CreateGraphicsPipelineState(device,d,&IID_ID3D12PipelineState,(void**)result);}
 char path[MAX_PATH];int len=snprintf(path,sizeof path,"%s/pso-%016llx.bin",dir,(unsigned long long)rsx_pso_hash(key.p,key.n));
 if(len<0 || len>=(int)sizeof path){free(key.p);return device->lpVtbl->CreateGraphicsPipelineState(device,d,&IID_ID3D12PipelineState,(void**)result);}
 CreateDirectoryA(dir,NULL);
 size_t size=0;void*blob=rsx_pso_read(path,&key,&size);
 D3D12_GRAPHICS_PIPELINE_STATE_DESC cached=*d;cached.CachedPSO.pCachedBlob=blob;cached.CachedPSO.CachedBlobSizeInBytes=size;
 *result=NULL;
 HRESULT hr=device->lpVtbl->CreateGraphicsPipelineState(device,&cached,&IID_ID3D12PipelineState,(void**)result);
 if(FAILED(hr) && blob){
  if(*result){(*result)->lpVtbl->Release(*result);*result=NULL;}
  hr=device->lpVtbl->CreateGraphicsPipelineState(device,d,&IID_ID3D12PipelineState,(void**)result);
  free(blob);blob=NULL;
 }
 if(SUCCEEDED(hr) && *result && !blob)rsx_pso_write(path,&key,*result);
 free(blob);free(key.p);return hr;
}
#endif
