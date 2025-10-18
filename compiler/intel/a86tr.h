
// a86tr.h : tracing asm

use a86, ../common;

void trace_opcode_reg_reg (string opcode, REG target, REG source, int size);
void trace_opcode_reg_mem (string opcode, REG target, EA  source, int size);
void trace_opcode_mem_reg (string opcode, EA  target, REG source, int size);
void trace_opcode_reg_imm (string opcode, REG target, int8 imm, int size);
void trace_opcode_mem_imm (string opcode, EA  target, int8 imm, int size);

void trace_opcode_reg (string opcode, REG source, int size);
void trace_opcode_mem (string opcode, EA  source, int size);


void trace_opcode_call_relative (string opcode, int func_label_nr);

void trace_opcode_jmp_relative (string opcode, int near_label_nr);

void trace_opcode_jcond (string opcode, int cond, int near_label_nr);

void trace_opcode_setcc_reg (string opcode, COMPARISON_FLAG condition, bool signed_operands, REG target, int  size);
void trace_opcode_setcc_mem (string opcode, COMPARISON_FLAG condition, bool signed_operands, EA  target, int  size);

void trace_opcode (string opcode);
void trace_opcode_imm (string opcode, int imm);
void trace_opcode_imm_reloc     (string opcode, int imm, RELOC_INFO preloc, int size);  // relocatable constant
void trace_opcode_reg_imm_reloc (string opcode, REG reg, int imm, RELOC_INFO preloc, int size);  // relocatable constant
void trace_opcode_mem_imm_reloc (string opcode, EA ea, int imm, RELOC_INFO preloc, int size);  // relocatable constant


void trace_opcode_reg_reg_sizes (string opcode, REG target, REG source, int size_target, int size_source);
void trace_opcode_reg_mem_sizes (string opcode, REG target, EA source, int size_target, int size_source);

void trace_opcode_reg_reg_imm (string opcode, REG target, REG source, int8 imm, int size);
void trace_opcode_reg_mem_imm (string opcode, REG target, EA source, int8 imm, int size);

void trace_opcode_ext_reg_reg (string opcode, REG htarget, REG ltarget, REG source, int size);
void trace_opcode_ext_reg_mem (string opcode, REG htarget, REG ltarget, EA source, int size);
void trace_opcode_mem_reg_imm (string opcode, EA  target, REG source, int8 imm, int size);
void trace_opcode_reg_reg_reg (string opcode, REG target, REG source, REG source2, int size);

void trace_opcode_mem_reg_reg (string opcode, EA target, REG source, REG source2, int size);


void trace_opcode_xm_xm (string opcode, XM target, XM source, int size);
void trace_opcode_xm_mem (string opcode, XM target, EA source, int size);
void trace_opcode_mem_xm (string opcode, EA target, XM source, int size);

void trace_opcode_xm_reg (string opcode, XM target, REG source, int size);
void trace_opcode_reg_xm (string opcode, REG target, XM source, int size);

void trace_opcode_xm_size_xm_size (string opcode, XM target, int size_target, XM source, int size_source);

void trace_opcode_xm_size_ireg_size   (string opcode, XM target, int size_target, REG source,  int size_source);
void trace_opcode_xm_size_imem_size   (string opcode, XM target, EA source, int size_target,  int size_source);
void trace_opcode_ireg_size_xm_size   (string opcode, REG target, int size_target, XM source,  int size_source);
void trace_opcode_ireg_size_memf_size (string opcode, REG target, EA source, int size_target, int size_source);

