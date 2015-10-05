;/*================================================================================*/
;/* Copyright (C) 2009, Don Milne.                                                 */
;/* All rights reserved.                                                           */
;/* See LICENSE.TXT for conditions on copying, distribution, modification and use. */
;/*================================================================================*/
;
; Assemble with NASM 2.07 like so:-
;
;    nasm -f bin -o mbr.bin mbr.asm
;
; I have a feature whereby if given a raw partition image I construct a complete
; disk image around it. This requires me to install a substitute MBR with appropriate
; boot code, so here it is. I do not include this assembly module directly in the
; project (might be tricky, as this is 16bit code), instead I assemble the code
; with the nasm command given above, creating a 512 byte bin file which I then
; embed in the executable as an RCDATA resource; which is why you found this in
; the "resource" folder.
;
; Unless you have a better (not Microsoft copyright) MBR to replace this with then
; I suggest you leave this alone.
;
; IMPORTANT WARNING: This is NOT generic MBR boot code, it is only intended to work
; on a VBox host!  In particular I don't bother querying BIOS capabilities (such as
; LBA support), because I already know that VirtualBox does support LBA reads.
;
BIOS_LOAD_ADDR equ 0x7C00
RELOC_ADDR     equ 0x600
SECTOR_MAGIC   equ 0xAA55

		BITS 16
        ORG  RELOC_ADDR

        SECTION .text

;===============================================================================
; MBR boot sector is 512 bytes, which the BIOS loads into memory at address
; 0000:7C00. However we need to free up this space because we intend to load the
; partition boot sector to the same area. So, we relocate the MBR code to a lower
; address in memory. Address 0000:0600 seems to be the standard place to relocate to.
;
START:  xor cx,cx
        mov ss,cx
        mov sp,BIOS_LOAD_ADDR  ; 7C00h and above is our data area, below 7C00h is our stack.
        sti
        push cx                ; Tech note: x86 processors decrement SP then write when
        push cx                ; pushing, so there's no overlap between our data area and stack.
        pop es
        pop ds
        mov si,BIOS_LOAD_ADDR  ; copy MBR sector from where the boot loader put it ...
		mov di,RELOC_ADDR      ; ... to my preferred address.
		mov cx,256             ; ... copy 256 words
        cld
		rep movsw
		
        ; ok, now that the MBR sector is relocated to 600h, we do a jump so
		; execution can continue at the relocated address.
		mov bx,FindBootPartition
		jmp bx

;===============================================================================
; Display an error message then hang forever.
;
DisplayMessage:
_NxtCh: lodsb
_Hang:  cmp al,0x0
        jz _Hang
        mov ah,14           ; int 10h function 14 == output single char in teletype mode
        mov bx,7            ; character attribute (light gray on black - CGA is still alive!)
        int 0x10
        jmp short _NxtCh

;===============================================================================
; Search the partition table for a bootable partition.
;
FindBootPartition:
        mov di,PARTITION_TABLE
        mov si,szInvPartTable      ; be prepared to display "Invalid partition table"
        mov cl,4                   ; max number of partition table entries
_FBPtop:
        mov bp,di
        mov al,[di]               ; First byte of partition table entry should be 0x80 (bootable) or 0.
		cmp al,0x80
        jz _GotBootable
        cmp al,0
        jne short DisplayMessage
        add di,byte 16
        loop _FBPtop
		
        int 0x18                   ; no bootable partition found, revert to ROM BASIC.

		; We found a partition entry marked bootable, make sure that no remaining entries
		; are bootable. This check could perhaps be dumped.
_NextEntry:
        cmp [di],byte 0
		jne short DisplayMessage
_GotBootable:
        add di,byte 16
        loop _NextEntry
        ; FALL THROUGH TO "BootVolume" function.
		
;===============================================================================
; Having found the partition table entry for a bootable partition, we attempt to
; read the boot sector from that partition. On success we jump to the partition
; boot code.
;
BootVolume: ; on entry BP points to part table entry, CX == 0.
        mov cl,2                   ; Boot retry counter.
        call word ReadBootSector
        jnc _CheckMagic
        dec cl
		
_RetryBoot:
        ; If first boot sector read fails then our second attempt looks for a
		; backup boot sector at pstart+6
        add word [bp+8],byte 6
        adc word [bp+10],byte 0
        call word ReadBootSector
        jnc _CheckMagic
        mov si,szErrLoadingOS      ; display "Error loading operating system"
        jmp short DisplayMessage

_CheckMagic:
        ; The sector read was successful, but check the magic number to confirm that
		; we actually got a boot sector.
        cmp word [BIOS_LOAD_ADDR+0x1fe],SECTOR_MAGIC
        jz _MagicOK
        loop _RetryBoot             ; Try the boot again using backup boot sector, if we haven't already done so.
        mov si,szErrMissingOS        ; else display "Missing operating system"
        jmp short DisplayMessage

_MagicOK: ; boot sector is ok - jump to partition boot code.
        mov si,bp                  ; si = ptr to partition table entry.
        mov di,BIOS_LOAD_ADDR      ; absolute jump to BIOS_LOAD_ADDR
        jmp di

;===============================================================================

ReadBootSector:
; Read first sector of the partition into address 0000:7C00
;
        pushaw
        mov cx,4                   ; cx==4 == retry counter
		jmp short _DoRead
		
_LBA_Retry:
        ; reset disk and try again.
		xor ah,ah                  ; else reset disk and try again.
        mov dl,[bp+0x0]
        int 0x13
		
_DoRead:
		; Note that I overwrite the first 16 bytes of my
		; own code with the disk address packet.
		mov si,RELOC_ADDR
		mov [si],word 16           ; disk packet length in bytes.
        mov [si+2],word 1          ; read 1 sector.
        mov [si+4],word BIOS_LOAD_ADDR ; into buffer at 0000:7C00
        xor ax,ax
        mov [si+6],word ax
        mov [si+12],word ax        ; LBA is a quad address, but we zero ...
        mov [si+14],word ax        ; ... the high dword.
        mov ax,[bp+8]              ; LOWORD(LBA)
		mov [si+8],ax
        mov ax,[bp+10]             ; HIWORD(LBA)
		mov [si+10],ax
        mov ah,0x42                ; int 13h function 42h == LBA read
        mov dl,[bp+0x0]
        push cx                    ; my docs don't say which regs are preserved by extended
		push bp                    ; BIOS calls, so I'm just being cautious with cx and bp.
		int 0x13
		pop bp
        pop cx
        jnc _ReadExit              ; exit if no err
		loop _LBA_Retry            ; decrement retry counter
		stc
_ReadExit:
        popaw                      ; pop regs
        ret

;===============================================================================

szInvPartTable   db "Invalid partition table",0
szErrLoadingOS   db "Error loading operating system",0
szErrMissingOS   db "Missing operating system",0

                 align 440, db 0
SIGNATURE:       dw 0,0,0
PARTITION_TABLE: dw 0
                 align 510, db 0
MAGIC:           db 0x55,0xAA

;===============================================================================

; end of mbr.asm
