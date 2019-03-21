/**************************************************************       includes      **************************************************************/
//feature test macros
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE


#include<ctype.h>
#include<errno.h>
#include<fcntl.h>
#include<pthread.h>
#include<stdarg.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/file.h>
#include<sys/ioctl.h>
#include<sys/types.h>
#include<unistd.h>
#include<termios.h>
#include<time.h>
#include<unistd.h>


/**************************************************************        defines      **************************************************************/
#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 8
#define KILO_QUIT_TIMES 3
#define CTRL_KEY(k) ((k) & 0x1f)
#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)
#define UNUSED(x) (void)(x)
#define THREAD_RANGE 10000

enum editorKey { BACKSPACE = 127, ARROW_LEFT = 1000, ARROW_RIGHT, ARROW_UP, ARROW_DOWN, DEL_KEY, HOME_KEY, END_KEY, PAGE_UP, PAGE_DOWN };
enum editorHighlight { HL_NORMAL = 0, HL_COMMENT, HL_MLCOMMENT, HL_KEYWORD1, HL_KEYWORD2, HL_STRING, HL_NUMBER, HL_MATCH };


/**************************************************************         data        **************************************************************/
//structure type to store syntax highlighting info for the different files
struct editorSyntax {
    char *filetype;     //filetype that will be displayed in the status bar (a string)
    char **filematch;   //it is an array of strings that contains a pattern to match a filename against
    char **keywords;    //it is an array of strings that contains common C keywords and datatypes
    char *singleline_comment_start; //stores the starting characters of a single line comment (a string)
    char *multiline_comment_start;  //stores the starting characters of a multi line string (a string)
    char *multiline_comment_end;    //stores the ending characters of a multi line string (a string)
    int flags;          //it is a bit field that will contain flags for whether to highlight numbers and whether to highlight strings for that filetype
};

//datatype to store a row of text
typedef struct erow {
    int size, rsize, idx;              //idx is the index of the row within the file
    char *chars, *render;              //pointer to a dynamically allocated character array representing actual row of text and the text to display
    unsigned char *hl;                 //pointer to an array storing the syntax highlighting details of each character of the current row
    int hl_open_comment;               //variable indicating whether the current row ended with an unclosed multiline comment
} erow;

//structure to store the editor configuration
struct editorConfig {
    int cx, cy;                        //to keep track of the current position of the cursor within the file
    int rx;                            //to keep track of column number in the actually rendered text on the screen (cx <= rx)
    int rowoff;                        //this will keep track of the row number corresponding to top of the screen 
    int coloff;                        //this will keep track of the column number corresponding to left edge of the screen
    int screenrows;                    //number of rows in our current editor configuration
    int screencols;                    //number of columns in our current editor configuration
    int numrows;                       //number of rows (non empty) lines in our file
    erow *row;                         //dynamically allocated array that will store the rows of our file
    pthread_rwlock_t row_lock;         //mutex for numrows and row.
    int dirty;                         //shows whether the currently opened file in the editor has been modified or not
    char *filename;                    //file currently opened in the text editor
    char statusmsg[80];
    time_t statusmsg_time;
    struct editorSyntax *syntax;       //structure to store the syntax highlighting info
    struct termios orig_termios;       //we will store the original terminal configurations
    pthread_mutex_t file_mutex;        //mutex to write to file while saving
} E;


/**************************************************************      file types     **************************************************************/
char *C_HL_extensions[] = { ".c", ".h", ".cpp", NULL };
char *C_HL_keywords[] = { "switch", "if", "while", "for", "break", "continue", "return", "else", "struct", "union", "typedef", "static", "enum", "class", "case", 
                         "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|", "void|", NULL };

struct editorSyntax HLDB[] = {{ "c", C_HL_extensions, C_HL_keywords, "//", "/*", "*/", (HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS) }};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))


/**************************************************************      prototypes     **************************************************************/
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char* editorPrompt(char *prompt, void (*callback)(char *, int));


/**************************************************************       terminal      **************************************************************/
//function to print error messages
void die(const char *error_message) {
    write(STDOUT_FILENO, "\x1b[2J", 4);     //clear the terminal screen
    write(STDOUT_FILENO, "\x1b[H", 3);      //reposition the cursor to the beginning of the screen
    perror(error_message);                  //perror prints a descriptive error message (including argument string which precedes detailed message)
                                            //by looking at the global error number set by the failing function
    exit(1);
}

//function to disable the raw mode upon exit
void disableRawMode() {
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) die("tcsetattr");
    //TCSAFLUSH is meant to flush the buffers both input and output before setting the new attributes
}

//function to enable the raw mode upon starting the text editor
void *enableRawMode(void *arg_p) {
    UNUSED(arg_p);

    //reading the original terminal configuration, we will use it to restore the terminal when the program exits
    if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
    //this function will be called whenever the program terminates normally
    atexit(disableRawMode);
    
    
    //modifying the terminal properties
    struct termios raw = E.orig_termios;
    //(ECHO) turning the echoing of entered character off (ECHO) 
    //(ICANON) disabling the canonical mode
    //(ISIG) stops the normal functions of ^C and ^Z
    //(IEXTEN) is for ^V (it allows us to send a character literally when pressed after it, eg - ^V^C won't terminate program but send ^C as input)
    //INPCK, ISTRIP, BRKINT are not of much significant to modern terminals 
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN | INPCK | ISTRIP | BRKINT);          //local flag
    //(IXON) truning off ^S and ^Q (they allow us to stop the program from sending data to the terminal and resume it)
    //(ICRNL) turning off the conversion of ^M and enter to newline, and make them convert to carriage return ie ASCII 13
    raw.c_iflag &= ~(IXON | ICRNL);                                                     //input flag
    //(OPOST) turning off all the terminal output processing
    //CS8 is of not much significant
    raw.c_oflag &= ~(OPOST);                                                            //output flag
    raw.c_cflag &= ~(CS8);                                                              //control flag
    //minimum bytes to be read by read() before returning
    raw.c_cc[VMIN] = 0;
    //timeout for which read waits for terminal input before returning (here we set it to 1/10th of a second)
    //when there is a timeout, read() will return 0
    raw.c_cc[VTIME] = 1;

    //setting the new terminal attributes
    //TCSAFLUSH tells to apply the change when pending outputs are written to the terminal and it also discards input that hasn't been read
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");

    return NULL;
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
        //fallback method when ioctl fails
        //first move the cursor to the bottom right corner and then get the cursor position
        //C is for cursor to move forward specified number of steps and B is for the cursor to move downwards
        //C and B also ensures that the cursor doesn't move outside the screen, so passing large numbers is safe here
        if(write(STDOUT_FILENO, "\x1b[2024C\x1b[2024B", 14) == -1) return -1;
        return getCursorPosition(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}


/************************************************************** syntax highlighting **************************************************************/
//function that returns boolean value of whether c is a separator or not
int is_separator(int c) {
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

//function to update the 'hl' array for a row
int editorUpdateSyntax(erow *row) {
    row->hl = realloc(row->hl, row->rsize);
    memset(row->hl, HL_NORMAL, row->rsize); //giving a default highlight value to all the charachters

    if(E.syntax == NULL) return row->idx;    //check if any syntax highlighting is to be done

    char **keywords = E.syntax->keywords;

    char *scs = E.syntax->singleline_comment_start;
    char *mcs = E.syntax->multiline_comment_start;
    char *mce = E.syntax->multiline_comment_end;

    int scs_len = scs ? strlen(scs) : 0;
    int mcs_len = mcs ? strlen(mcs) : 0;
    int mce_len = mce ? strlen(mce) : 0;

    int prev_sep = 1;   //variable to keep track of whether the previous character was a separator
    int in_string = 0;  //variable to keep track of whether we are currently inside a string or not 
    //in_string stores '"' or '\'' depending on whether we entered a charcter or string
    int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);
    //variable to keep track of whether we are currently inside a multiline comment or not
    //it is initialised depending on whether the previous row ended with an open multiline comment

    int i = 0;
    while(i < row->rsize) {
        char c = row->render[i];    //current character
        unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;   //previous highlight

        //check if comments are to be highlighted and there is start of singleline comment from the current position in the row
        //if so set highlighting for the remaining row to be that of comment and break the loop
        if(scs_len && !in_string && !in_comment) {
            if(!strncmp(&row->render[i], scs, scs_len)) {
                memset(&row->hl[i], HL_COMMENT, row->rsize - i);
                break;
            }
        }

        //check if multiline comments are to be highlighted and there is start of multiline comment from the current position in the row
        //if so set the highlighting
        if(mcs_len && mce_len && !in_string) {
            if(in_comment) {
                row->hl[i] = HL_MLCOMMENT;
                if(!strncmp(&row->render[i], mce, mce_len)) {
                    memset(&row->hl[i], HL_MLCOMMENT, mce_len);
                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                    continue;
                } else {
                    i++;
                    continue;
                }
            } else if(!strncmp(&row->render[i], mcs, mcs_len)) {
                memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
                i += mcs_len;
                in_comment = 1;
                continue;
            }
        }


        //check if strings are to be highlighted, if so highlight them
        if(E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
            if(in_string) {
                row->hl[i] = HL_STRING;

                //checking for escape sequence
                if(c == '\\' && i + 1 < row->rsize) {
                    row->hl[i + 1] = HL_STRING;
                    i += 2;
                    continue;
                }

                if(c == in_string) in_string = 0;
                i++;
                prev_sep = 1;
                continue;
            } else {
                if(c == '"' || c == '\'') {
                    in_string = c;
                    row->hl[i] = HL_STRING;
                    i++;
                    continue;
                }
            }
        }

        //check if numbers are to be highlighted, if so highlight them
        if(E.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
            if((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) || (c == '.' && prev_hl == HL_NUMBER)) {
                row->hl[i] = HL_NUMBER;
                i++;
                prev_sep = 0;
                continue;
            }
        }

        //if previous character was a separator, check whether the next word is a keyword, if so highlight it
        if(prev_sep) {
            int j = 0;
            for(j = 0; keywords[j]; j++) {
                int klen = strlen(keywords[j]);
                int kw2 = keywords[j][klen - 1] == '|';
                if(kw2) klen--;

                if(!strncmp(&row->render[i], keywords[j], klen) && is_separator(row->render[i + klen])) {
                    memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
                    i += klen;
                    break;
                }
            }
            if(keywords[j] != NULL) {
                prev_sep = 0;
                continue;
            }
        }

        prev_sep = is_separator(c);
        i++;
    }

    //updating the 'hl_open_comment' field of the current row
    //also checking if the current row was changed from being commented to uncommented and vice versa
    //if so calling the function recursively for the next row (as such commenting could effect multiple rows, so we should update all such rows not just the current row)
    int changed = (row->hl_open_comment != in_comment);
    row->hl_open_comment = in_comment;
    if(changed && row->idx + 1 < E.numrows)
        return editorUpdateSyntax(&E.row[row->idx + 1]);
    
    return row->idx;
}

//function to map the values in 'hl' to actual colours
int editorSyntaxToColor(int hl) {
    switch(hl) {
        case HL_COMMENT:
        case HL_MLCOMMENT:      return 36;          //foreground cyan
        case HL_KEYWORD1:       return 33;          //foreground yellow
        case HL_KEYWORD2:       return 32;          //foreground greed
        case HL_STRING:         return 35;          //foreground magenta
        case HL_NUMBER:         return 31;          //foreground red
        case HL_MATCH:          return 34;          //foreground blue
        default:                return 37;          //foreground white  
    }
}

//function that updates the 'E.syntax' field based on the file extension
void *editorSelectSyntaxHighlight(void *arg_p) {
    UNUSED(arg_p);

    E.syntax = NULL;
    if(E.filename == NULL) return NULL;

    char *ext = strchr(E.filename, '.');    //strchr() matches the last occurence of '.' in E.filename

    for(unsigned int j = 0; j < HLDB_ENTRIES; j++) {
        struct editorSyntax *s = &HLDB[j];
        unsigned int i = 0;
        while(s->filematch[i]) {
            int is_ext = (s->filematch[i][0] == '.');
            if((is_ext && ext && !strcmp(ext, s->filematch[i])) || (!is_ext && strstr(E.filename, s->filematch[i]))) {
                E.syntax = s;

                //updating the highlighting of the current file
                int filerow;
                for(filerow = 0; filerow < E.numrows; )
                    filerow = editorUpdateSyntax(&E.row[filerow]) + 1;
                
                return NULL;
            }
            i++;
        }
    }
    return NULL;
}


/**************************************************************   row operations    **************************************************************/
//function that converts 'chars' index to 'render' index
int editorRowCxToRx(erow *row, int cx) {
    int rx = 0, j;
    for(j = 0; j < cx; j++) {
        if(row->chars[j] == '\t')
            rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
        rx++;
    }
    return rx;
}

//function that converts 'render' index to 'chars' index
int editorRowRxToCx(erow *row, int rx) {
    int cur_rx = 0;
    int cx;
    for(cx = 0; cx < row->size; cx++) {
        if(row->chars[cx] == '\t')
            cur_rx += (KILO_TAB_STOP - 1) - (cur_rx % KILO_TAB_STOP);
        cur_rx++;

        if(cur_rx > rx) return cx;
    }
    return cx;  //in case the caller provieds an rx that's out of range, which shouldn't happen
}

//this function will update render field of a 'erow' from its 'chars' field
void editorUpdateRow(erow *row) {
    int tabs = 0, j;
    for(j = 0; j < row->size; j++)
        if(row->chars[j] == '\t') tabs++;

    free(row->render);
    row->render = malloc(row->size + tabs * (KILO_TAB_STOP - 1) + 1);

    int idx = 0;
    for(j = 0; j < row->size; j++) {
        if(row->chars[j] == '\t') {
            row->render[idx++] = ' ';
            while(idx % KILO_TAB_STOP != 0) row->render[idx++] = ' ';
        } else row->render[idx++] = row->chars[j];
    }
    row->render[idx] = '\0';
    row->rsize = idx;

    //calling the editorUpdateSyntax() function to work out the highlighting
    editorUpdateSyntax(row);
}

//this function will update render field of a 'erow' from its 'chars' field (called from only inside openEditor() function)
void *editorUpdateRowParallel(void *arg_p) {
    int my_start = THREAD_RANGE * (*((int *)arg_p));
    int my_end = my_start + THREAD_RANGE, i;
    if(my_end > E.numrows) my_end = E.numrows;
    
    for(i = my_start; i < my_end; i++) {
        pthread_rwlock_rdlock(&E.row_lock);
        char *rowchars = E.row[i].chars;
        int rowsz = E.row[i].size, j;
        pthread_rwlock_unlock(&E.row_lock);
        
        int tabs = 0;
        for(j = 0; j < rowsz; j++)
            if(rowchars[j] == '\t') tabs++;

        char *rowrender = malloc(rowsz + tabs * (KILO_TAB_STOP - 1) + 1);

        int idx = 0;
        for(j = 0; j < rowsz; j++) {
            if(rowchars[j] == '\t') {
                rowrender[idx++] = ' ';
                while(idx % KILO_TAB_STOP != 0) rowrender[idx++] = ' ';
            } else rowrender[idx++] = rowchars[j];
        }
        rowrender[idx] = '\0';
        
        pthread_rwlock_rdlock(&E.row_lock);
        free(E.row[i].render);
        E.row[i].render = rowrender;
        E.row[i].rsize = idx;
        pthread_rwlock_unlock(&E.row_lock);
    }

    return NULL;
}

//this function will insert a new row for a string to the E.row array
void editorInsertRow(int at, char *s, size_t len) {
    if(at < 0 || at > E.numrows) return;

    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
    for(int j = at + 1; j < E.numrows; j++)
        E.row[j].idx++;

    E.row[at].idx = at;

    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    E.row[at].hl = NULL;
    E.row[at].hl_open_comment = 0;
    editorUpdateRow(&E.row[at]);

    E.numrows++;
    E.dirty++;
}

//function to free an erow
void editorFreeRow(erow *row) {
    free(row->render);
    free(row->chars);
}

//function to delete a row, it frees the current row and moves the next row to the current row
void editorDelRow(int at) {
    if(at < 0 || at >= E.numrows) return;
    editorFreeRow(&E.row[at]);
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
    E.numrows--;
    E.dirty++;
}

//this function inserts a single character into an erow at a given position, it doesn't need to worry about where the cursor is
//note that here we have added 2, because we have to allocate space for the null byte also, which is not counted in row->size
void editorRowInsertChar(erow *row, int at, int c) {
    if(at <  0 || at > row->size) at = row->size;
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;
    editorUpdateRow(row);
    E.dirty++;
}

//function to append a string to the end of a row
void editorRowAppendString(erow *row, char *s, size_t len) {
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    E.dirty++;
}

//function to delete a character from an erow
void editorRowDelChar(erow *row, int at) {
    if(at < 0 || at >= row->size) return;
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editorUpdateRow(row);
    E.dirty++;
}

/**************************************************************  editor operations  **************************************************************/
//function to add a new character at the cursor position, it doesn't have to worry about the details of modifying erow
void editorInsertChar(int c) {
    //first check if the cursor is at the next line of the eof
    if(E.cy == E.numrows) editorInsertRow(E.numrows, "", 0);
    editorRowInsertChar(&E.row[E.cy], E.cx, c);
    E.cx++;
}

//function to insert a newline
void editorInsertNewLine() {
    if(E.cx == 0) editorInsertRow(E.cy, "", 0);
    else {
        erow *row = &E.row[E.cy];
        editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
        row = &E.row[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
    }
    E.cy++;
    E.cx = 0;
}

//function to delete a character from the cursor position
void editorDelChar() {
    if(E.cy == E.numrows) return;
    if(E.cx == 0 && E.cy == 0) return;

    erow *row = &E.row[E.cy];
    if(E.cx > 0) {
        editorRowDelChar(row, E.cx - 1);
        E.cx--;
    } else {
        E.cx = E.row[E.cy - 1].size;
        editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
        editorDelRow(E.cy);
        E.cy--;
    }
}


/**************************************************************       file io       **************************************************************/
//function for converting rows to a single string
//the function returns a pointer to the dynamically allocated character array that stores the file
void* editorRowsToString(void *arg_p) {
    int *my_arg = arg_p;
    int fd = my_arg[0], my_start = my_arg[1] * THREAD_RANGE, my_end = my_start + THREAD_RANGE;
    int my_seekpos = my_arg[2], my_len = my_arg[3], j;
    if(my_end > E.numrows) my_end = E.numrows;
    char *buf = malloc(my_len), *p = buf;

    for(j = my_start; j < my_end; j++) {
        memcpy(p, E.row[j].chars, E.row[j].size);
        p += E.row[j].size;
        *p = '\n';
        p++;
    }

    pthread_mutex_lock(&E.file_mutex);
    lseek(fd, my_seekpos, SEEK_SET);
    write(fd, buf, my_len);
    pthread_mutex_unlock(&E.file_mutex);
    free(buf);
    return NULL;
}

//function for opening and reading files from disk
void editorOpen(char *filename) {
    free(E.filename);
    E.filename = strdup(filename);

    editorSelectSyntaxHighlight(NULL);

    FILE *fp = fopen(filename, "r");
    if(!fp) die("open");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    pthread_t **thread_p = NULL;
    int **thread_arg = NULL;
    int lineidx = -1, thd_cnt = 0;

    while((linelen = getline(&line, &linecap, fp)) != -1) {
        while(linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) linelen--;

        E.numrows++, lineidx++;
        pthread_rwlock_wrlock(&E.row_lock);
        E.row = realloc(E.row, sizeof(erow) * E.numrows);
        pthread_rwlock_unlock(&E.row_lock);
        E.row[lineidx].chars = malloc(linelen + 1);
        memcpy(E.row[lineidx].chars, line, linelen);
        E.row[lineidx].chars[linelen] = '\0';
        E.row[lineidx].idx = lineidx;
        E.row[lineidx].size = linelen;

        E.row[lineidx].rsize = 0;
        E.row[lineidx].render = NULL;
        E.row[lineidx].hl = NULL;
        E.row[lineidx].hl_open_comment = 0;

        if(lineidx % THREAD_RANGE == (THREAD_RANGE - 1)) {
            thd_cnt++;
            thread_p = realloc(thread_p, sizeof(pthread_t *) * thd_cnt);
            thread_arg = realloc(thread_arg, sizeof(int *) * thd_cnt);
            thread_p[thd_cnt - 1] = malloc(sizeof(pthread_t));
            thread_arg[thd_cnt - 1] = malloc(sizeof(int));
            *thread_arg[thd_cnt - 1] = thd_cnt - 1;
            if(pthread_create(thread_p[thd_cnt - 1], NULL, editorUpdateRowParallel, (void *)thread_arg[thd_cnt - 1]) != 0)
                die("ThreadCreate");
        }
    }

    if(lineidx % THREAD_RANGE < THREAD_RANGE - 1) {
        thd_cnt++;
        thread_p = realloc(thread_p, sizeof(pthread_t *) * thd_cnt);
        thread_arg = realloc(thread_arg, sizeof(int *) * thd_cnt);
        thread_p[thd_cnt - 1] = malloc(sizeof(pthread_t));
        thread_arg[thd_cnt - 1] = malloc(sizeof(int));
        *thread_arg[thd_cnt - 1] = thd_cnt - 1;
        if(pthread_create(thread_p[thd_cnt - 1], NULL, editorUpdateRowParallel, (void *)thread_arg[thd_cnt - 1]) != 0)
                die("ThreadCreate");
    }

    int i;
    for(i = 0; i < thd_cnt; i++) {
        pthread_join(*thread_p[i], NULL);
        free(thread_p[i]), free(thread_arg[i]);
    }

    for(i = 0; i < E.numrows; )
        i = editorUpdateSyntax(&E.row[i]) + 1;

    free(line), free(thread_p), free(thread_arg);
    fclose(fp);
}

//function to save the currently opened file
void editorSave() {
    pthread_t editorSelectSyntaxHighlight_thd;
    int highlight_thd = 0;

    if(E.filename == NULL) {
        E.filename = editorPrompt("Save as: %s (ESC to cancel)", NULL);
        if(E.filename == NULL) {
            editorSetStatusMessage("Save aborted");
            return;
        }
        pthread_create(&editorSelectSyntaxHighlight_thd, NULL, editorSelectSyntaxHighlight, NULL);
        highlight_thd = 1;
    }

    if(E.numrows == 0) {
        editorSetStatusMessage("0 bytes written to disk");
        if(highlight_thd) pthread_join(editorSelectSyntaxHighlight_thd, NULL);
        return;
    }

    int *len = malloc(E.numrows * sizeof(int)), j;
    len[0] = E.row[0].size + 1;
    for(j = 1; j < E.numrows; j++)
        len[j] = len[j - 1] + E.row[j].size + 1;

    //because we are creating a new file, we would have to pass on permission for the file (here it is 0644)
    int fd = open(E.filename, O_RDWR | O_CREAT, 0644);

    if(fd != -1) {
        if(ftruncate(fd, len[E.numrows - 1]) != -1) {
            int thread_cnt = (E.numrows + (THREAD_RANGE - 1)) / THREAD_RANGE;
            pthread_t *thread_p = malloc(thread_cnt * sizeof(pthread_t));
            int (*arg_p)[4] = malloc(4 * sizeof(int) * thread_cnt), i;

            for(i = 0; i < thread_cnt; i++) {
                arg_p[i][0] = fd, arg_p[i][1] = i, arg_p[i][2] = i ? len[THREAD_RANGE * i - 1] : 0;
                if(THREAD_RANGE * (i + 1) >= E.numrows)
                    arg_p[i][3] = i == 0 ? len[E.numrows - 1] : len[E.numrows - 1] - len[THREAD_RANGE * i - 1];
                else arg_p[i][3] = i == 0 ? len[THREAD_RANGE - 1] : len[THREAD_RANGE * (i + 1) - 1] - len[THREAD_RANGE * i - 1];
                
                if(pthread_create(&thread_p[i], NULL, editorRowsToString, (void *)&arg_p[i]) != 0)
                    die("ThreadCreate");
            }

            for(i = 0; i < thread_cnt; i++)
                pthread_join(thread_p[i], NULL);
    
            E.dirty = 0, close(fd);
            editorSetStatusMessage("%d bytes written to disk", len[E.numrows - 1]);
            free(thread_p), free(arg_p), free(len);
            if(highlight_thd) pthread_join(editorSelectSyntaxHighlight_thd, NULL);
            return;
        }
        close(fd);
    }
    free(len);
    if(highlight_thd) pthread_join(editorSelectSyntaxHighlight_thd, NULL);
    editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
    //here strerror() function prints the error message corresponding to errno
}


/**************************************************************         find        **************************************************************/
//function to find the last occurrence of a text (of length lent) in a given pattern (of length lenp)
char* strrstr(char *text, int lent, char *pattern, int lenp) {
    int i, j;
    for(i = lent - lenp; i >= 0; i--) {
        for(j = 0; j < lenp; j++)
            if(text[i + j] != pattern[j]) break;
        if(j == lenp) return text + i;
    }
    return NULL;
}

//callback function for search used in call to editorPrompt
void editorFindCallback(char *query, int key) {
    //static variable to control search of a pattern within the file
    //last_match_row stores the line of previous match (-1 if no such line), and direction stores the direction to search (forward/backward)
    //last_match_col stores the starting column of previous match (-1 if no such value)
    static int last_match_row = 0;
    static int last_match_col = -1;
    static int direction = 1;

    //static variables to store the color information of searched text so that it can be restored later after the search
    static int saved_hl_line;
    static char *saved_hl = NULL;

    //if there is any saved color information restore it
    if(saved_hl) {
        memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
        free(saved_hl);
        saved_hl = NULL;
    }

    if(key == '\r' || key == '\x1b') {
        last_match_row = 0;
        last_match_col = -1;
        direction = 1;
        return;
    } else if(key == ARROW_RIGHT || key == ARROW_DOWN) direction = 1;
    else if(key == ARROW_LEFT || key == ARROW_UP) direction = -1;
    else {
        last_match_row = 0;
        last_match_col = -1;
        direction = 1;
    }

    int current_row = last_match_row, current_col = last_match_col + direction, qlen = strlen(query);
    if((direction == 1 && current_col < E.row[current_row].rsize) || (direction == -1 && current_col >= 0)) {
        erow *row = &E.row[current_row];
        char *match; 

        if(direction == 1) {
            if((match = strstr(row->render + current_col, query)))
                last_match_col = match - row->render;
        } else {
            if((match = strrstr(row->render, current_col + qlen, query, qlen)))
                last_match_col = match - row->render;
        }

        if(match) {
            E.cy = last_match_row;
            E.cx = editorRowRxToCx(row, last_match_col);
            E.rowoff = E.numrows;
            
            //highlighting the searched text after saving the current colour info
            saved_hl_line = current_row;
            saved_hl = malloc(row->rsize);
            memcpy(saved_hl, row->hl, row->rsize);
            memset(&row->hl[last_match_col], HL_MATCH, strlen(query));
            return;
        }
    }

    int current = last_match_row, i;
    for(i = 0; i < E.numrows; i++) {
        current += direction;

        //the search wraps around the file if we have reached end or the starting of the file
        if(current == -1) current = E.numrows - 1;
        else if(current == E.numrows) current = 0;

        erow *row = &E.row[current];
        char *match;
        if(direction == 1) match = strstr(row->render, query);
        else match = strrstr(row->render, row->rsize, query, qlen);

        if(match) {
            last_match_row = current;
            last_match_col = match - row->render;
            E.cy = current;
            E.cx = editorRowRxToCx(row, last_match_col);
            E.rowoff = E.numrows;

            //highlighting the searched text after saving the current colour info
            saved_hl_line = current;
            saved_hl = malloc(row->rsize);
            memcpy(saved_hl, row->hl, row->rsize);
            memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
            break;
        }
    }
}

//function to search for a word in the editor
void editorFind() {
    //saving the current position of the cursor, so as to restore it incase user cancels the search
    int saved_cx = E.cx;
    int saved_cy = E.cy;
    int saved_coloff = E.coloff;
    int saved_rowoff = E.rowoff;
    
    //prompting for input for search, and performing the search
    char *query = editorPrompt("Search: %s (Use ESC/ARROW/Enter)", editorFindCallback);

    //if search was successfully completed, freeing the user's input buffer else restoring the cursor position from stored info
    if(query) free(query);
    else {
        E.cx = saved_cx;
        E.cy = saved_cy;
        E.coloff = saved_coloff;
        E.rowoff = saved_rowoff;
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

void abJoin(struct abuf *abf, struct abuf *abs) {
    abAppend(abf, abs->b, abs->len);
    abFree(abs);
}

/**************************************************************       output        **************************************************************/
//function to keep track of the row and column numbers corresponding to the top and left edges of the screen
void editorScroll() {
    //setting E.rx to its correct value from the values of E.cx and E.cy
    E.rx = 0;
    if(E.cy < E.numrows)
        E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);

    if(E.cy < E.rowoff) E.rowoff = E.cy;
    if(E.cy >= E.rowoff + E.screenrows) E.rowoff = E.cy - E.screenrows + 1;
    if(E.rx < E.coloff) E.coloff = E.rx;
    if(E.rx >= E.coloff + E.screencols) E.coloff = E.rx - E.screencols + 1;
}

//function to add the text to display on the screen to a abuf buffer
void *editorDrawRow(void *arg_p) {
    int y = *((int *)arg_p), filerow = y + E.rowoff;
    struct abuf *ab = malloc(sizeof(struct abuf));
    ab->b = NULL, ab->len = 0;

    if(filerow >= E.numrows) {
        //if we have reached past the end of the current file, draw '~' on the left end of the rows

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
        //if we have not reached the end of the file append the current row to the abuf buffer

        int len = E.row[filerow].rsize - E.coloff;
        if(len < 0) len = 0;
        if(len > E.screencols) len = E.screencols;

        //checking if a character is a digit, if so colouring the digit with a different colour
        char *c = &E.row[filerow].render[E.coloff];
        unsigned char *hl = &E.row[filerow].hl[E.coloff];

        int j, current_color = -1;      //to keep track of current color so as to minimise the number of colour updates (-1 means color of normal text)

        for(j = 0; j < len; j++) {
            if(iscntrl(c[j])) {
                char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                abAppend(ab, "\x1b[7m", 4);     //switching to inverted colours
                abAppend(ab, &sym, 1);
                abAppend(ab, "\x1b[m", 3);      //restoring the normal colour
                
                //previous statement turns off all the previous text formatting, including colours, so we will restore them
                if(current_color != -1) {
                    char buf[16];
                    int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
                    abAppend(ab, buf, clen);
                }
            } else if(hl[j] == HL_NORMAL) {
                if(current_color != -1) {
                    abAppend(ab, "\x1b[39m", 5);
                    current_color = -1;
                }
                abAppend(ab, &c[j], 1);
            } else {
                int color = editorSyntaxToColor(hl[j]);
                if(color != current_color) {
                    current_color = color;
                    char buf[16];
                    int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
                    abAppend(ab, buf, clen);
                }
                abAppend(ab, &c[j], 1);
            }
        }
        abAppend(ab, "\x1b[39m", 5);
    }
    abAppend(ab, "\x1b[K\r\n", 5);
    return (void *)ab;
}

//function to add the text to display on the screen to a abuf buffer
void *editorDrawRows(void *arg_p) {
    struct abuf *ab = (struct abuf*)arg_p;
    pthread_t drawRow_thd[E.screenrows];
    int y, arg[E.screenrows];

    for(y = 0; y < E.screenrows; y++) {
        arg[y] = y;
        pthread_create(&drawRow_thd[y], NULL, editorDrawRow, (void *)&arg[y]);
    }

    for(y = 0; y < E.screenrows; y++) {
        void *ab_p;
        pthread_join(drawRow_thd[y], &ab_p);
        abJoin(ab, (struct abuf *)ab_p);
        free((struct abuf *)ab_p);
    }

    return NULL;
}

//function to display the status bar
void *editorDrawStatusBar(void *arg_p) {
    struct abuf *ab = arg_p;
    abAppend(ab, "\x1b[7m", 4);      //for dispalying the status bar with inverted colours
    
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s", E.filename ? E.filename : "[No Name]", E.numrows, E.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d/%d", E.syntax ? E.syntax->filetype : "no fit", E.cy + 1, E.numrows);
    if(len > E.screencols) len = E.screencols;
    abAppend(ab, status, len);
    while(len < E.screencols) {
        if(E.screencols - len == rlen) {
            abAppend(ab, rstatus, rlen);
            break;
        } else {
            abAppend(ab, " ", 1);
            len++;
        }
    }
    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);

    return NULL;
}

//function to display the message bar
void *editorDrawMessageBar(void *arg_p) {
    struct abuf *ab = arg_p;
    abAppend(ab, "\x1b[K", 3);
    int msglen = strlen(E.statusmsg);
    if(msglen > E.screencols) msglen = E.screencols;
    if(msglen && time(NULL) - E.statusmsg_time < 5)
        abAppend(ab, E.statusmsg, msglen);
    return NULL;
}

//function to refresh the screen after each keypress
void editorRefreshScreen() {
    pthread_t drawRows_thd, drawStatusBar_thd, drawMessageBar_thd;
    struct abuf drawRows_ab = ABUF_INIT;
    struct abuf drawStatusBar_ab = ABUF_INIT;
    struct abuf drawMessageBar_ab = ABUF_INIT;

    //hides the cursor and then reposition the cursor to the beginning of the screen
    abAppend(&drawRows_ab, "\x1b[?25l\x1b[H", 9);
    //call editorScroll() to determine the part of the file to be shown on the screen
    editorScroll();

    //call editorDrawRows() to write the text/tilde in abuf structure to be displayed on the screen
    pthread_create(&drawRows_thd, NULL, editorDrawRows, (void *)&drawRows_ab);
    //call editorDrawStatusBar() to draw the status bar
    pthread_create(&drawStatusBar_thd, NULL, editorDrawStatusBar, (void *)&drawStatusBar_ab);
    //call editorDrawMessageBar() to draw the message bar
    pthread_create(&drawMessageBar_thd, NULL, editorDrawMessageBar, (void *)&drawMessageBar_ab);

    pthread_join(drawRows_thd, NULL);
    pthread_join(drawStatusBar_thd, NULL);
    pthread_join(drawMessageBar_thd, NULL);

    abJoin(&drawRows_ab, &drawStatusBar_ab);
    abJoin(&drawRows_ab, &drawMessageBar_ab);

    //reposition the cursor
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy - E.rowoff + 1, E.rx - E.coloff + 1);
    abAppend(&drawRows_ab, buf, strlen(buf));
    //again turn the cursor on
    abAppend(&drawRows_ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, drawRows_ab.b, drawRows_ab.len);
    abFree(&drawRows_ab);
}

//general function to print the status message 
void editorSetStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}

/**************************************************************        input        **************************************************************/
//function to prompt the user for an input. The string to be displayed as prompt is passed as an argument
//'prompt' is supposed to be a format string containing %s, where user's input will be displayed
char* editorPrompt(char *prompt, void (*callback)(char *, int)) {
    size_t bufsize = 128;
    char *buf = malloc(bufsize);

    size_t buflen = 0;
    buf[0] = '\0';

    while(1) {
        editorSetStatusMessage(prompt, buf);
        editorRefreshScreen();

        int c = editorReadKey();
        if(c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
            if(buflen != 0) buf[--buflen] = '\0';
        } else if(c == '\x1b') {
            editorSetStatusMessage("");
            if(callback) callback(buf, c);
            free(buf);
            return NULL;
        } else if(c == '\r') {
            if(buflen != 0) {
                editorSetStatusMessage("");
                if(callback) callback(buf, c);
                return buf;
            }
        } else if(!iscntrl(c) && c < 128) {
            if(buflen == bufsize - 1) {
                bufsize *= 2;
                buf = realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
        }

        if(callback) callback(buf, c);
    }
}

//function to move the cursor on screen using wsad keys
void editorMoveCursor(int key) {
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

    switch(key) {
        case ARROW_LEFT:
            if(E.cx != 0) E.cx--;
            else if(E.cy > 0) {
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;
        case ARROW_RIGHT:
            if(row && E.cx < row->size) E.cx++;
            else if(row && E.cx == row->size) {
                E.cy++;
                E.cx = 0;
            }
            break;
        case ARROW_UP:
            if(E.cy != 0) E.cy--;
            break;
        case ARROW_DOWN:
            if(E.cy < E.numrows) E.cy++;
            break;
    }

    //we need to find 'row' again because our 'ey' might have changed
    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if(E.cx > rowlen) E.cx = rowlen;
}

//function to process keypresses
void editorProcessKeypress() {
    static int quit_times = KILO_QUIT_TIMES;
    int c = editorReadKey();

    switch(c) {
        case '\r':
            editorInsertNewLine();
            break;

        case CTRL_KEY('q'):
                
            if(E.dirty && quit_times > 0) {
                editorSetStatusMessage("Warning!!! File has unsaved changes. Press Ctrl-Q %d more times to quit", quit_times);
                quit_times--;
                return;
            }
            write(STDOUT_FILENO, "\x1b[2J", 4);     //clear the terminal screen
            write(STDOUT_FILENO, "\x1b[H", 3);      //reposition the cursor to the beginning of the screen
            exit(0);
            break;

        case CTRL_KEY('s'):
            editorSave();
            break;
        
        case HOME_KEY:
            E.cx = 0;
            break;

        case END_KEY:
            if(E.cy < E.numrows)
                E.cx = E.row[E.cy].size;
            break;

        case CTRL_KEY('f'):
            editorFind();
            break;

        case BACKSPACE:                             //BACKSPACE is mapped to ASCII 127
        case CTRL_KEY('h'):                         //CTRL_KEY('h') is ASCII 8, which was originally for BACKSPACE
        case DEL_KEY:                               //DEL_KEY is mapped to <esc>[3~
            if(c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
            editorDelChar();
            break;

        case PAGE_UP:
        case PAGE_DOWN:
            {
                if(c == PAGE_UP) E.cy = E.rowoff;
                else if(c == PAGE_DOWN) {
                    E.cy = E.rowoff + E.screenrows - 1;
                    if(E.cy > E.numrows) E.cy = E.numrows;
                }
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

        case CTRL_KEY('l'):                         //we would be ignoring CTRL_KEY('l') and ESCAPE as the screen refreshes (the tradational use of these keys) after any keypress
        case '\x1b':
            break;

        default:
            editorInsertChar(c);
            break;
    }

    quit_times = KILO_QUIT_TIMES;
}


/**************************************************************        init         **************************************************************/
//function to initialise all the fields in strucutre E for the editor
void *initEditor(void *arg_p) {
    UNUSED(arg_p);

    E.cx = E.cy = E.rx = E.numrows = E.rowoff = E.coloff = E.statusmsg_time = E.dirty = 0;
    E.row = NULL, E.filename = NULL, E.statusmsg[0] = '\0';
    E.syntax = NULL;
    if(getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
    E.screenrows -= 2;
    pthread_rwlock_init(&E.row_lock, NULL);
    pthread_mutex_init(&E.file_mutex, NULL);

    return NULL;
}


/**************************************************************        main         **************************************************************/
int main(int argc, char **argv) {

    //initialising the editor and setting the terminal
    pthread_t enableRawMode_thd, initEditor_thd;
    pthread_create(&enableRawMode_thd, NULL, enableRawMode, NULL);
    pthread_create(&initEditor_thd, NULL, initEditor, NULL);
    pthread_join(enableRawMode_thd, NULL);
    pthread_join(initEditor_thd, NULL);

    if(argc >= 2) editorOpen(argv[1]);
    editorSetStatusMessage("Help: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

    while(1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    
    return 0;
}