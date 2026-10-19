; rx.inc -- reading the reply where it lies (block 1).
;
; A /view page is up to 1024 bytes, which the cart shows through one
; 256-byte window ($2B00) a slice at a time. Nothing is buffered: a
; cursor (RXSLC = slice, RXOFF = offset in it) walks the reply and
; RXNEXT selects a slice only when the cursor's differs from the one the
; cart is publishing (CURSLC). Every commit makes the cart republish
; slice 0, which mailbox.inc's MSEQ records, so the cache never lies.
;
; bin=1 records are 189 bytes from offset 3 (fujinet.inc in block 2 has
; the field offsets); RXREC turns (record, field) into a cursor.

; RECTAB: where record 0-5 starts, as (slice, offset): 3 + i*189.
RECTAB: DB      0,3, 0,192, 1,125, 2,58, 2,247, 3,180

; RXREC: cursor := record R1 (0-5), field offset R0. Leaf.
; Clobbers R0,R1, V_T0.
RXREC:  STRA,R0 V_T0
        LODZ    R1
        RRL,R0
        STRZ    R1
        LODA,R0 RECTAB,R1
        STRA,R0 RXSLC
        LODA,R0 RECTAB+1,R1
        ADDA,R0 V_T0
        STRA,R0 RXOFF
        TPSL    $01             ; carry into the slice
        BCFR,EQ RXR9
        LODA,R0 RXSLC
        ADDI,R0 1
        STRA,R0 RXSLC
RXR9:   RETC,UN

; RXNEXT: R0 = the reply byte at the cursor, with CC set by it (EQ on a
; NUL); the cursor advances. PRESERVES R2 (render loops keep their
; screen cell there). Calls one down (MSLICE, on a slice change only).
; Clobbers R0,R1,R3.
RXNEXT: LODA,R0 RXSLC
        COMA,R0 CURSLC
        BCTR,EQ RXN1
        STRA,R0 CURSLC
        STRA,R2 V_SAV           ; shelter the caller's R2
        STRZ    R2              ; the slice wanted
        BSTA,UN MSLICE
        LODA,R2 V_SAV
RXN1:   LODA,R1 RXOFF
        LODA,R0 *FRDP,R1        ; the byte
        STRZ    R3
        ADDI,R1 1
        STRA,R1 RXOFF
        BCFR,EQ RXN2            ; wrapped to 0: next slice
        LODA,R0 RXSLC
        ADDI,R0 1
        STRA,R0 RXSLC
RXN2:   LODZ    R3
        RETC,UN

; RXHAS: R0 = 0 / CC EQ when the reply holds R0 bytes from the start of
; record R1, i.e. RXLEN >= start + R0. Calls one down. Clobbers R0,R1,
; V_T0, the cursor.
RXHAS:  BSTA,UN RXREC
        LODA,R0 RXSLC
        COMA,R0 V_RXH
        BCTR,LT RXH1
        BCFR,EQ RXH0
        LODA,R0 RXOFF
        COMA,R0 V_RXL
        BCTR,GT RXH0
RXH1:   EORZ    R0
        RETC,UN
RXH0:   LODI,R0 1
        RETC,UN

; RXFLD: the string at the cursor in exactly R0 cells from the cell
; after R2, in DCOLOR: truncated, or padded with blanks so it overdraws
; opaquely. Calls two down. Clobbers R0-R3, V_FLD.
RXFLD:  STRA,R0 V_FLD
RXF1:   LODA,R0 V_FLD
        BCTR,EQ RXF9
        SUBI,R0 1
        STRA,R0 V_FLD
        ZBSR    *ZRXNEXT
        BCTR,EQ RXFZ            ; the NUL: pad the rest
        BSTA,UN DXLAT
        ZBSR    *ZDPUTC
        BCTR,UN RXF1
RXFZ:   ZBSR    *ZDPUTC         ; R0 is 0: a blank
        LODA,R0 V_FLD
        BCTR,EQ RXF9
        SUBI,R0 1
        STRA,R0 V_FLD
        EORZ    R0
        BCTR,UN RXFZ
RXF9:   RETC,UN

; RXCOPY: R3 (>0) bytes from the cursor into zone B from low address
; byte R2, then a NUL. Calls two down. Clobbers R0-R3, V_FLD.
RXCOPY: STRA,R3 V_FLD
RXC1:   ZBSR    *ZRXNEXT
        STRA,R0 ZONEB,R2
        ADDI,R2 1
        LODA,R0 V_FLD
        SUBI,R0 1
        STRA,R0 V_FLD
        BCFR,EQ RXC1
        EORZ    R0
        STRA,R0 ZONEB,R2
        RETC,UN
