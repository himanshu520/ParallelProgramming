/**************************************************************       includes      **************************************************************/
//feature test macros
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE


#include<ctype.h>
#include<errno.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<unistd.h>
#include<termios.h>
#include<sys/ioctl.h>
#include<unistd.h>


/**************************************************************        defines      **************************************************************/
#define KILO_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey { ARROW_LEFT = 1000, ARROW_RIGHT, ARROW_UP, ARROW_DOWN, DEL_KEY, HOME_KEY, END_KEY, PAGE_UP, PAGE_DOWN };


/**************************************************************         data        **************************************************************/
//datatype to store a row of text
typedef struct erow {
    int size;
    char *chars;                       //pointer to a dynamically allocated character array representing a row of text
} erow;

//structure to store the editor configuration
struct editorConfig {
    int cx, cy;                        //to keep track of the current position of the cursor within the file
    int rowoff;                        //this will keep track of the row number corresponding to top of the screen 
    int coloff;                        //this will keep track of the column number corresponding to left edge of the screen
    int screenrows;                    //number of rows in our current editor configuration
    int screencols;                    //number of columns in our current editor configuration
    int numrows;                       //number of rows (non empty) lines in our file
    erow *row;                         //dynamically allocated array that will store the rows of our file
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
int editorReadKey() {
    int nread;
    char c;
    while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        //on cygwin read() timeout returns -1 instead of 0 with errno = EAGAIN, so here we are avoiding that situation
        if(nread == -1 && errno != EAGAIN) die("read");
    }

    if(c == '\x1b') {
        char seq[3];

        if(read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if(read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if(seq[0] == '[') {
            if(seq[1] >= '0' && seq[1] <= '9') {
                if(read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if(seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            } else {
                switch(seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        } else if(seq[0] == 'O') {
            switch(seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }

        return '\x1b';
    } 
    else return c;
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


/**************************************************************   row operations    **************************************************************/
void editorAppendRow(char *s, size_t len) {
    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));

    int at = E.numrows;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    E.numrows++;
}


/**************************************************************       file io       **************************************************************/
//function for opening and reading files from disk
void editorOpen(char *filename) {
    FILE *fp = fopen(filename, "r");
    if(!fp) die("open");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while((linelen = getline(&line, &linecap, fp)) != -1) {
        while(linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) linelen--;
        editorAppendRow(line, linelen);
    }
    free(line);
    fclose(fp);
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
//function to keep track of the row number corresponding to the top of the screen
void editorScroll() {
    if(E.cy < E.rowoff) E.rowoff = E.cy;
    if(E.cy >= E.rowoff + E.screenrows) E.rowoff = E.cy - E.screenrows + 1;
    if(E.cx < E.coloff) E.coloff = E.cx;
    if(E.cx >= E.coloff + E.screencols) E.coloff = E.cx - E.screencols + 1;
}

//drawing '~' on the left side of the screen after the end of file
void editorDrawRows(struct abuf *ab) {
    int y;
    for(y = 0; y < E.screenrows; y++) {
        int filerow = y + E.rowoff;
        if(filerow >= E.numrows) {
            if(E.numrows == 0 && y == E.screenrows / 3) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome), "Kilo editor -- version %s", KILO_VERSION);
                if(welcomelen > E.screencols) welcomelen = E.screencols;
                int padding = (E.screencols - welcomelen) / 2;
                if(padding) {
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while(padding--) abAppend(ab, " ", 1);
                abAppend(ab, welcome, welcomelen);
            } else {
                abAppend(ab, "~", 1);
            }
        } else {
            int len = E.row[filerow].size - E.coloff;
            if(len < 0) len = 0;
            if(len > E.screencols) len = E.screencols;
            abAppend(ab, &E.row[filerow].chars[E.coloff], len);
        }

        abAppend(ab, "\x1b[K", 3);
        if(y < E.screenrows - 1)
            abAppend(ab, "\r\n", 2);
    }
}

//function to refresh the screen after each keypress
void editorRefreshScreen() {
    editorScroll();

    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);   //hides the cursor
    abAppend(&ab, "\x1b[H", 3);      //reposition the cursor to the beginning of the screen
    
    editorDrawRows(&ab);             //call editorDrawRows() to draw the tilde on the screen

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy - E.rowoff + 1, E.cx - E.coloff + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);   //again turn the cursor on

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}


/**************************************************************        input        **************************************************************/
//function to move the cursor on screen using wsad keys
void editorMoveCursor(int key) {
    switch(key) {
        case ARROW_LEFT:
            if(E.cx != 0) E.cx--;
            break;
        case ARROW_RIGHT:
            E.cx++;
            break;
        case ARROW_UP:
            if(E.cy != 0) E.cy--;
            break;
        case ARROW_DOWN:
            if(E.cy < E.numrows) E.cy++;
            break;
    }
}

//function to process keypresses
void editorProcessKeypress() {
    int c = editorReadKey();

    switch(c) {
        case CTRL_KEY('q'):     
            write(STDOUT_FILENO, "\x1b[2J", 4);     //clear the terminal screen
            write(STDOUT_FILENO, "\x1b[H", 3);      //reposition the cursor to the beginning of the screen
            exit(0);
            break;
        
        case HOME_KEY:
            E.cx = 0;
            break;

        case END_KEY:
            E.cx = E.screencols - 1;
            break;

        case PAGE_UP:
        case PAGE_DOWN:
            {
                int times = E.screenrows;
                while(times--)
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
    }
}


/**************************************************************        init         **************************************************************/
//function to initialise all the fields in strucutre E for the editor
void initEditor() {
    E.cx = E.cy = E.numrows = E.rowoff = E.coloff = 0;
    E.row = NULL;
    if(getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}


/**************************************************************        main         **************************************************************/
int main(int argc, char **argv) {
    enableRawMode();
    initEditor();
    if(argc >= 2) editorOpen(argv[1]);
    
    while(1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    
    return 0;
}