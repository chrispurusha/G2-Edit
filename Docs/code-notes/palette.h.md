# palette.h notes

The longer comments from `palette.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `palette_new_module_colour()`

Add the selected group's nth module to the patch under the focused module, the way the manual's
double-click does. Exposed for the backdoor, which cannot synthesise a drag.
The colour new modules are created in, chosen from the band's swatches. The instrument works the
same way (manual p.61): the selection persists, so a run of modules can be added in one colour.
