#include <string>
#include <iostream>

using namespace std;

enum Token_value {
    NAME, NUMBER, END,
    PLUS = '+', MINUS = '-', MUL = '*', DIV = '/',
    PRINT = ';', ASSIGN = '=', LP = '(', RP = ')',
};

double number_value;
string string_value;
double 
prim(bool get)
{
    if (get) get_token();

    switch (curr_tok) {
    case NUMBER:
    }
}

double
term(bool get)
{
    double left = prim(get);
    for (;;) {
        switch (curr_tok) {
        case MUL:
            left *= prim(true);
            break;
        case DIV:
            left /= prim(true);
            break;
        default:
            return left;
        }
    }
}

double
expr(bool get)
{
    double left = term(get);
    for (;;) {
        switch (curr_tok) {
        case PLUS:
            left += term(true);
            break;
        case MINUS:
            left -= term(true);
            break;
        default:
            return left;
        }
    }
}

int
main(int argc, char *argv[])
{
    return 0;
}
