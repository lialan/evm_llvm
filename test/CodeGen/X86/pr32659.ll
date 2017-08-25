; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc -o - %s | FileCheck %s

target triple = "i386-unknown-linux-gnu"

@a = external global i32, align 4
@d = external global i32*, align 4
@k = external global i32**, align 4
@j = external global i32***, align 4
@h = external global i32, align 4
@c = external global i32, align 4
@i = external global i32, align 4
@b = external global i32, align 4
@f = external global i64, align 8
@e = external global i64, align 8
@g = external global i32, align 4

declare i32 @fn1(i32 returned) optsize readnone

declare i32 @main() optsize

declare i32 @putchar(i32) nounwind

define void @fn2() nounwind optsize {
; CHECK-LABEL: fn2:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    pushl %ebx
; CHECK-NEXT:    subl $8, %esp
; CHECK-NEXT:    movl $48, (%esp)
; CHECK-NEXT:    calll putchar
; CHECK-NEXT:    movl h, %eax
; CHECK-NEXT:    movl c, %ecx
; CHECK-NEXT:    movl j, %edx
; CHECK-NEXT:    movl (%edx), %edx
; CHECK-NEXT:    movl (%edx), %edx
; CHECK-NEXT:    xorl %ebx, %ebx
; CHECK-NEXT:    cmpl (%edx), %ecx
; CHECK-NEXT:    setg %bl
; CHECK-NEXT:    xorl %ecx, %ecx
; CHECK-NEXT:    cmpl %ebx, i
; CHECK-NEXT:    setg %cl
; CHECK-NEXT:    movl %ecx, b
; CHECK-NEXT:    xorl %edx, %edx
; CHECK-NEXT:    cmpl %ecx, %eax
; CHECK-NEXT:    setg %dl
; CHECK-NEXT:    xorl %edx, a
; CHECK-NEXT:    movl d, %eax
; CHECK-NEXT:    movl (%eax), %eax
; CHECK-NEXT:    andl %eax, e
; CHECK-NEXT:    sarl $31, %eax
; CHECK-NEXT:    andl %eax, e+4
; CHECK-NEXT:    decl g
; CHECK-NEXT:    movl f, %eax
; CHECK-NEXT:    addl $1, %eax
; CHECK-NEXT:    adcl $0, f+4
; CHECK-NEXT:    movl %eax, f
; CHECK-NEXT:    addl $8, %esp
; CHECK-NEXT:    popl %ebx
; CHECK-NEXT:    retl
entry:
  %putchar = tail call i32 @putchar(i32 48)
  %0 = load volatile i32, i32* @h, align 4
  %1 = load i32, i32* @c, align 4, !tbaa !1
  %2 = load i32***, i32**** @j, align 4
  %3 = load i32**, i32*** %2, align 4
  %4 = load i32*, i32** %3, align 4
  %5 = load i32, i32* %4, align 4
  %cmp = icmp sgt i32 %1, %5
  %conv = zext i1 %cmp to i32
  %6 = load i32, i32* @i, align 4
  %cmp1 = icmp sgt i32 %6, %conv
  %conv2 = zext i1 %cmp1 to i32
  store i32 %conv2, i32* @b, align 4
  %cmp3 = icmp sgt i32 %0, %conv2
  %conv4 = zext i1 %cmp3 to i32
  %7 = load i32, i32* @a, align 4
  %or = xor i32 %7, %conv4
  store i32 %or, i32* @a, align 4
  %8 = load i32*, i32** @d, align 4
  %9 = load i32, i32* %8, align 4
  %conv6 = sext i32 %9 to i64
  %10 = load i64, i64* @e, align 8
  %and = and i64 %10, %conv6
  store i64 %and, i64* @e, align 8
  %11 = load i32, i32* @g, align 4
  %dec = add nsw i32 %11, -1
  store i32 %dec, i32* @g, align 4
  %12 = load i64, i64* @f, align 8
  %inc = add nsw i64 %12, 1
  store i64 %inc, i64* @f, align 8
  ret void
}

!0 = !{i32 1, !"NumRegisterParameters", i32 0}
!1 = !{!2, !2, i64 0}
!2 = !{!"int", !3, i64 0}
!3 = !{!"omnipotent char", !4, i64 0}
!4 = !{!"Simple C/C++ TBAA"}
!5 = !{!6, !6, i64 0}
!6 = !{!"any pointer", !3, i64 0}
!7 = !{!8, !8, i64 0}
!8 = !{!"long long", !3, i64 0}
