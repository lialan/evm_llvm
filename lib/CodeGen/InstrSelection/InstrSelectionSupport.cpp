//===-- InstrSelectionSupport.cpp -----------------------------------------===//
//
// Target-independent instruction selection code.  See SparcInstrSelection.cpp
// for usage.
// 
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/InstrSelectionSupport.h"
#include "llvm/CodeGen/InstrSelection.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrAnnot.h"
#include "llvm/CodeGen/MachineCodeForInstruction.h"
#include "llvm/CodeGen/MachineCodeForMethod.h"
#include "llvm/CodeGen/InstrForest.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/MachineRegInfo.h"
#include "llvm/Constants.h"
#include "llvm/Function.h"
#include "llvm/Type.h"
#include "llvm/iMemory.h"
using std::vector;

//*************************** Local Functions ******************************/


// Generate code to load the constant into a TmpInstruction (virtual reg) and
// returns the virtual register.
// 
static TmpInstruction*
InsertCodeToLoadConstant(Function *F,
                         Value* opValue,
                         Instruction* vmInstr,
                         vector<MachineInstr*>& loadConstVec,
                         TargetMachine& target)
{
  // Create a tmp virtual register to hold the constant.
  TmpInstruction* tmpReg = new TmpInstruction(opValue);
  MachineCodeForInstruction &mcfi = MachineCodeForInstruction::get(vmInstr);
  mcfi.addTemp(tmpReg);
  
  target.getInstrInfo().CreateCodeToLoadConst(target, F, opValue, tmpReg,
                                              loadConstVec, mcfi);
  
  // Record the mapping from the tmp VM instruction to machine instruction.
  // Do this for all machine instructions that were not mapped to any
  // other temp values created by 
  // tmpReg->addMachineInstruction(loadConstVec.back());
  
  return tmpReg;
}


//---------------------------------------------------------------------------
// Function GetConstantValueAsUnsignedInt
// Function GetConstantValueAsSignedInt
// 
// Convenience functions to get the value of an integral constant, for an
// appropriate integer or non-integer type that can be held in a signed
// or unsigned integer respectively.  The type of the argument must be
// the following:
//      Signed or unsigned integer
//      Boolean
//      Pointer
// 
// isValidConstant is set to true if a valid constant was found.
//---------------------------------------------------------------------------

uint64_t
GetConstantValueAsUnsignedInt(const Value *V,
                              bool &isValidConstant)
{
  isValidConstant = true;

  if (isa<Constant>(V))
    if (const ConstantBool *CB = dyn_cast<ConstantBool>(V))
      return (int64_t)CB->getValue();
    else if (const ConstantSInt *CS = dyn_cast<ConstantSInt>(V))
      return (uint64_t)CS->getValue();
    else if (const ConstantUInt *CU = dyn_cast<ConstantUInt>(V))
      return CU->getValue();

  isValidConstant = false;
  return 0;
}

int64_t
GetConstantValueAsSignedInt(const Value *V,
                            bool &isValidConstant)
{
  uint64_t C = GetConstantValueAsUnsignedInt(V, isValidConstant);
  if (isValidConstant) {
    if (V->getType()->isSigned() || C < INT64_MAX) // safe to cast to signed
      return (int64_t) C;
    else
      isValidConstant = false;
  }
  return 0;
}


//---------------------------------------------------------------------------
// Function: FoldGetElemChain
// 
// Purpose:
//   Fold a chain of GetElementPtr instructions containing only
//   constant offsets into an equivalent (Pointer, IndexVector) pair.
//   Returns the pointer Value, and stores the resulting IndexVector
//   in argument chainIdxVec. This is a helper function for
//   FoldConstantIndices that does the actual folding. 
//---------------------------------------------------------------------------


// Check for a constant 0.
inline bool
IsZero(Value* idx)
{
  return (idx == ConstantSInt::getNullValue(idx->getType()));
}

static Value*
FoldGetElemChain(InstrTreeNode* ptrNode, vector<Value*>& chainIdxVec,
                 bool lastInstHasLeadingNonZero)
{
  InstructionNode* gepNode = dyn_cast<InstructionNode>(ptrNode);
  GetElementPtrInst* gepInst =
    dyn_cast_or_null<GetElementPtrInst>(gepNode ? gepNode->getInstruction() :0);

  // ptr value is not computed in this tree or ptr value does not come from GEP
  // instruction
  if (gepInst == NULL)
    return NULL;

  // Return NULL if we don't fold any instructions in.
  Value* ptrVal = NULL;

  // Now chase the chain of getElementInstr instructions, if any.
  // Check for any non-constant indices and stop there.
  // Also, stop if the first index of child is a non-zero array index
  // and the last index of the current node is a non-array index:
  // in that case, a non-array declared type is being accessed as an array
  // which is not type-safe, but could be legal.
  // 
  InstructionNode* ptrChild = gepNode;
  while (ptrChild && (ptrChild->getOpLabel() == Instruction::GetElementPtr ||
                      ptrChild->getOpLabel() == GetElemPtrIdx))
    {
      // Child is a GetElemPtr instruction
      gepInst = cast<GetElementPtrInst>(ptrChild->getValue());
      User::op_iterator OI, firstIdx = gepInst->idx_begin();
      User::op_iterator lastIdx = gepInst->idx_end();
      bool allConstantOffsets = true;

      // The first index of every GEP must be an array index.
      assert((*firstIdx)->getType() == Type::LongTy &&
             "INTERNAL ERROR: Structure index for a pointer type!");

      // If the last instruction had a leading non-zero index,
      // check if the current one ends with an array index.  If not,
      // the code is not type-safe and we would create an illegal GEP
      // by folding them, so don't fold any more instructions.
      // 
      if (lastInstHasLeadingNonZero)
        if (firstIdx != lastIdx && (*(lastIdx-1))->getType() != Type::LongTy)
          break; // cannot fold in any preceding getElementPtr instrs.

      // Check that all offsets are constant for this instruction
      for (OI = firstIdx; allConstantOffsets && OI != lastIdx; ++OI)
        allConstantOffsets = isa<ConstantInt>(*OI);

      if (allConstantOffsets)
        { // Get pointer value out of ptrChild.
          ptrVal = gepInst->getPointerOperand();

          // Remember if it has leading zero index: it will be discarded later.
          lastInstHasLeadingNonZero = ! IsZero(*firstIdx);

          // Insert its index vector at the start, skipping any leading [0]
          chainIdxVec.insert(chainIdxVec.begin(),
                             firstIdx + !lastInstHasLeadingNonZero, lastIdx);

          // Mark the folded node so no code is generated for it.
          ((InstructionNode*) ptrChild)->markFoldedIntoParent();

          // Get the previous GEP instruction and continue trying to fold
          ptrChild = dyn_cast<InstructionNode>(ptrChild->leftChild());
        }
      else // cannot fold this getElementPtr instr. or any preceding ones
        break;
    }

  // If the first getElementPtr instruction had a leading [0], add it back.
  // Note that this instruction is the *last* one successfully folded above.
  if (ptrVal && ! lastInstHasLeadingNonZero) 
    chainIdxVec.insert(chainIdxVec.begin(), ConstantSInt::get(Type::LongTy,0));

  return ptrVal;
}


//---------------------------------------------------------------------------
// Function: GetMemInstArgs
// 
// Purpose:
//   Get the pointer value and the index vector for a memory operation
//   (GetElementPtr, Load, or Store).  If all indices of the given memory
//   operation are constant, fold in constant indices in a chain of
//   preceding GetElementPtr instructions (if any), and return the
//   pointer value of the first instruction in the chain.
//   All folded instructions are marked so no code is generated for them.
//
// Return values:
//   Returns the pointer Value to use.
//   Returns the resulting IndexVector in idxVec.
//   Returns true/false in allConstantIndices if all indices are/aren't const.
//---------------------------------------------------------------------------

Value*
GetMemInstArgs(const InstructionNode* memInstrNode,
               vector<Value*>& idxVec,
               bool& allConstantIndices)
{
  allConstantIndices = true;
  Instruction* memInst = memInstrNode->getInstruction();
  assert(idxVec.size() == 0 && "Need empty vector to return indices");

  // If there is a GetElemPtr instruction to fold in to this instr,
  // it must be in the left child for Load and GetElemPtr, and in the
  // right child for Store instructions.
  InstrTreeNode* ptrChild = (memInst->getOpcode() == Instruction::Store
                             ? memInstrNode->rightChild()
                             : memInstrNode->leftChild()); 

  // Default pointer is the one from the current instruction.
  Value* ptrVal = ptrChild->getValue(); 

  // GEP is the only indexed memory instruction.  Extract its index vector.
  // Also, if all indices are constant and first index is zero, try to fold
  // in preceding GEPs with all constant indices.
  GetElementPtrInst* gepI = dyn_cast<GetElementPtrInst>(memInst);
  if (gepI)
    for (User::op_iterator OI=gepI->idx_begin(),  OE=gepI->idx_end();
         allConstantIndices && OI != OE; ++OI)
      if (! isa<Constant>(*OI))
        allConstantIndices = false;     // note: this also terminates loop!

  // If we have only constant indices, fold chains of constant indices
  // in this and any preceding GetElemPtr instructions.
  bool foldedGEPs = false;
  bool leadingNonZeroIdx = gepI && ! IsZero(*gepI->idx_begin());
  if (allConstantIndices)
    if (Value* newPtr = FoldGetElemChain(ptrChild, idxVec, leadingNonZeroIdx))
      {
        ptrVal = newPtr;
        foldedGEPs = true;
      }

  // Append the index vector of the current instruction, if any.
  // Skip the leading [0] index if preceding GEPs were folded into this.
  if (gepI)
    idxVec.insert(idxVec.end(),
                  gepI->idx_begin() + (foldedGEPs && !leadingNonZeroIdx),
                  gepI->idx_end());

  return ptrVal;
}

//------------------------------------------------------------------------ 
// Function Set2OperandsFromInstr
// Function Set3OperandsFromInstr
// 
// For the common case of 2- and 3-operand arithmetic/logical instructions,
// set the m/c instr. operands directly from the VM instruction's operands.
// Check whether the first or second operand is 0 and can use a dedicated "0"
// register.
// Check whether the second operand should use an immediate field or register.
// (First and third operands are never immediates for such instructions.)
// 
// Arguments:
// canDiscardResult: Specifies that the result operand can be discarded
//		     by using the dedicated "0"
// 
// op1position, op2position and resultPosition: Specify in which position
//		     in the machine instruction the 3 operands (arg1, arg2
//		     and result) should go.
// 
//------------------------------------------------------------------------ 

void
Set2OperandsFromInstr(MachineInstr* minstr,
		      InstructionNode* vmInstrNode,
		      const TargetMachine& target,
		      bool canDiscardResult,
		      int op1Position,
		      int resultPosition)
{
  Set3OperandsFromInstr(minstr, vmInstrNode, target,
			canDiscardResult, op1Position,
			/*op2Position*/ -1, resultPosition);
}


void
Set3OperandsFromInstr(MachineInstr* minstr,
		      InstructionNode* vmInstrNode,
		      const TargetMachine& target,
		      bool canDiscardResult,
		      int op1Position,
		      int op2Position,
		      int resultPosition)
{
  assert(op1Position >= 0);
  assert(resultPosition >= 0);
  
  // operand 1
  minstr->SetMachineOperandVal(op1Position, MachineOperand::MO_VirtualRegister,
			    vmInstrNode->leftChild()->getValue());   
  
  // operand 2 (if any)
  if (op2Position >= 0)
    minstr->SetMachineOperandVal(op2Position, MachineOperand::MO_VirtualRegister,
			      vmInstrNode->rightChild()->getValue());   
  
  // result operand: if it can be discarded, use a dead register if one exists
  if (canDiscardResult && target.getRegInfo().getZeroRegNum() >= 0)
    minstr->SetMachineOperandReg(resultPosition,
			      target.getRegInfo().getZeroRegNum());
  else
    minstr->SetMachineOperandVal(resultPosition,
			      MachineOperand::MO_VirtualRegister, vmInstrNode->getValue());
}


MachineOperand::MachineOperandType
ChooseRegOrImmed(int64_t intValue,
                 bool isSigned,
		 MachineOpCode opCode,
		 const TargetMachine& target,
		 bool canUseImmed,
		 unsigned int& getMachineRegNum,
		 int64_t& getImmedValue)
{
  MachineOperand::MachineOperandType opType=MachineOperand::MO_VirtualRegister;
  getMachineRegNum = 0;
  getImmedValue = 0;

  if (canUseImmed &&
	   target.getInstrInfo().constantFitsInImmedField(opCode, intValue))
    {
      opType = isSigned? MachineOperand::MO_SignExtendedImmed
                       : MachineOperand::MO_UnextendedImmed;
      getImmedValue = intValue;
    }
  else if (intValue == 0 && target.getRegInfo().getZeroRegNum() >= 0)
    {
      opType = MachineOperand::MO_MachineRegister;
      getMachineRegNum = target.getRegInfo().getZeroRegNum();
    }

  return opType;
}


MachineOperand::MachineOperandType
ChooseRegOrImmed(Value* val,
		 MachineOpCode opCode,
		 const TargetMachine& target,
		 bool canUseImmed,
		 unsigned int& getMachineRegNum,
		 int64_t& getImmedValue)
{
  getMachineRegNum = 0;
  getImmedValue = 0;

  // To use reg or immed, constant needs to be integer, bool, or a NULL pointer
  Constant *CPV = dyn_cast<Constant>(val);
  if (CPV == NULL ||
      (! CPV->getType()->isIntegral() &&
       ! (isa<PointerType>(CPV->getType()) && CPV->isNullValue())))
    return MachineOperand::MO_VirtualRegister;

  // Now get the constant value and check if it fits in the IMMED field.
  // Take advantage of the fact that the max unsigned value will rarely
  // fit into any IMMED field and ignore that case (i.e., cast smaller
  // unsigned constants to signed).
  // 
  int64_t intValue;
  if (isa<PointerType>(CPV->getType()))
    intValue = 0;                       // We checked above that it is NULL 
  else if (ConstantBool* CB = dyn_cast<ConstantBool>(CPV))
    intValue = (int64_t) CB->getValue();
  else if (CPV->getType()->isSigned())
    intValue = cast<ConstantSInt>(CPV)->getValue();
  else
    { // get the int value and sign-extend if original was less than 64 bits
      intValue = (int64_t) cast<ConstantUInt>(CPV)->getValue();
      switch(CPV->getType()->getPrimitiveID())
        {
        case Type::UByteTyID:  intValue = (int64_t) (int8_t) intValue; break;
        case Type::UShortTyID: intValue = (int64_t) (short)  intValue; break;
        case Type::UIntTyID:   intValue = (int64_t) (int)    intValue; break;
        default: break;
        }
    }

  return ChooseRegOrImmed(intValue, CPV->getType()->isSigned(),
                          opCode, target, canUseImmed,
                          getMachineRegNum, getImmedValue);
}


//---------------------------------------------------------------------------
// Function: FixConstantOperandsForInstr
// 
// Purpose:
// Special handling for constant operands of a machine instruction
// -- if the constant is 0, use the hardwired 0 register, if any;
// -- if the constant fits in the IMMEDIATE field, use that field;
// -- else create instructions to put the constant into a register, either
//    directly or by loading explicitly from the constant pool.
// 
// In the first 2 cases, the operand of `minstr' is modified in place.
// Returns a vector of machine instructions generated for operands that
// fall under case 3; these must be inserted before `minstr'.
//---------------------------------------------------------------------------

vector<MachineInstr*>
FixConstantOperandsForInstr(Instruction* vmInstr,
                            MachineInstr* minstr,
                            TargetMachine& target)
{
  vector<MachineInstr*> loadConstVec;
  
  MachineOpCode opCode = minstr->getOpCode();
  const MachineInstrInfo& instrInfo = target.getInstrInfo();
  const MachineInstrDescriptor& instrDesc = instrInfo.getDescriptor(opCode);
  int immedPos = instrInfo.getImmedConstantPos(opCode);

  Function *F = vmInstr->getParent()->getParent();

  for (unsigned op=0; op < minstr->getNumOperands(); op++)
    {
      const MachineOperand& mop = minstr->getOperand(op);
          
      // Skip the result position, preallocated machine registers, or operands
      // that cannot be constants (CC regs or PC-relative displacements)
      if (instrDesc.resultPos == (int) op ||
          mop.getOperandType() == MachineOperand::MO_MachineRegister ||
          mop.getOperandType() == MachineOperand::MO_CCRegister ||
          mop.getOperandType() == MachineOperand::MO_PCRelativeDisp)
        continue;

      bool constantThatMustBeLoaded = false;
      unsigned int machineRegNum = 0;
      int64_t immedValue = 0;
      Value* opValue = NULL;
      MachineOperand::MachineOperandType opType =
        MachineOperand::MO_VirtualRegister;

      // Operand may be a virtual register or a compile-time constant
      if (mop.getOperandType() == MachineOperand::MO_VirtualRegister)
        {
          assert(mop.getVRegValue() != NULL);
          opValue = mop.getVRegValue();
          if (Constant *opConst = dyn_cast<Constant>(opValue))
            {
              opType = ChooseRegOrImmed(opConst, opCode, target,
                             (immedPos == (int)op), machineRegNum, immedValue);
              if (opType == MachineOperand::MO_VirtualRegister)
                constantThatMustBeLoaded = true;
            }
        }
      else
        {
          assert(mop.getOperandType() == MachineOperand::MO_SignExtendedImmed ||
                 mop.getOperandType() == MachineOperand::MO_UnextendedImmed);

          bool isSigned = (mop.getOperandType() ==
                           MachineOperand::MO_SignExtendedImmed);

          // Bit-selection flags indicate an instruction that is extracting
          // bits from its operand so ignore this even if it is a big constant.
          if (mop.opHiBits32() || mop.opLoBits32() ||
              mop.opHiBits64() || mop.opLoBits64())
            continue;

          opType = ChooseRegOrImmed(mop.getImmedValue(), isSigned,
                                    opCode, target, (immedPos == (int)op), 
                                    machineRegNum, immedValue);

          if (opType == mop.getOperandType()) 
            continue;           // no change: this is the most common case

          if (opType == MachineOperand::MO_VirtualRegister)
            {
              constantThatMustBeLoaded = true;
              opValue = isSigned
                ? (Value*)ConstantSInt::get(Type::LongTy, immedValue)
                : (Value*)ConstantUInt::get(Type::ULongTy,(uint64_t)immedValue);
            }
        }

      if (opType == MachineOperand::MO_MachineRegister)
        minstr->SetMachineOperandReg(op, machineRegNum);
      else if (opType == MachineOperand::MO_SignExtendedImmed ||
               opType == MachineOperand::MO_UnextendedImmed)
        minstr->SetMachineOperandConst(op, opType, immedValue);
      else if (constantThatMustBeLoaded ||
               (opValue && isa<GlobalValue>(opValue)))
        { // opValue is a constant that must be explicitly loaded into a reg
          assert(opValue);
          TmpInstruction* tmpReg = InsertCodeToLoadConstant(F, opValue, vmInstr,
                                                        loadConstVec, target);
          minstr->SetMachineOperandVal(op, MachineOperand::MO_VirtualRegister,
                                       tmpReg);
        }
    }
  
  // Also, check for implicit operands used by the machine instruction
  // (no need to check those defined since they cannot be constants).
  // These include:
  // -- arguments to a Call
  // -- return value of a Return
  // Any such operand that is a constant value needs to be fixed also.
  // The current instructions with implicit refs (viz., Call and Return)
  // have no immediate fields, so the constant always needs to be loaded
  // into a register.
  // 
  bool isCall = instrInfo.isCall(opCode);
  unsigned lastCallArgNum = 0;          // unused if not a call
  CallArgsDescriptor* argDesc = NULL;   // unused if not a call
  if (isCall)
    argDesc = CallArgsDescriptor::get(minstr);
  
  for (unsigned i=0, N=minstr->getNumImplicitRefs(); i < N; ++i)
    if (isa<Constant>(minstr->getImplicitRef(i)) ||
        isa<GlobalValue>(minstr->getImplicitRef(i)))
      {
        Value* oldVal = minstr->getImplicitRef(i);
        TmpInstruction* tmpReg =
          InsertCodeToLoadConstant(F, oldVal, vmInstr, loadConstVec, target);
        minstr->setImplicitRef(i, tmpReg);
        
        if (isCall)
          { // find and replace the argument in the CallArgsDescriptor
            unsigned i=lastCallArgNum;
            while (argDesc->getArgInfo(i).getArgVal() != oldVal)
              ++i;
            assert(i < argDesc->getNumArgs() &&
                   "Constant operands to a call *must* be in the arg list");
            lastCallArgNum = i;
            argDesc->getArgInfo(i).replaceArgVal(tmpReg);
          }
      }
  
  return loadConstVec;
}


