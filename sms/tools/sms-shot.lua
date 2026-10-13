-- sms-shot.lua -- MAME autoboot script for tools/sms-shot.sh. The nes
-- branch's tools/nes-shot.lua, reading the Mode 4 name table at $3800 the
-- way fn-sms pico/sms/emu/smstext.lua does.
--
-- Presses pad buttons on a schedule, then prints the screen as text and
-- saves a snapshot.
--
--   SHOT_PRESS    "secs:Button secs:Button ..."  (Button: B1 B2 Up Down Left
--                 Right Pause, pressed for 6 frames)
--   SHOT_AT       seconds before the capture (default 12)
--   SHOT_EVERY    also print the screen at each press (debugging a script)
--
-- Each row is prefixed with the inks its text was drawn in (ink.h): W white
-- (tiles 32-127), G the game-header copy (160-255), D the dim-room copy
-- (288-383), and '*' when its cells carry A_SEL, the selection marker. The
-- cart's fujinet_mode output follows the screen: nonzero means a game was
-- swapped in.

local at = tonumber(os.getenv("SHOT_AT") or "12")
local every = os.getenv("SHOT_EVERY")
local presses = {}

for t, b in string.gmatch(os.getenv("SHOT_PRESS") or "", "([%d%.]+):(%w+)") do
  presses[#presses + 1] = { t = tonumber(t), b = b, state = 0 }
end

local pad = manager.machine.ioport.ports[":ctrl1:mspad:JOYPAD"].fields
local names = { B1 = "P1 Button 1", B2 = "P1 Button 2", Up = "P1 Up",
                Down = "P1 Down", Left = "P1 Left", Right = "P1 Right" }

-- Pause is on the console, not the pad; find whichever port carries it.
local function pause_field()
  for _, port in pairs(manager.machine.ioport.ports) do
    for name, f in pairs(port.fields) do
      if name == "Pause" or name == "Pause Button" then return f end
    end
  end
end

local function field(b)
  if b == "Pause" then return pause_field() end
  return pad[names[b] or ("P1 " .. b)]
end

local function vram()
  for tag, dev in pairs(manager.machine.devices) do
    if tag:match("vdp$") and dev.spaces["videoram"] then
      return dev.spaces["videoram"]
    end
  end
end
local v = vram()

-- font.txt's tiles below $20, as something readable.
local tiles = { [0x01] = "+", [0x02] = "-", [0x03] = "|", [0x04] = ">",
                [0x05] = ">", [0x08] = "[" }

local function screen()
  local out = {}
  for r = 0, 23 do
    local t, inks, sel = {}, {}, false
    for c = 0, 31 do
      local a = 0x3800 + (r * 32 + c) * 2
      local lo, hi = v:read_u8(a), v:read_u8(a + 1)
      local ch = lo & 0x7F
      if (hi & 0x08) ~= 0 then sel = true end
      if ch > 0x20 and ch < 0x80 then
        local ink = (hi & 0x01) ~= 0 and "D" or (lo >= 0x80 and "G" or "W")
        inks[ink] = true
        t[#t + 1] = string.char(ch)
      elseif ch == 0x20 then
        t[#t + 1] = " "
      elseif (hi & 0x01) == 0 and lo < 0x20 then
        t[#t + 1] = tiles[lo] or ((lo >= 0x09 and lo <= 0x11) and "#" or " ")
      else
        t[#t + 1] = " "
      end
    end
    local tag = (inks.G and "G" or " ") .. (inks.D and "D" or " ") ..
                (inks.W and "W" or " ")
    out[#out + 1] = string.format("%2d %s%s|%s|", r, tag, sel and "*" or " ",
                                  table.concat(t))
  end
  return table.concat(out, "\n")
end

local done = false
_G.__shot_sub = emu.add_machine_frame_notifier(function()
  if done then return end
  local now = manager.machine.time:as_double()
  for _, p in ipairs(presses) do
    if p.state == 0 and now >= p.t then
      if every then print(string.format("-- %.2fs before %s\n%s", now, p.b, screen())) end
      p.f = field(p.b)
      if p.f then p.f:set_value(1) else print("sms-shot: no button " .. p.b) end
      p.state = 1
      p.frames = 6
    elseif p.state == 1 then
      p.frames = p.frames - 1
      if p.frames <= 0 then
        if p.f then p.f:set_value(0) end
        p.state = 2
      end
    end
  end
  if now >= at then
    done = true
    print(screen())
    -- 0 while the resident client (this lobby) runs; nonzero once a joined
    -- game has been swapped in (fn-sms pico/sms/emu/fujinet.cpp).
    print("fujinet_mode " .. tostring(manager.machine.output:get_value("fujinet_mode")))
    local scr = manager.machine.screens[":screen"]
    if scr then scr:snapshot() end
    manager.machine:exit()
  end
end)
