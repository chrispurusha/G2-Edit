# nameCache.h notes

The longer comments from `nameCache.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `name_cache_save()`

── The bank name tables, remembered between runs ───────────────────────────────────────────────

WHY: send_list_names_sweep() reads every populated patch and performance name off the instrument,
and that takes EIGHT SECONDS — measured 2026-08-29 at 8,012 ms against 116 ms for the patch data
itself, so it is 98% of the wait before the editor considers itself ready. The names feed the
Load/Store/Delete pickers and nothing else. Remembering them means only the first run pays.

The same idea SynthEdit already uses for the Voyager and Z1, and the same store: SynthLib's
prefs.cpp keeps cache.txt beside prefs.txt precisely so that regenerable data never sits in the
file holding real settings.

WHAT THE CACHE IS NOT ALLOWED TO DO. It populates the PICKER and nothing else. Every destructive
operation — store, delete, load — already asks the instrument what is really at that location
first (peek_store_target/peek_delete_target/peek_load_target in usbComms.c), and that must stay
true: the G2's contents change from its own front panel and from other editors, so a remembered
name is a display convenience and never evidence that a location is free or occupied.
