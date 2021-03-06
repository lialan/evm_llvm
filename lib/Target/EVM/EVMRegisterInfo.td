//===-- EVMRegisterInfo.td - EVM Register defs --------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//  Declarations that describe the EVM register files
//  The content is copied from WebAssembly backend
//===----------------------------------------------------------------------===//

class EVMReg<string n> : Register<n> {
  let Namespace = "EVM";
}


//===----------------------------------------------------------------------===//
// Registers
//===----------------------------------------------------------------------===//

// Special registers used as the frame and stack pointer.

def FP : EVMReg<"%FP">;
def SP : EVMReg<"%SP">;

// The register allocation framework requires register classes have at least
// one register, so we define a few for the integer / floating point register
// classes since we otherwise don't need a physical register in those classes.
// These are also used a "types" in the generated assembly matcher.
def GPR256 : EVMReg<"%intreg">;

def GPR : RegisterClass<"EVM", [i256], 32, (add GPR256, FP, SP)>;

