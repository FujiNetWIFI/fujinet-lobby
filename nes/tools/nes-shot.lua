-- nes-shot.lua -- MAME autoboot script for tools/nes-shot.sh. Adapted from
-- fujinet-google-calendar's tools/nes-shot.lua; this client draws through
-- cc65's conio on the one name table at $2000, so there is no flip to follow.
--
-- Presses pad buttons on a schedule, then prints the screen as text and
-- saves a snapshot.
--
--   SHOT_PRESS    "secs:Button secs:Button ..."  (Button: A B Select Start
--                 Up Down Left Right, pressed for 6 frames)
--   SHOT_AT       seconds before the capture (default 8)
--   SHOT_EVERY    also print the screen at each press (debugging a script)

local at = tonumber(os.getenv("SHOT_AT") or "8")
local every = os.getenv("SHOT_EVERY")
local presses = {}

for t, b in string.gmatch(os.getenv("SHOT_PRESS") or "", "([%d%.]+):(%w+)") do
  presses[#presses + 1] = { t = tonumber(t), b = b, state = 0 }
end

local port = manager.machine.ioport.ports[":ctrl1:joypad:JOYPAD"]
local ppu = manager.machine.devices[":ppu"].spaces["videoram"]

-- font.txt's tiles below $20, as something readable. Reverse video ($80-$FF)
-- marks the row with '*' and reads as the plain character.
local tiles = {
  [0x01] = "=", [0x02] = "|", [0x03] = "+", [0x04] = "+", [0x05] = "+",
  [0x06] = "+", [0x07] = "#", [0x08] = "A", [0x09] = "B", [0x0B] = "<",
  [0x0C] = ">", [0x0E] = "+", [0x0F] = "+", [0x19] = "-",
}

local function ch(v)
  local c = v & 0x7F
  if tiles[c] then return tiles[c] end
  if c >= 0x10 and c <= 0x18 then return c == 0x10 and "." or "#" end
  if c >= 0x20 and c < 0x7F then return string.char(c) end
  return " "
end

local function screen()
  local out = {}
  for r = 0, 27 do
    local t, hl = {}, false
    for c = 0, 31 do
      local v = ppu:readv_u8(0x2000 + r * 32 + c)
      if v >= 0xA0 then hl = true end
      t[#t + 1] = ch(v)
    end
    out[#out + 1] = string.format("%2d%s|%s|", r, hl and "*" or " ", table.concat(t))
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
      port.fields["P1 " .. p.b]:set_value(1)
      p.state = 1
      p.frames = 6
    elseif p.state == 1 then
      p.frames = p.frames - 1
      if p.frames <= 0 then
        port.fields["P1 " .. p.b]:set_value(0)
        p.state = 2
      end
    end
  end
  if now >= at then
    done = true
    print(screen())
    local scr = manager.machine.screens[":screen"]
    if scr then scr:snapshot() end
    manager.machine:exit()
  end
end)
