
// a86tr.c : tracing asm

from std use tracing;
use ../goptions, ../common, ../dllnames;
use a86;


const string REGS[18] =
           {"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
            "r8",  "r9", "r10", "r11", "r12", "r13", "r14", "r15", "rip",
           ""};

const string XREGS[16] =
           {"xm0", "xm1", "xm2",  "xm3",  "xm4",  "xm5",  "xm6",  "xm7",
            "xm8", "xm9", "xm10", "xm11", "xm12", "xm13", "xm14", "xm15"};

void trace_border ()
{
  trace ("      ");
}


void trace_reg (REG r)
{
  trace ("%s", REGS[(int)r]);
}

void trace_xreg (XM r)
{
  trace ("%s", XREGS[(int)r]);
}

void trace_mem (EA ea)
{
  int offset;

  trace ("[");
  if (ea.base != NONE)
    trace ("%s", REGS[(int)ea.base]);

  if (ea.base != NONE && ea.index != NONE)
    trace (" + ");

  if (ea.index != NONE)
  {
    trace ("%s", REGS[(int)ea.index]);

    if (ea.scale != 1)
      trace ("*%d", ea.scale);
  }

  trace ("]");

  offset = ea.offset + ((ea.base == RSP && is_ESP_correction_ON()) ? ESP_correction_value() : 0);

  if (offset != 0)
  {
    if (offset > 0)
      trace ("+");
    trace ("%d", offset);
  }

  if (ea.reloc.kind != RELOC_NONE)
  {
    if (ea.reloc.kind == RELOC_FUNC)
      trace (" (function nr = %d)", ea.reloc.nr);

    if (ea.reloc.kind == RELOC_DLL)
    {
      string^ dll, func;
      get_dll_name ((uint4)ea.reloc.nr, out dll, out func);
      trace (" (func %s %s)", func^, dll^);
    }

    if (ea.reloc.kind == RELOC_POOL)
      trace (" (pool nr = %d)", ea.reloc.nr);

    if (ea.reloc.kind == RELOC_GLOBAL)
      trace (" (global nr=%d)", ea.reloc.nr);
  }
}

public
void trace_opcode_reg_reg (string opcode, REG target, REG source, int size)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d ", opcode, size);
  trace_reg (target);
  trace (" , ");
  trace_reg (source);
  trace ("\n");
}

public
void trace_opcode_reg_mem (string opcode, REG target, EA source, int size)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d ", opcode, size);
  trace_reg (target);
  trace (" , ");
  trace_mem (source);
  trace ("\n");
}

public
void trace_opcode_mem_reg (string opcode, EA target, REG source, int size)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d ", opcode, size);
  trace_mem (target);
  trace (" , ");
  trace_reg (source);
  trace ("\n");
}

public
void trace_opcode_reg_imm (string opcode, REG target, int8 imm, int size)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d ", opcode, size);
  trace_reg (target);
  trace (" , ");
  trace ("#%d", imm);
  trace ("\n");
}

public
void trace_opcode_mem_imm (string opcode, EA target, int8 imm, int size)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d ", opcode, size);
  trace_mem (target);
  trace (" , ");
  trace ("#%d", imm);
  trace ("\n");
}

public
void trace_opcode_reg (string opcode, REG source, int size)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d ", opcode, size);
  trace_reg (source);
  trace ("\n");
}


public
void trace_opcode_mem (string opcode, EA source, int size)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d ", opcode, size);
  trace_mem (source);
  trace ("\n");
}


public
void trace_opcode_call_relative (string opcode, int func_label_nr)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s ", opcode);
  trace (" func #%d", func_label_nr);
  trace ("\n");
}


public
void trace_opcode_jmp_relative (string opcode, int near_label_nr)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s ", opcode);
  trace (" lab #%d", near_label_nr);
  trace ("\n");
}


public
void trace_opcode_jcond (string opcode, int cond, int near_label_nr)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s %d ", opcode, cond);
  trace (" lab #%d", near_label_nr);
  trace ("\n");
}


public
void trace_opcode_setcc_reg (string opcode, COMPARISON_FLAG condition, bool signed_operands, REG  target, int size)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d %u %u ", opcode, size, condition, signed_operands);
  trace ("%s", REGS[(int)target]);
  trace ("\n");
}

public
void trace_opcode_setcc_mem (string opcode, COMPARISON_FLAG condition, bool signed_operands, EA target, int size)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d %u %u ", opcode, size, condition, signed_operands);
  trace_mem (target);
  trace ("\n");
}


public
void trace_opcode (string opcode)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s", opcode);
  trace ("\n");
}


public
void trace_opcode_imm (string opcode, int imm)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s #%d", opcode, imm);
  trace ("\n");
}


public
void trace_opcode_imm_reloc (string opcode, int imm, RELOC_INFO preloc, int size)  // relocatable constant
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d #%d", opcode, size, imm);

  if (preloc.kind != RELOC_NONE)
  {
    if (preloc.kind == RELOC_FUNC)
      trace (" (function)");
    if (preloc.kind == RELOC_DLL)
      trace (" (dll)");
    if (preloc.kind == RELOC_POOL)
      trace (" (pool)");
    if (preloc.kind == RELOC_GLOBAL)
      trace (" (global)");

    if (preloc.nr != 0)
      trace (" nr=%d", preloc.nr);
  }

  trace ("\n");
}


public
void trace_opcode_reg_imm_reloc (string opcode, REG reg, int imm, RELOC_INFO preloc, int size)  // relocatable constant
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d ", opcode, size);

  trace ("%s", REGS[(int)reg]);

  trace (" , ");

  trace ("#%d", imm);

  if (preloc.kind != RELOC_NONE)
  {
    if (preloc.kind == RELOC_FUNC)
      trace (" (function)");
    if (preloc.kind == RELOC_DLL)
      trace (" (dll)");
    if (preloc.kind == RELOC_POOL)
      trace (" (pool)");
    if (preloc.kind == RELOC_GLOBAL)
      trace (" (global)");

    if (preloc.nr != 0)
      trace (" nr=%d", preloc.nr);
  }

  trace ("\n");
}


public
void trace_opcode_mem_imm_reloc (string opcode, EA ea, int imm, RELOC_INFO preloc, int size)  // relocatable constant
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d ", opcode, size);

  trace_mem (ea);

  trace (" , ");

  trace ("#%d", imm);

  if (preloc.kind != RELOC_NONE)
  {
    if (preloc.kind == RELOC_FUNC)
      trace (" (function)");
    if (preloc.kind == RELOC_DLL)
      trace (" (dll)");
    if (preloc.kind == RELOC_POOL)
      trace (" (pool)");
    if (preloc.kind == RELOC_GLOBAL)
      trace (" (global)");

    if (preloc.nr != 0)
      trace (" nr=%d", preloc.nr);
  }

  trace ("\n");
}



public
void trace_opcode_reg_reg_sizes (string opcode, REG target, REG source, int size_target, int size_source)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s ", opcode);
  trace ("%d:", size_target);
  trace_reg (target);
  trace (" , ");
  trace ("%d:", size_source);
  trace_reg (source);
  trace ("\n");
}


public
void trace_opcode_reg_mem_sizes (string opcode, REG target, EA source, int size_target, int size_source)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s ", opcode);
  trace ("%d:", size_target);
  trace_reg (target);
  trace (" , ");
  trace ("%d:", size_source);
  trace_mem (source);
  trace ("\n");
}




public
void trace_opcode_reg_reg_imm (string opcode, REG target, REG source, int8 imm, int size)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d ", opcode, size);
  trace_reg (target);
  trace (" , ");
  trace_reg (source);
  trace (" , ");
  trace ("#%d", imm);
  trace ("\n");
}



public
void trace_opcode_reg_mem_imm (string opcode, REG target, EA source, int8 imm, int size)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d ", opcode, size);
  trace_reg (target);
  trace (" , ");
  trace_mem (source);
  trace (" , ");
  trace ("#%d", imm);
  trace ("\n");
}



public
void trace_opcode_ext_reg_reg (string opcode, REG htarget, REG ltarget, REG source, int size)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d ", opcode, size);
  trace_reg (htarget);
  trace (":");
  trace_reg (ltarget);
  trace (" , ");
  trace_reg (source);
  trace ("\n");
}



public
void trace_opcode_ext_reg_mem (string opcode, REG htarget, REG ltarget, EA source, int size)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d ", opcode, size);
  trace_reg (htarget);
  trace (":");
  trace_reg (ltarget);
  trace (" , ");
  trace_mem (source);
  trace ("\n");
}




public
void trace_opcode_mem_reg_imm (string opcode, EA  target, REG source, int8 imm, int size)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d ", opcode, size);
  trace_mem (target);
  trace (" , ");
  trace_reg (source);
  trace (" , ");
  trace ("#%d", imm);
  trace ("\n");
}



public
void trace_opcode_reg_reg_reg (string opcode, REG target, REG source, REG source2, int size)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d ", opcode, size);
  trace_reg (target);
  trace (" , ");
  trace_reg (source);
  trace (" , ");
  trace_reg (source2);
  trace ("\n");
}



public
void trace_opcode_mem_reg_reg (string opcode, EA target, REG source, REG source2, int size)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d ", opcode, size);
  trace_mem (target);
  trace (" , ");
  trace_reg (source);
  trace (" , ");
  trace_reg (source2);
  trace ("\n");
}


public
void trace_opcode_xm_xm (string opcode, XM target, XM source, int size)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d ", opcode, size);
  trace_xreg (target);
  trace (" , ");
  trace_xreg (source);
  trace ("\n");
}


public
void trace_opcode_xm_mem (string opcode, XM target, EA source, int size)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d ", opcode, size);
  trace_xreg (target);
  trace (" , ");
  trace_mem (source);
  trace ("\n");
}



public
void trace_opcode_mem_xm (string opcode, EA target, XM source, int size)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d ", opcode, size);
  trace_mem (target);
  trace (" , ");
  trace_xreg (source);
  trace ("\n");
}


public
void trace_opcode_xm_reg (string opcode, XM target, REG source, int size)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d ", opcode, size);
  trace_xreg (target);
  trace (" , ");
  trace_reg (source);
  trace ("\n");
}


public
void trace_opcode_reg_xm (string opcode, REG target, XM source, int size)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s_%d ", opcode, size);
  trace_reg (target);
  trace (" , ");
  trace_xreg (source);
  trace ("\n");
}


public
void trace_opcode_xm_size_xm_size (string opcode, XM target, int size_target, XM source, int size_source)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s ", opcode);

  trace ("%d:", size_target);
  trace_xreg (target);
  trace (" , ");
  trace ("%d:", size_source);
  trace_xreg (source);
  trace ("\n");
}


public
void trace_opcode_xm_size_ireg_size (string opcode, XM target, int size_target, REG source, int size_source)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s ", opcode);

  trace ("%d:", size_target);
  trace_xreg (target);
  trace (" , ");
  trace ("int%d:", size_source);
  trace_reg (source);
  trace ("\n");
}


public
void trace_opcode_xm_size_imem_size (string opcode, XM target, EA source, int size_target, int size_source)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s ", opcode);
  trace ("%d:", size_target);
  trace_xreg (target);
  trace (" , ");
  trace ("int%d:", size_source);
  trace_mem (source);
  trace ("\n");
}

public
void trace_opcode_ireg_size_xm_size  (string opcode, REG target, int size_target, XM source, int size_source)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s ", opcode);

  trace ("int%d:", size_target);
  trace_reg (target);
  trace (" , ");
  trace ("%d:", size_source);
  trace_xreg (source);
  trace ("\n");
}

public
void trace_opcode_ireg_size_memf_size (string opcode, REG target, EA source, int size_target, int size_source)
{
  if (!g_tracing) return;

  trace_border ();
  trace ("%s ", opcode);
  trace ("%d:", size_target);
  trace_reg (target);
  trace (" , ");
  trace ("float_%d:", size_source);
  trace_mem (source);
  trace ("\n");
}

