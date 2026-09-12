# cableChain.h notes

The longer comments from `cableChain.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `tCableNode`

A serial cable chain is a TREE of connectors. Its root is an OUTPUT (the signal source);
every other node is an INPUT. The G2 wire format encodes that shape directly:

```
  linkType == cableLinkTypeFromOutput  ->  from-end is an output, to-end is an input
  linkType == cableLinkTypeFromInput   ->  from-end is an input,  to-end is an input

```
so the to-end is ALWAYS an input, and (moduleIndex, ioCount) alone is ambiguous between an
input 0 and an output 0 — the direction has to be carried alongside it. Hence tCableNode.

A chain whose root is not an output has NO SIGNAL SOURCE. The original editor renders those
white and refuses to operate on them; see cable_chain_colour().

## 2. `cable_chain_connect()`

Creates a cable between two nodes, writing it to the database and telling the G2. The link
type follows the from-end's direction, which is how the wire format encodes it (and why
tCableKey.linkType can be assigned straight from a tConnectorDir — the two enums align).
Fails if the to-end is not an input; only inputs can be fed.

## 3. `cable_chain_collect_branch()`

Collects the BRANCH at `node`: the cable feeding it, if any, plus its whole subtree.

This is the scope of the original's COLOR and DELETE: both act on the branch from the
clicked connector down. It is what the manual means by "the entire serial cable chain
that the connection is part of", and why it also says a complete branch must be deleted from
its origin: start lower down and you only get what hangs off that point.

Break uses cable_chain_collect_subtree() instead — it deletes the feeding cable rather than
recolouring it, so including it here would be pointless.

## 4. `cable_chain_disconnect()`

DISCONNECT — splices `node` out of its chain and joins the chain back up around it, so what
remains keeps working. As the original does it, one
surviving neighbour becomes the new parent (the node's own parent, or its first child when
the node is the chain root, i.e. an output), and every other neighbour is reconnected to it.
Returns false if nothing was attached to `node`.

## 5. `cable_chain_break()`

BREAK — cuts the chain at `node` WITHOUT splicing, leaving everything past the cut connected
but dead, and recoloured white. Including the original's
no-op on a chain that has no source: such a chain is already
white, and so has nothing left to break. Returns false if nothing was broken.

## 6. `cable_chain_recolour()`

Re-applies the invariant to the whole chain containing `node`: recomputes the chain colour
and paints every cable in the chain with it.

Call this after any edit that can change a chain's SOURCE-REACHABILITY. It must NOT be
called unconditionally after unrelated edits: chain colour is user-overridable via the
colour menu, and the original only discards that override on a topology change.
