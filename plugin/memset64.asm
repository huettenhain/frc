.code

memset proc      ; rcx=dest, rdx=char, r8=size
  mov  r9, rdi
  mov rdi, rcx
  mov rcx, r8    ; rcx=size, rdx=char, rdi=dest
  mov rax, 0
  mov  al, dl
  mov rdx, rcx
  and rcx, 7
  rep stosb
  mov rcx, rdx
  mov rdx, 0
  mov  dl, al
  shl rax, 8
   or rax, rdx
  mov  dx, ax
  shl rax, 16
   or rax, rdx
  mov edx, eax
  shl rax, 32
   or rax, rdx
  shr rcx, 3
  rep stosq
  mov rdi, r9
  ret
memset endp

end