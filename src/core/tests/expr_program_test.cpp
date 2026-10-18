// The per-splat expression compiler (core/ExprProgram.h): precedence and
// associativity, functions, the variable table, the limits, and errors that
// name the column. The device evaluator is held to eval_expr by
// backend/tests/engine/splat_inspect_parity.cpp.

#include "core/ExprProgram.h"

#include "shaders/splat_expr.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using spirula::ExprProgram;

static int g_failures = 0;
static void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

static bool is_var(const std::string& n) {
    return n == "x_g1" || n == "y_g1" || n == "z_g1" || n == "a" || n == "b";
}

// Evaluates `text` with x_g1 = 1, y_g1 = -2, z_g1 = 3, a = 0.5, b = 4.
static float eval(const std::string& text, std::string* error = nullptr) {
    ExprProgram p;
    std::string err;
    if (!spirula::compile_expr(text, is_var, p, err)) {
        if (error) *error = err;
        return NAN;
    }
    std::vector<float> v;
    for (const std::string& name : p.vars)
        v.push_back(name == "x_g1" ? 1.f : name == "y_g1" ? -2.f : name == "z_g1" ? 3.f
                  : name == "a" ? 0.5f : 4.f);
    return spirula::eval_expr(p, v.data());
}

static void evals_to(const std::string& text, double want) {
    const float got = eval(text);
    check(std::fabs(got - want) <= 1e-5 * std::max(1.0, std::fabs(want)),
          text + " = " + std::to_string(got) + " (want " + std::to_string(want) + ")");
}

static void rejects(const std::string& text, const std::string& column) {
    std::string err;
    const float got = eval(text, &err);
    check(std::isnan(got) && err.rfind(column, 0) == 0,
          "'" + text + "' is refused at " + column + ": " + err);
}

int main() {
    evals_to("1 + 2 * 3", 7);
    evals_to("(1 + 2) * 3", 9);
    evals_to("10 - 4 - 3", 3);
    evals_to("2 ^ 3 ^ 2", 512);
    evals_to("2 ** 3", 8);
    evals_to("-2 ^ 2", -4);
    evals_to("2 ^ -1", 0.5);
    evals_to("1e-3 * 2E3 + .5", 2.5);
    evals_to("a < b", 1);
    evals_to("a >= b", 0);
    evals_to("x_g1 + y_g1 == -1", 1);
    evals_to("min(x_g1, y_g1, z_g1)", -2);
    evals_to("max(x_g1, y_g1, z_g1, 1e-18)", 3);
    evals_to("clamp(b, 0, 1)", 1);
    evals_to("where(a > 1, 10, 20)", 20);
    evals_to("sigmoid(0) + sign(y_g1) + floor(-0.5)", 0.5 - 1 - 1);
    evals_to("log(exp(2)) + ln(1) + log2(8) + log10(100)", 7);
    evals_to("ln((abs(x_g1)+abs(y_g1)+abs(z_g1))/sqrt(max(x_g1,y_g1,z_g1,1e-18)))",
             std::log(6.0 / std::sqrt(3.0)));

    ExprProgram p;
    std::string err;
    check(spirula::compile_expr("a * a + b * a", is_var, p, err) && p.vars.size() == 2 &&
              p.vars[0] == "a" && p.vars[1] == "b",
          "a variable gets one slot however often it is named");
    check(spirula::compile_expr("max(a, b, a, b, a, b, a, b)", is_var, p, err) &&
              p.max_stack == 2,
          "min/max fold as they go: stack " + std::to_string(p.max_stack));

    std::string deep = "a";
    for (int i = 0; i < SS_EXPR_MAX_STACK; i++) deep = "(a + " + deep + ")";
    check(!spirula::compile_expr(deep, is_var, p, err), "too deep a nest is refused: " + err);

    rejects("a +", "column 4");
    rejects("a + q", "column 5");
    rejects("lg(a)", "column 1");
    rejects("pow(a)", "column 1");
    rejects("(a + b", "column 7");
    rejects("a b", "column 3");
    rejects("a < b < a", "column 7");

    std::printf("expr_program_test: %s\n", g_failures ? "FAILED" : "PASSED");
    return g_failures ? 1 : 0;
}
