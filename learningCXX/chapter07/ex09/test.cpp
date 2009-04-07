#include <cstring>
#include <string>
#include <iostream>

using namespace std;

int
main(int argc, char *argv[])
{
    // read key
    char *key = NULL;
    int key_len = 0;
    if (argc == 2) {
        key = argv[1];
        key_len = strlen(key);
    }
    // encode
    char c;
    int idx = 0;
    string out = "";
    while (cin.get(c)) {
        if (key) {
            c ^= key[idx++];
            if (idx == key_len)
                idx = 0;
        }
        out.push_back(c);
    }
    // output
    cout << out;

    return 0;
}
