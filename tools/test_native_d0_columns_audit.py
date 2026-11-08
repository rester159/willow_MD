#!/usr/bin/env python3
"""Read-only exact-state replay of five frozen columns D0 failures.

Copies the already admitted observer instruction/ownership guards verbatim.
Adds only host snapshots and bus logs; all original failures remain failures.
"""
from pathlib import Path
import argparse,hashlib,json,re,shutil,subprocess
ROOT=Path(__file__).resolve().parents[1]
RUN=ROOT/'build/native-forest-extension/columns-v1-provenance-v2'
FREEZE=ROOT/'build/native-forest-extension/checkpoint-columns-v1'
CORE=Path.home()/'Library/Application Support/RetroArch/cores/genesis_plus_gx_libretro.dylib'
FRAMES=[1467,1468,1470,1472,1696]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def replace(text,old,new):assert text.count(old)==1,(old,text.count(old));return text.replace(old,new)
HOOKS=r'''
static FILE*d0AuditLog;
static unsigned d0AuditPrevious[2],d0AuditPreviousPC[2],d0AuditValid[2];
static unsigned d0AuditWindow(void){return(frameNumber>=1466&&frameNumber<=1473)||(frameNumber>=1695&&frameNumber<=1697);}
static void d0AuditBus(unsigned kind,unsigned at,unsigned width,unsigned value){
 if(!d0AuditWindow())return;
 unsigned path=renderPath,pc=sourcePC(),d0=cpu->dar[0],sp=cpu->dar[15],task=rawLong(0x204);
 unsigned important=(pc>=0x3b574&&pc<=0x3b5c6)||(pc>=0x1750&&pc<=0x1784)||(pc>=0x14d0&&pc<=0x1530)||pc==CADENCE_IRQ+2;
 if(!important&&d0AuditValid[path]&&d0==d0AuditPrevious[path]){d0AuditPreviousPC[path]=pc;return;}
 if(!d0AuditLog){d0AuditLog=outputFile("d0-lineage.csv","w");if(!d0AuditLog)abort();fputs("path,frame,master,pc,previous_bus_pc,d0,previous_d0,sr,sp,a0,a5,task,kind,address,width,value,source_context_ready,source_context_sp\n",d0AuditLog);}
 fprintf(d0AuditLog,"%u,%u,%" PRIu64 ",%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",path,frameNumber,stamp(),pc,d0AuditPreviousPC[path],d0,d0AuditPrevious[path],renderGetRegister(17),sp,cpu->dar[8],cpu->dar[13],task,kind,at,width,value,rawWord(RENDER_SOURCE_CONTEXT_READY),rawLong(RENDER_SOURCE_CONTEXT_SP));
 d0AuditPrevious[path]=d0;d0AuditPreviousPC[path]=pc;d0AuditValid[path]=1;
}
static void d0AuditSnapshot(unsigned path,const void*state,size_t bytes){
 static const unsigned wanted[]={1467,1468,1470,1472,1696};unsigned match=0;
 for(unsigned n=0;n<sizeof wanted/sizeof*wanted;n++)match|=frameNumber==wanted[n];if(!match)return;
 char name[128];FILE*f;
 #define D0_FILE(suffix,mode) do{snprintf(name,sizeof name,"frame-%u-path-%u." suffix,frameNumber,path);f=outputFile(name,mode);if(!f)abort();}while(0)
 D0_FILE("state","wb");if(fwrite(state,1,bytes,f)!=bytes)abort();fclose(f);
 D0_FILE("source","wb");backgroundDumpSource(f,0);fclose(f);
 D0_FILE("cps","wb");if(fwrite(cps,1,sizeof cps,f)!=sizeof cps)abort();fclose(f);
 D0_FILE("sram","wb");if(fwrite(metadataSRAM,1,65536,f)!=65536)abort();fclose(f);
 D0_FILE("registers.bin","wb");for(unsigned n=0;n<21;n++){unsigned v=renderGetRegister(n);for(int b=3;b>=0;b--)if(fputc((v>>(8*b))&255,f)==EOF)abort();}fclose(f);
 D0_FILE("registers.json","w");fprintf(f,"{\"frame\":%u,\"path\":%u,\"cycles\":%d,\"registers\":[",frameNumber,path,cpu->cycles);for(unsigned n=0;n<21;n++)fprintf(f,"%s%u",n?",":"",renderGetRegister(n));fputs("]}\n",f);fclose(f);
 #undef D0_FILE
 if(d0AuditLog)fflush(d0AuditLog);
}
'''
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=ROOT/'build/native-d0-residue-audit/columns-snapshots-v2');a=p.parse_args();out=a.output.resolve();assert not out.exists();out.mkdir(parents=True)
 checkpoint=json.loads((FREEZE/'checkpoint.json').read_text());pins={}
 for name,data in checkpoint['files'].items():
  path=FREEZE/name;assert sha(path)==data['sha256']
 old=json.loads((RUN/'observer-manifest.json').read_text());source=RUN/'test_native_render_cadence.c';assert sha(source)==old['generated_sha256']
 # Freeze the literal include closure. Resolve only matching immutable bytes;
 # the generated observer's existing code/guards are otherwise untouched.
 mapping={};pending=[source]
 def freeze(path):
  path=path.resolve()
  if path in mapping:return mapping[path]
  data=path.read_bytes();digest=hashlib.sha256(data).hexdigest()
  if path!=source and str(path) in old['inputs']:assert old['inputs'][str(path)]==digest,path
  pins[str(path)]=digest;dst=out/'inputs'/path.relative_to(ROOT);mapping[path]=dst;dst.parent.mkdir(parents=True,exist_ok=True)
  text=data.decode()
  for inc in re.findall(r'^#include "([^"]+)"',text,re.M):
   include=Path(inc);include=include if include.is_absolute()else path.parent/include
   text=text.replace('"'+inc+'"','"'+str(freeze(include))+'"')
  if path.name=='sprite-observer-with-background-hooks.h':
   text=replace(text,' return ram[(at&65535)^1];',' if(renderPath)d0AuditBus(0,at&65535,1,ram[(at&65535)^1]);\n return ram[(at&65535)^1];')
   for width in [1,2]:
    name='renderNativeWork8'if width==1 else'renderNativeWork16'
    declaration='static void '+name+'(unsigned at,unsigned value){'
    text=replace(text,declaration,declaration+'\n d0AuditBus(1,at&65535,'+str(width)+',value);')
  dst.write_text(text);return dst
 local=freeze(source);text=local.read_text()
 # Keep target products and resources read-only at their frozen input paths.
 text=replace(text,'static unsigned sourcePC(void);','static unsigned sourcePC(void);\nstatic void d0AuditBus(unsigned,unsigned,unsigned,unsigned);')
 text=replace(text,'static void headroomAccess(unsigned kind,unsigned at,unsigned width,unsigned value){','static void headroomAccess(unsigned kind,unsigned at,unsigned width,unsigned value){\n d0AuditBus(kind,at,width,value);')
 text=replace(text,'int main(',HOOKS+'\nint main(')
 text=replace(text,'   if(!path){for(unsigned n=0;n<65536;n++)sourceRAM[n]=ram[n^1];','   d0AuditSnapshot(path,states[path],stateSize);\n   if(!path){for(unsigned n=0;n<65536;n++)sourceRAM[n]=ram[n^1];')
 text=replace(text,'  if(forestCoverageStop){forestStopAfterPairedFrame(routeTicks);return 3;}','  if(forestCoverageStop){if(d0AuditLog)fclose(d0AuditLog);forestStopAfterPairedFrame(routeTicks);return 3;}')
 generated=out/'observer.c';generated.write_text(text);binary=out/'observer'
 cmd=['clang','-std=c11','-O2','-Wall','-Wextra','-Werror',str(generated),'-o',str(binary)];subprocess.run(cmd,check=True)
 assert sha(CORE)==old['run']['core_sha256']
 with(out/'observer.log').open('w')as f:r=subprocess.run([str(binary),str(CORE),'3600','--coin-route',str(out)],cwd=ROOT,stdout=f,stderr=subprocess.STDOUT)
 same=[]
 for name in ['register-failures.csv','frames.csv','background-publications.csv','render-events.csv','producer-epochs.csv','main-loop-events.csv']:
  if(RUN/name).exists():assert(out/name).read_bytes()==(RUN/name).read_bytes(),('Read-only replay changed observations',name);same.append(name)
 assert r.returncode==old['run']['exit_code']==3,r.returncode
 assert all(sha(Path(n))==digest for n,digest in pins.items())
 result={'scope':__doc__,'frames':FRAMES,'exit_code':r.returncode,'unchanged_outputs':same,'source_equivalence_admitted':False,'command':cmd,'core_sha256':sha(CORE),'inputs':pins,'frozen_checkpoint_sha256':sha(FREEZE/'checkpoint.json'),'generated_sha256':sha(generated),'binary_sha256':sha(binary),'tool_sha256':sha(Path(__file__)),'outputs':{p.name:sha(p)for p in sorted(out.glob('frame-*'))},'lineage_sha256':sha(out/'d0-lineage.csv')}
 (out/'evidence.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'snapshots':len(FRAMES)*2,'unchanged_outputs':same,'exit_code':r.returncode},indent=2))
if __name__=='__main__':main()
