/* Exact-source DXBC cache. Driver-independent bytecode only; PSOs retain
 * their complete original state keys. Missing/bad cache files compile normally. */
#ifndef RSX_D3D12_SHADER_CACHE_H
#define RSX_D3D12_SHADER_CACHE_H
#include <windows.h>
#include <d3dcompiler.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef RSX_D3D12_COMPILE
#define RSX_D3D12_COMPILE D3DCompile
#endif
#define RSX_DXBC_LIMIT (16u*1024u*1024u)
#define RSX_SOURCE_LIMIT (1024u*1024u)
#define RSX_DXBC_SLOTS 128
typedef struct {
    uint32_t magic, version, compiler_stamp, compiler_size;
    uint32_t flags1, flags2, source_size, bytecode_size;
    char entry[32], target[32];
    uint64_t bytecode_hash;
} rsx_dxbc_header;
typedef struct {
    rsx_dxbc_header header;
    uint64_t key;
    char* source;
    ID3DBlob* code;
} rsx_dxbc_entry;
static rsx_dxbc_entry rsx_dxbc_entries[RSX_DXBC_SLOTS];
static unsigned rsx_dxbc_next;
static size_t rsx_dxbc_bytes;
#define RSX_DXBC_MEMORY_LIMIT (32u*1024u*1024u)
static SRWLOCK rsx_dxbc_lock=SRWLOCK_INIT;
static uint64_t rsx_dxbc_hash(const void* bytes, size_t n, uint64_t h)
{
    const unsigned char* p=(const unsigned char*)bytes;
    while(n--) h=(h^*p++)*UINT64_C(1099511628211);
    return h;
}
static void rsx_dxbc_clear(void)
{
    AcquireSRWLockExclusive(&rsx_dxbc_lock);
    for(unsigned i=0;i<RSX_DXBC_SLOTS;i++) {
        if(rsx_dxbc_entries[i].code) rsx_dxbc_entries[i].code->lpVtbl->Release(rsx_dxbc_entries[i].code);
        free(rsx_dxbc_entries[i].source);
    }
    memset(rsx_dxbc_entries,0,sizeof rsx_dxbc_entries);rsx_dxbc_next=0;rsx_dxbc_bytes=0;
    ReleaseSRWLockExclusive(&rsx_dxbc_lock);
}
static int rsx_dxbc_identity(rsx_dxbc_header* h, size_t len, const char* entry,
                             const char* target, UINT flags1, UINT flags2)
{
    if(!len || len>RSX_SOURCE_LIMIT || !entry || !target || strlen(entry)>=32 || strlen(target)>=32) return 0;
    memset(h,0,sizeof *h);h->magic=0x43425844u;h->version=2;
    h->source_size=(uint32_t)len;h->flags1=flags1;h->flags2=flags2;
    strcpy(h->entry,entry);strcpy(h->target,target);
    /* Taking &D3DCompile can name an import thunk in the EXE with clang-cl.
     * Identify the actual compiler DLL, so rebuilding the game keeps valid DXBC. */
    HMODULE module=GetModuleHandleA("d3dcompiler_47.dll");
    if(!module || !GetProcAddress(module,"D3DCompile"))return 0;
    const IMAGE_DOS_HEADER* dos=(const IMAGE_DOS_HEADER*)module;
    const IMAGE_NT_HEADERS* nt=(const IMAGE_NT_HEADERS*)((const char*)module+dos->e_lfanew);
    h->compiler_stamp=nt->FileHeader.TimeDateStamp;h->compiler_size=nt->OptionalHeader.SizeOfImage;
    return 1;
}
static int rsx_dxbc_same(const rsx_dxbc_header* a,const rsx_dxbc_header* b)
{
    rsx_dxbc_header x=*a,y=*b;x.bytecode_size=y.bytecode_size=0;x.bytecode_hash=y.bytecode_hash=0;
    return memcmp(&x,&y,sizeof x)==0;
}
static ID3DBlob* rsx_dxbc_read(const char* path,const rsx_dxbc_header* want,const void* source)
{
    FILE* f=fopen(path,"rb");if(!f)return NULL;
    rsx_dxbc_header h;ID3DBlob* code=NULL;char* text=NULL;
    if(fread(&h,1,sizeof h,f)!=sizeof h || !rsx_dxbc_same(&h,want) || !h.bytecode_size || h.bytecode_size>RSX_DXBC_LIMIT)goto done;
    text=(char*)malloc(h.source_size);if(!text)goto done;
    if(fread(text,1,h.source_size,f)!=h.source_size || memcmp(text,source,h.source_size))goto done;
    if(FAILED(D3DCreateBlob(h.bytecode_size,&code)))goto done;
    void* bytes=code->lpVtbl->GetBufferPointer(code);
    if(fread(bytes,1,h.bytecode_size,f)!=h.bytecode_size || fgetc(f)!=EOF ||
       rsx_dxbc_hash(bytes,h.bytecode_size,UINT64_C(14695981039346656037))!=h.bytecode_hash ||
       h.bytecode_size<4 || memcmp(bytes,"DXBC",4)) {
        code->lpVtbl->Release(code);code=NULL;
    }
done:
    free(text);fclose(f);return code;
}
static void rsx_dxbc_write(const char* path,const rsx_dxbc_header* identity,const void* source,ID3DBlob* code)
{
    SIZE_T n=code->lpVtbl->GetBufferSize(code);if(!n || n>RSX_DXBC_LIMIT)return;
    rsx_dxbc_header h=*identity;h.bytecode_size=(uint32_t)n;
    h.bytecode_hash=rsx_dxbc_hash(code->lpVtbl->GetBufferPointer(code),n,UINT64_C(14695981039346656037));
    char tmp[MAX_PATH];
    if(snprintf(tmp,sizeof tmp,"%s.%lu.%lu.tmp",path,GetCurrentProcessId(),GetCurrentThreadId())>=(int)sizeof tmp)return;
    FILE* f=fopen(tmp,"wb");if(!f)return;
    int ok=fwrite(&h,1,sizeof h,f)==sizeof h && fwrite(source,1,h.source_size,f)==h.source_size &&
        fwrite(code->lpVtbl->GetBufferPointer(code),1,n,f)==n;
    if(fclose(f)!=0)ok=0;
    if(!ok || !MoveFileExA(tmp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))DeleteFileA(tmp);
}
static HRESULT rsx_d3d12_compile_cached(LPCVOID source,SIZE_T len,LPCSTR name,
 const D3D_SHADER_MACRO* defines,ID3DInclude* include,LPCSTR entry,LPCSTR target,
 UINT flags1,UINT flags2,ID3DBlob** code,ID3DBlob** errors)
{
    rsx_dxbc_header h;
    const char* disabled=getenv("PS3RECOMP_D3D12_SHADER_CACHE_DISABLE");
    if(!source || !code || defines || include || (disabled && atoi(disabled)) ||
       !rsx_dxbc_identity(&h,len,entry,target,flags1,flags2))
        return RSX_D3D12_COMPILE(source,len,name,defines,include,entry,target,flags1,flags2,code,errors);
    *code=NULL;if(errors)*errors=NULL;
    uint64_t key=rsx_dxbc_hash(&h,sizeof h,UINT64_C(14695981039346656037));
    key=rsx_dxbc_hash(source,len,key);
    AcquireSRWLockExclusive(&rsx_dxbc_lock);
    for(unsigned i=0;i<RSX_DXBC_SLOTS;i++) {
        rsx_dxbc_entry* e=&rsx_dxbc_entries[i];
        if(e->code && e->key==key && rsx_dxbc_same(&e->header,&h) && !memcmp(e->source,source,len)) {
            *code=e->code;(*code)->lpVtbl->AddRef(*code);
            ReleaseSRWLockExclusive(&rsx_dxbc_lock);return S_OK;
        }
    }
    char path[MAX_PATH]={0};const char* dir=getenv("PS3RECOMP_D3D12_SHADER_CACHE");
    if(dir && *dir) {
        CreateDirectoryA(dir,NULL);
        int n=snprintf(path,sizeof path,"%s/dxbc-%016llx.bin",dir,(unsigned long long)key);
        if(n<0 || n>=(int)sizeof path)path[0]=0;
        if(path[0])*code=rsx_dxbc_read(path,&h,source);
    }
    HRESULT hr=S_OK;
    if(!*code) {
        hr=RSX_D3D12_COMPILE(source,len,name,defines,include,entry,target,flags1,flags2,code,errors);
        if(SUCCEEDED(hr) && *code && path[0])rsx_dxbc_write(path,&h,source,*code);
    }
    if(SUCCEEDED(hr) && *code && (*code)->lpVtbl->GetBufferSize(*code)<=RSX_DXBC_LIMIT) {
        char* copy=(char*)malloc(len);
        if(copy) {
            memcpy(copy,source,len);rsx_dxbc_entry* e=&rsx_dxbc_entries[rsx_dxbc_next++%RSX_DXBC_SLOTS];
            if(e->code){rsx_dxbc_bytes-=e->header.source_size+e->code->lpVtbl->GetBufferSize(e->code);e->code->lpVtbl->Release(e->code);}free(e->source);
            size_t cost=len+(*code)->lpVtbl->GetBufferSize(*code);
            for(unsigned i=0;rsx_dxbc_bytes+cost>RSX_DXBC_MEMORY_LIMIT && i<RSX_DXBC_SLOTS;i++) {
                rsx_dxbc_entry* old=&rsx_dxbc_entries[(rsx_dxbc_next+i)%RSX_DXBC_SLOTS];
                if(old==e || !old->code)continue;
                rsx_dxbc_bytes-=old->header.source_size+old->code->lpVtbl->GetBufferSize(old->code);
                old->code->lpVtbl->Release(old->code);free(old->source);memset(old,0,sizeof *old);
            }
            rsx_dxbc_bytes+=cost;
            e->header=h;e->key=key;e->source=copy;e->code=*code;e->code->lpVtbl->AddRef(e->code);
        }
    }
    ReleaseSRWLockExclusive(&rsx_dxbc_lock);return hr;
}
#endif
