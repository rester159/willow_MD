-- Headless capture only. Run with SDL dummy drivers and -video none -sound none.
local m=manager.machine
local mem=m.devices[':maincpu'].spaces['program']
local fields={}
for _,port in pairs(m.ioport.ports) do for name,field in pairs(port.fields) do fields[name]=field end end
local out=assert(io.open('build/arcade-trace/camera-native/frames.bin','wb'))
local f=0
emu.register_frame_done(function()
 f=f+1
 local function press(name,on) if fields[name] then fields[name]:set_value(on and 1 or 0) end end
 press('Coin 1',f>=360 and f<370);press('1 Player Start',f>=420 and f<430)
 press('P1 Button 1',false);press('P1 Button 2',false);press('P1 Right',false)
 if f>=650 then
  out:write(string.pack('>I4',f))
  out:write(mem:read_range(0xff8000,0xff8401,8))
  out:write(mem:read_range(0xff8402,0xff8481,8))
  out:write(mem:read_range(0xff9846,0xff98c5,8))
  out:write(mem:read_range(0x918000,0x91bfff,8))
 end
 if f==1620 then out:close();m:exit() end
end,'frame')
