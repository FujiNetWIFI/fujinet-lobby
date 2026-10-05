-- shot.lua: drive the DEMO build and snapshot each screen.
--   mame astrocde ... -cart build/lobby.bin (DEMO=1) \
--        -autoboot_script emu/shot.lua -video none -seconds_to_run 24
-- Keypad 1 at the OS menu launches; then, a snapshot after each step:
--   0000 page 1, bar on the first room
--   0001 two moves down
--   0002 right: page 2
--   0003 left, then trigger: the join screen (DEMO stops before booting)
-- Every write to the tone A volume port (VOLAB, 16H) is logged with the
-- time, so the move tick and the select arpeggio show up as cues.
local function port_by_suffix(suffix)
    for tag, port in pairs(manager.machine.ioport.ports) do
        if tag:sub(-#suffix) == suffix then return port end
    end
    return nil
end

-- Handles must live in globals or they are collected and stop firing.
local io = manager.machine.devices[":maincpu"].spaces["io"]
snd_tap = io:install_write_tap(0x0000, 0xffff, "voltap", function(offset, data)
    if (offset & 0xff) == 0x16 and data ~= 0 then
        emu.print_info(string.format("shot.lua: cue vol=%02X t=%.2f",
            data, manager.machine.time.seconds))
    end
end)

local STEPS = {
    { 3,    "KEYPAD3", 0x10, 1 },   -- keypad 1: launch
    { 4,    "KEYPAD3", 0x10, 0 },
    { 9,    "shot" },
    { 10,   "HANDLE",  0x02, 1 },   -- down
    { 10.5, "HANDLE",  0x02, 0 },
    { 11,   "HANDLE",  0x02, 1 },   -- down
    { 11.5, "HANDLE",  0x02, 0 },
    { 12.5, "shot" },
    { 13,   "HANDLE",  0x08, 1 },   -- right: page 2
    { 13.5, "HANDLE",  0x08, 0 },
    { 15,   "shot" },
    { 16,   "HANDLE",  0x04, 1 },   -- left: page 1
    { 16.5, "HANDLE",  0x04, 0 },
    { 18,   "HANDLE",  0x10, 1 },   -- trigger: join
    { 18.5, "HANDLE",  0x10, 0 },
    { 22,   "shot" },
    { 23,   "exit" },
}
local step = 1
shot_frame = emu.register_frame(function()
    local s = STEPS[step]
    if s == nil then return end
    local t = manager.machine.time.seconds
    if t < s[1] then return end
    if s[2] == "shot" then
        emu.print_info("shot.lua: snapshot t=" .. t)
        manager.machine.video:snapshot()
    elseif s[2] == "exit" then
        manager.machine:exit()
    else
        local p = port_by_suffix(s[2])
        if s[4] == 1 then p:field(s[3]):set_value(1) else p:field(s[3]):clear_value() end
    end
    step = step + 1
end)
