#include<bits/stdc++.h>
#define MAX_INPUT_FILES 5
using namespace std;

int main() {

    int array_size = 1000;
    srand(time(0));
    
    for(int input_file_no = 1; input_file_no <= MAX_INPUT_FILES; input_file_no++) {
        const char * input_file_name = ("input" + to_string(input_file_no) + ".txt").c_str();
        freopen(input_file_name, "w", stdout);
        cout << array_size << "\n";
        for(int i = 0; i < array_size; i++)
            cout << (rand() % 200 - 100) << "\n";
        array_size *= 10;
    }
    
    return 0;
}