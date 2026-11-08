-- Headless capture of original palette and player state. No screen or GUI calls.
local m=manager.machine
local mem=m.devices[':maincpu'].spaces['program']
local fields={}
for _,port in pairs(m.ioport.ports) do for name,field in pairs(port.fields) do fields[name]=field end end
local out=assert(io.open('build/arcade-trace/player-colors/frames.bin','wb'))
local f=0
emu.register_frame_done(function()
 f=f+1
 local function press(name,on) if fields[name] then fields[name]:set_value(on and 1 or 0) end end
 press('Coin 1',f>=360 and f<370);press('1 Player Start',f>=420 and f<430)
 -- Baseline through 1620 is identical to the intro/player handoff trace.
 -- Later input is recorded by the original game for future active-play checks.
 press('P1 Button 1',f>=1625 and f<1685)
 press('P1 Button 2',f>=1725 and f<1730)
 press('P1 Right',f>=1690 and f<1760)
 if f>=650 and f<=1800 then
  out:write(string.pack('>I4',f))
  out:write(mem:read_range(0xff8000,0xff8401,8))
  out:write(mem:read_range(0xff9846,0xff98c5,8))
  out:write(mem:read_range(0x91c000,0x91d7ff,8))
 end
 if f==1800 then out:close();m:exit() end
end,'frame')
