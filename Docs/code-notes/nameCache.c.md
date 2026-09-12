# nameCache.c notes

The longer comments from `nameCache.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `REC_LEN`

ONE FIXED-WIDTH RECORD PER POPULATED LOCATION, and no separators at all. cache.txt is a
key=value file read a line at a time, so a value cannot contain a newline — and a patch name can
contain very nearly anything else, which rules out every obvious delimiter. Fixed width sidesteps
the question: a name is stored space-padded to its full length and trimmed on the way back.

```
  [0]    'P' patch or 'F' performance
  [1..2] bank     (2 hex)
  [3..4] location (2 hex)
  [5]    category (1 hex, 0-15)
  [6..]  name, space-padded to CLAVIA_NAME_SIZE
```

## 2. in `append_table()`

A newline in a name would split the value across two lines and take the rest of the
cache with it, since cache.txt is read a line at a time. Nothing else in the file's
format is positional, so every other control character is mapped out too rather than
reasoning about which ones survive a round trip.
