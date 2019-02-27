#include<bits/stdc++.h>
#include<unistd.h>
#include<termios.h>
using namespace std;

termios orig_termios;       //we will store the original terminal configurations


//function to print error messages
void die(const string &error_message) {
    perror(error_message.c_str());
    exit(1);
}

//function to disable the raw mode upon exit
void disableRawMode() {
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) die("tcsetattr");
}

//function to enable the raw mode upon starting the text editor
void enableRawMode() {
    //reading the original terminal configuration, we will use it to restore the terminal when the program exits
    if(tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
    //this function will be called whenever the program exits
    atexit(disableRawMode);
    
    
    //modifying the terminal properties
    termios raw = orig_termios;
    //(ECHO) turning the echoing of entered character off (ECHO) 
    //(ICANON) disabling the canonical mode
    //(ISIG) stops the normal functions of ^C and ^Z
    //(IEXTEN) is for ^V (it allows us to send a character literally when pressed after it, eg - ^V^C won't terminate program but send ^C as input)
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN | INPCK | ISTRIP | BRKINT);
    //(IXON) truning off ^S and ^Q (they allow us to stop the program from sending data to the terminal and resume it)
    //(ICRNL) turning off the conversion of ^M and enter to newline, and make them convert to carriage return ie ASCII 13
    raw.c_iflag &= ~(IXON | ICRNL);
    //(OPOST) turning off all the terminal output processing
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag &= ~(CS8);
    //minimum bytes to be read by read() before returning
    raw.c_cc[VMIN] = 0;
    //timeout for which read waits for terminal input before returning (here we set it to 1/10th of a second)
    //when there is a timeout, read() will return 0
    raw.c_cc[VTIME] = 1;

    //setting the new terminal attributes
    //TCSAFLUSH tells to apply the change when pending outputs are written to the terminal and it also discards input that hasn't been read
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}


int main() {
    char c;
    enableRawMode();
    
    while(true) {
        c = 0;
        //on cygwin read() timeout returns -1 instead of 0 with errno = EAGAIN, so here we are avoiding that situation
        if(read(STDIN_FILENO, &c, 1) == -1 and errno != EAGAIN) die("read");
        if(iscntrl(c)) cout  << (int)c << "\r\n";
        else cout << (int)c << " " << c << "\r\n";
        if(c == 'q') break;
    }
    
    return 0;
}