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
