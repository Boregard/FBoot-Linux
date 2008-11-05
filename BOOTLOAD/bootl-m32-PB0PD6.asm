;*************************************************************************
;*									 *
;*				Generic AVR Bootloader			 *
;*                                                                       *
;*                      Author: Peter Dannegger                          *
;*									 *
;*************************************************************************
.nolist
;			select the appropriate include file:
;.include "tn13def.inc"
;.include "tn2313def.inc"
;.include "tn25def.inc"
;.include "tn261def.inc"
;.include "tn44def.inc"
;.include "tn45def.inc"
;.include "tn461def.inc"
;.include "m48def.inc"
;.include "tn84def.inc"
;.include "tn85def.inc"
;.include "tn861def.inc"

;			set the SecondBootStart fuse on these AVRs:
;.include "m8def.inc"
;.include "m8515def.inc"
;.include "m8535def.inc"
;.include "m88def.inc"
;.include "m16def.inc"
;.include "m162def.inc"
;.include "m168def.inc"

;			set the FirstBootStart fuse on these AVRs:
.include "m32def.inc"
;.include "m64def.inc"
;.include "m644def.inc"
;.include "m128def.inc"
;.include "m1281def.inc"
;.include "m2561def.inc"


;			remove comment sign to exclude API-Call:
;			only on ATmega >= 8kB supported
;.equ	APICALL		= 0

;			remove comment sign to exclude Watchdog trigger:
;.equ   WDTRIGGER	= 0

;			remove comment sign to exclude CRC:
;.equ	CRC		= 0	

;			remove comment sign to exclude Verify:
;.equ	VERIFY		= 0

;-------------------------------------------------------------------------
;                               Port definitions
;-------------------------------------------------------------------------
;			set both lines equal for inverted onewire mode:

.equ    STX_PORT        = PORTB
.equ    STX             = PB0

.equ    SRX_PORT        = PORTD
.equ    SRX             = PD6

.equ	XTAL		= 16000000	; 8MHz, not critical 
.equ	BootDelay	= XTAL / 3	; 0.33s

;-------------------------------------------------------------------------
;.include "fastload.inc"
;*************************************************************************
;*									 *
;*				AVR universal Bootloader		 *
;*									 *
;*			Author: Peter Dannegger				 *
;*									 *
;*************************************************************************
;.include "fastload.h"
;*************************************************************************
;.include "compat.h"	; compatibility definitions
;------------------------------------------------------------------------
;			redefinitions for compatibility
;------------------------------------------------------------------------
.ifndef	WDTCSR
.equ	WDTCSR	= WDTCR
.endif
;---------------------------
.ifndef	WDCE
.equ	WDCE	= WDTOE
.endif
;---------------------------
.ifndef	SPMCSR
.equ    SPMCSR	= SPMCR
.endif
;---------------------------
.ifndef	RWWSRE
.ifdef  ASRE
.equ    RWWSRE  = ASRE
.endif
.endif
;---------------------------
.ifndef SPMEN
.equ    SPMEN	= SELFPRGEN
.endif
;----------------------	macros for extended IO access -------------------
.macro	xout
.if	@0 > 0x3F
	sts	@0, @1
.else
	out	@0, @1
.endif
.endmacro
;---------------------------
.macro	xin
.if	@1 > 0x3F
	lds	@0, @1
.else
	in	@0, @1
.endif
.endmacro
;---------------------------
.macro  xlpm
.if FLASHEND > 0x7FFF
	elpm	@0, @1
.else
	lpm	@0, @1
.endif
.endmacro
;------------------------------------------------------------------------
;.include "protocol.h"
;//*************************************************************************
;//                             Protocol definitions
;//-------------------------------------------------------------------------
.equ    COMMAND       = 0xA5            ;// Command sequence start
.equ    ESCAPE        = COMMAND

.equ    CONNECT       = 0xA6            ;// connection established
.equ    BADCOMMAND    = 0xA7            ;// command not supported
.equ    ANSWER        = 0xA8            ;// followed by length byte
.equ    CONTINUE      = 0xA9
.equ    SUCCESS       = 0xAA
.equ    FAIL          = 0xAB

.equ    ESC_SHIFT     = 0x80            ;// offset escape char
.equ    PROGEND       = ESC_SHIFT
;//-------------------------------------------------------------------------
;//                             APICALL definitions
;//-------------------------------------------------------------------------
.equ    API_PROG_PAGE = 0x81            ;// copy one Page from SRAM to Flash

.equ    API_SUCCESS   = 0x80            ;// success
.equ    API_ERR_FUNC  = 0xF0            ;// function not supported
.equ    API_ERR_RANGE = 0xF1            ;// address inside bootloader
.equ    API_ERR_PAGE  = 0xF2            ;// address not page aligned
;//-------------------------------------------------------------------------
;-------------------------------------------------------------------------
;				Constant definitions
;-------------------------------------------------------------------------
.equ	VERSION		= 0x0201

;------------------------------	select UART mode -------------------------
.if SRX == STX && SRX_PORT == STX_PORT
  .equ	ONEWIRE		= 3
.else
  .equ	ONEWIRE		= 0
.endif

.equ	SRX_PIN		= SRX_PORT - 2
.equ	STX_DDR		= STX_PORT - 1

;------------------------------	select bootloader size -------------------

.ifndef	APICALL
  .ifndef	FirstBootStart
    .equ	APICALL		= 0
  .else
    .equ	APICALL		= 12
  .endif
.endif

.ifndef	CRC
  .equ	CRC		= 15
.endif

.ifndef VERIFY
  .equ	VERIFY		= 14
.endif

.ifndef	WDTRIGGER
  .equ	WDTRIGGER	= 9
.endif

.ifndef	SPH
  .equ	MinSize		= 198
.else
  .equ	MinSize		= 203
.endif

.equ	BootSize	= CRC + VERIFY + ONEWIRE + WDTRIGGER + MinSize

;------------------------------	UART delay loop value --------------------
.if CRC
  .equ	UartLoop	= 28		; UART loop time
.else
  .equ	UartLoop	= 24
.endif

;------------------------------	Bootloader fuse setting ------------------
.ifdef	FirstBootStart
  .if	(FlashEnd - FirstBootStart) >= 255	; 256 Words needed
    .set	BootStart	= FirstBootStart
  .else
    .set	BootStart	= SecondBootStart
  .endif
  ;----------------------------	max possible buffer size -----------------
  .set	BufferSize	= SRAM_SIZE / 2 - PAGESIZE
  .macro testpage
    .if		BootStart % BufferSize
      .set	BufferSize = BufferSize - PAGESIZE
      .if	BootStart % BufferSize
        .set    BufferSize = BufferSize - PAGESIZE
        testpage
      .endif
    .endif
  .endmacro
	testpage	; calculate Buffersize to fit into BootStart
  ;-----------------------------------------------------------------------
  .equ	UserFlash	= (2*BootStart)
  .equ	Application	= 0
.else
  .set	BootStart	= ((FlashEnd - BootSize) / PageSize * PageSize)
  .equ	BufferSize	= PageSize
  .equ	UserFlash	= (2*BootStart - 2)
  .equ	Application	= BootStart - 1
.endif
;-------------------------------------------------------------------------
;				Using register
;-------------------------------------------------------------------------
.def	zerol		= r2
.def	zeroh		= r3
.def	baudl		= r4		; baud divider
.def	baudh		= r5
.def	crcl		= r6
.def	crch		= r7

;-------------------------------------------------------------------------
.def	appl		= r16		; rjmp to application
.def	apph		= r17
.def	polynoml	= r18		; CRC polynom 0xA001
.def	polynomh	= r19

.def	zx		= r21		; 3 byte Z pointer
.def	a0		= r22		; working registers
.def	a1		= r23
.def	twl		= r24		; wait time
.def	twh		= r25
;-------------------------------------------------------------------------
;				Using SRAM
;-------------------------------------------------------------------------
.dseg
	.org		SRAM_START
PROGBUFF:		.byte 2*BufferSize
PROGBUFFEND:
.cseg
;-------------------------------------------------------------------------
;				Macros
;-------------------------------------------------------------------------
.if ONEWIRE
  .macro	IOPortInit
	sbi	STX_PORT, SRX		; weak pullup on
	cbi	STX_DDR, SRX		; as input
  .endmacro
  .macro	TXD_0
	sbi	STX_DDR, SRX		; strong pullup = 0
  .endmacro
  .macro	TXD_1
	cbi	STX_DDR, SRX		; weak pullup = 1
  .endmacro
  .macro	SKIP_RXD_0
	sbis	SRX_PIN, SRX		; low = 1
  .endmacro
  .macro	SKIP_RXD_1
	sbic	SRX_PIN, SRX		; high = 0
  .endmacro
.else
  .macro	IOPortInit
	sbi	SRX_PORT, SRX
	sbi	STX_PORT, STX
	sbi	STX_DDR, STX
  .endmacro
  .macro	TXD_0
	cbi	STX_PORT, STX
  .endmacro
  .macro	TXD_1
	sbi	STX_PORT, STX
  .endmacro
  .macro	SKIP_RXD_0
	sbic	SRX_PIN, SRX
  .endmacro
  .macro	SKIP_RXD_1
	sbis	SRX_PIN, SRX
  .endmacro
.endif
;-------------------------------------------------------------------------
.list
	.org	BootStart
init:
	cli				; no interrupts allowed
	ldi	a0, low (RamEnd)	; initialize stack
	out	SPL, a0
.ifdef SPH
	ldi	a0, high(RamEnd)
	out	SPH, a0
.endif
	clr	zerol			; for faster clear
	clr	zeroh

.if WDTRIGGER
;.include "watchdog.inc"
;------------------------------	check, if watchdog active ----------------
	wdr
	xin	a0, WDTCSR
	ori	a0, 1<<WDCE				; change enable
	ldi	a1, 1<<WDE^1<<WDP2^1<<WDP1^1<<WDP0	; 2s
	xout	WDTCSR, a0
	sbrc	a0, WDE
	xout	WDTCSR, a1
;-------------------------------------------------------------------------
.endif

	IOPortInit
.if CRC
	ldi	polynoml, 0x01
	ldi	polynomh, 0xA0
.endif
;-------------------------------------------------------------------------
;.include "abaud.inc"			; measure baudrate
;-------------------------------------------------------------------------
;			automatic baud rate detection
;-------------------------------------------------------------------------
;
; recognize any byte,
; which contain 1 * bit time low, followed by 4 * bit times low
;                      ____    __    __ __             ____
;e.g. recognize 0x0D:      |__|  |__|  |  |__|__|__|__|
;                          0  1  2  3     5           9
;                                1*T               4*T
;
.equ	TOLERANCE	= 3
.equ	MINTIME		= 90
;
abaud:
	ldi	a0, byte3(BootDelay / 6)
_aba1:
	movw	zh:zl, zeroh:zerol	; cause first try invalid
_aba2:
	movw	yh:yl, zh:zl
	movw	zh:zl, zeroh:zerol	; z = 0x0000
_aba3:
	sbiw	twh:twl, 1		;2
	sbci	a0, 0			;1
	SKIP_RXD_0			;1	wait until RXD = 0
	brne	_aba3			;2 = 6
	breq	timeout
_aba4:
	sbiw	yh:yl, 1		;2
	adiw	zh:zl, 4		;2	count bit time
	brcs	timeout			;1	time to long
	SKIP_RXD_1			;1 	wait until RXD = 1
	rjmp	_aba4			;2 = 8
;------------------------------ correction for USB dongle !!! ------------
	mov	r0, zh
_aba5:
	asr	yl			; shift signed !
	lsr	r0
	brne	_aba5
;-------------------------------------------------------------------------
	sbiw	yh:yl, TOLERANCE
	adiw	yh:yl, 2 * TOLERANCE
	brcc	_aba2			; outside tolerance
	cpi	zl, MINTIME
	cpc	zh, zerol
	brcs	_aba2			; time to short
	sbiw	zh:zl, 2*UartLoop-1	; rounding, -loop time
	lsr	zh			; /2
	ror	zl
	movw	baudh:baudl, zh:zl
;inlined	ret
;-------------------------------------------------------------------------
;.include "password.inc"			; check password
;-------------------------------------------------------------------------
;				Check password
;-------------------------------------------------------------------------
	ldi	yl, 10			; try it 10 times
.if FLASHEND > 0x7FFF
	ldi	a0, byte3(2*Password)
	out	RAMPZ, a0
.endif
checkpwd:
	ldi	zl, low (2*Password)
	ldi	zh, high(2*Password)
_cpw2:
	XLPM	r0, z+
	tst	r0
	breq	_cpw3			; end if zero byte
	rcall	getchar
	cp	r0, a0
	breq	_cpw2			; compare next byte
	dec	yl			; count down wrong compares
	breq	timeout
	rjmp	checkpwd		; try again
_cpw3:
;-------------------------------------------------------------------------
;-------------------------------------------------------------------------
connected:
	ldi	a0, CONNECT		; password recognized
.if ONEWIRE
	rcall	syncputchar		; avoid message garbage
.else
	rcall	putchar
.endif
	rcall	getchar
	brne	connected		; until COMMAND received
;-------------------------------------------------------------------------
;.include "command.inc"			; execute commands
;-------------------------------------------------------------------------
;				Receive commands
;-------------------------------------------------------------------------
;00	get bootloader revision
;01	get buffer size
;02	get target signature
;03	get user flash size
;04	program flash
;05	start application
;06	check crc
;07	verify flash
;-------------------------------------------------------------------------
main_ok:
	ldi	a0, SUCCESS
_cex1:
	rcall	putchar
_cex2:
	rcall	getchar
	brne	_cex2			; ignore until COMMAND
_cex3:
	rcall	getchar
	breq	_cex3			; ignore further COMMAND
.if FLASHEND > 0x7FFF
	ldi	zx, 0
.endif
	movw	zh:zl, zeroh:zerol      ; Z = 0x0000,
	clt				; T = 0 (for program, verify)
	cpi	a0, 4
	brcs	SendMessage		; command 0 ... 3
	breq	program			; command 4
	cpi	a0, 5
	breq	timeout			; command 5
	cpi	a0, 7
.if VERIFY
	breq	VerifyFlash		; command 7
.endif
	ldi	a0, BADCOMMAND
;-------------------------------------------------------------------------
.if CRC
	brcc	_cex1			; command >7
;-------------------------------------------------------------------------
CheckCRC:				; command 6
	rcall	getchar			; read CRC low
	rcall	getchar			; read CRC high
	or	crcl, crch		; now CRC = 0x0000 ?
	breq	main_ok			; yes
	movw	crch:crcl, zeroh:zerol	; clear CRC
.else
	rjmp	_cex1
.endif
;-------------------------------------------------------------------------
main_error:
	ldi	a0, FAIL
	rjmp	_cex1
;-------------------------------------------------------------------------
timeout:				; command 5
	out	STX_DDR, zerol
	out	STX_PORT, zerol
.if SRX_PORT != STX_PORT
	out	SRX_PORT, zerol
.endif
.if FlashEnd > 0x0FFF
	jmp	Application
.else
	rjmp	Application		; run application
.endif
;-------------------------------------------------------------------------
;.include "message.inc"			; command 0 ... 3
;-------------------------------------------------------------------------
;				Send Messages
;-------------------------------------------------------------------------
;input: a0 = number of message 0 .. 3
;
SendMessage:
.if FLASHEND > 0x7FFF
	ldi	zx, byte3(2*Messages)
	out	RAMPZ, zx
.endif
	ldi	zl, low (2*Messages)
	ldi	zh, high(2*Messages)
	ldi	yl, 0
_sme1:
	add	zl, yl			; add offset to next message
	adc	zh, zerol
	XLPM	yl, z
	subi	a0, 1			; count down until message found
	brcc	_sme1
	ldi	a0, ANSWER		; first byte of message
_sme2:
	rcall	putchar
	XLPM	a0, z+
	subi	yl, 1
	brcc	_sme2
	rjmp	main_ok
;-------------------------------------------------------------------------
;-------------------------------------------------------------------------
.if VERIFY
;.include "verify.inc"			; command 7
;-------------------------------------------------------------------------
;		 		Verify User Flash
;-------------------------------------------------------------------------
;
_ver1:
.if FLASHEND > 0x7FFF
	out	RAMPZ, zx
	elpm	r0, z
	adiw	zh:zl, 1
	adc	zx, zerol		; 24 bit addition
.else
	lpm	r0, z+
.endif
.ifndef FirstBootStart
	cpi	zl, 3
	cpc	zh, zerol
	brcs	VerifyFlash		; exclude jump to bootloader
.endif
	cpse	r0, a0
	set
VerifyFlash:
	rcall	getchar
	brne	_ver1			; not COMMAND ?
	rcall	getchar
	subi	a0, ESC_SHIFT
	brne	_ver1			; COMMMAND + not COMMAND = End
	brts	main_error		; error, Flash not equal
	rjmp	main_ok
;-------------------------------------------------------------------------
.endif
;-------------------------------------------------------------------------
.ifdef FirstBootStart
;.include "progmega.inc"		; mega with bootstart fuse set
;-------------------------------------------------------------------------
;		 		Program User Flash
;-------------------------------------------------------------------------
_pro1:
	ldi	a0, CONTINUE
	rcall	putchar
program:
	set
	ldi	xl, low (ProgBuff)
	ldi	xh, high(ProgBuff)
	ldi	yh, high(ProgBuffEnd)
;------------------------------ Receive data into buffer -----------------
_pro2:
	rcall	getchar
	brne	_pro3
	rcall	getchar
	subi	a0, ESC_SHIFT
	brne	_pro3			; A5,80 = end mark
	brts	_pro6
	set
	rjmp	_pro4
_pro3:
	clt
	st	x+, a0
	cpi	xl, low (ProgBuffEnd)
	cpc	xh, yh
	brne	_pro2
;-------------------------------------------------------------------------
_pro4:
	ldi	xl, low (ProgBuff)
	ldi	xh, high(ProgBuff)
_pro5:
	rcall	prog_page		; CY = 1: o.k
	brcc	main_error		; error, bootloader reached
	subi	zl, low (-2*PageSize)
	sbci	zh, high(-2*PageSize)	; point to next page
.if FLASHEND > 0x7FFF
	sbci    zx, byte3(-2*BufferSize)
.endif
	cpi	xl, low (ProgBuffEnd)
	cpc	xh, yh
	brne	_pro5			; until buffer end
	brtc	_pro1
;-------------------------------------------------------------------------
_pro6:
	rjmp	main_ok
;-------------------------------------------------------------------------
;			Program page in Flash
;-------------------------------------------------------------------------
;use:	r0, r1, a0, xl, xh, zl, zh
;
;input:  X  = buffer to RAM
;        Z  = page to program
;output: CY = 0: error, attempt to overwrite itself
;
.equ    PAGEMASK	= (PAGESIZE * 2 - 1) & ~1

prog_page:
;------------------------------ Avoid self destruction ! -----------------
	cpi	zl, low (2*BootStart)
	ldi	a0, high(2*BootStart)
	cpc	zh, a0                  	; below bootloader ?
.if FLASHEND > 0x7FFF
	ldi     a0, byte3(2*BootStart)
	cpc     zx, a0
.endif
	brcc	_prp3				; CY = 0: error
;------------------------------ Fill page buffer -------------------------
_prp1:	ld	r0, x+
	ld	r1, x+
	ldi	a0, 1<<SPMEN
	rcall	do_spm
	adiw	zh:zl, 2
	mov	a0, zl
	andi	a0, low(PAGEMASK)
	brne	_prp1
;------------------------------ Erase page -------------------------------
	subi	zl, low (2*PAGESIZE)
	sbci	zh, high(2*PAGESIZE)
	ldi	a0, 1<<PGERS^1<<SPMEN		; erase page command
	rcall	do_spm
;------------------------------ Program page -----------------------------
	ldi	a0, 1<<PGWRT^1<<SPMEN		; write page command
	rcall	do_spm
	ldi	a0, 1<<RWWSRE^1<<SPMEN
do_spm:
	xout	SPMCSR, a0
.if FLASHEND > 0x7FFF
	out     RAMPZ, zx               ; 3 byte Z pointer
	xout    SPMCSR, a0
.endif
	spm
_prp2:
	xin	a0, SPMCSR
	sbrc	a0, SPMEN
	rjmp	_prp2
	sec					; CY = 1: successful
_prp3:
	ret
;-------------------------------------------------------------------------
.else
;.include "progtiny.inc"		; tiny, mega without RWW section
;-------------------------------------------------------------------------
;		 		Program User Flash
;-------------------------------------------------------------------------
;
Program:
	ldi	xl, low (ProgBuff)
	ldi	xh, high(ProgBuff)
	movw	yh:yl, xh:xl
	brts	_pro5
;---------------------- Receive data for one Page ------------------------
_pro1:
	rcall	getchar
	brne	_pro2
	rcall	getchar
	subi	a0, ESC_SHIFT
	set				; end mark received set
	breq	_pro3
	clt
_pro2:
	st	x+, a0
	cpi	xl, low(ProgBuffEnd)	; since buffer size below 256
	brne	_pro1
;-------------------------------------------------------------------------
_pro3:
	adiw	zh:zl, 0
	brne	_pro5
;------------------------------ Insert rjmp to boot loader ---------------
	ld	appl, y
	ldd	apph, y+1
	subi	appl, low (BootStart - 0x1001)	; new application jump
	sbci	apph, high(BootStart - 0x1001)
	ldi	a0, low (BootStart-1)
	ldi	a1, high(BootStart-1 + 0xC000)  ; = RJMP
	st	y, a0
	std	y+1, a1				; replace by bootloader jump
;-------------------------------------------------------------------------
;               Erase application Flash backward (avoid lock out)
;-------------------------------------------------------------------------
	ldi     zl, low (2*BootStart)
	ldi     zh, high(2*BootStart)
_pro4:
.if PageSize < 32
	sbiw	zh:zl, 2*PageSize
.else
	subi    zl, low (2*PageSize)
	sbci    zh, high(2*PageSize)
.endif
	ldi	a0, 1<<PGERS^1<<SPMEN
	out     SPMCSR, a0
	SPM                             ; CPU halted until erase done
	brne    _pro4			; until Z = 0x0000
;-------------------------------------------------------------------------
_pro5:
	brtc	_pro6
	std	y+2*PageSize-2, appl
	std	y+2*PageSize-1, apph
;---------------------- Fill page buffer ---------------------------------
_pro6:
	ld	r0, y+
	ld	r1, y+
	ldi	a0, 1<<SPMEN		; fill buffer command
	out	SPMCSR, a0
	SPM
	adiw	zh:zl, 2
	cpi	yl, low(ProgBuffEnd)
	brne	_pro6
.if PageSize < 32
	sbiw	zh:zl, 2*PageSize
.else
	subi	zl, low (PageSize * 2)
	sbci	zh, high(PageSize * 2)
.endif
;---------------------- Program page -------------------------------------
	ldi	a0, 1<<PGWRT^1<<SPMEN	; write page command
	out	SPMCSR, a0
	SPM
;---------------------- Next Page ----------------------------------------
.if PageSize < 32
	adiw	zh:zl, 2*PageSize
.else
	subi	zl, low (-PageSize * 2)
	sbci	zh, high(-PageSize * 2)	; point to next page
.endif
	brts	_pro8
	ldi	a0, CONTINUE
	rcall	putchar
_pro8:
	cpi	zl, low( 2*BootStart)
	ldi	a0, high(2*BootStart)
	cpc	zh, a0                  ; last page reached ?
	brcs	Program
	brts	_pro9
	rjmp	main_error		; error, size exceeded
_pro9:
	rjmp	main_ok
;-------------------------------------------------------------------------
.endif
;-------------------------------------------------------------------------
;.include "uart.inc"			; UART subroutines
;-------------------------------------------------------------------------
;				Receive Byte
;-------------------------------------------------------------------------
;output: a0 = byte
;used: a1
;
getchar:
.if WDTRIGGER
	wdr	
.endif
	SKIP_RXD_1			; wait for RXD = 1
	rjmp	getchar
_rx1:
	SKIP_RXD_0			; wait for RXD = 0 (start bit)
	rjmp	_rx1
	ldi	a1, 8
	movw	twh:twl, baudh:baudl
	lsr	twh
	ror	twl
	rcall	wait_time		; middle of start bit
_rx2:
	rcall	wait_bit_time		;14 + tw
	lsr	a0			;1
	SKIP_RXD_0			;1/2
	ori	a0, 0x80		;1
.if CRC
;------------------------------ CRC --------------------------------------
	sbrc	a0, 7			;1
	eor	crcl, polynoml		;1 crcl.0 ^= a0.7
	lsr	crch			;1
	ror	crcl			;1
	brcc	_rx5			;1
	eor	crcl, polynoml		;1 ^0x01
_rx5:
	brcc	_rx6			;1
	eor	crch, polynomh		;1 ^0xA0
_rx6:
;-------------------------------------------------------------------------
.else
	rjmp	pc+1			;2
	rjmp	pc+1			;2
.endif
	dec	a1			;1
	brne	_rx2			;2 = 24 + tw
	cpi	a0, COMMAND		; needed several times
	ret
;-------------------------------------------------------------------------
;				transmit byte
;-------------------------------------------------------------------------
;input: a0 = byte
;used: a1
;
putchar:
	rcall	wait_bit_time
	TXD_0
.if ONEWIRE
	rjmp	_tx2
syncputchar:				; start with 1->0 from master
	SKIP_RXD_1
	rjmp	syncputchar
_tx1:
	SKIP_RXD_0
	rjmp	_tx1
_tx2:
.else
	lpm	a1, z			;3
.endif
	ldi	a1, 9			;1
	com	a0			;1 = 5
_tx3:
.if CRC
	rjmp	pc+1			;2
	rjmp	pc+1			;2
.endif
	rcall	wait_bit_time		;14 + tw
	lsr	a0			;1
	brcc	_tx4			;1/2
	nop				;1
	TXD_0				;2
	rjmp	_tx5			;2
_tx4:
	TXD_1				;2
	rjmp	_tx5			;2
_tx5:
	dec	a1			;1
	brne	_tx3			;2 = 24 + tw
	ret
;-------------------------------------------------------------------------
;	Wait 14 cycle + tw
;
wait_bit_time:
	movw	twh:twl, baudh:baudl	;1
wait_time:
	sbiw	twh:twl, 4		;2
	brcc	wait_time		;2/1
	cpi	twl, 0xFD		;1
	brcs	_wt1			;2/1 (2)
	breq	_wt1			;2/1 (3)
	cpi	twl, 0xFF		;1
	breq	_wt1			;2/1 (4/5)
_wt1:
	ret				;4 + 3 (rcall) = 14
;-------------------------------------------------------------------------
;-------------------------------------------------------------------------
Password:
	.db	"BMIa", 0, 0		; 'a' was recognized by ABAUD
.list
Messages:
	.db 3, high(Version), low(Version), \
	    3, high(2*BufferSize), low(2*BufferSize), \
	    4, SIGNATURE_000, SIGNATURE_001, SIGNATURE_002, \
	    4, byte3(UserFlash), byte2(UserFlash), byte1(UserFlash)
;-------------------------------------------------------------------------
.if APICALL
;.include "apicall.inc"			; program Flash from application
;------------------------------------------------------------------------
;		API-Call to program Flash from application 
;------------------------------------------------------------------------
;
;input:	 R22 = command		a0 = r22
;	 X = source
;	 R21, Z = destination	zx = r21
;output: R22 = return code
;use:    R0, R1, R21, R22, R23, X, Z
;
api_call:
	in	a1, SREG
	cli
	cpi	a0, API_PROG_PAGE
	ldi	a0, API_ERR_FUNC
	brne	_apc1
	rcall	prog_page
	ldi	a0, API_SUCCESS
	brcs	_apc1			; CY = 1: o.k.
	ldi	a0, API_ERR_RANGE
_apc1:
	out	SREG, a1
	ret
;-------------------------------------------------------------------------
	.org	Flashend
	rjmp	api_call
;-------------------------------------------------------------------------
.else
	.org	Flashend
	ret
.endif
;-------------------------------------------------------------------------
;-------------------------------------------------------------------------
