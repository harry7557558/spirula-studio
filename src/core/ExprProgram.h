#pragma once

// An infix arithmetic expression compiled to the postfix program
// shaders/splat_expr.h defines, which the device evaluates once per splat and
// eval_expr() on the host (the parity test's reference).

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace spirula {

struct ExprProgram {
    std::vector<uint32_t> code;
    std::vector<std::string> vars;   // the operand of SS_EXPR_VAR, by index
    int max_stack = 0;
};

// False with `error` set when `text` does not parse or outgrows the limits in
// shaders/splat_expr.h. A name is a variable when `is_var` says so, else it
// must be a function call.
bool compile_expr(const std::string& text,
                  const std::function<bool(const std::string&)>& is_var,
                  ExprProgram& out, std::string& error);

// `vars` in the order of ExprProgram::vars.
float eval_expr(const ExprProgram& p, const float* vars);

}  // namespace spirula
