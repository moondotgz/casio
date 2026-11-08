#include "parser.h"
#include "error.h"
#include <QSet>

namespace {

class Parser {
public:
    Parser(QVector<Token> toks, const ParseOptions &opt) : t(std::move(toks)), o(opt) {}

    NodePtr statement()
    {
        NodePtr e = parseOr();
        if (o.allowRelations) {
            while (isOp({"=", "<", ">", "≤", "≥"})) {
                Token op = next();
                auto b = std::make_shared<Node>(Node::Type::Binary, op.pos);
                b->op = op.text;
                b->kids = {e, parseOr()};
                e = b;
            }
        }
        if (isOp({"→"})) {
            Token op = next();
            Token v = next();
            static const QSet<QString> targets = {"A", "B", "C", "D", "E", "F", "M", "x", "y", "MatA", "MatB",
                                                  "MatC", "MatD", "VctA", "VctB", "VctC", "VctD"};
            if (v.type != TokType::Var || !targets.contains(v.text))
                throw CalcError(ErrKind::Syntax, v.pos);
            auto a = std::make_shared<Node>(Node::Type::Assign, op.pos);
            a->op = v.text;
            a->kids = {e};
            e = a;
        }
        if (peek().type != TokType::End)
            throw CalcError(ErrKind::Syntax, peek().pos);
        return e;
    }

private:
    QVector<Token> t;
    const ParseOptions &o;
    int p = 0;

    const Token &peek(int k = 0) const { return t[std::min<int>(p + k, int(t.size()) - 1)]; }
    Token next() { Token r = peek(); if (p < t.size() - 1) ++p; return r; }

    bool isOp(std::initializer_list<const char *> ops) const
    {
        const Token &k = peek();
        if (k.type != TokType::Op) return false;
        for (const char *s : ops)
            if (k.text == QString::fromUtf8(s)) return true;
        return false;
    }
    bool isInfix(std::initializer_list<const char *> ops) const
    {
        const Token &k = peek();
        if (k.type != TokType::Infix) return false;
        for (const char *s : ops)
            if (k.text == QString::fromUtf8(s)) return true;
        return false;
    }

    NodePtr bin(const Token &op, NodePtr a, NodePtr b, const QString &name = {})
    {
        auto n = std::make_shared<Node>(Node::Type::Binary, op.pos);
        n->op = name.isEmpty() ? op.text : name;
        n->kids = {std::move(a), std::move(b)};
        return n;
    }

    NodePtr parseOr()
    {
        NodePtr l = parseAnd();
        while (isInfix({"or", "xor", "xnor"})) {
            Token op = next();
            l = bin(op, l, parseAnd());
        }
        return l;
    }
    NodePtr parseAnd()
    {
        NodePtr l = parseDivR();
        while (isInfix({"and"})) {
            Token op = next();
            l = bin(op, l, parseDivR());
        }
        return l;
    }
    NodePtr parseDivR()
    {
        NodePtr l = parseAdd();
        if (isInfix({"÷R"})) {
            Token op = next();
            l = bin(op, l, parseAdd());
        }
        return l;
    }
    NodePtr parseAdd()
    {
        NodePtr l = parseMul();
        while (isOp({"+", "−"})) {
            Token op = next();
            l = bin(op, l, parseMul());
        }
        return l;
    }
    NodePtr parseMul()
    {
        NodePtr l = parsePerm();
        while (isOp({"×", "÷"})) {
            Token op = next();
            l = bin(op, l, parsePerm());
        }
        return l;
    }
    NodePtr parsePerm()
    {
        NodePtr l = parseFrac();
        while (isInfix({"nPr", "nCr"}) || isOp({"∠"})) {
            Token op = next();
            l = bin(op, l, parseFrac());
        }
        return l;
    }
    NodePtr parseFrac()
    {
        NodePtr l = parseImplicit();
        while (isOp({"/"})) {
            Token op = next();
            l = bin(op, l, parseImplicit());
        }
        return l;
    }

    static bool startsOperand(const Token &k)
    {
        switch (k.type) {
        case TokType::Num: case TokType::Var: case TokType::Func: case TokType::Const:
        case TokType::LParen: case TokType::LGroup: case TokType::Cell:
            return true;
        default:
            return false;
        }
    }

    NodePtr parseImplicit()
    {
        NodePtr l = parseUnary();
        while (startsOperand(peek())) {
            Token here = peek();
            NodePtr r = parseUnary();
            l = bin(here, l, r, QStringLiteral("·"));
        }
        return l;
    }

    NodePtr parseUnary()
    {
        if (isOp({"−", "+"})) {
            Token op = next();
            auto n = std::make_shared<Node>(Node::Type::Unary, op.pos);
            n->op = op.text;
            n->kids = {parseUnary()};
            return n;
        }
        return parsePower();
    }

    NodePtr postfixes(NodePtr base)
    {
        while (peek().type == TokType::Postfix || peek().type == TokType::Conv) {
            Token op = next();
            auto n = std::make_shared<Node>(Node::Type::Postfix, op.pos);
            n->op = op.type == TokType::Conv ? QStringLiteral("conv") : op.text;
            n->text = op.text;
            n->kids = {base};
            base = n;
        }
        return base;
    }

    NodePtr parsePower()
    {
        NodePtr base = postfixes(parsePrimary());
        if (isOp({"^"})) {
            Token op = next();
            NodePtr ex = parseUnary();   // right associative, allows 2^−1
            base = postfixes(bin(op, base, ex));
        }
        return base;
    }

    NodePtr parsePrimary()
    {
        Token k = next();
        switch (k.type) {
        case TokType::Num: {
            auto n = std::make_shared<Node>(Node::Type::Num, k.pos);
            n->num = k.num;
            n->text = k.text;
            if (isOp({"⌟"})) {   // mixed fraction  a⌟b⌟c
                next();
                Token b = next();
                if (b.type != TokType::Num || !isOp({"⌟"})) throw CalcError(ErrKind::Syntax, b.pos);
                next();
                Token c = next();
                if (c.type != TokType::Num) throw CalcError(ErrKind::Syntax, c.pos);
                auto call = std::make_shared<Node>(Node::Type::Call, k.pos);
                call->op = QStringLiteral("mixed");
                for (const Token &x : {k, b, c}) {
                    auto nn = std::make_shared<Node>(Node::Type::Num, x.pos);
                    nn->num = x.num;
                    nn->text = x.text;
                    call->kids.push_back(nn);
                }
                return call;
            }
            return n;
        }
        case TokType::Var: {
            auto n = std::make_shared<Node>(Node::Type::Var, k.pos);
            n->op = k.text;
            return n;
        }
        case TokType::Const: case TokType::Cell: case TokType::Range: {
            auto n = std::make_shared<Node>(k.type == TokType::Const ? Node::Type::Const
                                            : k.type == TokType::Cell ? Node::Type::Cell : Node::Type::Range, k.pos);
            n->op = k.text;
            return n;
        }
        case TokType::LParen: {
            auto n = std::make_shared<Node>(Node::Type::Paren, k.pos);
            n->kids = {parseOr()};
            if (peek().type == TokType::RParen) next();
            else if (peek().type == TokType::End) n->closed = false;
            else throw CalcError(ErrKind::Syntax, peek().pos);
            return n;
        }
        case TokType::LGroup: {
            auto n = std::make_shared<Node>(Node::Type::Group, k.pos);
            n->kids = {parseOr()};
            if (peek().type != TokType::RGroup) throw CalcError(ErrKind::Syntax, peek().pos);
            next();
            return n;
        }
        case TokType::Func: {
            auto n = std::make_shared<Node>(Node::Type::Call, k.pos);
            n->op = k.text;
            if (k.text == "Ran#") return n;
            if (peek().type == TokType::LParen) {
                next();
                if (peek().type == TokType::RParen) { next(); return n; }
                for (;;) {
                    if (peek().type == TokType::Range) {
                        Token r = next();
                        auto rn = std::make_shared<Node>(Node::Type::Range, r.pos);
                        rn->op = r.text;
                        n->kids.push_back(rn);
                    } else {
                        n->kids.push_back(parseOr());
                    }
                    if (peek().type == TokType::Comma) { next(); continue; }
                    if (peek().type == TokType::RParen) { next(); break; }
                    if (peek().type == TokType::End) { n->closed = false; break; }
                    throw CalcError(ErrKind::Syntax, peek().pos);
                }
                return n;
            }
            // Function without parentheses applies to the next primary (e.g. √2, sin30)
            n->closed = false;
            n->op = k.text;
            n->kids = {postfixesIfNotRoot(k.text)};
            return n;
        }
        default:
            throw CalcError(ErrKind::Syntax, k.pos);
        }
    }

    NodePtr postfixesIfNotRoot(const QString &fn)
    {
        if (fn == "√" || fn == "∛") return parsePrimary();
        return parseUnary();
    }
};

} // namespace

NodePtr parseExpression(const QString &src, const ParseOptions &opt)
{
    auto toks = tokenize(src, opt.lex);
    for (const auto &k : toks)
        if (k.type == TokType::Colon) throw CalcError(ErrKind::Syntax, k.pos);
    if (toks.size() == 1) throw CalcError(ErrKind::Syntax, 0);
    Parser p(toks, opt);
    return p.statement();
}

std::vector<NodePtr> parseStatements(const QString &src, const ParseOptions &opt)
{
    auto toks = tokenize(src, opt.lex);
    std::vector<NodePtr> out;
    QVector<Token> cur;
    int depth = 0;
    auto flush = [&](int pos) {
        if (cur.isEmpty()) throw CalcError(ErrKind::Syntax, pos);
        Token end;
        end.type = TokType::End;
        end.pos = pos;
        cur.push_back(end);
        Parser p(cur, opt);
        out.push_back(p.statement());
        cur.clear();
    };
    for (const auto &k : toks) {
        if (k.type == TokType::LParen) ++depth;
        if (k.type == TokType::RParen) --depth;
        if (k.type == TokType::End) { flush(k.pos); break; }
        if (k.type == TokType::Colon && depth <= 0) { flush(k.pos); continue; }
        cur.push_back(k);
    }
    return out;
}

static void collectVars(const NodePtr &n, QStringList &out)
{
    if (!n) return;
    static const QSet<QString> vars = {"A", "B", "C", "D", "E", "F", "M", "x", "y"};
    if (n->type == Node::Type::Var && vars.contains(n->op) && !out.contains(n->op)) out << n->op;
    // the integration / derivative / sum variable x is bound, not free
    if (n->type == Node::Type::Call && (n->op == "∫" || n->op == "d/dx" || n->op == "Σ" || n->op == "Π")) {
        for (size_t i = 1; i < n->kids.size(); ++i) collectVars(n->kids[i], out);
        QStringList inner;
        if (!n->kids.empty()) collectVars(n->kids[0], inner);
        for (const auto &v : inner)
            if (v != "x" && !out.contains(v)) out << v;
        return;
    }
    for (const auto &k : n->kids) collectVars(k, out);
}

QStringList referencedVariables(const NodePtr &n)
{
    QStringList out;
    collectVars(n, out);
    return out;
}
