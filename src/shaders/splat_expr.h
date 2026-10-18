#pragma once

// The per-splat expression program, shared verbatim by its compiler
// (core/ExprProgram.cpp), the buffer table that feeds it
// (engine/EngineInspect.cpp) and its evaluator
// (backend/vulkan/shaders/splat_expr.slang). Plain #defines so both
// preprocessors read the same file. docs/notes/splat-inspector.md.

// ---- program: one word per op, the operand in the bits above the opcode ----
#define SS_EXPR_OP_BITS  8
#define SS_EXPR_VAR      1    // push variable <operand>
#define SS_EXPR_CONST    2    // push the float whose bits are the next word
#define SS_EXPR_NEG      3
#define SS_EXPR_ABS      4
#define SS_EXPR_SQRT     5
#define SS_EXPR_LN       6
#define SS_EXPR_LOG2     7
#define SS_EXPR_LOG10    8
#define SS_EXPR_EXP      9
#define SS_EXPR_SIN      10
#define SS_EXPR_COS      11
#define SS_EXPR_TANH     12
#define SS_EXPR_SIGMOID  13
#define SS_EXPR_FLOOR    14
#define SS_EXPR_SIGN     15
#define SS_EXPR_ADD      16
#define SS_EXPR_SUB      17
#define SS_EXPR_MUL      18
#define SS_EXPR_DIV      19
#define SS_EXPR_POW      20
#define SS_EXPR_MIN      21
#define SS_EXPR_MAX      22
#define SS_EXPR_LT       23   // comparisons push 1 or 0
#define SS_EXPR_LE       24
#define SS_EXPR_GT       25
#define SS_EXPR_GE       26
#define SS_EXPR_EQ       27
#define SS_EXPR_NE       28
#define SS_EXPR_WHERE    29   // c a b -> c != 0 ? a : b
#define SS_EXPR_CLAMP    30   // x lo hi

#define SS_EXPR_MAX_STACK 16
#define SS_EXPR_MAX_VARS  32
#define SS_EXPR_MAX_CODE  256

// ---- how a variable's cells are stored ----
#define SS_EXPR_F32     0
#define SS_EXPR_I32     1
#define SS_EXPR_U32     2
#define SS_EXPR_U16     3
#define SS_EXPR_QLIN    4     // QuantizedTensor<bits>, core/Tensor.h
#define SS_EXPR_GRADQ   5     // gradq::Codec<bits>, core/GradQuant.cuh
#define SS_EXPR_ADAM_M  6     // QuantizedAdamState<bits>: g1
#define SS_EXPR_ADAM_V  7     // QuantizedAdamState<bits>: g2

// ---- where splat i's cell c is ----
#define SS_EXPR_AOS     0     // stride * i + c
#define SS_EXPR_SH_PAIR 1     // the FPBO SH transpose, docs/notes/sh-quant-layout.md
