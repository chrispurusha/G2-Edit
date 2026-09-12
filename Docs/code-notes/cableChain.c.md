# cableChain.c notes

The longer comments from `cableChain.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. in `cable_chain_disconnect()`

The neighbour that survives as the new parent, and the first child that has to be
reattached to it. With no parent the first child IS the replacement, so the reattach loop
starts one along — that is the original's IsBase() branch, where an output is spliced out
and its inputs are left chained to each other with no source, hence white.
