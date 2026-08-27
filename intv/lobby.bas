' lobby.bas -- FujiNet Game Lobby for Intellivision, entry point + state
' machine. Fetches the same binary server list the C clients (Atari/
' Apple2/C64/CoCo/MS-DOS) use from lobby.fujinet.online, lets the player
' pick a room with the disc + an action button, and mounts+boots the
' chosen game's client ROM via the same FujiNet mailbox transport
' fujinet-config/intv uses.
'
' Execution is sequential in IntyBASIC, and INCLUDE pastes files in
' verbatim -- falling into a PROCEDURE or DATA body by straight-line
' execution corrupts the return stack. So every INCLUDE happens here,
' ahead of a GOTO that jumps clear of all of them before real code starts
' (same structure as fujinet-config/intv/config.bas and every game client).
    GOTO lb_boot_start

    INCLUDE "constants.bas"
    INCLUDE "fujinet.bas"
    INCLUDE "screen.bas"
    INCLUDE "scroll.bas"
    INCLUDE "input.bas"
    INCLUDE "st_name.bas"
    INCLUDE "st_list.bas"

    ' st_list.bas + st_name.bas push the default $5000-$6FFF segment close
    ' to full, and st_boot.bas (with its own hand-rolled MOUNT_IMAGE
    ' transaction) is exactly what pushed fujinet-config's single combined
    ' program over the edge. Give it its own explicit segment rather than
    ' letting the compiler auto-continue into $7000, which fujinet-config's
    ' config.bas documents as producing a cart that fails EXEC's boot
    ' detection under jzIntv.
    ASM ORG $D000
    INCLUDE "st_boot.bas"

lb_boot_start:
    ' Color stack backgrounds, laid out for the list screen (st_list.bas's
    ' lb_apply_stack explains the run-by-run reasoning). Position 0 is BLUE
    ' on purpose: every other screen starts with scr_clear, which zeroes
    ' every BACKTAB word including the advance bit, so with no advances
    ' anywhere they sit entirely on p0 and look exactly as they did before
    ' this program had any colour at all.
    '
    ' The WAIT is load-bearing. MODE packs its four colours into IntyBASIC's
    ' _color variable and flags _mode_select; the ISR consumes that on the
    ' next frame and resets _color to 7. Any PRINT ... COLOR in between
    ' would overwrite the packed word and bring the stack up as garbage.
    MODE 0, CS_BLUE, CS_DARKGREEN, CS_TAN, CS_DARKGREEN
    BORDER CS_BLUE
    WAIT

    GOSUB scr_clear
    GOSUB fn_wait_mailbox
    IF fn_ok = 0 THEN
        GOSUB lb_no_mailbox
        GOTO lb_halt
    END IF

    GOSUB lb_resolve_name

    state = ST_LIST

lb_main_loop:
    WAIT
    ' 0-based: ST_LIST=0, ST_BOOT=1 (constants.bas), matching this label
    ' order exactly -- ON...GOSUB indexes by state directly.
    ON state GOSUB do_list, do_boot
    GOTO lb_main_loop

lb_halt:
    WAIT
    GOTO lb_halt

lb_no_mailbox: PROCEDURE
    PRINT AT screenpos(0,0) COLOR COL_NORMAL,"FUJINET LOBBY   INTV"
    PRINT AT screenpos(2,3) COLOR COL_ERROR,"NO CARTRIDGE"
    PRINT AT screenpos(2,4) COLOR COL_ERROR,"MAILBOX"
END
