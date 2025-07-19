# C Shell

POSIX-compliant command-line shell written in C with custom autocompletion, history navigation, pipelines and I/O redirection.

GNU Readline is used for hooks but and line buffering, but the functionality is all custom.

## Features
- **Custom autocompletion** using prefix trees for:
  - Built-ins (`type`, `echo`, `exit`, `pwd`, `history`, `cd`)  
  - Executables in `$PATH`  
  - File paths (including current directory)
  - Displays possible matches and completes longest-common-prefix
- **Command history** stored in doubly-linked list, with:
  - Up/down arrow navigation  
  - `history <n>` to list the last *n* entries 
- **Pipelines** (`cmd1 | cmd2 | …`) and **output redirection** (`>`, `>>`, `2>`, etc.)
- **Builtin commands**: `exit`, `cd`, `pwd`, `echo`, `history`, `type`
- **Excutable Files**: `git`, `gdb`, etc.

## Repository 
```text
.
├── build.sh
├── main.c # shell loop, parsing, dispatch
├── prefixTree.c # trie implementation for autocomplete
├── prefixTree.h
├── autocomplete.c # readline integration & completion logic
├── autocomplete.h
├── historyList.c - doubly linked list storage
├── historyList.h
├── history.c # readline key bindings & history commands
├── history.h
├── readline_init.c # readline initialization hooks
└── readline_init.h
```
## Requirements
- **Libraries**: GNU Readline, ncurses (for `<curses.h>`)
- WSL2 or Linux

## How to build and run
```bash
chmod +x build.sh
./build.sh
./shell
```



## Todo
- Refresh autocomplete trie when `$PATH` is modified (e.g. after `export`)
- Add job‐control (`jobs`, `fg`, `bg`)
- Handle signals properly (e.g. `SIGINT`, `SIGTSTP`)
- Swap raw `printf` calls for Readline buffer APIs
