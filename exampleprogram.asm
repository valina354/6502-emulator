; original code from https://www.cs.otago.ac.nz/cosc243/resources/6502js-master/namedconsts.html modified for the 128x128 display



; This is a simple example to illustrate some techniques. The goal
; here is just to draw some different colours on the screen.

; First idea is manipulating the counter to get labels that point to 
; particular addresses. Use the syntax:
;       *=$abcd
; to set the counter to memory address $abcd (note, no upper case here,
; only lower case.) Then we can use a label to remember that address.

;set up some labels for the memory mapped I/O

*=$00fe         ;location of the random number generator
RANDOM:         ;gets associated with this label

*=$00ff       ;location of the keyboard input
KEYBORD:        ;gets associated with this label

*=$200         ;beginning of screen memory
SCREEN:         ;gets associated with this label

; normally this emulator starts execution at $600.  We messed up the
; counter, so we have to put it back.

*=$600         ;we messed with the counter, so set it back
BEGIN:          ;this is where execution starts

; Now we want to write some colours to the screen.
;colour palette:
;$0: Black
;$1: White
;$2: Red
;$3: Cyan
;$4: Purple
;$5: Green
;$6: Blue
;$7: Yellow
;$8: Orange
;$9: Brown
;$a: Light red
;$b: Dark grey
;$c: Grey
;$d: Light green
;$e: Light blue
;$f: Light grey

; We'll write to the screen by storing the colour in A, and then writing
; A into our desired memory location.  We will use indexed absolute
; addressing. The absolute address is SCREEN, which we have set up to
; point to $400. The indexing part is that we offset from $400 by X. So
; the final address is $400 + X. Then to change colours we change A and
; to change pixels we change X.

;first write 4 pixels across
LDA #$00        ;init accumulator to 0
LDX #$00        ;init X to 0
STA SCREEN, X   ;write accumulator to the screen, offset by X 

CLC
ADC #$01        ;next colour (A = 1)
INX             ;next pixel (X = 1)
STA SCREEN, X   ;write accumulator to the screen, offset by X

CLC
ADC #$01        ;next colour (A = 2)
INX             ;next pixel (X = 2)
STA SCREEN, X   ;write accumulator to the screen, offset by X

CLC
ADC #$01        ;next colour (A = 3)
INX             ;next pixel (X = 3)
STA SCREEN, X   ;write accumulator to the screen, offset by X

;now skip down a line and write 4 pixels, but going backwards.
; We can't do arithmetic on X other than incrementing, so we need to
; store X into A. But A is already in use. We'll push A onto the stack
; temporarily, then pull (pop) it back off when we are done.

PHA             ;push A onto the stack.  We need A for something else
TXA             ;transfer X into A
CLC
ADC #$80        ;add 128 to A. This drops us down a line
TAX             ;transfer A back into X. X = 131
PLA             ;pull the old value of A off the stack

CLC
ADC #$01        ;next colour (A = 4)
STA SCREEN, X   ;write the first pixel

; we want to go backwards here, so we need to decrement X between pixels.
CLC
ADC #$01        ;next colour (A = 5)
DEX             ;previous pixel (X = 130)
STA SCREEN, X   ;write accumulator to the screen, offset by X

CLC
ADC #$01        ;next colour (A = 6)
DEX             ;previous pixel (X = 129)
STA SCREEN, X   ;write accumulator to the screen, offset by X

CLC
ADC #$01        ;next colour (A = 7)
DEX             ;previous pixel (X = 128)
STA SCREEN, X   ;write accumulator to the screen, offset by X
