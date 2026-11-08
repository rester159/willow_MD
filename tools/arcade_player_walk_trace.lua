-- Original game, headless only; inputs are applied after each completed frame.
local m=manager.machine
local mem=m.devices[':maincpu'].spaces['program']
local fields={}
for _,port in pairs(m.ioport.ports) do for name,field in pairs(port.fields) do fields[name]=field end end
for _,name in ipairs({'Coin 1','1 Player Start','P1 Right','P1 Button 1','P1 Button 2'}) do assert(fields[name],name) end
local out=assert(io.open('build/arcade-trace/player-walk/frames.bin','wb'))
local f=0
emu.register_frame_done(function()
 f=f+1
 local function press(name,on) fields[name]:set_value(on and 1 or 0) end
 press('Coin 1',f>=360 and f<370);press('1 Player Start',f>=420 and f<430)
 press('P1 Button 1',false);press('P1 Button 2',false);press('P1 Right',f>=1625 and f<1665)
 if f>=1587 then
  out:write(string.pack('>I4',f))
  out:write(mem:read_range(0xff8000,0xff8401,8))
  out:write(mem:read_range(0xff8402,0xff8481,8))
  out:write(mem:read_range(0xff9846,0xff98c5,8))
  out:write(mem:read_range(0x918000,0x91bfff,8))
  out:write(mem:read_range(0x91c000,0x91d7ff,8))
  out:write(mem:read_range(0x910000,0x913fff,8))
 end
 if f==1700 then out:close();m:exit() end
end,'frame')
