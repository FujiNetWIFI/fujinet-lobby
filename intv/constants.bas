' constants.bas -- screen/color/input constants and the shared scratch-RAM
' map. Screen/color/disc/keypad/grid constants are the standard IntyBASIC
' set already used throughout the FujiNet Intellivision tree (verbatim from
' fujinet-config/intv/constants.bas); trimmed and extended with the lobby's
' own wire-record layout and scratch map.
'
' *** RAM BUDGET RULE (relaxed from fujinet-config's) ***
' Unlike fujinet-config's directory browser -- which draws straight out of
' the mailbox RX window because nothing is cached -- this client fetches a
' whole page of lobby records up front (see fujinet.bas's chunked net_read
' loop) and caches it in cart RAM at SC_RECS. Rendering and mounting both
' read from SC_RECS, not FN_RX. IntyBASIC's 228 8-bit / 47 16-bit scratchpad
' vars are still off-limits for anything list-shaped -- only SC_* cart RAM
' holds tabular data.

    CONST BACKTAB_ADDR       = $0200   ' Start of the BACKtab in RAM.
    CONST SCREEN_ROWS        = 12
    CONST SCREEN_COLS        = 20

    DEF FN screenpos(aColumn, aRow)  = (((aRow)*SCREEN_COLS)+(aColumn))
    DEF FN screenaddr(aColumn, aRow) = (BACKTAB_ADDR+(((aRow)*SCREEN_COLS)+(aColumn)))

    ' Color-stack mode colors (foreground; also usable as background via CS_ADVANCE).
    CONST CS_BLACK      = $0000
    CONST CS_BLUE       = $0001
    CONST CS_RED        = $0002
    CONST CS_TAN        = $0003
    CONST CS_DARKGREEN  = $0004
    CONST CS_GREEN      = $0005
    CONST CS_YELLOW     = $0006
    CONST CS_WHITE      = $0007
    ' 8-15 are BACKGROUND ONLY: a GROM card's foreground is bits 0-2 plus bit 12,
    ' and bit 12 doubles as the Colored Squares selector, so the STIC requires
    ' foreground bit 3 to be 0 for GROM cards. Color stack REGISTERS take the full
    ' 0-15 though, which is how CS_PURPLE reaches the screen -- input.bas's
    ' grid_video only ever uses it as a register, never as a foreground.
    CONST CS_PURPLE     = $000F
    CONST CS_ADVANCE    = $2000   ' Advance the color stack by one position.

    ' -------------------------------------------------------------------
    ' GRAM cards and MOB (sprite) register bits.
    ' -------------------------------------------------------------------
    ' GLYPH_BLOCK is the program's only GRAM card, and it is shared: input.bas
    ' parks it behind the character grid's selected cell as an inverse-video MOB,
    ' and st_boot.bas draws it straight into BACKTAB for the boot progress bar.
    ' Its bitmap and its DEFINE live in screen.bas, but the CONSTs have to be
    ' here: constants.bas is INCLUDEd first and input.bas fifth, and IntyBASIC
    ' silently treats a not-yet-declared CONST as an unassigned variable reading
    ' 0, with no diagnostic (fujinet-config/intv/csbar.bas:31-35 learned this the
    ' hard way).
    CONST GLYPH_BLOCK   = 0
    CONST GRAM_SELECT   = $0800   ' bit 11: take the card from GRAM, not GROM

    ' *** SPR_BEHIND is bit 13 of the ATTRIBUTE register and means PRIORITY. ***
    ' The IntyBASIC manual (manual.txt:944) calls it "Change color stack", which
    ' is the BACKTAB meaning copy-pasted into the SPRITE section by mistake. The
    ' STIC's own documentation is unambiguous -- jzintv/doc/programming/
    ' stic.txt:247, "PRIO: if set, the MOB appears behind background cards".
    CONST SPR_VISIBLE   = $0200   ' X reg
    CONST SPR_ZOOMY2    = $0100   ' Y reg: 1x card-pixel tall (MOBs are half-pixel by default)
    CONST SPR_BEHIND    = $2000   ' A reg bit 13 = PRIO

    ' Disc directions (8-way, used for menu/grid navigation).
    CONST DISC_UP     = $0004
    CONST DISC_RIGHT  = $0002
    CONST DISC_DOWN   = $0001
    CONST DISC_LEFT   = $0008

    ' Action buttons.
    CONST BUTTON_1     = $A0   ' Top left or top right.
    CONST BUTTON_2     = $60   ' Bottom left.
    CONST BUTTON_3     = $C0   ' Bottom right.
    CONST BUTTON_MASK  = $E0

    ' Keypad, as DECODED by CONT.KEY (manual: "0-9 for numbers, 10-Clear,
    ' 11-Enter, 12-Not pressed").
    CONST KEYPAD_0      = 0
    CONST KEYPAD_1      = 1
    CONST KEYPAD_2      = 2
    CONST KEYPAD_3      = 3
    CONST KEYPAD_4      = 4
    CONST KEYPAD_5      = 5
    CONST KEYPAD_6      = 6
    CONST KEYPAD_7      = 7
    CONST KEYPAD_8      = 8
    CONST KEYPAD_9      = 9
    CONST KEYPAD_CLEAR  = 10
    CONST KEYPAD_ENTER  = 11
    CONST KEYPAD_NONE   = 12

    ' -------------------------------------------------------------------
    ' Application-level state machine.
    ' -------------------------------------------------------------------
    ' 0-based: `ON state GOSUB label1,...,labelN` in IntyBASIC uses `state`
    ' directly as the jump-table index -- see fujinet-config/intv/config.bas.
    ' Username entry is NOT a state: it's a blocking GOSUB called directly
    ' from do_list (matching st_hosts.bas's hosts_edit_selected pattern),
    ' since grid_entry already blocks in its own internal WAIT loop.
    CONST ST_LIST = 0
    CONST ST_BOOT = 1

    ' -------------------------------------------------------------------
    ' Character-grid text entry layout (input.bas's grid_entry). All 95
    ' printable ASCII characters (32-126) fit in 6 rows x 16 columns.
    ' -------------------------------------------------------------------
    ' Rows 0-2 sit on color stack p0 (blue), 3-10 on p1 (dark green), 11 on p2
    ' (purple) -- grid_entry stamps the two advance cells, grid_video programs
    ' the palette. Row 3 is blank padding so the dark green run starts cleanly
    ' above the charset, and row 10 is a blank spacer above the action row.
    CONST GRID_VALUE_ROW  = 1
    CONST GRID_PANEL_ROW  = 3    ' dark green starts here: a blank row of padding
    CONST GRID_ROW0       = 4
    CONST GRID_COL0       = 2
    CONST GRID_ROWS       = 6
    CONST GRID_COLS       = 16
    CONST GRID_ACTION_ROW = 11
    CONST GRID_ACT_COL0   = 2    ' SPC
    CONST GRID_ACT_COL1   = 7    ' DEL
    CONST GRID_ACT_COL2   = 12   ' OK
    CONST GRID_ACT_COL3   = 17   ' ESC

    ' Pixel offset from background card (0,0) to MOB coordinates, for the
    ' inverse-video cursor. Measured against the emulator by pinning the cursor
    ' on a known cell and reading back where the block landed: both axes are
    ' offset by 8, i.e. MOB (8,8) is the top-left card. Don't infer this from
    ' the IntyBASIC manual's coordinate ranges (X 0-168, Y 0-95) -- they imply
    ' an asymmetry that isn't there.
    CONST GRID_MOB_X0     = 8
    CONST GRID_MOB_Y0     = 8

    ' -------------------------------------------------------------------
    ' Wire record layout (server/model.go's GameServerMin.appendAsBinary),
    ' 189 bytes/record after a 3-byte header (count, 2 reserved).
    ' -------------------------------------------------------------------
    CONST REC_GAMETYPE    = 0    '   1  appkey key_id for the URL handoff
    CONST REC_GAME        = 1    '  17  group header text
    CONST REC_SERVER      = 18   '  33  the selectable room row
    CONST REC_SERVERURL   = 51   '  65  written to appkey[game_type]
    CONST REC_CLIENTURL   = 116  '  65  TNFS host+path to mount
    CONST REC_REGION      = 181  '   3  unused
    CONST REC_ONLINE      = 184  '   1  unused
    CONST REC_PLAYERS     = 185  '   1
    CONST REC_MAXPLAYERS  = 186  '   1
    CONST REC_PINGAGE     = 187  '   2  unused
    CONST REC_STRIDE      = 189

    ' LIST_LAST_ROW is 9, not 10, because of the colour stack: the tan
    ' selection bar needs a non-empty dark green run beneath it, so row 10
    ' is a permanent blank spacer (see st_list.bas's lb_apply_stack). Costs
    ' at most one room per page; lb_render_list already truncates num_rows
    ' in place and lb_full is latched off the server's reply, so paging is
    ' unaffected.
    CONST ENTRIES_PER_PAGE = 9
    CONST LIST_START_ROW   = 1
    CONST LIST_LAST_ROW    = 9
    CONST LIST_SPACER_ROW  = 10
    CONST LEGEND_ROW       = 11

    CONST DEVICE_SLOT = 0
    CONST NUM_HOST_SLOTS = 8
    CONST HOST_NAME_LEN  = 32

    DIM host_slot

    ' -------------------------------------------------------------------
    ' Scratch RAM ($8000-$9BFF) -- ours, outside the mailbox proper ($9C00+).
    ' -------------------------------------------------------------------
    CONST SC_RECS      = $8000  ' 1701  9 x 189 verbatim wire records
    CONST SC_NAME       = $8700 '    9  username, NUL-terminated
    CONST SC_EDIT       = $8710 '   16  grid-entry accumulator / numeral scratch
    CONST SC_HOSTS      = $8800 '  256  8 x 32, verbatim READ_HOST_SLOTS mirror
    CONST SC_BOOTPATH   = $8900 '  256  full path for SET_DEVICE_FULLPATH
    CONST SC_PSTK       = $8A00 '   20  page-start offset stack, 10 x 2 LE
    CONST SC_EROW       = $8A20 '    9  per-record screen row it was drawn on
    CONST SC_ENTRY      = $8A30 '  128  selected room name, bounce-scroll source
    CONST SC_PREVGAME   = $8AB0 '   17  previous record's game, lb_count_pages
    ' $8AC1-$9BFF free
