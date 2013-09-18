//===-- lib/MC/Disassembler.cpp - Disassembler Public C Interface ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS

#include <stdio.h>
#include <llvm-c/Disassembler.h>

#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/MemoryObject.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Debug.h"

#include "X86BaseInfo.h"
#include "decode_inst_disasm.h"
#include "decode_inst.h"
#include "opcodes.h"
#include "decode_inst_helper.h"

namespace llvm {
class Target;
} // namespace llvm
using namespace llvm;

struct dis_info_s {
	MCInst *Inst;
	int offset[16];
	int size[16];
};

void LLVMPrintTargets(void) {
	TargetRegistry::printRegisteredTargetsForVersion();
}

void LLVMDisasmInstructionPrint(int octets, uint8_t *buffer, int buffer_size, uint8_t *buffer1) {
		int n;
		outs() << format("LLVM DIS octets = 0x%x:", octets);
		if (octets > buffer_size) {
			octets = buffer_size;
		}
		for (n = 0; n < octets; n++) {
			outs() << format("%02x ", buffer[n] & 0xff);
		}
		outs() << format(":%s\n", buffer1);
}


void *LLVMCreateMCInst(void) {
	MCInst *inst = new MCInst;
	struct dis_info_s *dis_info = (struct dis_info_s*) calloc (1, sizeof (struct dis_info_s));
	dis_info->Inst = inst;
//	DebugFlag = true;
//	EnableDebugBuffering = true;
	outs() << "Debug flag set true\n";
	return (void*)dis_info;
}

int LLVMDecodeOpInfoCallback(void *DisInfo, uint64_t PC,
                                  uint64_t Offset, uint64_t Size,
                                  int TagType, void *TagBuf) {
	struct dis_info_s *dis_info = (struct dis_info_s *) DisInfo;
	MCInst *Inst = dis_info->Inst;
	outs() << "DisInfo = " << DisInfo << "\n";
	int num_operands = Inst->getNumOperands();
	if (num_operands >= 16) {
		outs() << "num_operands >= 16\n";
		exit(1);
	}
	dis_info->offset[num_operands] = Offset;
	dis_info->size[num_operands] = Size;
	outs() << format("NumOperands = 0x%x, ", num_operands) << format("Offset = 0x%x, ", Offset) << format("Size = 0x%x", Size) << "\n";
	return 0;
}


// LLVMCreateDecodeAsm() creates a disassembler for the TripleName.  Symbolic
// disassembly is supported by passing a block of information in the DisInfo
// parameter and specifying the TagType and callback functions as described in
// the header llvm-c/Disassembler.h .  The pointer to the block and the 
// functions can all be passed as NULL.  If successful, this returns a
// disassembler context.  If not, it returns NULL.
//
LLVMDecodeAsmContextRef LLVMCreateDecodeAsm(const char *TripleName, void *DisInfo,
                                      int TagType, LLVMOpInfoCallback GetOpInfo,
                                      LLVMSymbolLookupCallback SymbolLookUp) {
  // Initialize targets and assembly printers/parsers.
  // FIXME: Clients are responsible for initializing the targets. And this
  // would be done by calling routines in "llvm-c/Target.h" which are static
  // line functions. But the current use of LLVMCreateDecodeAsm() is to dynamically
  // load libLTO with dlopen() and then lookup the symbols using dlsym().
  // And since these initialize routines are static that does not work which
  // is why the call to them in this 'C' library API was added back.
//  llvm::InitializeAllTargetInfos();
//  llvm::InitializeAllTargetMCs();
//  llvm::InitializeAllAsmParsers();
//  llvm::InitializeAllDisassemblers();

	// Get the target.
	std::string Error;
//	TargetRegistry::printRegisteredTargetsForVersion();
	const llvm::Target *TheTarget = TargetRegistry::lookupTarget(TripleName, Error);
	if (!TheTarget)
		return 0;

	const MCRegisterInfo *MRI = TheTarget->createMCRegInfo(TripleName);
	if (!MRI)
		return 0;

	// Get the assembler info needed to setup the MCContext.
	const MCAsmInfo *MAI = TheTarget->createMCAsmInfo(*MRI, TripleName);
	if (!MAI)
		return 0;

	const MCInstrInfo *MII = TheTarget->createMCInstrInfo();
	if (!MII)
		return 0;

	int tmp = MII->getNumOpcodes();
	outs() << format("Number of opcodes = 0x%x\n", tmp);

	// Package up features to be passed to target/subtarget
	std::string FeaturesStr;
	std::string CPU;

	const MCSubtargetInfo *STI = TheTarget->createMCSubtargetInfo(TripleName, CPU,
                                                                FeaturesStr);
	if (!STI)
		return 0;

	// Set up the MCContext for creating symbols and MCExpr's.
	MCContext *Ctx = new MCContext(MAI, MRI, 0);
	if (!Ctx)
		return 0;

	// Set up disassembler.
	MCDisassembler *DisAsm = TheTarget->createMCDisassembler(*STI);
	if (!DisAsm)
		return 0;

	OwningPtr<MCRelocationInfo> RelInfo(
		TheTarget->createMCRelocationInfo(TripleName, *Ctx));
	if (!RelInfo)
		return 0;

//	OwningPtr<MCSymbolizer> Symbolizer(
//		TheTarget->createMCSymbolizer(TripleName, GetOpInfo, SymbolLookUp, DisInfo,
//			Ctx, RelInfo.take()));
	OwningPtr<MCSymbolizer> Symbolizer(
		TheTarget->createMCSymbolizer(TripleName, &LLVMDecodeOpInfoCallback, SymbolLookUp, DisInfo,
			Ctx, RelInfo.take()));
	DisAsm->setSymbolizer(Symbolizer);

	//DisAsm->setupForSymbolicDisassembly(GetOpInfo, SymbolLookUp, DisInfo, Ctx, RelInfo);
	DisAsm->setupForSymbolicDisassembly(&LLVMDecodeOpInfoCallback, SymbolLookUp, DisInfo, Ctx, RelInfo);

	// Set up the instruction printer.
	int AsmPrinterVariant = MAI->getAssemblerDialect();
	MCInstPrinter *IP = TheTarget->createMCInstPrinter(AsmPrinterVariant,
                                                     *MAI, *MII, *MRI, *STI);
	if (!IP)
		return 0;

	LLVMDisasmContext *DC = new LLVMDisasmContext(TripleName, DisInfo, TagType,
		GetOpInfo, SymbolLookUp,
		TheTarget, MAI, MRI,
		STI, MII, Ctx, DisAsm, IP);
	if (!DC)
		return 0;

	return (LLVMDecodeAsmContextRef) DC;
}

//
// LLVMDecodeAsmDispose() disposes of the disassembler specified by the context.
//
void LLVMDecodeAsmDispose(LLVMDecodeAsmContextRef DCR){
  LLVMDisasmContext *DC = (LLVMDisasmContext *)DCR;
  delete DC;
}

namespace llvm {
//
// The memory object created by LLVMDecodeAsmInstruction().
//
class DecodeAsmMemoryObject : public llvm::MemoryObject {
  uint8_t *Bytes;
  uint64_t Size;
  uint64_t BasePC;
public:
  DecodeAsmMemoryObject(uint8_t *bytes, uint64_t size, uint64_t basePC) :
                     Bytes(bytes), Size(size), BasePC(basePC) {}
 
  uint64_t getBase() const { return BasePC; }
  uint64_t getExtent() const { return Size; }

  int readByte(uint64_t Addr, uint8_t *Byte) const {
    if (Addr - BasePC >= Size)
      return -1;
    *Byte = Bytes[Addr - BasePC];
    return 0;
  }
};
} // end anonymous namespace

//
// LLVMDecodeAsmInstruction() disassembles a single instruction using the
// disassembler context specified in the parameter DC.  The bytes of the
// instruction are specified in the parameter Bytes, and contains at least
// BytesSize number of bytes.  The instruction is at the address specified by
// the PC parameter.  If a valid instruction can be disassembled its string is
// returned indirectly in OutString which whos size is specified in the
// parameter OutStringSize.  This function returns the number of bytes in the
// instruction or zero if there was no valid instruction.  If this function
// returns zero the caller will have to pick how many bytes they want to step
// over by printing a .byte, .long etc. to continue.
//
size_t LLVMDecodeAsmInstruction(void *DisInfo, LLVMDecodeAsmContextRef DCR, uint8_t *Bytes,
                             uint64_t BytesSize, uint64_t PC, uint8_t *OutString,
                             size_t OutStringSize, uint64_t *opcode, const char **opcode_name, uint64_t *TSFlags){
  LLVMDisasmContext *DC = (LLVMDisasmContext *)DCR;
  int n;
  // Wrap the pointer to the Bytes, BytesSize and PC in a MemoryObject.
  llvm::DecodeAsmMemoryObject MemoryObject2(Bytes, BytesSize, PC);

	outs() << "DECODE INST\n";
  uint64_t Size;
	struct dis_info_s *dis_info = (struct dis_info_s *) DisInfo;
	MCInst *Inst = dis_info->Inst;
	Inst->clear();
	for (n = 0; n < 16; n++) {
		dis_info->offset[n] = 0;
		dis_info->size[n] = 0;
	}
  const MCDisassembler *DisAsm = DC->getDisAsm();
  MCInstPrinter *IP = DC->getIP();
  MCDisassembler::DecodeStatus S;
	if (Bytes[0] == 0) {
		outs() << "Bytes reset to 0\n";
		exit(1);
	}
  S = DisAsm->getInstruction(*Inst, Size, MemoryObject2, PC,
                             /*REMOVE*/ nulls(), DC->CommentStream);
  switch (S) {
  case MCDisassembler::Fail:
  case MCDisassembler::SoftFail:
    // FIXME: Do something different for soft failure modes?
    return 0;

  case MCDisassembler::Success: {
	StringRef Name;
	StringRef Reg;
	DC->CommentStream.flush();
	StringRef Comments = DC->CommentsToEmit.str();

	SmallVector<char, 64> InsnStr;
	InsnStr.empty();
	raw_svector_ostream OS(InsnStr);
	OS.flush();
	SmallVector<char, 64> RegStr;
	RegStr.empty();
	const MCInstrInfo *MII = DC->getInstInfo();
	int num_opcodes = MII->getNumOpcodes();
	*opcode = Inst->getOpcode();
	const MCInstrDesc Desc = MII->get(*opcode);
	*TSFlags = Desc.TSFlags;
	int opcode_form = *TSFlags & X86II::FormMask;
	Name = IP->getOpcodeName(Inst->getOpcode());
	*opcode_name = Name.data();
	outs() << format("Opcode 0x%x:", Inst->getOpcode()) << format("%s", *opcode_name) << "\n";
	int num_operands = Inst->getNumOperands();
	outs() << format("opcode_form = 0x%x", opcode_form) << format(", num_operands = 0x%x", num_operands) << "\n";
	MCOperand *Operand;
	switch (opcode_form) {
	case 1: // RawFrm
		if (num_operands != 1) {
			outs() << "Unrecognised num_operands\n";
			break;
		}
		outs() << "DST0 reg = %al\n";
		Operand = &Inst->getOperand(0);
		if (Operand->isValid() &&
			Operand->isImm() ) {
			uint32_t value;
			value = Operand->getImm();
			outs() << format("SRC0 index multiplier Imm = 0x%x\n", value);
			outs() << format("SRC0 bytes at inst offset = 0x%x octets, size = 0x%x octets, value = 0x%x\n", dis_info->offset[0], dis_info->size[0], Bytes[dis_info->offset[0]]);
			outs() << format("SRC0 bytes at inst offset = 1 octets, size = 8 bits, value = 0x%x\n", Bytes[1]);
		}
		break;
	case 3: // MRMDestReg
		if (num_operands != 3) {
			outs() << "Unrecognised num_operands\n";
			break;
		}
		Operand = &Inst->getOperand(0);
		if (Operand->isValid() &&
			Operand->isReg()) {
			uint32_t value;
			value = Operand->getReg();
			outs() << format("DST0 Reg = 0x%x\n", value);
		}
		Operand = &Inst->getOperand(1);
		if (Operand->isValid() &&
			Operand->isReg()) {
			uint32_t value;
			value = Operand->getReg();
			outs() << format("SRC0 Reg = 0x%x\n", value);
		}
		Operand = &Inst->getOperand(2);
		if (Operand->isValid() &&
			Operand->isReg()) {
			uint32_t value;
			value = Operand->getReg();
			outs() << format("SRC1 Reg = 0x%x\n", value);
		}
		break;
	case 6: // MRMSrcMem
		if (num_operands != 6) {
			outs() << "Unrecognised num_operands\n";
			break;
		}
		Operand = &Inst->getOperand(0);
		if (Operand->isValid() &&
			Operand->isReg()) {
			uint32_t value;
			value = Operand->getReg();
			outs() << format("DST0 Reg = 0x%x\n", value);
		}
		Operand = &Inst->getOperand(1);
		if (Operand->isValid() &&
			Operand->isReg() ) {
			uint32_t value;
			value = Operand->getReg();
			outs() << format("SRC0 pointer Reg = 0x%x\n", value);
		}
		Operand = &Inst->getOperand(2);
		if (Operand->isValid() &&
			Operand->isImm() ) {
			uint32_t value;
			value = Operand->getImm();
			outs() << format("SRC1 index multiplier Imm = 0x%x\n", value);
			outs() << format("SRC1 bytes at inst offset = 0x%x octets, size = 0x%x octets, value = 0x%x\n", dis_info->offset[2], dis_info->size[2], Bytes[dis_info->offset[2]]);
		}
		Operand = &Inst->getOperand(3);
		if (Operand->isValid() &&
			Operand->isReg() ) {
			uint32_t value;
			value = Operand->getReg();
			outs() << format("SRC2 index Reg = 0x%x\n", value);
		}
		Operand = &Inst->getOperand(4);
		if (Operand->isValid() &&
			Operand->isImm() ) {
			uint32_t value;
			value = Operand->getImm();
			outs() << format("SRC3 offset Imm  = 0x%x\n", value);
			outs() << format("SRC3 bytes at inst offset = 0x%x octets, size = 0x%x octets, value = 0x%x\n", dis_info->offset[4], dis_info->size[4], Bytes[dis_info->offset[4]]);
			outs() << format("SRC3 bytes at inst offset = 3 octets, size = 8 bits, value = 0x%x\n", Bytes[3]);
		}
		Operand = &Inst->getOperand(5);
		if (Operand->isValid() &&
			Operand->isReg() ) {
			uint32_t value;
			value = Operand->getReg();
			outs() << format("SRC4 Segment Reg  = 0x%x\n", value);
		}
		break;
	case 0x10: //
		if (num_operands != 2) {
			outs() << "Unrecognised num_operands\n";
			break;
		}
		Operand = &Inst->getOperand(0);
		if (Operand->isValid() &&
			Operand->isReg()) {
			uint32_t value;
			value = Operand->getReg();
			outs() << format("DST0 Reg = 0x%x\n", value);
		}
		Operand = &Inst->getOperand(1);
		if (Operand->isValid() &&
			Operand->isImm() ) {
			uint32_t value;
			value = Operand->getImm();
			outs() << format("SRC0 index multiplier Imm = 0x%x\n", value);
			outs() << format("SRC0 bytes at inst offset = 0x%x octets, size = 0x%x octets, value = 0x%x\n", dis_info->offset[1], dis_info->size[1], Bytes[dis_info->offset[1]]);
			outs() << format("SRC0 bytes at inst offset = 1 octets, size = 8 bits, value = 0x%x\n", Bytes[1]);
		}
		break;
	case 0x18: // MRM2r
		if (num_operands != 6) {
			outs() << "Unrecognised num_operands\n";
			break;
		}
		Operand = &Inst->getOperand(0);
		if (Operand->isValid() &&
			Operand->isReg() ) {
			uint32_t value;
			value = Operand->getReg();
			outs() << format("DST0 pointer Reg = 0x%x\n", value);
		}
		Operand = &Inst->getOperand(1);
		if (Operand->isValid() &&
			Operand->isImm() ) {
			uint32_t value;
			value = Operand->getImm();
			outs() << format("DST1 index multiplier Imm = 0x%x\n", value);
			outs() << format("DST1 bytes at inst offset = 0x%x octets, size = 0x%x octets, value = 0x%x\n", dis_info->offset[1], dis_info->size[1], Bytes[dis_info->offset[1]]);
		}
		Operand = &Inst->getOperand(2);
		if (Operand->isValid() &&
			Operand->isReg() ) {
			uint32_t value;
			value = Operand->getReg();
			outs() << format("DST2 index Reg = 0x%x\n", value);
		}
		Operand = &Inst->getOperand(3);
		if (Operand->isValid() &&
			Operand->isImm() ) {
			uint32_t value;
			value = Operand->getImm();
			outs() << format("DST3 offset Imm  = 0x%x\n", value);
			outs() << format("DST3 bytes at inst offset = 0x%x octets, size = 0x%x octets, value = 0x%x\n", dis_info->offset[3], dis_info->size[3], Bytes[dis_info->offset[3]]);
			outs() << format("DST3 bytes at inst offset = 3 octets, size = %d bits, value = 0x%x\n", 1 * 8, Bytes[3]);
		}
		Operand = &Inst->getOperand(4);
		if (Operand->isValid() &&
			Operand->isReg() ) {
			uint32_t value;
			value = Operand->getReg();
			outs() << format("DST4 unknown Reg  = 0x%x\n", value);
		}
		Operand = &Inst->getOperand(5);
		if (Operand->isValid() &&
			Operand->isImm() ) {
			uint32_t value;
			value = Operand->getImm();
			outs() << format("SRC0 offset Imm  = 0x%x\n", value);
			int size_of_imm = X86II::getSizeOfImm(*TSFlags);
			outs() << format("SRC0 bytes at inst offset = 0x%x octets, size = 0x%x octets, value = 0x%x\n", dis_info->offset[5], dis_info->size[5], Bytes[dis_info->offset[5]]);
			outs() << format("SRC0 bytes at inst offset = 4 octets, size = %d bits, value = 0x%x\n", size_of_imm * 8, Bytes[4]);
		}
		break;
	default:
		outs() << "Unrecognised form\n";
		break;
	}



	for (n = 0; n < num_operands; n++) {
		Operand = &Inst->getOperand(n);
		outs() << "Operand = " << Operand << "\n";
		outs() << "Valid = " << Operand->isValid(); 
		outs() << ", isReg = " << Operand->isReg();
		outs() << ", isImm = " << Operand->isImm();
		outs() << ", isFPImm = " << Operand->isFPImm();
		outs() << ", isExpr = " << Operand->isExpr();
		outs() << ", isInst = " << Operand->isInst() << "\n";
		//outs() << format("Operand.Kind = 0x%x\n", Operand->Kind);
		if (Operand->isImm()) {
			outs() << format("Imm = 0x%lx, ", Operand->getImm());
			int size_of_imm = X86II::getSizeOfImm(*TSFlags);
			outs() << format("sizeof(Imm) = 0x%x", size_of_imm) << "\n";
		}
		if (Operand->isReg()) {
			uint32_t reg;
			reg = Operand->getReg();
			outs() << format("Reg = 0x%x\n", reg);
			if (reg) {
				std::string Buf2;
				raw_string_ostream OS2(Buf2);
				IP->printRegName(OS2, reg);
				OS2.flush();
				Reg = OS2.str();
				outs() << "Reg: " << Reg << "\n";
			}
		}
	}
	SmallVector<char, 6400> Buffer2;
	raw_svector_ostream OS3(Buffer2);
	Inst->dump_pretty(OS3);
	OS3.flush();
	

	// Tell the comment stream that the vector changed underneath it.
	DC->CommentsToEmit.clear();
	DC->CommentStream.resync();

	assert(OutStringSize != 0 && "Output buffer cannot be zero size");
	size_t OutputSize = std::min(OutStringSize-1, InsnStr.size());
	std::memcpy(OutString, InsnStr.data(), OutputSize);
	OutString[OutputSize] = '\0'; // Terminate string.
	if (Bytes[0] == 0) {
		outs() << "Bytes reset to 0\n";
		exit(1);
	}

	return Size;
	}
  }
  llvm_unreachable("Invalid DecodeStatus!");
}

LLVMDecodeAsmMIIRef LLVMDecodeAsmGetMII(LLVMDecodeAsmContextRef DCR) {
	LLVMDisasmContext *DC = (LLVMDisasmContext *)DCR;
	return (LLVMDecodeAsmMIIRef) DC->getInstInfo();
}

int LLVMDecodeAsmGetNumOpcodes(LLVMDecodeAsmContextRef DCR) {
	LLVMDisasmContext *DC = (LLVMDisasmContext *)DCR;
	const MCInstrInfo *MII = DC->getInstInfo();
	int num_opcodes = MII->getNumOpcodes();
	return num_opcodes;
}

uint64_t LLVMDecodeAsmGetTSFlags(LLVMDecodeAsmContextRef DCR, uint64_t opcode) {
	LLVMDisasmContext *DC = (LLVMDisasmContext *)DCR;
	const MCInstrInfo *MII = DC->getInstInfo();
	const MCInstrDesc Desc = MII->get(opcode);
	uint64_t TSFlags = Desc.TSFlags;
	outs() << format("OpcodeByteShift = 0x%lx:0x%x\n", (int)X86II::OpcodeShift, X86II::getBaseOpcodeFor(TSFlags));
	outs() << format("OpSizeMask = 0x%lx:0x%lx\n", (int)X86II::OpSize, TSFlags & X86II::OpSize);
	outs() << format("AdSizeMask = 0x%lx:0x%lx\n", (int)X86II::AdSize, TSFlags & X86II::AdSize);
	outs() << format("Op0Mask = 0x%lx:0x%lx\n", (int)X86II::Op0Mask, (TSFlags & X86II::Op0Mask) >> X86II::Op0Shift);
	outs() << format("REX_W_Mask = 0x%lx:0x%lx\n", (int)X86II::REX_W, (TSFlags & X86II::REX_W) >> X86II::REXShift);
	outs() << format("Imm_Mask = 0x%lx:0x%lx\n", (int)X86II::ImmMask, (TSFlags & X86II::ImmMask) >> X86II::ImmShift);
	outs() << format("FormMask = 0x%lx:0x%lx\n", (int)X86II::FormMask, TSFlags & X86II::FormMask);
	return TSFlags;
}

int LLVMDecodeAsmPrintOpcodes(LLVMDecodeAsmContextRef DCR) {
	LLVMDisasmContext *DC = (LLVMDisasmContext *)DCR;
	const MCInstrInfo *MII = DC->getInstInfo();
	MCInstPrinter *IP = DC->getIP();
	StringRef Name;
	const char *opcode_name;
	int num_opcodes = MII->getNumOpcodes();
	int n;
	for (n = 0; n < num_opcodes; n++) {
		const MCInstrDesc Desc = MII->get(n);
		uint64_t TSFlags = Desc.TSFlags;
		outs() << format("n = 0x%x:", n);
		Name = IP->getOpcodeName(n);
		opcode_name = Name.data();
		outs() << format("opcode_name = %p:%s, 0x%lx\n", Name.data(), opcode_name, TSFlags);
	};
	return 0;
}

int LLVMDecodeAsmOpcodesSource(LLVMDecodeAsmContextRef DCR) {
	LLVMDisasmContext *DC = (LLVMDisasmContext *)DCR;
	const MCInstrInfo *MII = DC->getInstInfo();
	MCInstPrinter *IP = DC->getIP();
	StringRef Name;
	int n,m;
	int tmp;
	const char *opcode_name;
	int num_opcodes = MII->getNumOpcodes();
	int inst_helper_size = sizeof(decode_inst_helper) / sizeof(struct decode_inst_helper_s);
	struct decode_inst_helper_s *new_helper = (struct decode_inst_helper_s *)calloc(num_opcodes, sizeof(struct decode_inst_helper_s));
	if (!new_helper) {
		return 1;
	}
	for (n = 0; n < num_opcodes; n++) {
		int start;
		const MCInstrDesc Desc = MII->get(n);
		Name = IP->getOpcodeName(n);
		new_helper[n].mc_inst = Name.data();
		start = 0;
		if ((n < inst_helper_size) &&
			(0 == strcmp(Name.data(), decode_inst_helper[n].mc_inst))) {
			/* Small optimization if the table is not changing */
			start = n;
		}
		for (m = start; m < inst_helper_size; m++) {
			tmp = strcmp(Name.data(), decode_inst_helper[m].mc_inst);
			if (tmp == 0) {
				new_helper[n].opcode = decode_inst_helper[m].opcode;
				new_helper[n].predicate = decode_inst_helper[m].predicate;
				new_helper[n].srcA_size = decode_inst_helper[m].srcA_size;
				new_helper[n].srcB_size = decode_inst_helper[m].srcB_size;
				new_helper[n].dstA_size = decode_inst_helper[m].dstA_size;
				break;
			}
		}
	}
	
	for (n = 0; n < num_opcodes; n++) {
		/* OpcodeID, Operand, offset, size, Operand, offset, size, OpcodeName */
		outs() << format("	{ %s, ", helper_opcode_table[new_helper[n].opcode]);
		outs() << format("0x%x, ", new_helper[n].predicate);
		outs() << format("0x%x, ", new_helper[n].srcA_size);
		outs() << format("0x%x, ", new_helper[n].srcB_size);
		outs() << format("0x%x, ", new_helper[n].dstA_size);
		outs() << format("0x%x, ", 0);
		outs() << format("0x%x, ", 0);
		outs() << format("\"%s\" },  // 0x%04x\n",
			new_helper[n].mc_inst,
			n);
	};
	free (new_helper);
	return 0;
}

