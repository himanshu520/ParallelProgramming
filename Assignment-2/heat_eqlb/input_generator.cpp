#include<bits/stdc++.h>
#define MAX_INPUT_FILES 10
using namespace std;

const double PI = acos(-1);

int main() {

    int array_size = 2, no_of_iterations = 10;
    srand(time(0));
    for(int input_file_no = 1; input_file_no <= MAX_INPUT_FILES; input_file_no++) {
        const char * input_file_name = ("input" + to_string(input_file_no) + ".txt").c_str();
        freopen(input_file_name, "w", stdout);
        cout << array_size << "\n";
        cout << no_of_iterations << "\n";
        for(int i = 0; i < array_size; i++)
            cout << fixed << setprecision(5) << (rand() % 1000) * sin((rand() % 1000) * PI / 1000) << "\n";
        array_size += 2;
    }
    
    return 0;
}