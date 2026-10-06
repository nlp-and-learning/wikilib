#pragma once
#include <cctype>
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>

namespace wikilib::templates::detail {
// Small, locale-independent numeric grammar; no implicit fallback to zero.
class Expression {
    std::string_view input_;
    size_t pos_ = 0;
    int nesting_ = 0;

    void whitespace() {
        while (pos_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[pos_])))
            ++pos_;
    }

    [[noreturn]] void fail(const char *message) const {
        throw std::runtime_error(std::string("Expression error: ") + message);
    }

    double finite(double value) const {
        if (!std::isfinite(value))
            fail("non-finite result");
        return value;
    }

    struct Operator {
        std::string_view name;
        int precedence;
    };

    Operator next_operator() {
        whitespace();
        for (const auto op: {Operator{"or", 1},
                             {"and", 2},
                             {"!=", 3},
                             {"<>", 3},
                             {"<=", 3},
                             {">=", 3},
                             {"=", 3},
                             {"<", 3},
                             {">", 3},
                             {"+", 4},
                             {"-", 4},
                             {"*", 5},
                             {"/", 5},
                             {"div", 5},
                             {"mod", 5},
                             {"^", 6}}) {
            if (!input_.substr(pos_).starts_with(op.name))
                continue;
            const auto end = pos_ + op.name.size();
            if (std::isalpha(static_cast<unsigned char>(op.name.front())) && end < input_.size() &&
                (std::isalpha(static_cast<unsigned char>(input_[end])) || input_[end] == '_'))
                continue;
            return op;
        }
        return {{}, 0};
    }

    double prefix() {
        whitespace();
        if (++nesting_ > 128)
            fail("nesting limit exceeded");

        struct Guard {
            int &value;

            ~Guard() {
                --value;
            }
        } guard{nesting_};

        if (pos_ == input_.size())
            fail("expected number");
        const char ch = input_[pos_];
        if (ch == '+' || ch == '-') {
            ++pos_;
            const auto value = prefix();
            return ch == '-' ? -value : value;
        }
        if (input_.substr(pos_).starts_with("not") &&
            (pos_ + 3 == input_.size() || !std::isalpha(static_cast<unsigned char>(input_[pos_ + 3])))) {
            pos_ += 3;
            return prefix() == 0 ? 1 : 0;
        }
        if (ch == '(') {
            ++pos_;
            const auto value = binary(1);
            whitespace();
            if (pos_ == input_.size() || input_[pos_++] != ')')
                fail("missing closing parenthesis");
            return value;
        }
        double value = 0;
        const auto parsed = std::from_chars(input_.data() + pos_, input_.data() + input_.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr == input_.data() + pos_)
            fail("invalid number or unsupported identifier");
        pos_ = static_cast<size_t>(parsed.ptr - input_.data());
        return finite(value);
    }

    double apply(std::string_view op, double a, double b) {
        if (op == "+")
            return finite(a + b);
        if (op == "-")
            return finite(a - b);
        if (op == "*")
            return finite(a * b);
        if (op == "/" || op == "div" || op == "mod") {
            if (b == 0)
                fail("division by zero");
            if (op == "/")
                return finite(a / b);
            if (op == "div")
                return finite(std::trunc(a / b));
            return finite(std::fmod(a, b));
        }
        if (op == "^")
            return finite(std::pow(a, b));
        if (op == "=")
            return a == b;
        if (op == "!=" || op == "<>")
            return a != b;
        if (op == "<")
            return a < b;
        if (op == ">")
            return a > b;
        if (op == "<=")
            return a <= b;
        if (op == ">=")
            return a >= b;
        if (op == "and")
            return a != 0 && b != 0;
        return a != 0 || b != 0;
    }

    double binary(int minimum) {
        auto value = prefix();
        while (true) {
            const auto op = next_operator();
            if (op.precedence < minimum)
                return value;
            pos_ += op.name.size();
            // Operators of the same precedence associate left to right.
            value = apply(op.name, value, binary(op.precedence + 1));
        }
    }
public:
    explicit Expression(std::string_view input) : input_(input) {
        if (input.size() > 65536)
            fail("input limit exceeded");
    }

    double evaluate() {
        const auto value = binary(1);
        whitespace();
        if (pos_ != input_.size())
            fail("unexpected token");
        return value;
    }
};
} // namespace wikilib::templates::detail
