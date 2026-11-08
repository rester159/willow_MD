"""Decode task 78F0's text arguments via the source 7994 table arithmetic."""
def decode(program,message,style):
 task=message>>8
 if task not in (0,4,6):raise ValueError('Unvalidated text handler')
 index=message&255
 format=(style>>12)&15
 if format>2:raise ValueError('Unvalidated text format')
 record=0x70000+(index>>format)*168
 slot=index&program[0x7980+format]
 start=record+8+program[0x7984+format*2+slot]
 attr=record+program[0x798c+format*2+slot]
 if task==6:
  end=start
  while program[end:end+2]!=b'\x00/':
   end+=2
   if end>=record+168:raise ValueError('Word text lacks terminator within record')
  end+=1
 else:end=program.index(b'/',start)
 destination=int.from_bytes(program[0x7a1e+(style&255)*4:0x7a22+(style&255)*4],'big')
 return {'message':message,'message_index':index,'handler':task,'style':style,'record':record,'start':start,'stream':bytes(program[start:end+1]),'attributes':[int.from_bytes(program[attr+i*2:attr+i*2+2],'big') for i in range(program[0x7a1a+format]+1)],'destination':destination,'delay':(style>>8)&15,'rows':program[0x7a1a+format]+1}
