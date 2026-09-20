# cableChain.c notes

The longer comments from `cableChain.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. in `cable_chain_disconnect()`

The neighbour that survives as the new parent, and the first child that has to be
reattached to it. With no parent the first child IS the replacement, so the reattach loop
starts one along — that is the original's IsBase() branch, where an output is spliced out
and its inputs are left chained to each other with no source, hence white.

## 3. in `find_source_undirected()`, used by `cable_chain_find_root()`

An input-to-input link joins two inputs, and WHICH end the patch records as the from-end is just
which connector the drag started at - it carries no signal direction. The parent walk above only
ever steps `to` -> `from`, so a module whose input is joined to a fed input *as the from-end* looked
unconnected: 01 Mini Emulator wires Osc 3's pitch switch that way, and the oscillator sat at a fixed
pitch, audible only in variation 7 where it is the one turned up (CT, 2026-09-20). The same patch
also puts TWO cables on one input - an output's and an input-to-input link's - so even where the
parent walk did resolve, which source it found depended on the order the cables happened to sit in.

So when the parent walk ends on an input rather than an output, this searches the chain as an
UNDIRECTED graph from the original node and takes the first output it reaches. It cannot wander
downstream: it only ever leaves an input, and every cable on an input either comes from an output
or is a link to another input.

The parent walk is still run first and still decides the answer whenever it finds a source, so
nothing that resolved before resolves differently. Its terminal node is also what a genuinely
sourceless chain still returns, because Break sweeps the dead tree from there and that behaviour
was checked against the original editor.
