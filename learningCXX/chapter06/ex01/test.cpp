#include <cstring>
#include <iostream>

using namespace std;

int
main(int argc, char *argv[])
{
    const char *input_line = "hello, who are you?";
    int max_length = strlen(input_line), quest_count = 0;

    for (int i = 0; i < max_length; i++) 
        if (input_line[i] == '?')
            quest_count++;
    cout << "quest_count = " << quest_count << endl;

    quest_count = 0;
    const char *p = input_line;
    while (*p != '\0') {
        if (*p == '?')
            quest_count++;
        p++;
    }
    cout << "quest_count = " << quest_count << endl;

    return 0;
}
