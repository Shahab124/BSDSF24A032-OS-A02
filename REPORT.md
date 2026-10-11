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
