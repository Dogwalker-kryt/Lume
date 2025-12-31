/*
 * Lume
 * Copyright (C) 2025 Dogwalker-kryt
 *
*/

// [Indcludes]
#include <ncurses.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>


// [Defines]
#define CTRL_LEFT 554 
#define CTRL_RIGHT 569
// Helper: simple CTRL macro
#ifndef KEY_CTRL
#define CTRL_KEY(k) ((k) & 0x1f)
#endif


// [config structure]
struct EditorConfig {
    int tabSize = 4;
    bool showLineNumbers = true;
    std::unordered_map<std::string, int> keyMap; // action -> key code
};


// [Editor states]
struct EditorState {
    int cx = 0; // cursor x (in characters, not including line number)
    int cy = 0; // cursor y in file (row index)
    int rowOffset = 0; // top row visible
    int colOffset = 0; // left column visible
    int screenRows = 0;
    int screenCols = 0;
    bool quit = false;
    bool dirty = false;
    std::string filename;
    std::vector<std::string> rows;
    EditorConfig config;
};


// [Actions]
enum class Action {
    MOVE_UP,
    MOVE_DOWN,
    MOVE_LEFT,
    MOVE_RIGHT,
    MOVE_WORD_LEFT,
    MOVE_WORD_RIGHT,
    QUIT,
    SAVE,
    UNDO,
    NONE
};


// [Default keybindings (fallback if not set in config)]
void setDefaultKeybindings(EditorConfig &conf) {
    conf.keyMap["quit"] = CTRL_KEY('q');   
    conf.keyMap["save"] = CTRL_KEY('s');
    conf.keyMap["move_up"] = KEY_UP;
    conf.keyMap["move_down"] = KEY_DOWN;
    conf.keyMap["move_left"] = KEY_LEFT;
    conf.keyMap["move_right"] = KEY_RIGHT;
    conf.keyMap["move_word_left"] = CTRL_LEFT; 
    conf.keyMap["move_word_right"] = CTRL_RIGHT;
    conf.keyMap["undo"] = CTRL_KEY('z');
}


// We treat certain strings in config.toml as special keys like "Ctrl-q", "ArrowUp"
int parseKeyString(const std::string &keyStr) {
    if (keyStr == "ArrowUp") return KEY_UP;
    if (keyStr == "ArrowDown") return KEY_DOWN;
    if (keyStr == "ArrowLeft") return KEY_LEFT;
    if (keyStr == "ArrowRight") return KEY_RIGHT;
    if (keyStr == "PageUp") return KEY_PPAGE;
    if (keyStr == "PageDown") return KEY_NPAGE;
    if (keyStr == "Home") return KEY_HOME;
    if (keyStr == "End") return KEY_END;
    if (keyStr == "Ctrl-ArrowLeft") return 554; 
    if (keyStr == "Ctrl-ArrowRight") return 569;

    // Ctrl-X: "Ctrl-q", "Ctrl-s", etc.
    if (keyStr.rfind("Ctrl-", 0) == 0 && keyStr.size() == 6) {
        char c = keyStr[5];
        if (std::isalpha(static_cast<unsigned char>(c))) {
            c = std::tolower(static_cast<unsigned char>(c));
            return CTRL_KEY(c);
        }
    }

    // Single character key
    if (keyStr.size() == 1) {
        return static_cast<unsigned char>(keyStr[0]);
    }

    // Fallback
    return -1;
}


// [moving Cursor with tabs syncronisation]
int computeScreenX(const EditorState &E) {
    if (E.cy >= (int)E.rows.size()) return 0;

    const std::string &row = E.rows[E.cy];
    int screenX = 0;

    for (int i = 0; i < E.cx && i < (int)row.size(); i++) {
        if (row[i] == '\t') {
            int spaces = E.config.tabSize - (screenX % E.config.tabSize);
            screenX += spaces;
        } else {
            screenX++;
        }
    }
    return screenX;
}


// [C/C++ Syntax Highlighting]
bool isKeyword(const std::string &word) {
    static const std::vector<std::string> keywords = {
        "if","else","for","while","switch","case","default","break","continue",
        "return","goto","do","sizeof","typedef","static","const","volatile",
        "inline","struct","class","public","private","protected","virtual",
        "override","template","typename","using","namespace","enum","union",
        "new","delete","this","operator","try","catch","throw"
    };
    return std::find(keywords.begin(), keywords.end(), word) != keywords.end();
}


bool isTypeLike(const std::string &word) {
    static const std::vector<std::string> types = {
        "int","long","short","char","float","double","void","bool",
        "unsigned","signed","auto","std","string","size_t"
    };
    return std::find(types.begin(), types.end(), word) != types.end();
}


void drawHighlightedLine(const std::string &row, int y, int colOffset, int startCol, int maxCols, const EditorState &E) {
    int x = 0;
    int screenCol = startCol;

    bool inString = false;
    char stringChar = 0;
    bool inLineComment = false;

    while (x < (int)row.size() && screenCol - startCol < maxCols) {
        char c = row[x];

        // Skip until visible column
        if (screenCol < startCol + colOffset) {
            if (!inString && !inLineComment && c == '/' && x + 1 < (int)row.size() && row[x+1] == '/') {
                inLineComment = true;
            } else if (!inLineComment && (c == '"' || c == '\'')) {
                inString = true;
                stringChar = c;
            } else if (inString && c == stringChar && (x == 0 || row[x-1] != '\\')) {
                inString = false;
            }
            x++;
            screenCol++;
            continue;
        }

        // Comments
        if (!inString && !inLineComment && c == '/' && x + 1 < (int)row.size() && row[x+1] == '/') {
            inLineComment = true;
            attron(COLOR_PAIR(4));
            mvaddnstr(y, screenCol, row.c_str() + x,
                      std::min(maxCols - (screenCol - startCol), (int)row.size() - x));
            attroff(COLOR_PAIR(4));
            break;
        }

        // Strings
        if (!inLineComment && (c == '"' || c == '\'')) {
            inString = true;
            stringChar = c;
            attron(COLOR_PAIR(5));
            mvaddch(y, screenCol, c);
            attroff(COLOR_PAIR(5));
            x++;
            screenCol++;
            continue;
        }

        if (inString) {
            attron(COLOR_PAIR(5));
            mvaddch(y, screenCol, c);
            attroff(COLOR_PAIR(5));
            if (c == stringChar && (x == 0 || row[x-1] != '\\')) {
                inString = false;
            }
            x++;
            screenCol++;
            continue;
        }

        // Numbers
        if (std::isdigit((unsigned char)c)) {
            int start = x;
            while (x < (int)row.size() && (std::isdigit((unsigned char)row[x]) || row[x]=='.')) x++;
            int len = x - start;
            attron(COLOR_PAIR(6));
            mvaddnstr(y, screenCol, row.c_str() + start,
                      std::min(len, maxCols - (screenCol - startCol)));
            attroff(COLOR_PAIR(6));
            screenCol += len;
            continue;
        }

        // Identifiers
        if (std::isalpha((unsigned char)c) || c == '_' ) {
            int start = x;
            while (x < (int)row.size() && (std::isalnum((unsigned char)row[x]) || row[x]=='_')) x++;
            int len = x - start;
            std::string word = row.substr(start, len);

            if (isKeyword(word)) {
                attron(COLOR_PAIR(1));
                mvaddnstr(y, screenCol, row.c_str() + start,
                          std::min(len, maxCols - (screenCol - startCol)));
                attroff(COLOR_PAIR(1));
            } else if (isTypeLike(word)) {
                attron(COLOR_PAIR(2));
                mvaddnstr(y, screenCol, row.c_str() + start,
                          std::min(len, maxCols - (screenCol - startCol)));
                attroff(COLOR_PAIR(2));
            } else {
                mvaddnstr(y, screenCol, row.c_str() + start,
                          std::min(len, maxCols - (screenCol - startCol)));
            }
            screenCol += len;
            continue;
        }

        if (c == '\t') {
            // Expand real tab into spaces visually
            int spaces = E.config.tabSize - ((screenCol - startCol) % E.config.tabSize);
            for (int i = 0; i < spaces && screenCol - startCol < maxCols; i++) {
                mvaddch(y, screenCol++, ' ');
            }
            x++;
            continue;
        }

        // Normal character
        mvaddch(y, screenCol, c);
        x++;
        screenCol++;

    }
}

// [Config and Key Management]

// Minimal "TOML-like" parser for our config file
void loadConfig(EditorConfig &conf, const std::string &path) {
    setDefaultKeybindings(conf);

    std::ifstream in(path);
    if (!in) {
        // No config file, keep defaults
        return;
    }

    std::string line;
    std::string section;
    while (std::getline(in, line)) {
        // Trim
        auto trim = [](std::string &s) {
            s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                   [](unsigned char ch){ return !std::isspace(ch); }));
            s.erase(std::find_if(s.rbegin(), s.rend(),
                   [](unsigned char ch){ return !std::isspace(ch); }).base(),
                   s.end());
        };

        trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            trim(section);
            continue;
        }

        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        trim(key);
        trim(value);

        // Remove quotes if present
        if (!value.empty() && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }

        if (section == "options") {
            if (key == "tabsize") {
                conf.tabSize = std::atoi(value.c_str());
            } else if (key == "show_line_numbers") {
                conf.showLineNumbers = (value == "true" || value == "1");
            }
        } else if (section == "keys") {
            int kcode = parseKeyString(key);
            if (kcode != -1) {
                conf.keyMap[value] = kcode;
            }
        }
    }
}


Action mapKeyToAction(const EditorState &E, int key) {
    for (const auto &kv : E.config.keyMap) {
        if (kv.second == key) {
            const std::string &actionName = kv.first;
            if (actionName == "quit") return Action::QUIT;
            if (actionName == "save") return Action::SAVE;
            if (actionName == "move_up") return Action::MOVE_UP;
            if (actionName == "move_down") return Action::MOVE_DOWN;
            if (actionName == "move_left") return Action::MOVE_LEFT;
            if (actionName == "move_right") return Action::MOVE_RIGHT;
            if (actionName == "move_word_left") return Action::MOVE_WORD_LEFT;
            if (actionName == "move_word_right") return Action::MOVE_WORD_RIGHT;
            if (actionName == "undo") return Action::UNDO;

        }
    }
    return Action::NONE;
}


// [Initilisation]
void die(const char *msg) {
    endwin();
    std::perror(msg);
    std::exit(1);
}


void initEditor(EditorState &E, const std::string &configPath) {
    loadConfig(E.config, configPath);

    if (initscr() == nullptr) {
        std::fprintf(stderr, "Failed to init ncurses\n");
        std::exit(1);
    }

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_RED,   -1); // keywords
        init_pair(2, COLOR_CYAN,  -1); // types
        init_pair(3, COLOR_YELLOW,-1); // functions (optional later)
        init_pair(4, COLOR_BLUE,  -1); // comments
        init_pair(5, COLOR_MAGENTA,-1);// strings
        init_pair(6, COLOR_GREEN, -1); // numbers
    }

    raw();
    noecho();
    keypad(stdscr, TRUE);
    // hide cursor while rendering; we'll set later
    curs_set(1);

    getmaxyx(stdscr, E.screenRows, E.screenCols);
    // Reserve one row for status bar
    E.screenRows -= 1;
}


// [Snapshot and Undo Logic]

struct UndoSnapshot {
    std::vector<std::string> rows;
    int cx;
    int cy;
};

std::vector<UndoSnapshot> undoStack;
const size_t UNDO_LIMIT = 100;


void pushUndo(const EditorState &E) {
    UndoSnapshot snap;
    snap.rows = E.rows;
    snap.cx = E.cx;
    snap.cy = E.cy;
    undoStack.push_back(std::move(snap));
    if (undoStack.size() > UNDO_LIMIT) {
        undoStack.erase(undoStack.begin());
    }
}


void undo(EditorState &E) {
    if (undoStack.empty()) return;
    UndoSnapshot snap = undoStack.back();
    undoStack.pop_back();

    E.rows = std::move(snap.rows);
    E.cx = snap.cx;
    E.cy = snap.cy;
    E.dirty = true; // still dirty after undo
}



// [file I/O Logic]
void openFile(EditorState &E, const std::string &filename) {
    E.filename = filename;
    E.rows.clear();

    std::ifstream in(filename);
    if (!in) {
        // File doesn't exist yet: treat as empty buffer
        return;
    }

    std::string line;
    while (std::getline(in, line)) {
        // Remove trailing '\r'
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        E.rows.push_back(line);
    }
}


void saveFile(EditorState &E) {
    if (E.filename.empty()) return;

    std::ofstream out(E.filename);
    if (!out) return; // For now, ignore errors

    for (size_t i = 0; i < E.rows.size(); ++i) {
        out << E.rows[i];
        if (i + 1 < E.rows.size()) out << "\n";
    }
    E.dirty = false;
}


// [Editor Layout and Logic]
void editorScroll(EditorState &E) {
    if (E.cy < E.rowOffset) {
        E.rowOffset = E.cy;
    }
    if (E.cy >= E.rowOffset + E.screenRows) {
        E.rowOffset = E.cy - E.screenRows + 1;
    }

    if (E.cx < E.colOffset) {
        E.colOffset = E.cx;
    }
    if (E.cx >= E.colOffset + E.screenCols) {
        E.colOffset = E.cx - E.screenCols + 1;
    }
}


void drawRows(EditorState &E) {
    int screenRow = 0;
    int lineNumberWidth = 0;

    if (E.config.showLineNumbers) {
        int maxLine = std::max<int>(1, E.rows.size());
        lineNumberWidth = std::to_string(maxLine).size() + 1; // "N " 
    }

    for (int y = 0; y < E.screenRows; y++) {
        int fileRow = E.rowOffset + y;
        move(y, 0);
        clrtoeol();

        if (fileRow >= (int)E.rows.size()) {
            // Empty tilde lines (like vim)
            if (E.rows.empty() && y == E.screenRows / 3) {
                std::string msg = "vLte -- very Light Terminal Editor with 54.5 kb";
                int padding = (E.screenCols - (int)msg.size()) / 2;
                if (padding < 0) padding = 0;
                for (int i = 0; i < padding; ++i) addch('~');
                addstr(msg.c_str());
            } else {
                addch('~');
            }
        } else {
            // Line number
            if (E.config.showLineNumbers) {
                char ln[32];
                std::snprintf(ln, sizeof(ln), "%*d ", lineNumberWidth - 1, fileRow + 1);
                attron(A_DIM);
                addstr(ln);
                attroff(A_DIM);
            }

            const std::string &row = E.rows[fileRow];
            int len = (int)row.size() - E.colOffset;
            if (len < 0) len = 0;
            if (len > E.screenCols - lineNumberWidth) {
                len = E.screenCols - lineNumberWidth;
            }

            if (has_colors()) {
                int maxCols = E.screenCols - lineNumberWidth;
                if (maxCols < 0) maxCols = 0;
                drawHighlightedLine(row, y, E.colOffset, lineNumberWidth, maxCols, E);
            } else {
                int len = (int)row.size() - E.colOffset;
                if (len < 0) len = 0;
                if (len > E.screenCols - lineNumberWidth) {
                    len = E.screenCols - lineNumberWidth;
                }
                if (len > 0) {
                    addnstr(row.c_str() + E.colOffset, len);
                }
            }

        }
        screenRow++;
    }
}


void drawStatusBar(EditorState &E) {
    attron(A_REVERSE);
    move(E.screenRows, 0);
    clrtoeol();

    std::string status;
    if (!E.filename.empty()) {
        status = E.filename;
    } else {
        status = "[No Name]";
    }

    if (E.dirty) {
        status += " [+]"; // unsaved changes
    } else {
        status += " []"; // clean/saved
    }

    status += "  |  ";
    status += "Ln " + std::to_string(E.cy + 1);
    status += ", Col " + std::to_string(E.cx + 1);

    int len = status.size();
    if (len > E.screenCols) len = E.screenCols;

    addnstr(status.c_str(), len);
    attroff(A_REVERSE);
}


void editorRefreshScreen(EditorState &E) {
    editorScroll(E);
    drawRows(E);
    drawStatusBar(E);

    int lineNumberWidth = 0;
    if (E.config.showLineNumbers) {
        int maxLine = std::max<int>(1, E.rows.size());
        lineNumberWidth = std::to_string(maxLine).size() + 1;
    }

    int screenY = E.cy - E.rowOffset;
    int screenX = computeScreenX(E) - E.colOffset + lineNumberWidth;
    if (screenY < 0) screenY = 0;
    if (screenY >= E.screenRows) screenY = E.screenRows - 1;
    if (screenX < 0) screenX = 0;
    if (screenX >= E.screenCols) screenX = E.screenCols - 1;

    move(screenY, screenX);
    refresh();
}

// --insert and delete--
void insertChar(EditorState &E, char c) {
    pushUndo(E);
    if (E.cy < 0 || E.cy > (int)E.rows.size()) return;
    if (E.cy == (int)E.rows.size()) {
        E.rows.push_back("");
    }

    std::string &row = E.rows[E.cy];
    if (E.cx < 0) E.cx = 0;
    if (E.cx > (int)row.size()) E.cx = row.size();
    row.insert(row.begin() + E.cx, c);
    E.cx++;
    E.dirty = true;
}


void insertNewline(EditorState &E) {
    pushUndo(E);
    if (E.cy < 0 || E.cy > (int)E.rows.size()) return;

    if (E.cy == (int)E.rows.size()) {
        E.rows.push_back("");
        E.cy++;
        E.cx = 0;
        return;
    }

    std::string &row = E.rows[E.cy];
    std::string newRow = row.substr(E.cx);
    row.erase(E.cx);
    E.rows.insert(E.rows.begin() + E.cy + 1, newRow);
    E.cy++;
    E.cx = 0;
    E.dirty = true;
}


void deleteChar(EditorState &E) {
    pushUndo(E);
    if (E.cy < 0 || E.cy >= (int)E.rows.size()) return;
    if (E.cx == 0 && E.cy == 0) return;

    std::string &row = E.rows[E.cy];
    if (E.cx > 0) {
        row.erase(row.begin() + E.cx - 1);
        E.cx--;
    } else {
        // merge with previous line
        int prevLen = E.rows[E.cy - 1].size();
        E.rows[E.cy - 1] += row;
        E.rows.erase(E.rows.begin() + E.cy);
        E.cy--;
        E.cx = prevLen;
    }
    E.dirty = true;
}

// --Cursor--
void moveCursor(EditorState &E, Action action) {
    switch (action) {
        case Action::MOVE_UP:
            if (E.cy > 0) E.cy--;
            break;

        case Action::MOVE_DOWN:
            if (E.cy + 1 < (int)E.rows.size()) {
                E.cy++;
            }
            break;

        case Action::MOVE_LEFT:
            if (E.cx > 0) {
                E.cx--;
            } else if (E.cy > 0) {
                E.cy--;
                E.cx = E.rows[E.cy].size();
            }
            break;

        case Action::MOVE_RIGHT:
            if (E.cy < (int)E.rows.size()) {
                int rowLen = (E.cy == (int)E.rows.size()) ? 0 : E.rows[E.cy].size();
                if (E.cx < rowLen) {
                    E.cx++;
                } else if (E.cx == rowLen && E.cy + 1 < (int)E.rows.size()) {
                    E.cy++;
                    E.cx = 0;
                }
            }
            break;
        default:
            break;
    }

    // clamp cx to row length
    if (E.cy < (int)E.rows.size()) {
        int rowLen = E.rows[E.cy].size();
        if (E.cx > rowLen) E.cx = rowLen;
    } else {
        E.cx = 0;
    }
}

// --Cursor jump to next Word--
void moveWordRight(EditorState &E) {
    if (E.cy >= (int)E.rows.size()) return;
    std::string &row = E.rows[E.cy];

    int len = row.size();
    int x = E.cx;

    if (x >= len) {
        if (E.cy + 1 < (int)E.rows.size()) {
            E.cy++;
            E.cx = 0;
        }
        return;
    }

    // Skip current word
    while (x < len && !std::isspace(row[x])) x++;

    // Skip spaces
    while (x < len && std::isspace(row[x])) x++;

    E.cx = x;
}

void moveWordLeft(EditorState &E) {
    if (E.cy >= (int)E.rows.size()) return;
    std::string &row = E.rows[E.cy];

    if (E.cx == 0) {
        if (E.cy > 0) {
            E.cy--;
            E.cx = E.rows[E.cy].size();
        }
        return;
    }

    int x = E.cx - 1;

    // Skip spaces
    while (x > 0 && std::isspace(row[x])) x--;

    // Skip word characters
    while (x > 0 && !std::isspace(row[x])) x--;

    // If we stopped on a space, move forward one
    if (std::isspace(row[x]) && x < (int)row.size() - 1) x++;

    E.cx = x;
}


// [Key Process Action]
void editorProcessKeypress(EditorState &E) {
    int c = getch();

    // Map to actions if possible
    Action act = mapKeyToAction(E, c);
    if (act != Action::NONE) {
        switch (act) {
            case Action::QUIT:
                E.quit = true;
                return;

            case Action::SAVE:
                saveFile(E);
                return;

            case Action::MOVE_UP:
            case Action::MOVE_DOWN:
            case Action::MOVE_LEFT:
            case Action::MOVE_RIGHT:
                moveCursor(E, act);
                return;

            case Action::MOVE_WORD_LEFT:
                moveWordLeft(E);
                return;

            case Action::MOVE_WORD_RIGHT:
                moveWordRight(E);
                return;

            case Action::UNDO:
                undo(E);
                return;

            default:
                break;
        }
    }

    switch (c) {
        case KEY_HOME:
            E.cx = 0;
            break;

        case KEY_END:
            if (E.cy < (int)E.rows.size()) {
                E.cx = E.rows[E.cy].size();
            }
            break;

        case KEY_PPAGE:
            E.cy -= E.screenRows;
            if (E.cy < 0) E.cy = 0;
            break;

        case KEY_NPAGE:
            E.cy += E.screenRows;
            if (E.cy >= (int)E.rows.size()) {
                E.cy = E.rows.empty() ? 0 : (int)E.rows.size() - 1;
            }
            break;

        case KEY_BACKSPACE:
        case 127:
            deleteChar(E);
            break;
        case '\r':
        case '\n':
            insertNewline(E);
            break;

        case '\t': {
            pushUndo(E);
            insertChar(E, '\t');
            E.dirty = true;
            return;
        }

        default:
            if (std::isprint(c)) {
                insertChar(E, (char)c);
            }
            break;
    }
}


// [main]
int main(int argc, char *argv[]) {
    EditorState E;
    std::string configPath = std::string(getenv("HOME") ? getenv("HOME") : ".") +
                             "/.config/Lume/config.toml";

    initEditor(E, configPath);

    if (argc >= 2) {
        openFile(E, argv[1]);
    }

    while (!E.quit) {
        editorRefreshScreen(E);
        editorProcessKeypress(E);
    }

    endwin();
    return 0;
}
