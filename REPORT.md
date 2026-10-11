# OS Programming Assignment 02 – Report

**Roll No:** BSDSF24A032  
**Repository:** https://github.com/Shahab124/BSDSF24A032-OS-A02

---

## Feature 2: ls-v1.1.0 – Complete Long Listing Format

### Q1. What is the crucial difference between `stat()` and `lstat()`? When is `lstat()` more appropriate for `ls`?

Both fill a `struct stat` with a file's metadata. The difference is how they treat **symbolic links**:

- `stat()` **follows** the link and returns information about the file the link points to (the target).
- `lstat()` does **not** follow the link and returns information about the link itself.

For `ls` we use `lstat()`, because `ls -l` must show the link itself: its type `l`, its own size (the length of the
target path), and the `name -> target` arrow. With `stat()` a link would look like a normal file or directory, and
a broken link (one whose target is missing) would give an error. In `-R` (recursive) mode `lstat()` also stops us
from following a link to a directory, which could cause an endless loop.

### Q2. How do you use bitwise operators and macros to get the file type and permissions out of `st_mode`?

`st_mode` is one integer whose bits hold two kinds of information:

- the **file type** in the high bits (masked by `S_IFMT`), and
- the **permission bits** in the low 12 bits (rwx for user/group/others, plus setuid, setgid and sticky).

**File type:** AND the mode with the mask `S_IFMT` to keep only the type bits, then compare:

```c
if ((st.st_mode & S_IFMT) == S_IFDIR)   /* it is a directory */
```

The macros `S_ISDIR(m)`, `S_ISREG(m)`, `S_ISLNK(m)`, `S_ISCHR(m)`, `S_ISBLK(m)`, `S_ISFIFO(m)` and `S_ISSOCK(m)` do
exactly this comparison for us.

**Permissions:** each permission is a single bit. Bitwise AND with that bit's macro gives a non-zero value if the bit
is on:

```c
if (st.st_mode & S_IRUSR) p[1] = 'r';   /* owner can read     (0400) */
if (st.st_mode & S_IWGRP) p[5] = 'w';   /* group can write    (0020) */
if (st.st_mode & S_IXOTH) p[9] = 'x';   /* others can execute (0001) */
```

Special bits work the same way: `S_ISUID` (setuid), `S_ISGID` (setgid) and `S_ISVTX` (sticky). In my code they replace
the `x` with `s`/`t` (or `S`/`T` if the execute bit is off), just like the real `ls`.

---

## Feature 3: ls-v1.2.0 – Column Display (Down Then Across)

### Q1. Explain the general logic for printing in a "down then across" format. Why is a single loop insufficient?

First, all names are read into an array and the longest name is found. Then:

1. `col_width = maxlen + 2` (2 spaces between columns)
2. `cols = terminal_width / col_width` (at least 1)
3. `rows = ceil(n / cols)` computed as `(n + cols - 1) / cols`

The screen is printed **row by row**, but the names are laid out **column by column**. So for row `r` and column `c`
the name to print is `names[r + c * rows]`:

```c
for (r = 0; r < rows; r++) {
    for (c = 0; c < cols; c++) {
        i = r + c * rows;
        if (i >= n) break;
        printf("%-*s", col_width, names[i]);
    }
    printf("\n");
}
```

A single loop `for (i = 0; i < n; i++)` prints the names in array order, i.e. **left to right**. That gives
"across" order, but in "down then across" the neighbour of `names[0]` on the screen is `names[rows]`, not `names[1]`.
A terminal can only print forwards, line by line (we cannot go back up to fill a column), so we need two nested loops
and the index formula, and we must know `rows` (which needs `n` and `maxlen`) before printing anything.

### Q2. What is the purpose of `ioctl` here? What are the limitations of a fixed 80-column width?

`ioctl(STDOUT_FILENO, TIOCGWINSZ, &w)` asks the terminal driver for the current window size. It fills a
`struct winsize`, and `w.ws_col` is the number of columns. We use it to decide how many filename columns fit.

If we always assumed 80 columns:
- On a **wide** terminal (e.g. 200 columns) we would use less than half the screen and print many more rows than needed.
- On a **narrow** terminal (e.g. 40 columns) lines would be longer than the screen, so they wrap and the columns
  become broken and unreadable.
- The output would not adapt when the user resizes the window.

The 80-column value is only a fallback for when `ioctl` fails, e.g. when the output is redirected to a file or a pipe
(there is no terminal to ask).

---

## Feature 4: ls-v1.3.0 – Horizontal Column Display (-x Option)

### Q1. Compare the complexity of "down then across" vs "across" printing. Which needs more pre-calculation and why?

**"Down then across" needs more pre-calculation.** Before printing the first line it must know:
- the longest name (column width),
- the terminal width → number of columns,
- the total number of names → number of **rows**,

because the name printed next to `names[0]` is `names[rows]`. Without `rows` we cannot know which element goes where.
It also needs two nested loops (rows, columns) and the index formula `r + c * rows`.

**"Across" (-x) is simpler.** It only needs the column width and the terminal width. It walks through the array
once, in normal order, and keeps a counter `pos` of how many characters are on the current line. When
`pos + col_width > width`, it prints `\n` and resets `pos` to 0. It does not need to know the number of rows or even
the total number of files in advance.

### Q2. Describe the strategy used to manage the display modes (-l, -x, default). How does the program decide which function to call?

I use one global integer, `display_mode`, with three named values:

```c
#define MODE_DOWN   0   /* default */
#define MODE_LONG   1   /* -l */
#define MODE_ACROSS 2   /* -x */
int display_mode = MODE_DOWN;
```

In `main()`, the `getopt(argc, argv, "lx")` loop sets `display_mode = MODE_LONG` for `-l` and
`display_mode = MODE_ACROSS` for `-x`. If neither is given it stays `MODE_DOWN`. If both are given, the last one wins,
like the real `ls`.

In `do_ls()`, after reading all names into the array, a single `if / else if / else` checks `display_mode` and calls
`long_listing()`, `print_across()` or `print_down_across()`. Because every display function receives the same array,
adding a new mode only needs one new value, one `getopt` case and one new function.

---

## Feature 5: ls-v1.4.0 – Alphabetical Sort

### Q1. Why must all directory entries be read into memory before sorting? What are the drawbacks for directories with millions of files?

`readdir()` returns entries one at a time in the order they are stored in the directory (not alphabetical). To know
which name comes **first**, you have to compare it with **every** other name, and the very last entry returned by
`readdir()` could be the alphabetically first one. So no name can be printed until all names have been seen. `qsort()`
also needs a complete array in memory to work on.

Drawbacks with millions of files:
- **Memory:** every name is `strdup`'d (one `malloc` each) plus one pointer per name in the array. Millions of entries can
  use hundreds of MB of RAM, and could fail with out-of-memory.
- **Delay:** nothing is printed until the whole directory is read and sorted (O(n log n)), so the user sees no output for a
  long time, instead of output starting immediately.
- **realloc cost:** the array is doubled and copied several times as it grows.

### Q2. Explain the purpose and signature of the `qsort()` comparison function. How does it work, and why `const void *`?

```c
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));

int compare_names(const void *a, const void *b)
{
    const char *name1 = *(const char **)a;
    const char *name2 = *(const char **)b;
    return strcmp(name1, name2);
}
```

`qsort()` knows how to **move** elements around, but not how to **compare** them, because it does not know their type. So we
pass it a pointer to our own function (a function pointer). `qsort` calls it with pointers to two elements, and the
function must return a **negative** number if the first should come first, **0** if they are equal, and a **positive**
number if the second should come first. `strcmp()` already returns exactly that for strings.

The arguments are `void *` because `qsort` is **generic**: the same `qsort` sorts ints, structs, strings, etc., and
`void *` is C's "pointer to anything". They are `const` because the comparison must only read the elements, never
change them. Inside the function we cast back to the real type. Our array elements are `char *`, so a pointer to an
element is a `char **`. We cast to `const char **` and dereference it once to get the actual string.

**Hidden files (optional task):** `strcmp` compares byte values. `.` (46) is smaller than digits, capital letters and
lowercase letters, so if hidden files were included they would simply be sorted first. No change to the sorting code
would be needed.
