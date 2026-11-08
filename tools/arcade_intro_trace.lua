-- Run with MAME willowu; see PORTING.md for the isolated trace command.
local m=manager.machine
local mem=m.devices[':maincpu'].spaces['program']
local fields={}
for tag,port in pairs(m.ioport.ports) do
 for name,field in pairs(port.fields) do fields[name]=field; print('INPUT',tag,name) end
end
for tag,share in pairs(m.memory.shares) do print('SHARE',tag,share.size) end
local trace=io.open('build/arcade-trace/intro-camera.csv','w')
trace:write('frame,state,timer,target_y,camera_y,scroll2_y,scroll3_y,gate\n')
local f=0
emu.register_frame_done(function()
 f=f+1
 if f>=680 and f<=1040 then
  local a=m.memory.shares[':cps_a_regs']
  trace:write(string.format('%d,%d,%d,%d,%d,%d,%d,%d\n',f,mem:read_u16(0xff8008),mem:read_u16(0xff800a),mem:read_u16(0xff0fb2),mem:read_u16(0xff840c),a:read_u16(0x12),a:read_u16(0x16),mem:read_u8(0xff82a6)))
 end
 local function press(name,on) if fields[name] then fields[name]:set_value(on and 1 or 0) end end
 press('Coin 1',f>=360 and f<370)
 press('1 Player Start',f>=420 and f<430)
 press('P1 Button 1',false)
 press('P1 Button 2',f>=1100 and f<1101)
 press('P1 Right',false)
 if f%60==0 or (f>=690 and f<=1600) then
  print(string.format('F %d PC %06x input %02x stage %d pos %d,%d camera %d,%d',f,m.devices[':maincpu'].state['PC'].value,mem:read_u8(0xff806a),mem:read_u16(0xff8344),mem:read_u16(0xff984c),mem:read_u16(0xff9850),mem:read_u16(0xff8408),mem:read_u16(0xff840c)))
 end
 if f>=690 and f<=1600 and f%30==0 then
  m.screens[':screen']:snapshot('natural-intro-'..f..'.png')
  local out=io.open('build/arcade-trace/intro-vram-'..f..'.bin','wb');out:write(mem:read_range(0x900000,0x92ffff,8));out:close()
  for tag,share in pairs(m.memory.shares) do
   if tag:find('regs') then
    local out=io.open('build/arcade-trace/intro-'..tag:gsub(':','_')..'-'..f..'.bin','wb')
    for i=0,share.size-2,2 do local w=share:read_u16(i);out:write(string.char(w>>8,w&255)) end
    out:close()
   end
  end
 end
 if f>=1650 then trace:close();m:exit() end
end,'frame')
