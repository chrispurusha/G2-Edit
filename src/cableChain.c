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

#ifdef __cplusplus
extern "C" {
#endif

#include "defs.h"
#include "types.h"
#include "msgQueue.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "globalVars.h"
#include "cableChain.h"

void cable_send_message(uint32_t cmd, uint32_t slot, uint32_t location, tCableKey * key, uint32_t colour) {
    tMessageContent messageContent = {0};

    messageContent.cmd                            = cmd;
    messageContent.slot                           = slot;
    messageContent.cableData.location             = location;
    messageContent.cableData.moduleFromIndex      = key->moduleFromIndex;
    messageContent.cableData.connectorFromIoIndex = key->connectorFromIoCount;
    messageContent.cableData.moduleToIndex        = key->moduleToIndex;
    messageContent.cableData.connectorToIoIndex   = key->connectorToIoCount;
    messageContent.cableData.linkType             = key->linkType;
    messageContent.cableData.colour               = colour;

    msg_send(&gToUsbThread, &messageContent);
}

// The from-end of a cable carries its own direction in the link type; the to-end is always
// an input. Everything below is expressed through these two so the ambiguity between
// "output 0" and "input 0" can never leak into a comparison.
tCableNode cable_chain_from_node(tCable * cable) {
    return (tCableNode){
        cable->key.moduleFromIndex,
        cable->key.connectorFromIoCount,
        cable->key.linkType == cableLinkTypeFromOutput
    };
}

tCableNode cable_chain_to_node(tCable * cable) {
    return (tCableNode){
        cable->key.moduleToIndex, cable->key.connectorToIoCount, false
    };
}

bool cable_chain_node_equal(tCableNode a, tCableNode b) {
    return (a.moduleIndex == b.moduleIndex) && (a.ioCount == b.ioCount) && (a.isOutput == b.isOutput);
}

bool cable_chain_connect(uint32_t slot, uint32_t location, tCableNode from, tCableNode to, tCableColour colour) {
    tCableKey key   = {0};
    tCable    cable = {0};

    if (to.isOutput) {
        return false;  // The to-end of a cable is always an input
    }
    key.slot                 = slot;
    key.location             = location;
    key.moduleFromIndex      = from.moduleIndex;
    key.connectorFromIoCount = from.ioCount;
    key.linkType             = from.isOutput ? cableLinkTypeFromOutput : cableLinkTypeFromInput;
    key.moduleToIndex        = to.moduleIndex;
    key.connectorToIoCount   = to.ioCount;

    cable.key                = key;
    cable.colour             = (uint32_t)colour;
    cable.active             = true;

    write_cable(key, &cable);
    cable_send_message(eMsgCmdWriteCable, slot, location, &key, cable.colour);

    return true;
}

bool cable_chain_node_from_connector(tModule * module, uint32_t connectorIndex, tCableNode * node) {
    if ((module == NULL) || (node == NULL)) {
        return false;
    }
    tConnectorDir dir   = module->connector[connectorIndex].dir;
    int           count = find_io_count_from_index(module, dir, (int)connectorIndex);

    if (count < 0) {
        return false;
    }
    node->moduleIndex = module->key.index;
    node->ioCount     = (uint32_t)count;
    node->isOutput    = (dir == connectorDirOut);

    return true;
}

tCable * cable_chain_feeding_cable(uint32_t slot, uint32_t location, tCableNode node) {
    if (node.isOutput) {
        return NULL;  // An output is a source; nothing can feed it
    }

    for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
        tCable * cable = get_cable_slot(slot, location, i);

        if ((cable == NULL) || !cable->active) {
            continue;
        }

        if (cable_chain_node_equal(cable_chain_to_node(cable), node)) {
            return cable;
        }
    }

    return NULL;
}

bool cable_chain_find_root(uint32_t slot, uint32_t location, tCableNode node, tCableNode * root) {
    for (uint32_t step = 0; step < MAX_NUM_CABLES; step++) {
        if (node.isOutput) {
            break;  // Reached the source
        }
        tCable * feed = cable_chain_feeding_cable(slot, location, node);

        if (feed == NULL) {
            break;  // Ran out of chain without ever reaching an output
        }
        node = cable_chain_from_node(feed);
    }

    if (root != NULL) {
        *root = node;
    }
    return node.isOutput;
}

tCableColour cable_chain_colour(uint32_t slot, uint32_t location, tCableNode node) {
    tCableNode root   = {0};

    if (!cable_chain_find_root(slot, location, node, &root)) {
        return cableColourWhite;  // No source output — the chain is dead
    }
    tModule *  module = get_module_slot(slot, location, root.moduleIndex);

    if ((module == NULL) || !module->active) {
        return cableColourWhite;
    }
    int        index  = find_index_from_io_count(module, connectorDirOut, (int)root.ioCount);

    if (index < 0) {
        return cableColourWhite;
    }
    // Matches the original's CPatchData::GetConnectorColor(), which resolves the connector to
    // its rendered hole widget and asks that for its colour — bandwidth-dependent for logic
    // holes, which is exactly what the upRate promotion in effective_connector_type() does.
    return cable_colour_for_connector_type(
        effective_connector_type(module->connector[index].type, module->upRate));
}

uint32_t cable_chain_collect_subtree(uint32_t slot, uint32_t location, tCableNode node,
                                     tCableKey * out, uint32_t maxOut) {
    tCableNode pending[MAX_NUM_CABLES];
    uint32_t   pendingCount = 0;
    uint32_t   visited      = 0;
    uint32_t   count        = 0;

    if ((out == NULL) || (maxOut == 0)) {
        return 0;
    }
    pending[pendingCount++] = node;

    // Each input takes at most one incoming cable, so the reachable set is a tree and this
    // cannot loop; the visited counter is belt-and-braces against a malformed cable list.
    while ((pendingCount > 0) && (visited < MAX_NUM_CABLES)) {
        tCableNode from = pending[--pendingCount];

        visited++;

        for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
            tCable * cable = get_cable_slot(slot, location, i);

            if ((cable == NULL) || !cable->active) {
                continue;
            }

            if (!cable_chain_node_equal(cable_chain_from_node(cable), from)) {
                continue;
            }

            if (count < maxOut) {
                out[count++] = cable->key;
            }

            if (pendingCount < MAX_NUM_CABLES) {
                pending[pendingCount++] = cable_chain_to_node(cable);
            }
        }
    }
    return count;
}

uint32_t cable_chain_collect(uint32_t slot, uint32_t location, tCableNode node,
                             tCableKey * out, uint32_t maxOut) {
    tCableNode root = {0};

    // Sourceless chains have no output root, in which case find_root leaves us on the topmost
    // input — still the right place to sweep the whole dead tree from.
    cable_chain_find_root(slot, location, node, &root);

    return cable_chain_collect_subtree(slot, location, root, out, maxOut);
}

uint32_t cable_chain_collect_branch(uint32_t slot, uint32_t location, tCableNode node,
                                    tCableKey * out, uint32_t maxOut) {
    uint32_t count = 0;

    if ((out == NULL) || (maxOut == 0)) {
        return 0;
    }
    tCable * feed  = cable_chain_feeding_cable(slot, location, node);

    if (feed != NULL) {
        out[count++] = feed->key;
    }
    return count + cable_chain_collect_subtree(slot, location, node, &out[count], maxOut - count);
}

uint32_t cable_chain_apply_colour(uint32_t slot, uint32_t location,
                                  tCableKey * keys, uint32_t count, tCableColour colour) {
    uint32_t changed = 0;

    for (uint32_t i = 0; i < count; i++) {
        tCable * cable = get_cable(keys[i]);

        if ((cable == NULL) || !cable->active || (cable->colour == (uint32_t)colour)) {
            continue;
        }
        cable->colour = (uint32_t)colour;
        cable_send_message(eMsgCmdWriteCable, slot, location, &keys[i], cable->colour);
        changed++;
    }

    return changed;
}

void cable_chain_delete_keys(uint32_t slot, uint32_t location, tCableKey * keys, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        tCable * cable = get_cable(keys[i]);

        if ((cable == NULL) || !cable->active) {
            continue;
        }
        cable_send_message(eMsgCmdDeleteCable, slot, location, &keys[i], cable->colour);
        delete_cable(keys[i]);
    }
}

// Every cable leaving `node`, i.e. the cables to its direct children.
static uint32_t collect_children(uint32_t slot, uint32_t location, tCableNode node,
                                 tCableNode * children, tCableKey * keys, uint32_t maxOut) {
    uint32_t count = 0;

    for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
        tCable * cable = get_cable_slot(slot, location, i);

        if ((cable == NULL) || !cable->active) {
            continue;
        }

        if (!cable_chain_node_equal(cable_chain_from_node(cable), node)) {
            continue;
        }

        if (count < maxOut) {
            children[count] = cable_chain_to_node(cable);
            keys[count]     = cable->key;
            count++;
        }
    }

    return count;
}

bool cable_chain_disconnect(uint32_t slot, uint32_t location, tCableNode node) {
    tCableNode children[MAX_NUM_CABLES];
    tCableKey  childKeys[MAX_NUM_CABLES];
    tCable *   feed        = cable_chain_feeding_cable(slot, location, node);
    bool       hasParent   = (feed != NULL);
    tCableNode parent      = {0};
    tCableKey  feedKey     = {0};

    if (hasParent) {
        parent  = cable_chain_from_node(feed);
        feedKey = feed->key;
    }
    uint32_t   childCount  = collect_children(slot, location, node, children, childKeys, MAX_NUM_CABLES);

    if (!hasParent && (childCount == 0)) {
        return false;  // Nothing is attached here, so there is nothing to splice out
    }
    // The neighbour that survives as the new parent, and the first child that has to be
    // reattached to it. With no parent the first child IS the replacement, so the reattach loop
    // starts one along — that is the original's IsBase() branch, where an output is spliced out
    // and its inputs are left chained to each other with no source, hence white.
    tCableNode replacement = hasParent ? parent : children[0];
    uint32_t   firstChild  = hasParent ? 0 : 1;

    if (hasParent) {
        cable_chain_delete_keys(slot, location, &feedKey, 1);
    }
    cable_chain_delete_keys(slot, location, childKeys, childCount);

    for (uint32_t i = firstChild; i < childCount; i++) {
        cable_chain_connect(slot, location, replacement, children[i], cableColourWhite);
    }

    // The original recolours from a flag computed before the edit; we re-derive from
    // source-reachability afterwards. Same result where it matters, and it cannot strand a
    // coloured sourceless chain — a state the original has no way to represent.
    cable_chain_recolour(slot, location, replacement);

    return true;
}

bool cable_chain_break(uint32_t slot, uint32_t location, tCableNode node) {
    tCableNode orphaned[MAX_NUM_CABLES];
    uint32_t   orphanedCount = 0;
    tCableKey  deleteKeys[MAX_NUM_CABLES];
    uint32_t   deleteCount   = 0;
    tCableKey  keys[MAX_NUM_CABLES];

    if (!node.isOutput) {
        // An already-sourceless chain is already broken - leave it entirely alone. Confirmed
        // against the real editor 2026-07-31: breaking mid-chain with the oscillator
        // disconnected does nothing at all.
        if (!cable_chain_find_root(slot, location, node, NULL)) {
            return false;
        }
        tCable * feed = cable_chain_feeding_cable(slot, location, node);

        if (feed == NULL) {
            return false;
        }
        deleteKeys[deleteCount++] = feed->key;
        orphaned[orphanedCount++] = node;
    } else {
        tCableNode children[MAX_NUM_CABLES];

        // At an output, every cable leaving it goes, and each input it was feeding heads a dead
        // tail of its own
        orphanedCount = collect_children(slot, location, node, children, deleteKeys, MAX_NUM_CABLES);
        deleteCount   = orphanedCount;

        for (uint32_t i = 0; i < orphanedCount; i++) {
            orphaned[i] = children[i];
        }
    }

    if (deleteCount == 0) {
        return false;  // Nothing was connected here, so there is nothing to break
    }
    cable_chain_delete_keys(slot, location, deleteKeys, deleteCount);

    for (uint32_t i = 0; i < orphanedCount; i++) {
        uint32_t count = cable_chain_collect_subtree(slot, location, orphaned[i], keys, MAX_NUM_CABLES);

        cable_chain_apply_colour(slot, location, keys, count, cableColourWhite);
    }

    return true;
}

void cable_chain_recolour(uint32_t slot, uint32_t location, tCableNode node) {
    tCableKey    keys[MAX_NUM_CABLES];
    uint32_t     count  = cable_chain_collect(slot, location, node, keys, MAX_NUM_CABLES);
    tCableColour colour = cable_chain_colour(slot, location, node);

    cable_chain_apply_colour(slot, location, keys, count, colour);
}

#ifdef __cplusplus
}
#endif
