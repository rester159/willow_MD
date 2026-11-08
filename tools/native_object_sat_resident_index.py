#!/usr/bin/env python3
"""Checked immutable paged code-to-group-span index emitter."""
import re

def build_resident_index(assets,epoch=0):
    if not 0<=epoch<=0xffffffff:raise ValueError('epoch must be uint32')
    if len(assets)>65535:raise ValueError('entry offsets exceed uint16')
    ordered=[]
    for key,(attribute,status) in sorted(assets.items()):
        if not isinstance(key,int) or not 0<=key<=0x1fffff:raise ValueError('invalid code/group key')
        if not 0<=attribute<=65535 or status not in (1,2):raise ValueError('explicit READY or TRANSPARENT required')
        if status==1 and ((attribute&0x1800) or (attribute&0x7ff)>2044):raise ValueError('READY needs four consecutive unflipped Genesis tiles')
        ordered.append((key,attribute,status))
    if not ordered:return {'epoch':epoch,'first_page':0,'page_count':0,'page_directory':[],'code_offsets':[],'entries':[]}
    first,last=ordered[0][0]>>13,ordered[-1][0]>>13
    directory=[];offsets=[];at=0
    for page in range(first,last+1):
        if at==len(ordered) or ordered[at][0]>>13!=page:directory.append(65535);continue
        if len(offsets)+257>65535:raise ValueError('code offsets exceed uint16; split residency descriptor')
        directory.append(len(offsets))
        for low in range(257):
            boundary=((page<<8)+low)<<5
            while at<len(ordered) and ordered[at][0]<boundary:at+=1
            offsets.append(at)
    assert at==len(ordered)
    return {'epoch':epoch,'first_page':first,'page_count':last-first+1,'page_directory':directory,'code_offsets':offsets,'entries':[(key&31,status,attribute) for key,attribute,status in ordered]}

def emit_resident_header(assets,epoch=0,prefix='fixtureResident'):
    if not re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*',prefix):raise ValueError('invalid C prefix')
    r=build_resident_index(assets,epoch);text='#include "willow_native_object_sat_resident_fast.h"\n'
    for name,values in [('Pages',r['page_directory']),('Offsets',r['code_offsets'])]:
        text+=f'static const uint16_t {prefix}{name}[]={{'+(','.join(map(str,values)) or '0')+'};\n'
    text+=f'static const WillowObjectSatResidentEntry {prefix}Entries[]={{\n'
    text+=''.join(f'{{{g},{s},{a}}},\n' for g,s,a in r['entries']) or '{0,0,0},\n'
    text+='};\n'
    text+=f'static const WillowObjectSatResidentIndex {prefix}Index={{'+f'{prefix}Pages,{prefix}Offsets,{prefix}Entries,{epoch}u,{r["first_page"]},{r["page_count"]},{len(r["entries"])},{len(r["code_offsets"])}'+'};\n'
    return text,r
