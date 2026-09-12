# g2Patch.c notes

The longer comments from `g2Patch.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

Loading a .pch2 into the patch database, for the plug-in.

This is deliberately NOT read_file_into_memory_and_process() from graphics.c. That function is
the editor's loader and carries two things a plug-in must not have: an online branch that hands
the file to the USB thread, and a home in a translation unit that pulls in GLFW. What is left
once both are removed is the offline patch path, which is what this is - the same CRC check and
the same parse_patch() call, with the performance-file and naming branches dropped since a
plug-in hosts exactly one patch.

## 2. in `g2_plugin_parse_patch()`

THE CAST IS DELIBERATE AND THE CONST IS NOT A LIE: parse_patch() never writes through this
buffer (checked), but its signature and those of the seven sub-parsers and read_bit_stream()
below it all take a non-const uint8_t *. Const-correcting that chain is a protocol.c-wide job,
not part of a warnings sweep — so the discard happens here, once, visibly, instead of as an
implicit conversion the compiler has to complain about on every build.

## 3. in `g2_plugin_open_file()`

A PERFORMANCE: all four slots at once. What the application does with one offline
(graphics.c), because the plug-in holds all four slots too now - which is what
performance mode needs. The slot names come from the file; the performance's name from
its file name.
