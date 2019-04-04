/**************************************************************       includes      **************************************************************/
//feature test macros
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE


#include<ctype.h>
#include<errno.h>
#include<fcntl.h>
#include<stdarg.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/ioctl.h>
#include<sys/types.h>
#include<sys/time.h>
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
#define HL_HIGHLIGHT_SPELLCHECK (1 << 2)

enum editorKey { BACKSPACE = 127, ARROW_LEFT = 1000, ARROW_RIGHT, ARROW_UP, ARROW_DOWN, DEL_KEY, HOME_KEY, END_KEY, PAGE_UP, PAGE_DOWN };
enum editorHighlight { HL_NORMAL = 0, HL_COMMENT, HL_MLCOMMENT, HL_KEYWORD1, HL_KEYWORD2, HL_STRING, HL_NUMBER, HL_MATCH };


/**************************************************************         data        **************************************************************/
//structure type to store syntax highlighting info for the current file
struct editorSyntax {
    char *filetype;     //filetype that will be displayed in the status bar
    char **filematch;   //it is an array of strings that contains a pattern to match a filename against
    char **keywords;    //it is an array of strings that contains common C keywords and datatypes
    char *singleline_comment_start; //stores the starting characters of a single line comment
    char *multiline_comment_start;  //stores the starting characters of a multi line string
    char *multiline_comment_end;    //stores the ending characters of a multi line string
    int flags;          //it is a bit field that will contain flags for whether to highlight numbers and whether to highlight strings for that filetype
};

//datatype to store a row of text
typedef struct erow {
    int size, rsize, idx;              //idx is the index of the row within the file
    char *chars, *render;              //pointer to a dynamically allocated character array representing actual row of text and the text to display
    unsigned char *hl;                 //pointer to an array storing the syntax highlighting details of each character of the current row
    int *spell_ch;                     //pointer to an array that stores the spell checking info of the file
    int hl_open_comment;               //variable indicating whether the current row ended with an unclosed multiline comment
} erow;

//structure for node used to maintain tree for spell checking
struct spellCheckTreeNode {
    int isWord;
    struct spellCheckTreeNode *ptr[26];
};

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
    int dirty;                         //shows whether the currently opened file in the editor has been modified or not
    char *filename;                    //file currently opened in the text editor
    char statusmsg[80];
    time_t statusmsg_time;
    struct editorSyntax *syntax;       //structure to store the syntax highlighting info
    struct termios orig_termios;       //we will store the original terminal configurations
    struct spellCheckTreeNode *spellTree;
} E;


/**************************************************************      file types     **************************************************************/
char *C_HL_extensions[] = { ".c", ".h", ".cpp", NULL };
char *C_HL_keywords[] = { "switch", "if", "while", "for", "break", "continue", "return", "else", "struct", "union", "typedef", "static", "enum", "class", "case", 
                         "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|", "void|", NULL };

char *TXT_HL_extensions[] = { ".txt", NULL };
char *TXT_HL_keywords[] = { NULL };
struct editorSyntax HLDB[] = { { "c", C_HL_extensions, C_HL_keywords, "//", "/*", "*/", (HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS) },
                               { "txt", TXT_HL_extensions, TXT_HL_keywords, NULL, NULL, NULL, (HL_HIGHLIGHT_SPELLCHECK) } };

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


/**************************************************************    spell checker    **************************************************************/
//function to initialise a structure variable of type spellCheckTreeNode
void spellCheckTreeNodeInit(struct spellCheckTreeNode *node) {
    node->isWord = 0;
    int i;
    for(i = 0; i < 26; i++)
        node->ptr[i] = NULL;
}

//function to add a new word to the spell checker tree
void spellCheckTreeAddWord(struct spellCheckTreeNode *node, char *word) {
    if((*word) == '\0') node->isWord = 1;
    else {
        int child = tolower(*word) - 'a';
        if(node->ptr[child] == NULL) {
            node->ptr[child] = malloc(sizeof(struct spellCheckTreeNode));
            spellCheckTreeNodeInit(node->ptr[child]);
        }
        spellCheckTreeAddWord(node->ptr[child], word + 1);
    }
}

//function to initialise the spell checker tree with words from words.txt
void spellCheckTreeInit() {
    FILE *fptr = fopen("words.txt", "r");
    char *word = NULL;
    size_t wordcap = 0;
    ssize_t wordlen;
    E.spellTree = malloc(sizeof(struct spellCheckTreeNode));
    spellCheckTreeNodeInit(E.spellTree);
    while((wordlen = getline(&word, &wordcap, fptr)) != -1) {
        word[wordlen - 1] = '\0';
        spellCheckTreeAddWord(E.spellTree, word);
    }
    fclose(fptr);
}

//function to delete the spell checker tree
void spellCheckTreeDelete(struct spellCheckTreeNode *node) {
    if(node == NULL) return;
    int i;
    for(i = 0; i < 26; i++)
        spellCheckTreeDelete(node->ptr[i]);
    free(node);
}

//function to check whether a word exists in the spell checker tree
int spellCheckTreeCheckWord(struct spellCheckTreeNode *node, char *word) {
    if(node == NULL) return 0;
    if((*word) == '\0') return node->isWord;
    return spellCheckTreeCheckWord(node->ptr[tolower(*word) - 'a'], word + 1);
}

//function to update the 'spell_ch' array for a row
void editorUpdateSpellCheck(erow *row) {
    row->spell_ch = realloc(row->spell_ch, row->rsize * sizeof(int));
    memset(row->spell_ch, 0, row->rsize * sizeof(int));          //giving a default value that there is no syntax error for now

    if(E.syntax == NULL || (E.syntax->flags & HL_HIGHLIGHT_SPELLCHECK) == 0) return;    //check if spell checking is to be done

    int i = 0;
    char *word = malloc(row->rsize);
    while(i < row->rsize) {
        int j, k;
        for(j = i; j < row->rsize && isalpha(row->render[j]); j++)
            word[j - i] = row->render[j];
        word[j - i] = '\0';
        if(!spellCheckTreeCheckWord(E.spellTree, word))
            for(k = i; k < j; k++)
                row->spell_ch[k] = 1;
        i = j + 1;
    }
    free(word);
}


/************************************************************** syntax highlighting **************************************************************/
//function that returns boolean value of whether c is a separator or not
int is_separator(int c) {
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

//function to update the 'hl' array for a row
void editorUpdateSyntax(erow *row) {
    row->hl = realloc(row->hl, row->rsize);
    memset(row->hl, HL_NORMAL, row->rsize); //giving a default highlight value to all the charachters

    if(E.syntax == NULL) return;    //check if any syntax highlighting is to be done

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
    editorUpdateSpellCheck(row);

    //updating the 'hl_open_comment' field of the current row
    //also checking if the current row was changed from being commented to uncommented and vice versa
    //if so calling the function recursively for the next row (as such commenting could effect multiple rows, so we should update all such rows not just the current row)
    int changed = (row->hl_open_comment != in_comment);
    row->hl_open_comment = in_comment;
    if(changed && row->idx + 1 < E.numrows)
        editorUpdateSyntax(&E.row[row->idx + 1]);
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
void editorSelectSyntaxHighlight() {
    E.syntax = NULL;
    if(E.filename == NULL) return;

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
                for(filerow = 0; filerow < E.numrows; filerow++)
                    editorUpdateSyntax(&E.row[filerow]);
                
                return;
            }
            i++;
        }
    }
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

//this function will insert a new row for a string string to the E.row array
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
    E.row[at].spell_ch = NULL;
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
    free(row->spell_ch);
    free(row->hl);
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
char* editorRowsToString(int *buflen) {
    int totlen = 0;
    int j;
    for(j = 0; j < E.numrows; j++)
        totlen += E.row[j].size + 1;
    *buflen = totlen;

    char *buf = malloc(totlen);
    char *p = buf;
    for(j = 0; j < E.numrows; j++) {
        memcpy(p, E.row[j].chars, E.row[j].size);
        p += E.row[j].size;
        *p = '\n';
        p++;
    }

    return buf;
}

//function for opening and reading files from disk
void editorOpen(char *filename) {
    FILE *logFile = fopen("log.txt", "a");
    struct timeval start_time, end_time; 
    gettimeofday(&start_time, NULL);

    free(E.filename);
    E.filename = strdup(filename);

    editorSelectSyntaxHighlight();

    FILE *fp = fopen(filename, "r");
    if(!fp) die("open");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while((linelen = getline(&line, &linecap, fp)) != -1) {
        while(linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) linelen--;
        editorInsertRow(E.numrows, line, linelen);
    }
    free(line);
    fclose(fp);
    E.dirty = 0;

    gettimeofday(&end_time, NULL);
    double time_taken;
    time_taken = (end_time.tv_sec - start_time.tv_sec) * 1e6; 
    time_taken = (time_taken + (end_time.tv_usec - start_time.tv_usec)) * 1e-6;

    fprintf(logFile, "Filename : %s, Open time : %.4f\n", E.filename, time_taken);
    fclose(logFile);
}

//function to save the currently opened file
void editorSave() {
    FILE *logFile = fopen("log.txt", "a");
    struct timeval start_time, end_time; 
    gettimeofday(&start_time, NULL);

    if(E.filename == NULL) {
        E.filename = editorPrompt("Save as: %s (ESC to cancel)", NULL);
        if(E.filename == NULL) {
            editorSetStatusMessage("Save aborted");
            fclose(logFile);
            return;
        }
        editorSelectSyntaxHighlight();
    }

    int len;
    char *buf = editorRowsToString(&len);

    int fd = open(E.filename, O_RDWR | O_CREAT, 0644);  //because we are creating a new file, we would have to pass on permission for the file (here it is 0644)
    if(fd != -1) {
        if(ftruncate(fd, len) != -1)
            if(write(fd, buf, len) == len) {
                close(fd);
                free(buf);
                E.dirty = 0;
                editorSetStatusMessage("%d bytes written to disk", len);

                gettimeofday(&end_time, NULL);
                double time_taken;
                time_taken = (end_time.tv_sec - start_time.tv_sec) * 1e6; 
                time_taken = (time_taken + (end_time.tv_usec - start_time.tv_usec)) * 1e-6;
                fprintf(logFile, "Filename : %s, Save time : %.4f\n", E.filename, time_taken);
                fclose(logFile);
                return;
            }
        close(fd);
    }

    free(buf);
    editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));   //here strerror() function prints the error message corresponding to errno
}


/**************************************************************         find        **************************************************************/
//callback function for search used in call to editorPrompt
void editorFindCallback(char *query, int key) {
    //static variable to control search of a pattern within the file
    //last_match stores the line of previous match (-1 if no such line), and direction stores the direction to search (forward/backward)
    static int last_match = -1;
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
        last_match = -1;
        direction = 1;
        return;
    } else if(key == ARROW_RIGHT || key == ARROW_DOWN) direction = 1;
    else if(key == ARROW_LEFT || key == ARROW_UP) direction = -1;
    else {
        last_match = -1;
        direction = 1;
    }

    if(last_match == -1) direction = 1;
    int current = last_match, i;

    for(i = 0; i < E.numrows; i++) {
        current += direction;

        //the search wraps around the file if we have reached end or the starting of the file
        if(current == -1) current = E.numrows - 1;
        else if(current == E.numrows) current = 0;

        erow *row = &E.row[current];
        char *match = strstr(row->render, query);
        if(match) {
            last_match = current;
            E.cy = current;
            E.cx = editorRowRxToCx(row, match - row->render);
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


/**************************************************************       output        **************************************************************/
//function to keep track of the row number corresponding to the top of the screen
void editorScroll() {
    //setting E.rx to its correct value
    E.rx = 0;
    if(E.cy < E.numrows)
        E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);

    if(E.cy < E.rowoff) E.rowoff = E.cy;
    if(E.cy >= E.rowoff + E.screenrows) E.rowoff = E.cy - E.screenrows + 1;
    if(E.rx < E.coloff) E.coloff = E.rx;
    if(E.rx >= E.coloff + E.screencols) E.coloff = E.rx - E.screencols + 1;
}

//function to add the text to display on the screen to a abuf buffer
void editorDrawRows(struct abuf *ab) {
    int y;
    for(y = 0; y < E.screenrows; y++) {
        int filerow = y + E.rowoff;
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
            int *spell_ch = &E.row[filerow].spell_ch[E.coloff];

            int j, current_color = -1;      //to keep track of current color so as to minimise the number of colour updates (-1 means color of normal text)
                int prev_spell_ch = 0, spell_chk = (spell_ch != NULL);
            for(j = 0; j < len; j++) {
                if(spell_chk && *spell_ch != prev_spell_ch) {
                    if(*spell_ch == 0) abAppend(ab, "\x1b[24m", 5);
                    else abAppend(ab, "\x1b[4m", 5);
                    prev_spell_ch = *spell_ch;
                }
                spell_ch++;

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
            abAppend(ab, "\x1b[24m", 5);
            abAppend(ab, "\x1b[39m", 5);
        }

        abAppend(ab, "\x1b[K\r\n", 5);
    }
}

//function to display the status bar
void editorDrawStatusBar(struct abuf *ab) {
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
}

//function to display the message bar
void editorDrawMessageBar(struct abuf *ab) {
    abAppend(ab, "\x1b[K", 3);
    int msglen = strlen(E.statusmsg);
    if(msglen > E.screencols) msglen = E.screencols;
    if(msglen && time(NULL) - E.statusmsg_time < 5)
        abAppend(ab, E.statusmsg, msglen);
}

//function to refresh the screen after each keypress
void editorRefreshScreen() {
    editorScroll();

    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);   //hides the cursor
    abAppend(&ab, "\x1b[H", 3);      //reposition the cursor to the beginning of the screen
    
    editorDrawRows(&ab);             //call editorDrawRows() to draw the tilde on the screen
    editorDrawStatusBar(&ab);        //call editorDrawStatusBar() to draw the status bar
    editorDrawMessageBar(&ab);       //call editorDrawMessageBar() to draw the message bar

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy - E.rowoff + 1, E.rx - E.coloff + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);   //again turn the cursor on

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

//function to print the status message 
void editorSetStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}

/**************************************************************        input        **************************************************************/
//function to prompt the user for an input. The string to be displayed as prompt is passed as an argument
//'prompt' is supposed to be a format string containing %s, where user input will be displayed
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
        case CTRL_KEY('h'):                         //CTRL_KEY('h') is ASCII 8, which was originally for BAACKSPACE
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
void initEditor() {
    E.cx = E.cy = E.rx = E.numrows = E.rowoff = E.coloff = E.statusmsg_time = E.dirty = 0;
    E.row = NULL, E.filename = NULL, E.statusmsg[0] = '\0';
    E.syntax = NULL, E.spellTree = NULL;
    if(getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
    E.screenrows -= 2;
    spellCheckTreeInit();
}


/**************************************************************        main         **************************************************************/
int main(int argc, char **argv) {
    enableRawMode();
    initEditor();
    if(argc >= 2) editorOpen(argv[1]);
    
    editorSetStatusMessage("Help: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

    while(1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    
    return 0;
}
