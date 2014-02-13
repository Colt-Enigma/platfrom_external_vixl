// Copyright 2013, ARM Limited
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of ARM Limited nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "a64/macro-assembler-a64.h"
namespace vixl {

void MacroAssembler::And(const Register& rd,
                         const Register& rn,
                         const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  LogicalMacro(rd, rn, operand, AND);
}


void MacroAssembler::Ands(const Register& rd,
                          const Register& rn,
                          const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  LogicalMacro(rd, rn, operand, ANDS);
}


void MacroAssembler::Tst(const Register& rn,
                         const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  Ands(AppropriateZeroRegFor(rn), rn, operand);
}


void MacroAssembler::Bic(const Register& rd,
                         const Register& rn,
                         const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  LogicalMacro(rd, rn, operand, BIC);
}


void MacroAssembler::Bics(const Register& rd,
                          const Register& rn,
                          const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  LogicalMacro(rd, rn, operand, BICS);
}


void MacroAssembler::Orr(const Register& rd,
                         const Register& rn,
                         const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  LogicalMacro(rd, rn, operand, ORR);
}


void MacroAssembler::Orn(const Register& rd,
                         const Register& rn,
                         const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  LogicalMacro(rd, rn, operand, ORN);
}


void MacroAssembler::Eor(const Register& rd,
                         const Register& rn,
                         const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  LogicalMacro(rd, rn, operand, EOR);
}


void MacroAssembler::Eon(const Register& rd,
                         const Register& rn,
                         const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  LogicalMacro(rd, rn, operand, EON);
}


void MacroAssembler::LogicalMacro(const Register& rd,
                                  const Register& rn,
                                  const Operand& operand,
                                  LogicalOp op) {
  if (operand.IsImmediate()) {
    int64_t immediate = operand.immediate();
    unsigned reg_size = rd.size();
    ASSERT(rd.Is64Bits() || is_uint32(immediate));

    // If the operation is NOT, invert the operation and immediate.
    if ((op & NOT) == NOT) {
      op = static_cast<LogicalOp>(op & ~NOT);
      immediate = ~immediate;
      if (rd.Is32Bits()) {
        immediate &= kWRegMask;
      }
    }

    // Special cases for all set or all clear immediates.
    if (immediate == 0) {
      switch (op) {
        case AND:
          Mov(rd, 0);
          return;
        case ORR:  // Fall through.
        case EOR:
          Mov(rd, rn);
          return;
        case ANDS:  // Fall through.
        case BICS:
          break;
        default:
          UNREACHABLE();
      }
    } else if ((rd.Is64Bits() && (immediate == -INT64_C(1))) ||
               (rd.Is32Bits() && (immediate == INT64_C(0xffffffff)))) {
      switch (op) {
        case AND:
          Mov(rd, rn);
          return;
        case ORR:
          Mov(rd, immediate);
          return;
        case EOR:
          Mvn(rd, rn);
          return;
        case ANDS:  // Fall through.
        case BICS:
          break;
        default:
          UNREACHABLE();
      }
    }

    unsigned n, imm_s, imm_r;
    if (IsImmLogical(immediate, reg_size, &n, &imm_s, &imm_r)) {
      // Immediate can be encoded in the instruction.
      LogicalImmediate(rd, rn, n, imm_s, imm_r, op);
    } else {
      // Immediate can't be encoded: synthesize using move immediate.
      Register temp = AppropriateTempFor(rn);
      Mov(temp, immediate);
      if (rd.Is(sp)) {
        // If rd is the stack pointer we cannot use it as the destination
        // register so we use the temp register as an intermediate again.
        Logical(temp, rn, Operand(temp), op);
        Mov(sp, temp);
      } else {
        Logical(rd, rn, Operand(temp), op);
      }
    }
  } else if (operand.IsExtendedRegister()) {
    ASSERT(operand.reg().size() <= rd.size());
    // Add/sub extended supports shift <= 4. We want to support exactly the
    // same modes here.
    ASSERT(operand.shift_amount() <= 4);
    ASSERT(operand.reg().Is64Bits() ||
           ((operand.extend() != UXTX) && (operand.extend() != SXTX)));
    Register temp = AppropriateTempFor(rn, operand.reg());
    EmitExtendShift(temp, operand.reg(), operand.extend(),
                    operand.shift_amount());
    Logical(rd, rn, Operand(temp), op);
  } else {
    // The operand can be encoded in the instruction.
    ASSERT(operand.IsShiftedRegister());
    Logical(rd, rn, operand, op);
  }
}


void MacroAssembler::Mov(const Register& rd,
                         const Operand& operand,
                         DiscardMoveMode discard_mode) {
  ASSERT(allow_macro_instructions_);
  if (operand.IsImmediate()) {
    // Call the macro assembler for generic immediates.
    Mov(rd, operand.immediate());
  } else if (operand.IsShiftedRegister() && (operand.shift_amount() != 0)) {
    // Emit a shift instruction if moving a shifted register. This operation
    // could also be achieved using an orr instruction (like orn used by Mvn),
    // but using a shift instruction makes the disassembly clearer.
    EmitShift(rd, operand.reg(), operand.shift(), operand.shift_amount());
  } else if (operand.IsExtendedRegister()) {
    // Emit an extend instruction if moving an extended register. This handles
    // extend with post-shift operations, too.
    EmitExtendShift(rd, operand.reg(), operand.extend(),
                    operand.shift_amount());
  } else {
    // Otherwise, emit a register move only if the registers are distinct, or
    // if they are not X registers.
    //
    // Note that mov(w0, w0) is not a no-op because it clears the top word of
    // x0. A flag is provided (kDiscardForSameWReg) if a move between the same W
    // registers is not required to clear the top word of the X register. In
    // this case, the instruction is discarded.
    //
    // If the sp is an operand, add #0 is emitted, otherwise, orr #0.
    if (!rd.Is(operand.reg()) || (rd.Is32Bits() &&
                                  (discard_mode == kDontDiscardForSameWReg))) {
      mov(rd, operand.reg());
    }
  }
}


void MacroAssembler::Mvn(const Register& rd, const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  if (operand.IsImmediate()) {
    // Call the macro assembler for generic immediates.
    Mvn(rd, operand.immediate());
  } else if (operand.IsExtendedRegister()) {
    // Emit two instructions for the extend case. This differs from Mov, as
    // the extend and invert can't be achieved in one instruction.
    Register temp = AppropriateTempFor(rd, operand.reg());
    EmitExtendShift(temp, operand.reg(), operand.extend(),
                    operand.shift_amount());
    mvn(rd, Operand(temp));
  } else {
    // Otherwise, register and shifted register cases can be handled by the
    // assembler directly, using orn.
    mvn(rd, operand);
  }
}


void MacroAssembler::Mov(const Register& rd, uint64_t imm) {
  ASSERT(allow_macro_instructions_);
  ASSERT(is_uint32(imm) || is_int32(imm) || rd.Is64Bits());

  // Immediates on Aarch64 can be produced using an initial value, and zero to
  // three move keep operations.
  //
  // Initial values can be generated with:
  //  1. 64-bit move zero (movz).
  //  2. 32-bit move inverted (movn).
  //  3. 64-bit move inverted.
  //  4. 32-bit orr immediate.
  //  5. 64-bit orr immediate.
  // Move-keep may then be used to modify each of the 16-bit half words.
  //
  // The code below supports all five initial value generators, and
  // applying move-keep operations to move-zero and move-inverted initial
  // values.

  unsigned reg_size = rd.size();
  unsigned n, imm_s, imm_r;
  if (IsImmMovz(imm, reg_size) && !rd.IsSP()) {
    // Immediate can be represented in a move zero instruction. Movz can't
    // write to the stack pointer.
    movz(rd, imm);
  } else if (IsImmMovn(imm, reg_size) && !rd.IsSP()) {
    // Immediate can be represented in a move negative instruction. Movn can't
    // write to the stack pointer.
    movn(rd, rd.Is64Bits() ? ~imm : (~imm & kWRegMask));
  } else if (IsImmLogical(imm, reg_size, &n, &imm_s, &imm_r)) {
    // Immediate can be represented in a logical orr instruction.
    ASSERT(!rd.IsZero());
    LogicalImmediate(rd, AppropriateZeroRegFor(rd), n, imm_s, imm_r, ORR);
  } else {
    // Generic immediate case. Imm will be represented by
    //   [imm3, imm2, imm1, imm0], where each imm is 16 bits.
    // A move-zero or move-inverted is generated for the first non-zero or
    // non-0xffff immX, and a move-keep for subsequent non-zero immX.

    uint64_t ignored_halfword = 0;
    bool invert_move = false;
    // If the number of 0xffff halfwords is greater than the number of 0x0000
    // halfwords, it's more efficient to use move-inverted.
    if (CountClearHalfWords(~imm, reg_size) >
        CountClearHalfWords(imm, reg_size)) {
      ignored_halfword = INT64_C(0xffff);
      invert_move = true;
    }

    // Mov instructions can't move values into the stack pointer, so set up a
    // temporary register, if needed.
    Register temp = rd.IsSP() ? AppropriateTempFor(rd) : rd;

    // Iterate through the halfwords. Use movn/movz for the first non-ignored
    // halfword, and movk for subsequent halfwords.
    ASSERT((reg_size % 16) == 0);
    bool first_mov_done = false;
    for (unsigned i = 0; i < (temp.size() / 16); i++) {
      uint64_t imm16 = (imm >> (16 * i)) & INT64_C(0xffff);
      if (imm16 != ignored_halfword) {
        if (!first_mov_done) {
          if (invert_move) {
            movn(temp, (~imm16) & INT64_C(0xffff), 16 * i);
          } else {
            movz(temp, imm16, 16 * i);
          }
          first_mov_done = true;
        } else {
          // Construct a wider constant.
          movk(temp, imm16, 16 * i);
        }
      }
    }

    ASSERT(first_mov_done);

    // Move the temporary if the original destination register was the stack
    // pointer.
    if (rd.IsSP()) {
      mov(rd, temp);
    }
  }
}


unsigned MacroAssembler::CountClearHalfWords(uint64_t imm, unsigned reg_size) {
  ASSERT((reg_size % 8) == 0);
  int count = 0;
  for (unsigned i = 0; i < (reg_size / 16); i++) {
    if ((imm & 0xffff) == 0) {
      count++;
    }
    imm >>= 16;
  }
  return count;
}


// The movn instruction can generate immediates containing an arbitrary 16-bit
// value, with remaining bits set, eg. 0x00001234, 0x0000123400000000.
bool MacroAssembler::IsImmMovz(uint64_t imm, unsigned reg_size) {
  ASSERT((reg_size == kXRegSize) || (reg_size == kWRegSize));
  return CountClearHalfWords(imm, reg_size) >= ((reg_size / 16) - 1);
}


// The movn instruction can generate immediates containing an arbitrary 16-bit
// value, with remaining bits set, eg. 0xffff1234, 0xffff1234ffffffff.
bool MacroAssembler::IsImmMovn(uint64_t imm, unsigned reg_size) {
  return IsImmMovz(~imm, reg_size);
}


void MacroAssembler::Ccmp(const Register& rn,
                          const Operand& operand,
                          StatusFlags nzcv,
                          Condition cond) {
  ASSERT(allow_macro_instructions_);
  if (operand.IsImmediate() && (operand.immediate() < 0)) {
    ConditionalCompareMacro(rn, -operand.immediate(), nzcv, cond, CCMN);
  } else {
    ConditionalCompareMacro(rn, operand, nzcv, cond, CCMP);
  }
}


void MacroAssembler::Ccmn(const Register& rn,
                          const Operand& operand,
                          StatusFlags nzcv,
                          Condition cond) {
  ASSERT(allow_macro_instructions_);
  if (operand.IsImmediate() && (operand.immediate() < 0)) {
    ConditionalCompareMacro(rn, -operand.immediate(), nzcv, cond, CCMP);
  } else {
    ConditionalCompareMacro(rn, operand, nzcv, cond, CCMN);
  }
}


void MacroAssembler::ConditionalCompareMacro(const Register& rn,
                                             const Operand& operand,
                                             StatusFlags nzcv,
                                             Condition cond,
                                             ConditionalCompareOp op) {
  ASSERT((cond != al) && (cond != nv));
  if ((operand.IsShiftedRegister() && (operand.shift_amount() == 0)) ||
      (operand.IsImmediate() && IsImmConditionalCompare(operand.immediate()))) {
    // The immediate can be encoded in the instruction, or the operand is an
    // unshifted register: call the assembler.
    ConditionalCompare(rn, operand, nzcv, cond, op);
  } else {
    // The operand isn't directly supported by the instruction: perform the
    // operation on a temporary register.
    Register temp = AppropriateTempFor(rn);
    Mov(temp, operand);
    ConditionalCompare(rn, temp, nzcv, cond, op);
  }
}


void MacroAssembler::Csel(const Register& rd,
                          const Register& rn,
                          const Operand& operand,
                          Condition cond) {
  ASSERT(allow_macro_instructions_);
  ASSERT(!rd.IsZero());
  ASSERT(!rn.IsZero());
  ASSERT((cond != al) && (cond != nv));
  if (operand.IsImmediate()) {
    // Immediate argument. Handle special cases of 0, 1 and -1 using zero
    // register.
    int64_t imm = operand.immediate();
    Register zr = AppropriateZeroRegFor(rn);
    if (imm == 0) {
      csel(rd, rn, zr, cond);
    } else if (imm == 1) {
      csinc(rd, rn, zr, cond);
    } else if (imm == -1) {
      csinv(rd, rn, zr, cond);
    } else {
      Register temp = AppropriateTempFor(rn);
      Mov(temp, operand.immediate());
      csel(rd, rn, temp, cond);
    }
  } else if (operand.IsShiftedRegister() && (operand.shift_amount() == 0)) {
    // Unshifted register argument.
    csel(rd, rn, operand.reg(), cond);
  } else {
    // All other arguments.
    Register temp = AppropriateTempFor(rn);
    Mov(temp, operand);
    csel(rd, rn, temp, cond);
  }
}


void MacroAssembler::Add(const Register& rd,
                         const Register& rn,
                         const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  if (operand.IsImmediate() && (operand.immediate() < 0)) {
    AddSubMacro(rd, rn, -operand.immediate(), LeaveFlags, SUB);
  } else {
    AddSubMacro(rd, rn, operand, LeaveFlags, ADD);
  }
}


void MacroAssembler::Adds(const Register& rd,
                          const Register& rn,
                          const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  if (operand.IsImmediate() && (operand.immediate() < 0)) {
    AddSubMacro(rd, rn, -operand.immediate(), SetFlags, SUB);
  } else {
    AddSubMacro(rd, rn, operand, SetFlags, ADD);
  }
}


void MacroAssembler::Sub(const Register& rd,
                         const Register& rn,
                         const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  if (operand.IsImmediate() && (operand.immediate() < 0)) {
    AddSubMacro(rd, rn, -operand.immediate(), LeaveFlags, ADD);
  } else {
    AddSubMacro(rd, rn, operand, LeaveFlags, SUB);
  }
}


void MacroAssembler::Subs(const Register& rd,
                          const Register& rn,
                          const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  if (operand.IsImmediate() && (operand.immediate() < 0)) {
    AddSubMacro(rd, rn, -operand.immediate(), SetFlags, ADD);
  } else {
    AddSubMacro(rd, rn, operand, SetFlags, SUB);
  }
}


void MacroAssembler::Cmn(const Register& rn, const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  Adds(AppropriateZeroRegFor(rn), rn, operand);
}


void MacroAssembler::Cmp(const Register& rn, const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  Subs(AppropriateZeroRegFor(rn), rn, operand);
}


void MacroAssembler::Neg(const Register& rd,
                         const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  if (operand.IsImmediate()) {
    Mov(rd, -operand.immediate());
  } else {
    Sub(rd, AppropriateZeroRegFor(rd), operand);
  }
}


void MacroAssembler::Negs(const Register& rd,
                          const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  Subs(rd, AppropriateZeroRegFor(rd), operand);
}


void MacroAssembler::AddSubMacro(const Register& rd,
                                 const Register& rn,
                                 const Operand& operand,
                                 FlagsUpdate S,
                                 AddSubOp op) {
  if (operand.IsZero() && rd.Is(rn) && rd.Is64Bits() && rn.Is64Bits() &&
      (S == LeaveFlags)) {
    // The instruction would be a nop. Avoid generating useless code.
    return;
  }

  if ((operand.IsImmediate() && !IsImmAddSub(operand.immediate())) ||
      (rn.IsZero() && !operand.IsShiftedRegister())                ||
      (operand.IsShiftedRegister() && (operand.shift() == ROR))) {
    Register temp = AppropriateTempFor(rn);
    Mov(temp, operand);
    AddSub(rd, rn, temp, S, op);
  } else {
    AddSub(rd, rn, operand, S, op);
  }
}


void MacroAssembler::Adc(const Register& rd,
                         const Register& rn,
                         const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  AddSubWithCarryMacro(rd, rn, operand, LeaveFlags, ADC);
}


void MacroAssembler::Adcs(const Register& rd,
                          const Register& rn,
                          const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  AddSubWithCarryMacro(rd, rn, operand, SetFlags, ADC);
}


void MacroAssembler::Sbc(const Register& rd,
                         const Register& rn,
                         const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  AddSubWithCarryMacro(rd, rn, operand, LeaveFlags, SBC);
}


void MacroAssembler::Sbcs(const Register& rd,
                          const Register& rn,
                          const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  AddSubWithCarryMacro(rd, rn, operand, SetFlags, SBC);
}


void MacroAssembler::Ngc(const Register& rd,
                         const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  Register zr = AppropriateZeroRegFor(rd);
  Sbc(rd, zr, operand);
}


void MacroAssembler::Ngcs(const Register& rd,
                         const Operand& operand) {
  ASSERT(allow_macro_instructions_);
  Register zr = AppropriateZeroRegFor(rd);
  Sbcs(rd, zr, operand);
}


void MacroAssembler::AddSubWithCarryMacro(const Register& rd,
                                          const Register& rn,
                                          const Operand& operand,
                                          FlagsUpdate S,
                                          AddSubWithCarryOp op) {
  ASSERT(rd.size() == rn.size());

  if (operand.IsImmediate() ||
      (operand.IsShiftedRegister() && (operand.shift() == ROR))) {
    // Add/sub with carry (immediate or ROR shifted register.)
    Register temp = AppropriateTempFor(rn);
    Mov(temp, operand);
    AddSubWithCarry(rd, rn, Operand(temp), S, op);
  } else if (operand.IsShiftedRegister() && (operand.shift_amount() != 0)) {
    // Add/sub with carry (shifted register).
    ASSERT(operand.reg().size() == rd.size());
    ASSERT(operand.shift() != ROR);
    ASSERT(is_uintn(rd.size() == kXRegSize ? kXRegSizeLog2 : kWRegSizeLog2,
                    operand.shift_amount()));
    Register temp = AppropriateTempFor(rn, operand.reg());
    EmitShift(temp, operand.reg(), operand.shift(), operand.shift_amount());
    AddSubWithCarry(rd, rn, Operand(temp), S, op);
  } else if (operand.IsExtendedRegister()) {
    // Add/sub with carry (extended register).
    ASSERT(operand.reg().size() <= rd.size());
    // Add/sub extended supports a shift <= 4. We want to support exactly the
    // same modes.
    ASSERT(operand.shift_amount() <= 4);
    ASSERT(operand.reg().Is64Bits() ||
           ((operand.extend() != UXTX) && (operand.extend() != SXTX)));
    Register temp = AppropriateTempFor(rn, operand.reg());
    EmitExtendShift(temp, operand.reg(), operand.extend(),
                    operand.shift_amount());
    AddSubWithCarry(rd, rn, Operand(temp), S, op);
  } else {
    // The addressing mode is directly supported by the instruction.
    AddSubWithCarry(rd, rn, operand, S, op);
  }
}


#define DEFINE_FUNCTION(FN, REGTYPE, REG, OP)                         \
void MacroAssembler::FN(const REGTYPE REG, const MemOperand& addr) {  \
  LoadStoreMacro(REG, addr, OP);                                      \
}
LS_MACRO_LIST(DEFINE_FUNCTION)
#undef DEFINE_FUNCTION

void MacroAssembler::LoadStoreMacro(const CPURegister& rt,
                                    const MemOperand& addr,
                                    LoadStoreOp op) {
  int64_t offset = addr.offset();
  LSDataSize size = CalcLSDataSize(op);

  // Check if an immediate offset fits in the immediate field of the
  // appropriate instruction. If not, emit two instructions to perform
  // the operation.
  if (addr.IsImmediateOffset() && !IsImmLSScaled(offset, size) &&
      !IsImmLSUnscaled(offset)) {
    // Immediate offset that can't be encoded using unsigned or unscaled
    // addressing modes.
    Register temp = AppropriateTempFor(addr.base());
    Mov(temp, addr.offset());
    LoadStore(rt, MemOperand(addr.base(), temp), op);
  } else if (addr.IsPostIndex() && !IsImmLSUnscaled(offset)) {
    // Post-index beyond unscaled addressing range.
    LoadStore(rt, MemOperand(addr.base()), op);
    Add(addr.base(), addr.base(), Operand(offset));
  } else if (addr.IsPreIndex() && !IsImmLSUnscaled(offset)) {
    // Pre-index beyond unscaled addressing range.
    Add(addr.base(), addr.base(), Operand(offset));
    LoadStore(rt, MemOperand(addr.base()), op);
  } else {
    // Encodable in one load/store instruction.
    LoadStore(rt, addr, op);
  }
}


void MacroAssembler::Push(const CPURegister& src0, const CPURegister& src1,
                          const CPURegister& src2, const CPURegister& src3) {
  ASSERT(allow_macro_instructions_);
  ASSERT(AreSameSizeAndType(src0, src1, src2, src3));
  ASSERT(src0.IsValid());

  int count = 1 + src1.IsValid() + src2.IsValid() + src3.IsValid();
  int size = src0.SizeInBytes();

  PrepareForPush(count, size);
  PushHelper(count, size, src0, src1, src2, src3);
}


void MacroAssembler::Pop(const CPURegister& dst0, const CPURegister& dst1,
                         const CPURegister& dst2, const CPURegister& dst3) {
  // It is not valid to pop into the same register more than once in one
  // instruction, not even into the zero register.
  ASSERT(allow_macro_instructions_);
  ASSERT(!AreAliased(dst0, dst1, dst2, dst3));
  ASSERT(AreSameSizeAndType(dst0, dst1, dst2, dst3));
  ASSERT(dst0.IsValid());

  int count = 1 + dst1.IsValid() + dst2.IsValid() + dst3.IsValid();
  int size = dst0.SizeInBytes();

  PrepareForPop(count, size);
  PopHelper(count, size, dst0, dst1, dst2, dst3);
}


void MacroAssembler::PushCPURegList(CPURegList registers) {
  int size = registers.RegisterSizeInBytes();

  PrepareForPush(registers.Count(), size);
  // Push up to four registers at a time because if the current stack pointer is
  // sp and reg_size is 32, registers must be pushed in blocks of four in order
  // to maintain the 16-byte alignment for sp.
  ASSERT(allow_macro_instructions_);
  while (!registers.IsEmpty()) {
    int count_before = registers.Count();
    const CPURegister& src0 = registers.PopHighestIndex();
    const CPURegister& src1 = registers.PopHighestIndex();
    const CPURegister& src2 = registers.PopHighestIndex();
    const CPURegister& src3 = registers.PopHighestIndex();
    int count = count_before - registers.Count();
    PushHelper(count, size, src0, src1, src2, src3);
  }
}


void MacroAssembler::PopCPURegList(CPURegList registers) {
  int size = registers.RegisterSizeInBytes();

  PrepareForPop(registers.Count(), size);
  // Pop up to four registers at a time because if the current stack pointer is
  // sp and reg_size is 32, registers must be pushed in blocks of four in order
  // to maintain the 16-byte alignment for sp.
  ASSERT(allow_macro_instructions_);
  while (!registers.IsEmpty()) {
    int count_before = registers.Count();
    const CPURegister& dst0 = registers.PopLowestIndex();
    const CPURegister& dst1 = registers.PopLowestIndex();
    const CPURegister& dst2 = registers.PopLowestIndex();
    const CPURegister& dst3 = registers.PopLowestIndex();
    int count = count_before - registers.Count();
    PopHelper(count, size, dst0, dst1, dst2, dst3);
  }
}


void MacroAssembler::PushMultipleTimes(int count, Register src) {
  ASSERT(allow_macro_instructions_);
  int size = src.SizeInBytes();

  PrepareForPush(count, size);
  // Push up to four registers at a time if possible because if the current
  // stack pointer is sp and the register size is 32, registers must be pushed
  // in blocks of four in order to maintain the 16-byte alignment for sp.
  while (count >= 4) {
    PushHelper(4, size, src, src, src, src);
    count -= 4;
  }
  if (count >= 2) {
    PushHelper(2, size, src, src, NoReg, NoReg);
    count -= 2;
  }
  if (count == 1) {
    PushHelper(1, size, src, NoReg, NoReg, NoReg);
    count -= 1;
  }
  ASSERT(count == 0);
}


void MacroAssembler::PushHelper(int count, int size,
                                const CPURegister& src0,
                                const CPURegister& src1,
                                const CPURegister& src2,
                                const CPURegister& src3) {
  // Ensure that we don't unintentionally modify scratch or debug registers.
  InstructionAccurateScope scope(this);

  ASSERT(AreSameSizeAndType(src0, src1, src2, src3));
  ASSERT(size == src0.SizeInBytes());

  // When pushing multiple registers, the store order is chosen such that
  // Push(a, b) is equivalent to Push(a) followed by Push(b).
  switch (count) {
    case 1:
      ASSERT(src1.IsNone() && src2.IsNone() && src3.IsNone());
      str(src0, MemOperand(StackPointer(), -1 * size, PreIndex));
      break;
    case 2:
      ASSERT(src2.IsNone() && src3.IsNone());
      stp(src1, src0, MemOperand(StackPointer(), -2 * size, PreIndex));
      break;
    case 3:
      ASSERT(src3.IsNone());
      stp(src2, src1, MemOperand(StackPointer(), -3 * size, PreIndex));
      str(src0, MemOperand(StackPointer(), 2 * size));
      break;
    case 4:
      // Skip over 4 * size, then fill in the gap. This allows four W registers
      // to be pushed using sp, whilst maintaining 16-byte alignment for sp at
      // all times.
      stp(src3, src2, MemOperand(StackPointer(), -4 * size, PreIndex));
      stp(src1, src0, MemOperand(StackPointer(), 2 * size));
      break;
    default:
      UNREACHABLE();
  }
}


void MacroAssembler::PopHelper(int count, int size,
                               const CPURegister& dst0,
                               const CPURegister& dst1,
                               const CPURegister& dst2,
                               const CPURegister& dst3) {
  // Ensure that we don't unintentionally modify scratch or debug registers.
  InstructionAccurateScope scope(this);

  ASSERT(AreSameSizeAndType(dst0, dst1, dst2, dst3));
  ASSERT(size == dst0.SizeInBytes());

  // When popping multiple registers, the load order is chosen such that
  // Pop(a, b) is equivalent to Pop(a) followed by Pop(b).
  switch (count) {
    case 1:
      ASSERT(dst1.IsNone() && dst2.IsNone() && dst3.IsNone());
      ldr(dst0, MemOperand(StackPointer(), 1 * size, PostIndex));
      break;
    case 2:
      ASSERT(dst2.IsNone() && dst3.IsNone());
      ldp(dst0, dst1, MemOperand(StackPointer(), 2 * size, PostIndex));
      break;
    case 3:
      ASSERT(dst3.IsNone());
      ldr(dst2, MemOperand(StackPointer(), 2 * size));
      ldp(dst0, dst1, MemOperand(StackPointer(), 3 * size, PostIndex));
      break;
    case 4:
      // Load the higher addresses first, then load the lower addresses and skip
      // the whole block in the second instruction. This allows four W registers
      // to be popped using sp, whilst maintaining 16-byte alignment for sp at
      // all times.
      ldp(dst2, dst3, MemOperand(StackPointer(), 2 * size));
      ldp(dst0, dst1, MemOperand(StackPointer(), 4 * size, PostIndex));
      break;
    default:
      UNREACHABLE();
  }
}


void MacroAssembler::PrepareForPush(int count, int size) {
  if (sp.Is(StackPointer())) {
    // If the current stack pointer is sp, then it must be aligned to 16 bytes
    // on entry and the total size of the specified registers must also be a
    // multiple of 16 bytes.
    ASSERT((count * size) % 16 == 0);
  } else {
    // Even if the current stack pointer is not the system stack pointer (sp),
    // the system stack pointer will still be modified in order to comply with
    // ABI rules about accessing memory below the system stack pointer.
    BumpSystemStackPointer(count * size);
  }
}


void MacroAssembler::PrepareForPop(int count, int size) {
  USE(count);
  USE(size);
  if (sp.Is(StackPointer())) {
    // If the current stack pointer is sp, then it must be aligned to 16 bytes
    // on entry and the total size of the specified registers must also be a
    // multiple of 16 bytes.
    ASSERT((count * size) % 16 == 0);
  }
}

void MacroAssembler::Poke(const Register& src, const Operand& offset) {
  ASSERT(allow_macro_instructions_);
  if (offset.IsImmediate()) {
    ASSERT(offset.immediate() >= 0);
  }

  Str(src, MemOperand(StackPointer(), offset));
}


void MacroAssembler::Peek(const Register& dst, const Operand& offset) {
  ASSERT(allow_macro_instructions_);
  if (offset.IsImmediate()) {
    ASSERT(offset.immediate() >= 0);
  }

  Ldr(dst, MemOperand(StackPointer(), offset));
}


void MacroAssembler::Claim(const Operand& size) {
  ASSERT(allow_macro_instructions_);

  if (size.IsZero()) {
    return;
  }

  if (size.IsImmediate()) {
    ASSERT(size.immediate() > 0);
    if (sp.Is(StackPointer())) {
      ASSERT((size.immediate() % 16) == 0);
    }
  }

  if (!sp.Is(StackPointer())) {
    BumpSystemStackPointer(size);
  }

  Sub(StackPointer(), StackPointer(), size);
}


void MacroAssembler::Drop(const Operand& size) {
  ASSERT(allow_macro_instructions_);

  if (size.IsZero()) {
    return;
  }

  if (size.IsImmediate()) {
    ASSERT(size.immediate() > 0);
    if (sp.Is(StackPointer())) {
      ASSERT((size.immediate() % 16) == 0);
    }
  }

  Add(StackPointer(), StackPointer(), size);
}


void MacroAssembler::PushCalleeSavedRegisters() {
  // Ensure that the macro-assembler doesn't use any scratch registers.
  InstructionAccurateScope scope(this);

  // This method must not be called unless the current stack pointer is sp.
  ASSERT(sp.Is(StackPointer()));

  MemOperand tos(sp, -2 * kXRegSizeInBytes, PreIndex);

  stp(d14, d15, tos);
  stp(d12, d13, tos);
  stp(d10, d11, tos);
  stp(d8, d9, tos);

  stp(x29, x30, tos);
  stp(x27, x28, tos);
  stp(x25, x26, tos);
  stp(x23, x24, tos);
  stp(x21, x22, tos);
  stp(x19, x20, tos);
}


void MacroAssembler::PopCalleeSavedRegisters() {
  // Ensure that the macro-assembler doesn't use any scratch registers.
  InstructionAccurateScope scope(this);

  // This method must not be called unless the current stack pointer is sp.
  ASSERT(sp.Is(StackPointer()));

  MemOperand tos(sp, 2 * kXRegSizeInBytes, PostIndex);

  ldp(x19, x20, tos);
  ldp(x21, x22, tos);
  ldp(x23, x24, tos);
  ldp(x25, x26, tos);
  ldp(x27, x28, tos);
  ldp(x29, x30, tos);

  ldp(d8, d9, tos);
  ldp(d10, d11, tos);
  ldp(d12, d13, tos);
  ldp(d14, d15, tos);
}

void MacroAssembler::BumpSystemStackPointer(const Operand& space) {
  ASSERT(!sp.Is(StackPointer()));
  // TODO: Several callers rely on this not using scratch registers, so we use
  // the assembler directly here. However, this means that large immediate
  // values of 'space' cannot be handled.
  InstructionAccurateScope scope(this);
  sub(sp, StackPointer(), space);
}


// This is the main Printf implementation. All callee-saved registers are
// preserved, but NZCV and the caller-saved registers may be clobbered.
void MacroAssembler::PrintfNoPreserve(const char * format,
                                      const CPURegister& arg0,
                                      const CPURegister& arg1,
                                      const CPURegister& arg2,
                                      const CPURegister& arg3) {
  // We cannot handle a caller-saved stack pointer. It doesn't make much sense
  // in most cases anyway, so this restriction shouldn't be too serious.
  ASSERT(!kCallerSaved.IncludesAliasOf(StackPointer()));

  // We cannot print Tmp0() or Tmp1() as they're used internally by the macro
  // assembler. We cannot print the stack pointer because it is typically used
  // to preserve caller-saved registers (using other Printf variants which
  // depend on this helper).
  ASSERT(!AreAliased(Tmp0(), Tmp1(), StackPointer(), arg0));
  ASSERT(!AreAliased(Tmp0(), Tmp1(), StackPointer(), arg1));
  ASSERT(!AreAliased(Tmp0(), Tmp1(), StackPointer(), arg2));
  ASSERT(!AreAliased(Tmp0(), Tmp1(), StackPointer(), arg3));

  static const int kMaxArgCount = 4;
  // Assume that we have the maximum number of arguments until we know
  // otherwise.
  int arg_count = kMaxArgCount;

  // The provided arguments.
  CPURegister args[kMaxArgCount] = {arg0, arg1, arg2, arg3};

  // The PCS registers where the arguments need to end up.
  CPURegister pcs[kMaxArgCount];

  // Promote FP arguments to doubles, and integer arguments to X registers.
  // Note that FP and integer arguments cannot be mixed, but we'll check
  // AreSameSizeAndType once we've processed these promotions.
  for (int i = 0; i < kMaxArgCount; i++) {
    if (args[i].IsRegister()) {
      // Note that we use x1 onwards, because x0 will hold the format string.
      pcs[i] = Register::XRegFromCode(i + 1);
      // For simplicity, we handle all integer arguments as X registers. An X
      // register argument takes the same space as a W register argument in the
      // PCS anyway. The only limitation is that we must explicitly clear the
      // top word for W register arguments as the callee will expect it to be
      // clear.
      if (!args[i].Is64Bits()) {
        const Register& as_x = args[i].X();
        And(as_x, as_x, 0x00000000ffffffff);
        args[i] = as_x;
      }
    } else if (args[i].IsFPRegister()) {
      pcs[i] = FPRegister::DRegFromCode(i);
      // C and C++ varargs functions (such as printf) implicitly promote float
      // arguments to doubles.
      if (!args[i].Is64Bits()) {
        FPRegister s(args[i]);
        const FPRegister& as_d = args[i].D();
        Fcvt(as_d, s);
        args[i] = as_d;
      }
    } else {
      // This is the first empty (NoCPUReg) argument, so use it to set the
      // argument count and bail out.
      arg_count = i;
      break;
    }
  }
  ASSERT((arg_count >= 0) && (arg_count <= kMaxArgCount));
  // Check that every remaining argument is NoCPUReg.
  for (int i = arg_count; i < kMaxArgCount; i++) {
    ASSERT(args[i].IsNone());
  }
  ASSERT((arg_count == 0) || AreSameSizeAndType(args[0], args[1],
                                                args[2], args[3],
                                                pcs[0], pcs[1],
                                                pcs[2], pcs[3]));

  // Move the arguments into the appropriate PCS registers.
  //
  // Arranging an arbitrary list of registers into x1-x4 (or d0-d3) is
  // surprisingly complicated.
  //
  //  * For even numbers of registers, we push the arguments and then pop them
  //    into their final registers. This maintains 16-byte stack alignment in
  //    case sp is the stack pointer, since we're only handling X or D registers
  //    at this point.
  //
  //  * For odd numbers of registers, we push and pop all but one register in
  //    the same way, but the left-over register is moved directly, since we
  //    can always safely move one register without clobbering any source.
  if (arg_count >= 4) {
    Push(args[3], args[2], args[1], args[0]);
  } else if (arg_count >= 2) {
    Push(args[1], args[0]);
  }

  if ((arg_count % 2) != 0) {
    // Move the left-over register directly.
    const CPURegister& leftover_arg = args[arg_count - 1];
    const CPURegister& leftover_pcs = pcs[arg_count - 1];
    if (leftover_arg.IsRegister()) {
      Mov(Register(leftover_pcs), Register(leftover_arg));
    } else {
      Fmov(FPRegister(leftover_pcs), FPRegister(leftover_arg));
    }
  }

  if (arg_count >= 4) {
    Pop(pcs[0], pcs[1], pcs[2], pcs[3]);
  } else if (arg_count >= 2) {
    Pop(pcs[0], pcs[1]);
  }

  // Load the format string into x0, as per the procedure-call standard.
  //
  // To make the code as portable as possible, the format string is encoded
  // directly in the instruction stream. It might be cleaner to encode it in a
  // literal pool, but since Printf is usually used for debugging, it is
  // beneficial for it to be minimally dependent on other features.
  Label format_address;
  Adr(x0, &format_address);

  // Emit the format string directly in the instruction stream.
  { BlockLiteralPoolScope scope(this);
    Label after_data;
    B(&after_data);
    Bind(&format_address);
    EmitStringData(format);
    Unreachable();
    Bind(&after_data);
  }

  // We don't pass any arguments on the stack, but we still need to align the C
  // stack pointer to a 16-byte boundary for PCS compliance.
  if (!sp.Is(StackPointer())) {
    Bic(sp, StackPointer(), 0xf);
  }

  // Actually call printf. This part needs special handling for the simulator,
  // since the system printf function will use a different instruction set and
  // the procedure-call standard will not be compatible.
#ifdef USE_SIMULATOR
  { InstructionAccurateScope scope(this, kPrintfLength / kInstructionSize);
    hlt(kPrintfOpcode);
    dc32(pcs[0].type());
  }
#else
  Mov(Tmp0(), reinterpret_cast<uintptr_t>(printf));
  Blr(Tmp0());
#endif
}


void MacroAssembler::Printf(const char * format,
                            const CPURegister& arg0,
                            const CPURegister& arg1,
                            const CPURegister& arg2,
                            const CPURegister& arg3) {
  // Preserve all caller-saved registers as well as NZCV.
  // If sp is the stack pointer, PushCPURegList asserts that the size of each
  // list is a multiple of 16 bytes.
  PushCPURegList(kCallerSaved);
  PushCPURegList(kCallerSavedFP);
  // Use Tmp0() as a scratch register. It is not accepted by Printf so it will
  // never overlap an argument register.
  Mrs(Tmp0(), NZCV);
  Push(Tmp0(), xzr);

  PrintfNoPreserve(format, arg0, arg1, arg2, arg3);

  Pop(xzr, Tmp0());
  Msr(NZCV, Tmp0());
  PopCPURegList(kCallerSavedFP);
  PopCPURegList(kCallerSaved);
}

void MacroAssembler::Trace(TraceParameters parameters, TraceCommand command) {
  ASSERT(allow_macro_instructions_);

#ifdef USE_SIMULATOR
  // The arguments to the trace pseudo instruction need to be contiguous in
  // memory, so make sure we don't try to emit a literal pool.
  InstructionAccurateScope scope(this, kTraceLength / kInstructionSize);

  Label start;
  bind(&start);

  // Refer to instructions-a64.h for a description of the marker and its
  // arguments.
  hlt(kTraceOpcode);

  ASSERT(SizeOfCodeGeneratedSince(&start) == kTraceParamsOffset);
  dc32(parameters);

  ASSERT(SizeOfCodeGeneratedSince(&start) == kTraceCommandOffset);
  dc32(command);
#else
  // Emit nothing on real hardware.
  USE(parameters);
  USE(command);
#endif
}


void MacroAssembler::Log(TraceParameters parameters) {
  ASSERT(allow_macro_instructions_);

#ifdef USE_SIMULATOR
  // The arguments to the log pseudo instruction need to be contiguous in
  // memory, so make sure we don't try to emit a literal pool.
  InstructionAccurateScope scope(this, kLogLength / kInstructionSize);

  Label start;
  bind(&start);

  // Refer to instructions-a64.h for a description of the marker and its
  // arguments.
  hlt(kLogOpcode);

  ASSERT(SizeOfCodeGeneratedSince(&start) == kLogParamsOffset);
  dc32(parameters);
#else
  // Emit nothing on real hardware.
  USE(parameters);
#endif
}


void MacroAssembler::EnableInstrumentation() {
  ASSERT(!isprint(InstrumentStateEnable));
  InstructionAccurateScope scope(this, 1);
  movn(xzr, InstrumentStateEnable);
}


void MacroAssembler::DisableInstrumentation() {
  ASSERT(!isprint(InstrumentStateDisable));
  InstructionAccurateScope scope(this, 1);
  movn(xzr, InstrumentStateDisable);
}


void MacroAssembler::AnnotateInstrumentation(const char* marker_name) {
  ASSERT(strlen(marker_name) == 2);

  // We allow only printable characters in the marker names. Unprintable
  // characters are reserved for controlling features of the instrumentation.
  ASSERT(isprint(marker_name[0]) && isprint(marker_name[1]));

  InstructionAccurateScope scope(this, 1);
  movn(xzr, (marker_name[1] << 8) | marker_name[0]);
}

}  // namespace vixl
