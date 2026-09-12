/*
 * The G2 Editor application.
 *
 * Copyright (C) 2026 Chris Turner <chris_purusha@icloud.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
// Notes: Docs/code-notes/cableChain.h.md - "// notes §k" refers there.

#ifndef CABLE_CHAIN_H
#define CABLE_CHAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "types.h"

// notes §1
typedef struct {
    uint32_t moduleIndex;
    uint32_t ioCount;
    bool     isOutput;
} tCableNode;

// Every cable command carries the same key fields; only the command itself and the colour
// differ (the delete command ignores the colour).
void cable_send_message(uint32_t cmd, uint32_t slot, uint32_t location, tCableKey * key, uint32_t colour);

// Builds a node from a connector index into a module (the form the context menu has).
// Returns false if the connector index does not resolve to a real io connector.
bool cable_chain_node_from_connector(tModule * module, uint32_t connectorIndex, tCableNode * node);

// A cable's two ends as nodes. The from-end carries its own direction in the link type; the
// to-end is always an input.
tCableNode cable_chain_from_node(tCable * cable);
tCableNode cable_chain_to_node(tCable * cable);

bool cable_chain_node_equal(tCableNode a, tCableNode b);

// notes §2
bool cable_chain_connect(uint32_t slot, uint32_t location, tCableNode from, tCableNode to, tCableColour colour);

// The one cable feeding this node, or NULL. Only inputs can be fed, and each input takes at
// most one incoming cable (mouseHandle.c's input_connector_has_cable() enforces that at
// connect time), so this is single-valued rather than a list.
tCable * cable_chain_feeding_cable(uint32_t slot, uint32_t location, tCableNode node);

// Walks back to the far end of the chain. Returns the node the walk ended on in *root, and
// true only if that node is an OUTPUT — i.e. the chain actually reaches a signal source.
// A false return means the chain is dead, and *root is the topmost input it terminated at.
bool cable_chain_find_root(uint32_t slot, uint32_t location, tCableNode node, tCableNode * root);

// The colour the whole chain containing `node` should carry, per the invariant established
// every cable in a chain shares ONE colour — the source output's
// signal-type colour, or white when the chain has no source output at all.
tCableColour cable_chain_colour(uint32_t slot, uint32_t location, tCableNode node);

// Collects every cable in the whole chain containing `node` (walks to the root first, so it
// does not matter where in the chain you start). Returns the number written to `out`.
uint32_t cable_chain_collect(uint32_t slot, uint32_t location, tCableNode node, tCableKey * out, uint32_t maxOut);

// Collects the cables in the SUBTREE below `node` — everything `node` feeds, directly or
// indirectly, not including whatever feeds `node` itself. This is the scope Break whitens
// (the original uses a PARTIAL tree iterator there, where connect uses a COMPLETE one).
uint32_t cable_chain_collect_subtree(uint32_t slot, uint32_t location, tCableNode node, tCableKey * out, uint32_t maxOut);

// notes §3
uint32_t cable_chain_collect_branch(uint32_t slot, uint32_t location, tCableNode node, tCableKey * out, uint32_t maxOut);

// Applies `colour` to the given cables, updating the database and telling the G2 about any
// that actually changed. Returns the number of cables whose colour was not already `colour`.
uint32_t cable_chain_apply_colour(uint32_t slot, uint32_t location, tCableKey * keys, uint32_t count, tCableColour colour);

// Deletes the given cables, from the database and from the G2.
void cable_chain_delete_keys(uint32_t slot, uint32_t location, tCableKey * keys, uint32_t count);

// notes §4
bool cable_chain_disconnect(uint32_t slot, uint32_t location, tCableNode node);

// notes §5
bool cable_chain_break(uint32_t slot, uint32_t location, tCableNode node);

// notes §6
void cable_chain_recolour(uint32_t slot, uint32_t location, tCableNode node);

#ifdef __cplusplus
}
#endif

#endif // CABLE_CHAIN_H
