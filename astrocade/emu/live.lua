-- live.lua: shot.lua's walk against a real lobby, at real speed (no
-- -nothrottle: the server and the fujinet-pc keep wall-clock time), then
-- a join that really boots the chosen room's client.
--   ENDPOINT=http://127.0.0.1:8093/view ./build.sh
--   mame astrocde ... -autoboot_script emu/live.lua -video none \
--        -seconds_to_run 74
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
    { 16,   "shot" },               -- page 1, live
    { 17,   "HANDLE",  0x02, 1 },   -- down
    { 17.5, "HANDLE",  0x02, 0 },
    { 18,   "HANDLE",  0x02, 1 },   -- down
    { 18.5, "HANDLE",  0x02, 0 },
    { 19.5, "shot" },
    { 20,   "HANDLE",  0x08, 1 },   -- right: page 2
    { 20.5, "HANDLE",  0x08, 0 },
    { 30,   "shot" },
    { 31,   "HANDLE",  0x04, 1 },   -- left: page 1
    { 31.5, "HANDLE",  0x04, 0 },
    { 42,   "HANDLE",  0x10, 1 },   -- trigger: join the first room
    { 42.5, "HANDLE",  0x10, 0 },
    { 45,   "shot" },               -- joining
    { 70,   "shot" },               -- whatever booted
    { 72,   "exit" },
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
