#!/usr/bin/env python3
"""Adversarial tests of the independent event/ownership pixel-verifier ledger."""
from pathlib import Path
import argparse,copy,json,re,struct
from native_far_observer_resources import load_resources
from verify_native_background_far_publication import FarOracle,put16,put32
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--platform',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();assert not a.output.exists();a.output.mkdir(parents=True)
 d=load_resources(ROOT/'build/native-background-far-stream/prepared-v1');defines=(a.platform/'render-addresses.h').read_text()
 o={k.lower():int(v,0)for k,v in re.findall(r'^#define RENDER_BG_FAR_CACHE_(\w+)_OFFSET (0x[\da-fA-F]+|\d+)[uUlL]*$',defines,re.M)}
 initial=tuple(p for t,p in d['pool']);ref=next(i for i,(v,w)in enumerate(d['refs'])if v and v not in initial and d['fixed'][v]==65535);key,sub=divmod(ref,16);pattern=d['refs'][ref][0]
 init=bytearray(436)
 for n,v in enumerate(initial):put16(init,n*2,v)
 init[o['slots']:o['slots']+16]=bytes([255])*16;put16(init,o['key'],65535);put16(init,o['phase'],1);put32(init,o['residency_epoch'],1)
 queue=bytearray(init);put16(queue,o['key'],key);put16(queue,o['requested'],1<<sub);put16(queue,o['phase'],2);queue[o['slots']+sub]=0
 done=bytearray(queue);put16(done,0,pattern);put16(done,o['completed'],1<<sub);put16(done,o['cursor'],sub+1);put16(done,o['phase'],1);put32(done,o['residency_epoch'],2)
 def event(path,phase,epoch,status=0):return dict(sample=0,path=path,frame=200,phase=phase,status=status,master_cycles=100,pc='146100',residency_epoch=epoch,key=key,requested=1<<sub,completed=1<<sub if phase==10 else 0,cache_phase=1 if phase!=9 else 2,patterns=status,words=16*status,controls=2*status,retire_upper_master=101,deadline_master=1000)
 events=[];states=[]
 for path in range(2):
  for phase,state in [(11,init),(9,queue),(10,done)]:events.append(event(path,phase,2 if phase==10 else 1,1 if phase==10 else 0));states.append(bytes(state))
 for n,r in enumerate(events):r['sample']=n
 transfers=d['patterns'][pattern*32:(pattern+1)*32]*2
 def run(es,ss,ts):
  oracle=FarOracle.__new__(FarOracle);oracle.data=d;oracle.offsets=o;oracle.initial=initial;oracle._replay(es,b''.join(ss),ts);return oracle
 good=run(events,states,transfers);assert good.counts['patterns']==1 and good.history[0][2][0]==pattern
 results=[{'case':'exact complete two-bus transfer','accepted':True}]
 def bad(name,mutator):
  es,ss,ts=copy.deepcopy(events),[bytearray(s)for s in states],bytearray(transfers);mutator(es,ss,ts)
  try:run(es,ss,ts)
  except (ValueError,IndexError,struct.error):results.append({'case':name,'rejected':True})
  else:raise AssertionError('Accepted corrupt proof: '+name)
 bad('wrong transferred pixel',lambda e,s,t:t.__setitem__(0,t[0]^1))
 bad('truncated pattern',lambda e,s,t:t.pop())
 bad('extra transfer bytes',lambda e,s,t:t.extend(bytes(32)))
 bad('half transfer count',lambda e,s,t:e[2].__setitem__('words',15))
 bad('missing command word',lambda e,s,t:e[2].__setitem__('controls',1))
 bad('unobserved epoch advance',lambda e,s,t:put32(s[2],o['residency_epoch'],3))
 bad('owner commit differs',lambda e,s,t:put16(s[2],0,pattern^1))
 bad('unrelated metadata mutation',lambda e,s,t:put16(s[2],o['view'],1))
 bad('missing initialization',lambda e,s,t:e[0].__setitem__('phase',9))
 bad('repeated initialization',lambda e,s,t:e[1].__setitem__('phase',11))
 bad('init nonzero prepared epoch',lambda e,s,t:put32(s[0],o['prepared_residency_epoch'],1))
 bad('protected physical victim',lambda e,s,t:put32(s[1],o['displayed_use'],1))
 bad('protected prepared victim',lambda e,s,t:put32(s[1],o['prepared_use'],1))
 bad('wrong copy count',lambda e,s,t:e[2].__setitem__('patterns',2))
 bad('failed upload bit',lambda e,s,t:e[2].__setitem__('status',129))
 bad('wrong completed mask',lambda e,s,t:put16(s[2],o['completed'],0))
 bad('unfinished copying state',lambda e,s,t:put16(s[2],o['phase'],3))
 bad('wrong cursor',lambda e,s,t:put16(s[2],o['cursor'],sub))
 bad('owner changes before upload',lambda e,s,t:put16(s[1],0,pattern))
 bad('unpaired upload completion',lambda e,s,t:e[1].__setitem__('phase',10))
 bad('bus owner history divergence',lambda e,s,t:put16(s[5],0,initial[0]))
 (a.output/'evidence.json').write_text(json.dumps({'scope':__doc__,'cases':results,'negative_cases':len(results)-1,'does_not_replace_actual_target_gate':True},indent=2)+'\n')
 print(f'Passed {len(results)} independent far-ledger cases')
if __name__=='__main__':main()
