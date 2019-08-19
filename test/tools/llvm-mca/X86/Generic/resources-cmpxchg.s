# NOTE: Assertions have been autogenerated by utils/update_mca_test_checks.py
# RUN: llvm-mca -mtriple=x86_64-unknown-unknown -mcpu=x86-64 -instruction-tables < %s | FileCheck %s

cmpxchg8b  (%rax)
cmpxchg16b (%rax)
lock cmpxchg8b  (%rax)
lock cmpxchg16b (%rax)

cmpxchgb  %bl, %cl
cmpxchgw  %bx, %cx
cmpxchgl  %ebx, %ecx
cmpxchgq  %rbx, %rcx

cmpxchgb  %bl, (%rsi)
cmpxchgw  %bx, (%rsi)
cmpxchgl  %ebx, (%rsi)
cmpxchgq  %rbx, (%rsi)

lock cmpxchgb  %bl, (%rsi)
lock cmpxchgw  %bx, (%rsi)
lock cmpxchgl  %ebx, (%rsi)
lock cmpxchgq  %rbx, (%rsi)

# CHECK:      Instruction Info:
# CHECK-NEXT: [1]: #uOps
# CHECK-NEXT: [2]: Latency
# CHECK-NEXT: [3]: RThroughput
# CHECK-NEXT: [4]: MayLoad
# CHECK-NEXT: [5]: MayStore
# CHECK-NEXT: [6]: HasSideEffects (U)

# CHECK:      [1]    [2]    [3]    [4]    [5]    [6]    Instructions:
# CHECK-NEXT:  3      6     1.00    *      *            cmpxchg8b	(%rax)
# CHECK-NEXT:  3      6     1.00    *      *            cmpxchg16b	(%rax)
# CHECK-NEXT:  3      6     1.00    *      *            lock		cmpxchg8b	(%rax)
# CHECK-NEXT:  3      6     1.00    *      *            lock		cmpxchg16b	(%rax)
# CHECK-NEXT:  4      5     1.33                        cmpxchgb	%bl, %cl
# CHECK-NEXT:  4      5     1.33                        cmpxchgw	%bx, %cx
# CHECK-NEXT:  4      5     1.33                        cmpxchgl	%ebx, %ecx
# CHECK-NEXT:  4      5     1.33                        cmpxchgq	%rbx, %rcx
# CHECK-NEXT:  6      8     2.00    *      *            cmpxchgb	%bl, (%rsi)
# CHECK-NEXT:  6      8     2.00    *      *            cmpxchgw	%bx, (%rsi)
# CHECK-NEXT:  6      8     2.00    *      *            cmpxchgl	%ebx, (%rsi)
# CHECK-NEXT:  6      8     2.00    *      *            cmpxchgq	%rbx, (%rsi)
# CHECK-NEXT:  6      8     2.00    *      *            lock		cmpxchgb	%bl, (%rsi)
# CHECK-NEXT:  6      8     2.00    *      *            lock		cmpxchgw	%bx, (%rsi)
# CHECK-NEXT:  6      8     2.00    *      *            lock		cmpxchgl	%ebx, (%rsi)
# CHECK-NEXT:  6      8     2.00    *      *            lock		cmpxchgq	%rbx, (%rsi)

# CHECK:      Resources:
# CHECK-NEXT: [0]   - SBDivider
# CHECK-NEXT: [1]   - SBFPDivider
# CHECK-NEXT: [2]   - SBPort0
# CHECK-NEXT: [3]   - SBPort1
# CHECK-NEXT: [4]   - SBPort4
# CHECK-NEXT: [5]   - SBPort5
# CHECK-NEXT: [6.0] - SBPort23
# CHECK-NEXT: [6.1] - SBPort23

# CHECK:      Resource pressure per iteration:
# CHECK-NEXT: [0]    [1]    [2]    [3]    [4]    [5]    [6.0]  [6.1]
# CHECK-NEXT:  -      -     10.00  8.00   12.00  26.00  12.00  12.00

# CHECK:      Resource pressure by instruction:
# CHECK-NEXT: [0]    [1]    [2]    [3]    [4]    [5]    [6.0]  [6.1]  Instructions:
# CHECK-NEXT:  -      -     0.33   0.33   1.00   0.33   1.00   1.00   cmpxchg8b	(%rax)
# CHECK-NEXT:  -      -     0.33   0.33   1.00   0.33   1.00   1.00   cmpxchg16b	(%rax)
# CHECK-NEXT:  -      -     0.33   0.33   1.00   0.33   1.00   1.00   lock		cmpxchg8b	(%rax)
# CHECK-NEXT:  -      -     0.33   0.33   1.00   0.33   1.00   1.00   lock		cmpxchg16b	(%rax)
# CHECK-NEXT:  -      -     1.50   1.00    -     1.50    -      -     cmpxchgb	%bl, %cl
# CHECK-NEXT:  -      -     1.50   1.00    -     1.50    -      -     cmpxchgw	%bx, %cx
# CHECK-NEXT:  -      -     1.50   1.00    -     1.50    -      -     cmpxchgl	%ebx, %ecx
# CHECK-NEXT:  -      -     1.50   1.00    -     1.50    -      -     cmpxchgq	%rbx, %rcx
# CHECK-NEXT:  -      -     0.33   0.33   1.00   2.33   1.00   1.00   cmpxchgb	%bl, (%rsi)
# CHECK-NEXT:  -      -     0.33   0.33   1.00   2.33   1.00   1.00   cmpxchgw	%bx, (%rsi)
# CHECK-NEXT:  -      -     0.33   0.33   1.00   2.33   1.00   1.00   cmpxchgl	%ebx, (%rsi)
# CHECK-NEXT:  -      -     0.33   0.33   1.00   2.33   1.00   1.00   cmpxchgq	%rbx, (%rsi)
# CHECK-NEXT:  -      -     0.33   0.33   1.00   2.33   1.00   1.00   lock		cmpxchgb	%bl, (%rsi)
# CHECK-NEXT:  -      -     0.33   0.33   1.00   2.33   1.00   1.00   lock		cmpxchgw	%bx, (%rsi)
# CHECK-NEXT:  -      -     0.33   0.33   1.00   2.33   1.00   1.00   lock		cmpxchgl	%ebx, (%rsi)
# CHECK-NEXT:  -      -     0.33   0.33   1.00   2.33   1.00   1.00   lock		cmpxchgq	%rbx, (%rsi)
