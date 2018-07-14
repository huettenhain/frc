.code

memset proc        ; rcx=dest, rdx=char, r8=size
    mov  r9, rdi   ; backup rdi (nonvolatile)
    mov rdi, rcx
    mov rcx, r8    ; rcx=size, rdx=char, rdi=dest
  movzx rax, dl    ; rax=char (only one byte)
    mov r10, rcx   ; backup size
    shr rcx, 3     ; memset in qword steps
     jz mb         ; not even one qword
    mov rdx, rax   ; prepare patch in rax
    shl rax, 8     ; rax=000000X0
     or rax, rdx   ; rax=000000XX
    mov  dx, ax
    shl rax, 16    ; rax=0000XX00
     or rax, rdx   ; rax=0000XXXX
    mov edx, eax
    shl rax, 32    ; rax=XXXX0000
     or rax, rdx   ; rax=XXXXXXXX
    rep stosq
mb: mov rcx, r10   ; restore size
    and rcx, 7     ; calculate remainder
    rep stosb      ; write bytes
    mov rdi, r9    ; restore rdi
    ret
memset endp

end