#include<bits/stdc++.h>
using namespace std;

int main() {
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    int cnt = 1;
    for(int k = 2; k < 10000; k *= 10, cnt++)  {
        ifstream in("kilo.c");
        ofstream out("test" + to_string(cnt) + ".c");
        string str;

        for(int i = 0; i < k; i++) {
            in.seekg(0, in.beg);
            while(getline(in, str))
                out << str << "\n";
            in.clear();
        }

        in.close();
        out.close();
    }


    for(int k = 5; k < 10000; k *= 10, cnt++)  {
        ifstream in("kilo.c");
        ofstream out("test" + to_string(cnt) + ".txt");
        string str;

        for(int i = 0; i < k; i++) {
            in.seekg(0, in.beg);
            while(getline(in, str))
                out << str << "\n";
            in.clear();
        }

        in.close();
        out.close();
    }

    return 0;
}