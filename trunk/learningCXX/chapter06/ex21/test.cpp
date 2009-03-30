#include <map>
#include <string>
#include <iostream>

using namespace std;

enum Token_value {
    NAME, NUMBER, END,
    PLUS = '+', MINUS = '-', MUL = '*', DIV = '/',
    PRINT = ';', ASSIGN = '=', LP = '(', RP = ')',
};

int no_of_errors;
int curr_lineno = 1;

double
error(const string &s)
{
    no_of_errors++;
    cerr << "error in line " << curr_lineno << ": " << s << '\n';
    return 1;
}

struct symbol {
    Token_value curr_tok;
    double number_value;
    string string_value;
};

symbol
get_token()
{
    char ch = 0;
    symbol ret;
    do {
        if (!cin.get(ch)) { 
            ret.curr_tok = END;
            return ret;
        }
    } while (ch != '\n' && isspace(ch));

    switch (ch) {
    case 0:
        ret.curr_tok = END;
        return ret;
    case '+':
    case '-':
    case '*':
    case '/':
    case ';':
    case '=':
    case '(':
    case ')':
        ret.curr_tok = Token_value(ch);
        return ret;
    case '\n':
        curr_lineno++;
        ret.curr_tok = PRINT;
        return ret;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        cin.putback(ch);
        cin >> ret.number_value;
        ret.curr_tok = NUMBER;
        return ret;
    default:
        if (isalpha(ch)) {
            ret.string_value = ch;
            while (cin.get(ch) && isalnum(ch))
                ret.string_value.push_back(ch);
            cin.putback(ch);
            ret.curr_tok = NAME;
            return ret;
        }
        error("bad token");
        ret.curr_tok = PRINT;
        return ret;
    }
}

map<string, double> table;

double expr(bool get, symbol *s);

double
prim(bool get, symbol *s)
{
    if (get) 
        *s = get_token();

    switch (s->curr_tok) {
    case NUMBER:
    {
        double v = s->number_value;
        *s = get_token();
        return v;
    }
    case NAME:
    {
        double &v = table[s->string_value];
        *s = get_token();
        if (s->curr_tok == ASSIGN)
            v = expr(true, s);
        return v;
    }
    case MINUS:
        return -prim(true, s);
    case LP:
    {
        double e = expr(true, s);
        if (s->curr_tok != RP)
            return error("_) expected");
        *s = get_token();
        return e;
    }
    default:
        return error("primary expected");
    }
}

double
term(bool get, symbol *s)
{
    double left = prim(get, s);
    for (;;) {
        switch (s->curr_tok) {
        case MUL:
            left *= prim(true, s);
            break;
        case DIV:
            left /= prim(true, s);
            break;
        default:
            return left;
        }
    }
}

double
expr(bool get, symbol *s)
{
    double left = term(get, s);
    for (;;) {
        switch (s->curr_tok) {
        case PLUS:
            left += term(true, s);
            break;
        case MINUS:
            left -= term(true, s);
            break;
        default:
            return left;
        }
    }
}

int
main(int argc, char *argv[])
{
    table["pi"] = 3.141593;
    table["e"] = 2.718282;

    while (cin) {
        symbol s;
        s = get_token();
        if (s.curr_tok == END)
            break;
        if (s.curr_tok == PRINT)
            continue;
        cout << expr(false, &s) << endl;
    }
    return no_of_errors;
}
