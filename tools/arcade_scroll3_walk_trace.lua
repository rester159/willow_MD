-- Original game, headless only; inputs are applied after each completed frame.
local m=manager.machine
local mem=m.devices[':maincpu'].spaces['program']
local fields={}
for _,port in pairs(m.ioport.ports) do for name,field in pairs(port.fields) do fields[name]=field end end
for _,name in ipairs({'Coin 1','1 Player Start','P1 Right','P1 Button 1','P1 Button 2'}) do assert(fields[name],name) end
local out=assert(io.open('build/arcade-trace/scroll3-walk/frames.bin','wb'))
local maps=assert(io.open('build/arcade-trace/scroll3-walk/maps.bin','wb'))
local f=0
emu.register_frame_done(function()
 f=f+1
 local function press(name,on) fields[name]:set_value(on and 1 or 0) end
 press('Coin 1',f>=360 and f<370);press('1 Player Start',f>=420 and f<430)
 press('P1 Button 1',false);press('P1 Button 2',false);press('P1 Right',f>=1625 and f<1665)
 if f>=650 then
  out:write(string.pack('>I4',f))
  out:write(mem:read_range(0xff8000,0xff8501,8))
  maps:write(mem:read_range(0x914000,0x917fff,8))
 end
 if f==1700 then out:close();maps:close();m:exit() end
end,'frame')
