/**************************************************************       includes      **************************************************************/
#include<ctype.h>
#include<errno.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<termios.h>
#include<sys/ioctl.h>
#include<unistd.h>


/**************************************************************        defines      **************************************************************/
#define CTRL_KEY(k) ((k) & 0x1f)


/**************************************************************         data        **************************************************************/
//structure to store the editor configuration
struct editorConfig {
    int screenrows;             //number of rows in our current editor configuration
    int screencols;             //number of columns in our current editor configuration
    struct termios orig_termios;       //we will store the original terminal configurations
} E;


/**************************************************************       terminal      **************************************************************/
//function to print error messages
void die(const char *error_message) {
    write(STDOUT_FILENO, "\x1b[2J", 4);     //clear the terminal screen
    write(STDOUT_FILENO, "\x1b[H", 3);      //reposition the cursor to the beginning of the screen
    perror(error_message);
    exit(1);
}

//function to disable the raw mode upon exit
void disableRawMode() {
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) die("tcsetattr");
}

//function to enable the raw mode upon starting the text editor
void enableRawMode() {
    //reading the original terminal configuration, we will use it to restore the terminal when the program exits
    if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
    //this function will be called whenever the program exits
    atexit(disableRawMode);
    
    
    //modifying the terminal properties
    struct termios raw = E.orig_termios;
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

//function to read keypresses
char editorReadKey() {
    int nread;
    char c;
    while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        //on cygwin read() timeout returns -1 instead of 0 with errno = EAGAIN, so here we are avoiding that situation
        if(nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

//function to get the position of the cursor
int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int sz = 0;
    
    //escape sequence to get the cursor position
    //the result is also an escape sequence written in standard input, so we will have to parse the result to extract the position
    if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;  
    
    while(sz < sizeof(buf) - 1) {
        if(read(STDIN_FILENO, &buf[sz], 1) != 1) break;
        if(buf[sz] == 'R') break;
        sz++;
    }
    buf[sz] = '\0';

    if(buf[0] != '\x1b' || buf[1] != '[') return -1;
    if(sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
    
    return 0;
}

//function to get the size of the terminal window
int getWindowSize(int *rows, int *cols) {
    //'winsize' is a structure defined in 'sys/ioctl.h'
    struct winsize ws;

    //ioctl() is used to read terminal properties and TIOCGWINSZ is argument to get the windows size
    //both of these are also defined in 'sys/ioctl.h'
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if(write(STDOUT_FILENO, "\x1b[2024C\x1b[2024B", 14) == -1) return -1;
        return getCursorPosition(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}


/**************************************************************    append buffer    **************************************************************/
struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0};

void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);

    if(new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab) {
    free(ab->b);
}


/**************************************************************       output        **************************************************************/
//drawing '~' on the left side of the screen after the end of file
void editorDrawRows(struct abuf *ab) {
    int y;
    for(y = 0; y < E.screenrows - 1; y++)
        abAppend(ab, "~\r\n", 3);

    abAppend(ab, "~", 1);
}

//function to refresh the screen after each keypress
void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);   //hides the cursor
    abAppend(&ab, "\x1b[2J", 4);     //clear the terminal screen
    abAppend(&ab, "\x1b[H", 3);      //reposition the cursor to the beginning of the screen
    
    editorDrawRows(&ab);             //call editorDrawRows() to draw the tilde on the screen

    abAppend(&ab, "\x1b[H", 3);      //reposition the cursor to the beginning of the screen
    abAppend(&ab, "\x1b[?25h", 6);   //again turn the cursor on

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}


/**************************************************************        input        **************************************************************/
//function to process keypresses
void editorProcessKeypress() {
    char c = editorReadKey();

    switch(c) {
        case CTRL_KEY('q'):     
            write(STDOUT_FILENO, "\x1b[2J", 4);     //clear the terminal screen
            write(STDOUT_FILENO, "\x1b[H", 3);      //reposition the cursor to the beginning of the screen
            exit(0);
            break;
    }
}


/**************************************************************        init         **************************************************************/
//function to initialise all the fields in strucutre E for the editor
void initEditor() {
    if(getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}


/**************************************************************        main         **************************************************************/
int main() {
    char c;
    enableRawMode();
    initEditor();
    
    while(1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    
    return 0;
}