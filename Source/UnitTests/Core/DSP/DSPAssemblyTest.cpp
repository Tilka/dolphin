// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/FileUtil.h"
#include "Core/DSP/DSPAssembler.h"
#include "Core/DSP/DSPCodeUtil.h"
#include "Core/DSP/DSPDisassembler.h"
#include "Core/DSP/Interpreter/DSPInterpreter.h"

#include "DSPTestBinary.h"
#include "DSPTestText.h"
#include "HermesBinary.h"

#include <gtest/gtest.h>

static bool RoundTrippableDissassemble(const std::vector<u16>& code, std::string& text)
{
  DSP::AssemblerSettings settings;
  settings.ext_separator = '\'';
  settings.decode_names = true;
  settings.decode_registers = true;
  // These two prevent roundtripping.
  settings.show_hex = false;
  settings.show_pc = false;
  DSP::DSPDisassembler disasm(settings);

  return disasm.Disassemble(code, text);
}

// This test goes from text ASM to binary to text ASM and once again back to binary.
// Then the two binaries are compared.
static bool RoundTrip(const std::vector<u16>& code1)
{
  std::vector<u16> code2;
  std::string text;
  if (!RoundTrippableDissassemble(code1, text))
  {
    printf("RoundTrip: Disassembly failed.\n");
    return false;
  }
  if (!DSP::Assemble(text, code2))
  {
    printf("RoundTrip: Assembly failed.\n");
    return false;
  }
  if (!DSP::Compare(code1, code2))
  {
    DSP::Disassemble(code1, true, text);
    printf("%s", text.c_str());
  }
  return true;
}

// This test goes from text ASM to binary to text ASM and once again back to binary.
// Very convenient for testing. Then the two binaries are compared.
static bool SuperTrip(const char* asm_code)
{
  std::vector<u16> code1, code2;
  std::string text;
  if (!DSP::Assemble(asm_code, code1))
  {
    printf("SuperTrip: First assembly failed\n");
    return false;
  }
  printf("First assembly: %i words\n", (int)code1.size());

  if (!RoundTrippableDissassemble(code1, text))
  {
    printf("SuperTrip: Disassembly failed\n");
    return false;
  }
  else
  {
    printf("Disassembly:\n");
    printf("%s", text.c_str());
  }

  if (!DSP::Assemble(text, code2))
  {
    printf("SuperTrip: Second assembly failed\n");
    return false;
  }
  return true;
}

// Let's start out easy - a trivial instruction..
TEST(DSPAssembly, TrivialInstruction)
{
  ASSERT_TRUE(SuperTrip("	NOP\n"));
}

// Now let's do several.
TEST(DSPAssembly, SeveralTrivialInstructions)
{
  ASSERT_TRUE(SuperTrip("	NOP\n"
                        "	NOP\n"
                        "	NOP\n"));
}

// Turning it up a notch.
TEST(DSPAssembly, SeveralNoParameterInstructions)
{
  ASSERT_TRUE(SuperTrip("	SET16\n"
                        "	SET40\n"
                        "	CLR15\n"
                        "	M0\n"
                        "	M2\n"));
}

// Time to try labels and parameters, and comments.
TEST(DSPAssembly, LabelsParametersAndComments)
{
  ASSERT_TRUE(SuperTrip("DIRQ_TEST:	equ	0xfffb	; DSP Irq Request\n"
                        "	si		@0xfffc, #0x8888\n"
                        "	si		@0xfffd, #0xbeef\n"
                        "	si		@DIRQ_TEST, #0x0001\n"));
}

// Let's see if registers roundtrip. Also try predefined labels.
TEST(DSPAssembly, RegistersAndPredefinedLabels)
{
  ASSERT_TRUE(SuperTrip("	si		@0xfffc, #0x8888\n"
                        "	si		@0xfffd, #0xbeef\n"
                        "	si		@DIRQ, #0x0001\n"));
}

// Let's try some messy extended instructions.
TEST(DSPAssembly, ExtendedInstructions)
{
  ASSERT_TRUE(SuperTrip("   MULMV'SN    $AX0.L, $AX0.H, $ACC0 : @$AR2, $AC1.M\n"
                        "   ADDAXL'MV   $ACC1, $AX1.L : $AX1.H, $AC1.M\n"));
}

TEST(DSPAssembly, HermesBinary)
{
  ASSERT_TRUE(RoundTrip(s_hermes_bin));
}

TEST(DSPAssembly, DSPTestText)
{
  ASSERT_TRUE(SuperTrip(s_dsp_test_text));
}

TEST(DSPAssembly, DSPTestBinary)
{
  ASSERT_TRUE(RoundTrip(s_dsp_test_bin));
}

static DSP::DSP_Regs RunInterpreter(std::string asm_text)
{
  DSP::InitInstructionTable();
  DSP::AssemblerSettings settings{.pc = 0x8000};
  DSP::DSPAssembler assembler(settings);
  // FIXME: we should run test code from iram, not irom
  std::vector<u16> irom;
  assembler.Assemble(asm_text, irom);
  DSP::DSPInitOptions opts;
  std::copy(irom.begin(), irom.end(), opts.irom_contents.begin());
  // FIXME: JIT crashes since it relies on global config state -.-
  opts.core_type = DSP::DSPInitOptions::CoreType::Interpreter;
  DSP::DSPCore core;
  core.Initialize(opts);
  core.GetInterpreter().WriteCR(DSP::CR_RESET);
  core.RunCycles(100);
  return core.DSPState().r;
}

TEST(DSPInterpreter, TestIsLess)
{
  DSP::DSP_Regs r = RunInterpreter(R"(
    CLR $acc0
    CLR $acc1
    LRI $ac0.h, #0x0050
    LRI $ac1.h, #0x0050
    ADD $acc0, $acc1      ; Causes acc0 to overflow, and thus also become negative
    LRI $AX0.L, #0x0000
    IFL
    LRI $AX0.L, #0x0001
    HALT
    )");
  ASSERT_EQ(r.ax[0].l, 0x0000);
}
