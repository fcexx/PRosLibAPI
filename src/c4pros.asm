; x16-PRos API — реализация на NASM

; --- Important; do not touch ---
%macro ptr_to_si 0
    push ds
    mov ax, cs
    mov ds, ax
    mov si, [bp+6]
    mov ax, cs
    mov cl, 4
    shl ax, cl
    sub si, ax
%endmacro
%macro ptr_to_si_done 0
    pop ds
%endmacro
%macro ptr_to_bx 0
    push ds
    mov ax, cs
    mov ds, ax
    mov bx, [bp+6]
    mov ax, cs
    mov cl, 4
    shl ax, cl
    sub bx, ax
%endmacro

[BITS 16]
section .text

; --- INT 0x21: Output API ---

; c4pros_out_init()
global c4pros_out_init
c4pros_out_init:
    push bp
    mov bp, sp
    mov ah, 0x00
    int 0x21
    pop bp
    ret

; c4pros_print_white(const char *)
global c4pros_print_white
c4pros_print_white:
    push bp
    mov bp, sp
    ptr_to_si
    mov ah, 0x01
    int 0x21
    ptr_to_si_done
    pop bp
    ret

; c4pros_print_green(const char *)
global c4pros_print_green
c4pros_print_green:
    push bp
    mov bp, sp
    ptr_to_si
    mov ah, 0x02
    int 0x21
    ptr_to_si_done
    pop bp
    ret

; c4pros_print_cyan(const char *)
global c4pros_print_cyan
c4pros_print_cyan:
    push bp
    mov bp, sp
    ptr_to_si
    mov ah, 0x03
    int 0x21
    ptr_to_si_done
    pop bp
    ret

; c4pros_print_red(const char *)
global c4pros_print_red
c4pros_print_red:
    push bp
    mov bp, sp
    ptr_to_si
    mov ah, 0x04
    int 0x21
    ptr_to_si_done
    pop bp
    ret

; c4pros_print_newline(const char *)
global c4pros_print_newline
c4pros_print_newline:
    push bp
    mov bp, sp
    mov ah, 0x05
    int 0x21
    pop bp
    ret

; c4pros_clear_screen()
global c4pros_clear_screen
c4pros_clear_screen:
    push bp
    mov bp, sp
    mov ah, 0x06
    int 0x21
    pop bp
    ret

; c4pros_set_color()
global c4pros_set_color
c4pros_set_color:
    push bp
    mov bp, sp
    mov bl, [bp+6]
    mov ah, 0x07
    int 0x21
    pop bp
    ret

global c4pros_print
c4pros_print:
    push bp
    mov bp, sp
    ptr_to_si
    mov ah, 0x08
    int 0x21
    ptr_to_si_done
    pop bp
    ret

; --- INT 10h: BIOS видео ---

; c4pros_set_video_mode(mode): INT 10h AH=0, AL=mode. Стек: ret=[bp+2], mode=[bp+4].
global c4pros_set_video_mode
c4pros_set_video_mode:
    push bp
    mov bp, sp
    mov al, [bp+4]
    mov ah, 0x00
    int 0x10
    pop bp
    ret

; c4pros_set_pixel — перенесена в main.c (inline asm GAS)

; --- INT 0x22: File System API ---

global c4pros_fs_init
c4pros_fs_init:
    push bp
    mov bp, sp
    mov ah, 0x00
    int 0x22
    pushf
    pop ax
    and ax, 1
    pop bp
    ret

; buf=[bp+4], size_lo=[bp+6], size_hi=[bp+8], count=[bp+10]
global c4pros_fs_get_file_list
c4pros_fs_get_file_list:
    push bp
    mov bp, sp
    ptr_to_bx
    mov ah, 0x01
    int 0x22
    pop ds
    mov di, [bp+8]
    mov [di], bx
    mov di, [bp+10]
    mov [di], cx
    mov di, [bp+12]
    mov [di], dx
    pushf
    pop ax
    and ax, 1
    pop bp
    ret

; name=[bp+4], addr=[bp+6]; Out: BX=size, CF
global c4pros_fs_load_file
c4pros_fs_load_file:
    push bp
    mov bp, sp
    ptr_to_si
    mov cx, [bp+8]
    mov ah, 0x02
    int 0x22
    ptr_to_si_done
    pushf
    pop ax
    and ax, 1
    ; при ошибке возвращаем -1, иначе BX
    test ax, ax
    jz .ok
    mov bx, -1
.ok:
    mov ax, bx
    pop bp
    ret

; name=[bp+4], data=[bp+6], size=[bp+8]
global c4pros_fs_write_file
c4pros_fs_write_file:
    push bp
    mov bp, sp
    ptr_to_si
    mov bx, [bp+8]
    mov cx, [bp+10]
    mov ah, 0x03
    int 0x22
    ptr_to_si_done
    pushf
    pop ax
    and ax, 1
    pop bp
    ret

; name=[bp+4]
global c4pros_fs_file_exists
c4pros_fs_file_exists:
    push bp
    mov bp, sp
    ptr_to_si
    mov ah, 0x04
    int 0x22
    ptr_to_si_done
    pushf
    pop ax
    and ax, 1
    xor ax, 1
    pop bp
    ret

; name=[bp+4]
global c4pros_fs_create_empty
c4pros_fs_create_empty:
    push bp
    mov bp, sp
    ptr_to_si
    mov ah, 0x05
    int 0x22
    ptr_to_si_done
    pushf
    pop ax
    and ax, 1
    pop bp
    ret

; name=[bp+4]
global c4pros_fs_remove_file
c4pros_fs_remove_file:
    push bp
    mov bp, sp
    ptr_to_si
    mov ah, 0x06
    int 0x22
    ptr_to_si_done
    pushf
    pop ax
    and ax, 1
    pop bp
    ret

; old=[bp+4], new=[bp+6] — оба указателя, переводим в смещения
global c4pros_fs_rename_file
c4pros_fs_rename_file:
    push bp
    mov bp, sp
    ptr_to_si
    mov bx, [bp+8]
    mov ax, cs
    mov cl, 4
    shl ax, cl
    sub bx, ax
    mov ah, 0x07
    int 0x22
    ptr_to_si_done
    pushf
    pop ax
    and ax, 1
    pop bp
    ret

; name=[bp+4]
global c4pros_fs_get_file_size
c4pros_fs_get_file_size:
    push bp
    mov bp, sp
    ptr_to_si
    mov ah, 0x08
    int 0x22
    ptr_to_si_done
    pushf
    pop ax
    and ax, 1
    test ax, ax
    jz .ok
    mov bx, -1
.ok:
    mov ax, bx
    pop bp
    ret

; dir=[bp+4]
global c4pros_fs_chdir
c4pros_fs_chdir:
    push bp
    mov bp, sp
    ptr_to_si
    mov ah, 0x09
    int 0x22
    ptr_to_si_done
    pushf
    pop ax
    and ax, 1
    pop bp
    ret

; AH=0x0A. Out: CF
global c4pros_fs_parent_dir
c4pros_fs_parent_dir:
    push bp
    mov bp, sp
    mov ah, 0x0A
    int 0x22
    pushf
    pop ax
    and ax, 1
    pop bp
    ret

; dir=[bp+4]
global c4pros_fs_mkdir
c4pros_fs_mkdir:
    push bp
    mov bp, sp
    ptr_to_si
    mov ah, 0x0B
    int 0x22
    ptr_to_si_done
    pushf
    pop ax
    and ax, 1
    pop bp
    ret

; dir=[bp+4]
global c4pros_fs_rmdir
c4pros_fs_rmdir:
    push bp
    mov bp, sp
    ptr_to_si
    mov ah, 0x0C
    int 0x22
    ptr_to_si_done
    pushf
    pop ax
    and ax, 1
    pop bp
    ret

; AH=0x0D, SI=name. Out: CF set if directory. Возвращаем 1=dir, 0=no
global c4pros_fs_is_dir
c4pros_fs_is_dir:
    push bp
    mov bp, sp
    ptr_to_si
    mov ah, 0x0D
    int 0x22
    ptr_to_si_done
    pushf
    pop ax
    and ax, 1
    pop bp
    ret

; AH=0x0E
global c4pros_fs_save_dir
c4pros_fs_save_dir:
    push bp
    mov bp, sp
    mov ah, 0x0E
    int 0x22
    pop bp
    ret

; AH=0x0F
global c4pros_fs_restore_dir
c4pros_fs_restore_dir:
    push bp
    mov bp, sp
    mov ah, 0x0F
    int 0x22
    pop bp
    ret


; name=[bp+4], offset=[bp+6], segment=[bp+8]
global c4pros_fs_load_huge_file
c4pros_fs_load_huge_file:
    push bp
    mov bp, sp
    ptr_to_si
    mov cx, [bp+8]
    mov dx, [bp+10]
    mov ah, 0x10
    int 0x22
    ptr_to_si_done
    pushf
    pop ax
    and ax, 1
    pop bp
    ret
