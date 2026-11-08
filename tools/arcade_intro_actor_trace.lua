-- Headless only: SDL_VIDEODRIVER=dummy, -video none -sound none.
local m=manager.machine
local mem=m.devices[':maincpu'].spaces['program']
local fields={}
for _,port in pairs(m.ioport.ports) do for name,field in pairs(port.fields) do fields[name]=field end end
local out=assert(io.open('build/arcade-trace/actor-native/actors.csv','w'))
out:write('frame,camera,manager,slot,active,selector,state,substate,x,y,timer,animation_timer,animation\n')
local player=assert(io.open('build/arcade-trace/actor-native/player.csv','w'))
player:write('frame,camera,manager,gate,record\n')
local control=assert(io.open('build/arcade-trace/actor-native/controller.csv','w'))
control:write('frame,major,state,timer,target_y,camera_y,gate,message_busy\n')
local handoff=assert(io.open('build/arcade-trace/actor-native/handoff-player.csv','w'))
handoff:write('frame,major,state,gate,record\n')
local f=0
local visual_frames={[1154]=true,[1158]=true,[1162]=true,[1166]=true,[1170]=true,[1320]=true,[1490]=true,[1500]=true,[1520]=true,[1540]=true,[1580]=true}
emu.register_frame_done(function()
 f=f+1
 local function press(name,on) if fields[name] then fields[name]:set_value(on and 1 or 0) end end
 press('Coin 1',f>=360 and f<370);press('1 Player Start',f>=420 and f<430)
 press('P1 Button 1',false);press('P1 Button 2',false);press('P1 Right',false)
 if f>=691 and f<=1650 then
  control:write(string.format('%d,%d,%d,%d,%d,%d,%d,%d\n',f,mem:read_u16(0xff8004),mem:read_u16(0xff8008),mem:read_u16(0xff800a),mem:read_u16(0xff0fb2),mem:read_u16(0xff840c),mem:read_u8(0xff8348),mem:read_u8(0xff82a6)))
 end
 if f>=1140 and f<=1600 then
  local v=assert(io.open('build/arcade-trace/actor-native/text-'..f..'.bin','wb'))
  v:write(mem:read_range(0x910000,0x913fff,8));v:close()
 end
 if visual_frames[f] then
  m.screens[':screen']:snapshot('scroll1-'..f..'.png')
  local v=assert(io.open('build/arcade-trace/actor-native/palette-'..f..'.bin','wb'))
  v:write(mem:read_range(0x91c400,0x91c7ff,8));v:close()
 end
 if f>=680 and f<=1587 then
  local bytes={}
  for i=0,95 do bytes[#bytes+1]=string.format('%02x',mem:read_u8(0xff9846+i)) end
  player:write(string.format('%d,%d,%d,%d,%s\n',f,mem:read_u16(0xff840c),mem:read_u16(0xff8008),mem:read_u8(0xff8348),table.concat(bytes)))
 end
 if f>=1587 and f<=1620 then
  local bytes={}
  for i=0,95 do bytes[#bytes+1]=string.format('%02x',mem:read_u8(0xff9846+i)) end
  handoff:write(string.format('%d,%d,%d,%d,%s\n',f,mem:read_u16(0xff8004),mem:read_u16(0xff8008),mem:read_u8(0xff8348),table.concat(bytes)))
 end
 if f==1587 or f==1588 or f==1589 then
  local v=assert(io.open('build/arcade-trace/actor-native/handoff-ram-'..f..'.bin','wb'))
  v:write(mem:read_range(0xff0000,0xffffff,8));v:close()
 end
 if f>=750 and f<=1587 then
  for slot,at in ipairs({0xffdb86,0xffdb46}) do
   out:write(string.format('%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n',f,mem:read_u16(0xff840c),mem:read_u16(0xff8008),slot-1,mem:read_u8(at),mem:read_u8(at+32),mem:read_u8(at+2),mem:read_u8(at+3),mem:read_u16(at+6),mem:read_u16(at+10),mem:read_u16(at+48),mem:read_u16(at+28),mem:read_u32(at+20)))
  end
 end
 if f==1650 then out:close();player:close();control:close();handoff:close();m:exit() end
end,'frame')
