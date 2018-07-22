; auto-generated code, do not edit
bits 16          ; 16-bit real mode

%include '_memmodl.inc'

%include "_watcall.inc"

%include "_comregn.inc"

%include "_segcode.inc"

..start:
global asm_entry
asm_entry:

extern entry_c_

%ifndef TINYMODE
    mov     ax,_DATA
    mov     ds,ax
%else
    ; tiny model (.com) already has CS == DS HOWEVER Open Watcom Linker insists
    ; on offset=0 in the segment, which is off by 0x100
    mov     ax,cs
    add     ax,10h
    mov     [correctseg+0x100],ax
    ; go
    db      0xEA            ; jmp seg:off
    dw      correct
correctseg:
    dw      0
correct:
    mov     ds,ax
    mov     es,ax
    mov     ss,ax
    sub     sp,0x100
%endif

; unsigned int near entry_c(void)
    call    entry_c_

global _exit_
_exit_:
    mov     ah,4Ch
    int     21h

%include "_segdata.inc"

; stack
%ifidni segment_use,use32
section _STACK align=4 class=STACK use32
%else
 %ifidni segment_use,use16
segment _STACK align=4 class=STACK use16
 %else
  %error unknown or undefined segment_use
 %endif
%endif

group DGROUP _DATA
