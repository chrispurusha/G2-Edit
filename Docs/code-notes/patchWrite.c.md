# patchWrite.c notes

The longer comments from `patchWrite.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

Writing the patch database back out as a .pch2 / .prf2 file.

SPLIT OUT OF graphics.c on 2026-09-09 so the VST3 plug-in can save. Both functions are pure
serialisation — database in, file out — with no GLFW, no window and no device in them; they only
ever lived in graphics.c because that is where the file-browser callback that calls them lives.
The plug-in does not compile graphics.c (it has its own render loop in vst3/g2Draw.c), so File >
Save there consumed its own message and did nothing.

The application's on_file_saved() still owns the POLICY around a save — online vs offline, the
recent-files list, the remembered path. Only the bytes-to-disk half is here.

## 2. .pchx - a patch with engine-only modules (`patch_save_path()`, `engine_only_header_apply()`)

A .pch2 cannot carry a module the G2 does not have: the instrument and the original editor would meet a type
they do not know. A patch that holds one is saved as .pchx instead - the same file in every byte, except:
- the binary keeps the engine-only modules and everything that names them (the writers' guard is lifted for
  the thread writing it, `protocol_include_engine_only()`, dataBase.c notes §8);
- the text header gains a line per Sampler, `Sample=VA,<index>,<path>`, naming its sample file. Loading reads each
  file at once, so a Sampler whose file has gone or cannot be read shows it in red on its face
  (moduleGraphics.c notes §93) rather than only falling silent.
Every reader skips the text header up to its first zero byte, so the binary parses as it always has, and the
Sample= lines are applied after it (`engine_only_header_apply()`, called by each of the four readers: the
application offline and online, and the plug-in's two). Saving chooses the extension: a patch with an
engine-only module is written as .pchx whatever name was typed (`patch_save_path()`), one without as asked.
A .pchx sent to the G2 still goes through the guarded writers, so the instrument gets the patch less its
engine-only part. Performances (.prf2) do not carry engine-only modules yet.

A slot also saves as .pchx when a 2-Out sends to Out 5/6 or above, the engine's own outputs
(moduleResourcesAccess.c notes §7): the .pch2 writer gives those as Out 1/2, so a plain .pch2 stays one the G2
can load, and only a .pchx carries the real destination back.

## 3. `write_perf_to_memory()` - a .prf2, a .prfx, or the plug-in's project state

The same performance image serves three readers. A .prf2 is one the G2 can load, so it leaves out what only
the engine has - engine-only modules, and a 2-Out sent to Out 5/6 and above, given as Out 1/2
(moduleResourcesAccess.c notes §7). A .prfx (CT, 2026-10-11) is the performance counterpart of a .pchx: it
keeps them, and names each Sampler's file in the text header as `Sample=<slot A-D>,<VA|FX>,<module>,<path>` -
the .pchx line with the slot a performance needs, which a .pchx reader passes over. `perf_save_path()` picks
.prfx when any slot needs it, as `patch_save_path()` does for a patch, and `perf_engine_only_header_apply()`
reads the lines back after the slots are parsed - on every load path: the application offline (graphics.c)
and online (usbComms.c), and the plug-in (g2Patch.c). The plug-in's project state is the .prfx image
(g2Plugin.c notes §15): leaving those things out had folded a drum kit's separate outputs back onto Out 1/2
on every reopened project.
