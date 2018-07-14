.586
.model flat
.code

_memset proc      ; tos = ret / dest / char / size
   push edi
    mov ecx, dword ptr [esp+16]
  movzx eax,  byte ptr [esp+12]
    mov edi, dword ptr [esp+ 8]
    shr ecx, 2
     jz mb
    mov edx, eax
    shl eax, 8
     or eax, edx
    mov  dx, ax
    shl eax, 16
     or eax, edx
    rep stosd
mb: mov ecx, dword ptr [esp+16]
    and ecx, 3
    rep stosb
    pop edi
    ret
_memset endp

end