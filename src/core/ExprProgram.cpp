// ExprProgram.cpp -- see ExprProgram.h. Recursive descent straight to
// postfix: the program is the only representation.

#include "core/ExprProgram.h"

#include "shaders/splat_expr.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace spirula {
namespace {

struct Fn {
    const char* name;
    uint32_t op;
    int arity;   // -1: one or more, folded pairwise (min, max)
};
constexpr Fn kFns[] = {
    {"abs", SS_EXPR_ABS, 1},     {"sqrt", SS_EXPR_SQRT, 1},
    {"ln", SS_EXPR_LN, 1},       {"log", SS_EXPR_LN, 1},
    {"log2", SS_EXPR_LOG2, 1},   {"log10", SS_EXPR_LOG10, 1},
    {"exp", SS_EXPR_EXP, 1},     {"sin", SS_EXPR_SIN, 1},
    {"cos", SS_EXPR_COS, 1},     {"tanh", SS_EXPR_TANH, 1},
    {"sigmoid", SS_EXPR_SIGMOID, 1},
    {"floor", SS_EXPR_FLOOR, 1}, {"sign", SS_EXPR_SIGN, 1},
    {"pow", SS_EXPR_POW, 2},     {"min", SS_EXPR_MIN, -1},
    {"max", SS_EXPR_MAX, -1},    {"where", SS_EXPR_WHERE, 3},
    {"clamp", SS_EXPR_CLAMP, 3},
};

struct ParseError {
    size_t pos;
    std::string what;
};

class Parser {
public:
    Parser(const std::string& s, const std::function<bool(const std::string&)>& is_var,
           ExprProgram& out)
        : _s(s), _is_var(is_var), _out(out) {}

    void run() {
        comparison();
        skip_ws();
        if (_pos < _s.size()) fail("unexpected '" + std::string(1, _s[_pos]) + "'");
        if (_out.code.size() > SS_EXPR_MAX_CODE)
            fail("expression too long", 0);
    }

private:
    const std::string& _s;
    const std::function<bool(const std::string&)>& _is_var;
    ExprProgram& _out;
    size_t _pos = 0;
    int _depth = 0;

    [[noreturn]] void fail(const std::string& what) { throw ParseError{_pos, what}; }
    [[noreturn]] void fail(const std::string& what, size_t pos) { throw ParseError{pos, what}; }

    void emit(uint32_t op, int pops, uint32_t operand = 0) {
        _depth += 1 - pops;
        if (_depth > SS_EXPR_MAX_STACK) fail("expression nested too deeply");
        _out.max_stack = std::max(_out.max_stack, _depth);
        _out.code.push_back(op | (operand << SS_EXPR_OP_BITS));
    }

    void skip_ws() {
        while (_pos < _s.size() && std::isspace((unsigned char)_s[_pos])) _pos++;
    }
    bool eat(const char* tok) {
        skip_ws();
        const size_t n = std::strlen(tok);
        if (_s.compare(_pos, n, tok) != 0) return false;
        _pos += n;
        return true;
    }
    void expect(const char* tok) {
        if (!eat(tok)) fail(std::string("expected '") + tok + "'");
    }

    void comparison() {
        additive();
        static const std::pair<const char*, uint32_t> kOps[] = {
            {"<=", SS_EXPR_LE}, {">=", SS_EXPR_GE}, {"==", SS_EXPR_EQ},
            {"!=", SS_EXPR_NE}, {"<", SS_EXPR_LT},  {">", SS_EXPR_GT}};
        for (const auto& [tok, op] : kOps)
            if (eat(tok)) {
                additive();
                emit(op, 2);
                return;
            }
    }

    void additive() {
        term();
        for (;;) {
            if (eat("+"))      { term(); emit(SS_EXPR_ADD, 2); }
            else if (eat("-")) { term(); emit(SS_EXPR_SUB, 2); }
            else return;
        }
    }

    void term() {
        unary();
        for (;;) {
            if (eat("*"))      { unary(); emit(SS_EXPR_MUL, 2); }
            else if (eat("/")) { unary(); emit(SS_EXPR_DIV, 2); }
            else return;
        }
    }

    // -x^2 is -(x^2), and the exponent may itself be negated: 2^-1.
    void unary() {
        if (eat("-")) { unary(); emit(SS_EXPR_NEG, 1); }
        else if (eat("+")) unary();
        else power();
    }

    void power() {
        primary();
        if (eat("^") || eat("**")) { unary(); emit(SS_EXPR_POW, 2); }
    }

    void primary() {
        skip_ws();
        if (_pos >= _s.size()) fail("unexpected end of expression");
        const char c = _s[_pos];
        if (eat("(")) {
            comparison();
            expect(")");
        } else if (std::isdigit((unsigned char)c) || c == '.') {
            number();
        } else if (std::isalpha((unsigned char)c) || c == '_') {
            name();
        } else {
            fail("unexpected '" + std::string(1, c) + "'");
        }
    }

    void number() {
        const size_t start = _pos;
        auto digits = [&] {
            while (_pos < _s.size() && std::isdigit((unsigned char)_s[_pos])) _pos++;
        };
        digits();
        if (_pos < _s.size() && _s[_pos] == '.') { _pos++; digits(); }
        if (_pos < _s.size() && (_s[_pos] == 'e' || _s[_pos] == 'E')) {
            size_t p = _pos + 1;
            if (p < _s.size() && (_s[p] == '+' || _s[p] == '-')) p++;
            if (p < _s.size() && std::isdigit((unsigned char)_s[p])) { _pos = p; digits(); }
        }
        const std::string tok = _s.substr(start, _pos - start);
        if (tok == ".") fail("malformed number", start);
        const float v = (float)std::strtod(tok.c_str(), nullptr);
        uint32_t bits;
        std::memcpy(&bits, &v, sizeof bits);
        emit(SS_EXPR_CONST, 0);
        _out.code.push_back(bits);
    }

    void name() {
        const size_t start = _pos;
        while (_pos < _s.size() &&
               (std::isalnum((unsigned char)_s[_pos]) || _s[_pos] == '_'))
            _pos++;
        const std::string id = _s.substr(start, _pos - start);
        skip_ws();
        if (_pos < _s.size() && _s[_pos] == '(') {
            call(id, start);
            return;
        }
        if (!_is_var(id)) fail("unknown name '" + id + "'", start);
        auto it = std::find(_out.vars.begin(), _out.vars.end(), id);
        if (it == _out.vars.end()) {
            if (_out.vars.size() >= SS_EXPR_MAX_VARS)
                fail("more than " + std::to_string(SS_EXPR_MAX_VARS) + " variables", start);
            it = _out.vars.insert(_out.vars.end(), id);
        }
        emit(SS_EXPR_VAR, 0, (uint32_t)(it - _out.vars.begin()));
    }

    void call(const std::string& id, size_t start) {
        const Fn* fn = nullptr;
        for (const Fn& f : kFns)
            if (id == f.name) fn = &f;
        if (!fn) fail("unknown function '" + id + "'", start);
        expect("(");
        int n = 0;
        if (!eat(")")) {
            do {
                comparison();
                // min/max fold as they go, so their stack stays two deep.
                if (fn->arity < 0 && ++n > 1) emit(fn->op, 2);
                else if (fn->arity >= 0) n++;
            } while (eat(","));
            expect(")");
        }
        if (fn->arity < 0 ? n < 1 : n != fn->arity)
            fail(id + "() takes " +
                 (fn->arity < 0 ? std::string("one or more arguments")
                                : std::to_string(fn->arity) +
                                      (fn->arity == 1 ? " argument" : " arguments")),
                 start);
        if (fn->arity > 0) emit(fn->op, fn->arity);
    }
};

}  // namespace

bool compile_expr(const std::string& text,
                  const std::function<bool(const std::string&)>& is_var,
                  ExprProgram& out, std::string& error) {
    out = ExprProgram{};
    try {
        Parser(text, is_var, out).run();
    } catch (const ParseError& e) {
        error = "column " + std::to_string(e.pos + 1) + ": " + e.what;
        out = ExprProgram{};
        return false;
    }
    return true;
}

float eval_expr(const ExprProgram& p, const float* vars) {
    float st[SS_EXPR_MAX_STACK];
    int sp = 0;
    for (size_t pc = 0; pc < p.code.size(); pc++) {
        const uint32_t w = p.code[pc];
        const uint32_t op = w & ((1u << SS_EXPR_OP_BITS) - 1);
        if (op == SS_EXPR_VAR) { st[sp++] = vars[w >> SS_EXPR_OP_BITS]; continue; }
        if (op == SS_EXPR_CONST) { std::memcpy(&st[sp++], &p.code[++pc], 4); continue; }
        if (op == SS_EXPR_WHERE || op == SS_EXPR_CLAMP) {
            const float a = st[sp - 3], b = st[sp - 2], c = st[sp - 1];
            sp -= 2;
            st[sp - 1] = op == SS_EXPR_WHERE ? (a != 0.0f ? b : c)
                                             : std::min(std::max(a, b), c);
            continue;
        }
        if (op >= SS_EXPR_ADD) {
            const float a = st[sp - 2], b = st[sp - 1];
            float r = 0.0f;
            switch (op) {
                case SS_EXPR_ADD: r = a + b; break;
                case SS_EXPR_SUB: r = a - b; break;
                case SS_EXPR_MUL: r = a * b; break;
                case SS_EXPR_DIV: r = a / b; break;
                case SS_EXPR_POW: r = std::pow(a, b); break;
                case SS_EXPR_MIN: r = std::min(a, b); break;
                case SS_EXPR_MAX: r = std::max(a, b); break;
                case SS_EXPR_LT:  r = a < b; break;
                case SS_EXPR_LE:  r = a <= b; break;
                case SS_EXPR_GT:  r = a > b; break;
                case SS_EXPR_GE:  r = a >= b; break;
                case SS_EXPR_EQ:  r = a == b; break;
                case SS_EXPR_NE:  r = a != b; break;
                default: throw std::logic_error("eval_expr: bad opcode");
            }
            st[--sp - 1] = r;
            continue;
        }
        float& x = st[sp - 1];
        switch (op) {
            case SS_EXPR_NEG:     x = -x; break;
            case SS_EXPR_ABS:     x = std::fabs(x); break;
            case SS_EXPR_SQRT:    x = std::sqrt(x); break;
            case SS_EXPR_LN:      x = std::log(x); break;
            case SS_EXPR_LOG2:    x = std::log2(x); break;
            case SS_EXPR_LOG10:   x = std::log10(x); break;
            case SS_EXPR_EXP:     x = std::exp(x); break;
            case SS_EXPR_SIN:     x = std::sin(x); break;
            case SS_EXPR_COS:     x = std::cos(x); break;
            case SS_EXPR_TANH:    x = std::tanh(x); break;
            case SS_EXPR_SIGMOID: x = 1.0f / (1.0f + std::exp(-x)); break;
            case SS_EXPR_FLOOR:   x = std::floor(x); break;
            case SS_EXPR_SIGN:    x = (float)((x > 0.0f) - (x < 0.0f)); break;
            default: throw std::logic_error("eval_expr: bad opcode");
        }
    }
    return sp == 1 ? st[0] : NAN;
}

}  // namespace spirula
