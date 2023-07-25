#ifndef _LOGICEXPR_TOKENIZER_HPP
#define _LOGICEXPR_TOKENIZER_HPP

#include <queue>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#include <fmt/format.h>
#include <parsec/parsec.hpp>

#include "token.hpp"

namespace logicexpr::parser
{
namespace
{
constexpr int ERROR_TOKEN_START = -1;
constexpr int TOKEN_START = 0;
constexpr int OPERATOR_TOKEN_START = 10;

} // namespace

namespace opstr
{
const static std::string OR = "OR";
const static std::string AND = "AND";
const static std::string NOT = "!";
const static std::string P_OPEN = "(";
const static std::string P_CLOSE = ")";
} // namespace opstr

template<typename TermP>
class Tokenizer
{
    // Assert that TermP is a Parser specialization
    static_assert(parsec::traits::is_parser<TermP>::value, "TermP must be a specialization of parsec::Parser");

private:
    TermP m_termParser;

    parsec::Result<Token> termParser(std::string_view sv, size_t pos) const
    {
        auto res = m_termParser(sv, pos);
        auto buildToken = std::move(res.value());

        if (res.success())
        {
            return parsec::makeSuccess(Token {TermToken<decltype(buildToken)>::create(
                                           std::move(buildToken), std::string(sv.substr(pos, res.index() - pos)), pos)},
                                       res.index());
        }
        else
        {
            return parsec::makeError<Token>("Unexpected token", pos);
        }
    }

    parsec::Result<Token> operatorParser(std::string_view sv, size_t pos) const
    {
        if (sv.substr(pos, opstr::OR.size()) == opstr::OR)
        {
            return parsec::makeSuccess(Token {OrToken::create(opstr::OR, pos)}, pos + opstr::OR.size());
        }
        else if (sv.substr(pos, opstr::AND.size()) == opstr::AND)
        {
            return parsec::makeSuccess(Token {AndToken::create(opstr::AND, pos)}, pos + opstr::AND.size());
        }
        else if (sv.substr(pos, opstr::NOT.size()) == opstr::NOT)
        {
            return parsec::makeSuccess(Token {NotToken::create(opstr::NOT, pos)}, pos + opstr::NOT.size());
        }
        else if (sv.substr(pos, opstr::P_OPEN.size()) == opstr::P_OPEN)
        {
            return parsec::makeSuccess(Token {ParenthOpenToken::create(opstr::P_OPEN, pos)},
                                       pos + opstr::P_OPEN.size());
        }
        else if (sv.substr(pos, opstr::P_CLOSE.size()) == opstr::P_CLOSE)
        {
            return parsec::makeSuccess(Token {ParenthCloseToken::create(opstr::P_CLOSE, pos)},
                                       pos + opstr::P_CLOSE.size());
        }
        else
        {
            return parsec::makeError<Token>("Unexpected token", pos);
        }
    }

    parsec::Result<Token> tokenParser(std::string_view sv, size_t pos) const
    {
        auto res = termParser(sv, pos);
        if (res.success())
        {
            return res;
        }
        else
        {
            return operatorParser(sv, pos);
        }
    }

    parsec::Result<std::queue<Token>> parser(std::string_view sv, size_t pos) const
    {
        if (sv.empty())
        {
            return parsec::makeError<std::queue<Token>>("Empty input", pos);
        }

        std::queue<Token> tokens;
        parsec::Result<std::queue<Token>> res;

        parsec::Result<Token> partial;
        std::vector<parsec::Trace> traces;

        bool empty = true;

        do
        {
            // Ignore spaces between tokens
            while (pos < sv.size() && std::isspace(sv[pos]))
            {
                ++pos;
            }

            if (pos >= sv.size())
            {
                break;
            }

            partial = tokenParser(sv, pos);
            traces.push_back(partial.trace());

            if (partial.success())
            {
                empty = false;
                tokens.push(partial.value());
                pos = partial.index();
            }
            else
            {
                res = parsec::makeError<std::queue<Token>>("Unkown token found", pos, traces);
            }
        } while (pos < sv.size() && partial.success());

        if (!partial.failure())
        {
            res = parsec::makeSuccess(std::move(tokens), pos, {}, traces);
        }

        if (empty)
        {
            res = parsec::makeError<std::queue<Token>>("Empty input or spaces only", pos, traces);
        }

        return res;
    }

public:
    Tokenizer(TermP termParser)
        : m_termParser {termParser}
    {
    }

    std::queue<Token> operator()(std::string_view input) const
    {
        // Tokenizer algorithm
        // Try first to parse terms, to avoid scaping other tokens
        // If term parser fails, try to parse rest of tokens
        //   If rest of tokens parser fails, throw exception
        //   If rest of tokens parser succeeds, advance
        // If term parser succeeds, advance
        auto res = parser(input, 0);

        if (res.failure())
        {
            // TODO: Print all traces
            throw std::runtime_error(fmt::format("Parsing error: {}", res.error()));
        }

        return res.value();
    }
};

} // namespace logicexpr::parser

#endif // _LOGICEXPR_TOKENIZER_HPP
