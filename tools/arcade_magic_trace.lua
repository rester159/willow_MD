-- Run with MAME willowu; see PORTING.md for the isolated trace command.
local m=manager.machine
local mem=m.devices[':maincpu'].spaces['program']
local fields={}
for tag,port in pairs(m.ioport.ports) do
 for name,field in pairs(port.fields) do fields[name]=field; print('INPUT',tag,name) end
end
local f=0
emu.register_frame_done(function()
 f=f+1
 local function press(name,on) if fields[name] then fields[name]:set_value(on and 1 or 0) end end
 press('Coin 1',f>=360 and f<370)
 press('1 Player Start',f>=420 and f<430)
 press('P1 Button 1',(f>=700 and f<780 and f%20<10) or (f>=1000 and f<1030))
 press('P1 Button 2',f>=1100 and f<1101)
 press('P1 Right',false)
 if f%60==0 then
  print(string.format('F %d PC %06x input %02x stage %d pos %d,%d camera %d,%d',f,m.devices[':maincpu'].state['PC'].value,mem:read_u8(0xff806a),mem:read_u16(0xff8344),mem:read_u16(0xff984c),mem:read_u16(0xff9850),mem:read_u16(0xff8408),mem:read_u16(0xff840c)))
 end
 if f>=990 and f<=1140 and f%5==0 then
  m.screens[':screen']:snapshot('action-'..f..'.png')
  local out=io.open('build/arcade-trace/ram-'..f..'.bin','wb');out:write(mem:read_range(0xff0000,0xffffff,8));out:close()
 end
 if f==1040 then
  assert(mem:read_u32(0xff8444)==0x2dc24,'Normal-play map pointer differs')
  assert(mem:read_u16(0xffc78c)==328,'Basic shot speed/cast cadence differs')
  assert(mem:read_u16(0xffc74c)==208,'Release shot cadence differs')
  assert(mem:read_u16(0xffc790)==80,'Standing shot origin differs')
  print('ARCADE MAGIC TRACE PASS: normal-play map, cast cadence, shot travel and origin')
 end
 if f>=1150 then m:exit() end
end,'frame')
