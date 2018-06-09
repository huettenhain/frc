.586
.model flat
.code

_memset proc      ; tos = ret / dest / char / size
 push edi
  mov eax, 0
  mov  al, byte ptr [esp+12]
  mov ecx, [esp+16]
  mov edx, ecx
  and ecx, 7
  mov edi, [esp+8]
  rep stosb
  mov ecx, edx
  mov edx, eax
  shl eax, 8
   or eax, edx
  mov  dx, ax
  shl eax, 16
   or eax, edx
  shr ecx, 2
  rep stosd
  pop edi
  ret
_memset endp

end