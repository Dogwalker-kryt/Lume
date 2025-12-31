# lume — a tiny, fast terminal editor

**lume** is a small, focused text editor for the terminal.  
It doesn’t try to be an IDE, a plugin host, or a programming language.  
It does one thing well:

**open a file instantly and let you edit it without getting in your way.**

The entire binary is around **54.5 KB**, yet lume includes:

- C/C++ syntax highlighting  
- Real tab support with proper visual expansion  
- Undo (Ctrl‑Z)  
- Line numbers  
- Word‑jumping  
- A clean status bar with save/dirty indicator  
- Smooth scrolling  
- Simple, modeless editing (no modes to memorize)  

lume is built for people who want a fast, predictable editor that stays out of
the way — whether you’re fixing a config file or working inside a large
codebase.

---

## Why lume?

lume isn’t designed as a toy editor or a temporary fallback when nothing else is
installed. It’s built to be a tool you can rely on every day.

The goal is simple: **keep the editor small, but never make it feel small.**

Many editors grow by adding layers of features, plugins, and abstractions. lume
grows by refining the essentials. The result is an editor that starts instantly,
remains responsive even with large files, and stays predictable during long
editing sessions.

You can use lume for quick one‑off edits, but it’s just as comfortable for real
development work. Syntax highlighting, undo, word‑jumping, proper tab handling,
and a clean interface make it easy to stay focused, even in big projects.

lume doesn’t try to compete with nano, vim, or emacs — it simply proves that an
editor doesn’t need to be big to be capable.

---

## Features

- **Modeless editing** — no insert/normal mode dance  
- **Syntax highlighting** for C/C++  
- **Undo** (Ctrl‑Z)  
- **Line numbers**  
- **Word‑jumping** with Ctrl‑Left / Ctrl‑Right  
- **Real tabs** with correct visual width  
- **Configurable tab size**  
- **Status bar** with filename, cursor position, and dirty flag  
- **Fast screen rendering** using ncurses  
- **Tiny binary** (~50 KB with optimizations)  

---

## Usage

Open a file:
```bash
lume file.txt
```

Save a file: 
```
Ctrl s
```

Quit: 
```
Ctrl q
```

Undo:
```
Ctrl z
```

Jump Words:
```
Ctrl LeftArrow for moving left
Ctrl RightArrow for moving right
```

## Building

You will need:
g++ (min version 17)
ncurses

After you made sure the required things are instlled you simply run the setup script (Work in Progress)

recommenden compile command for best performance and light weight binary
```
g++ -O3 -march=native -mtune=native -fno-exceptions -fno-rtti -fno-unwind-tables -fno-asynchronous-unwind-tables -fdata-sections -ffunction-sections -WL,--gc-sections -s main_Lume.cpp -lncurses -o Lume
```



---

## Philosophy

lume is built around a few simple ideas:

- **Speed matters.** An editor should open instantly.  
- **Small is beautiful.** A tiny codebase is easier to understand and maintain.  
- **No modes.** Editing shouldn’t require memorizing a state machine.  
- **No distractions.** The editor should disappear and let you focus on the text.  
- **Hackability.** lume is small enough that you can read the entire source in one sitting.

This project exists because sometimes you don’t want a full IDE.  
Sometimes you just want to open a file, make a change, and move on — without
sacrificing comfort or modern features.

---

## Project Status

lume is stable and usable for everyday editing.  
New features are added slowly and intentionally — only when they improve the
core experience without adding bloat.

---

## License

