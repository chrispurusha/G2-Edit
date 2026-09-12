# g2Menu.c notes

The longer comments from `g2Menu.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

The plug-in's menu bar.

A SEPARATE, SMALLER MENU THAN THE APPLICATION'S — not a port of appMenuBar.c, and deliberately so.
Of that file's eight menus, five (Settings, Backup, Restore, Controls, Experimental) exist to talk
to a G2 over USB, which a plug-in has no connection to and should not pretend to. Porting them
would produce a menu full of entries that either do nothing or, worse, look as though they might.

What IS shared is everything underneath: SynthLib's context-menu and menu-bar rendering and
interaction, unchanged. Those needed only two things to link into a plug-in — see
plugin-gui-notes.md — so the machinery is the application's; only the contents are ours.

## 2. file scope

defs.h BEFORE synthlibDefs.h — it defines G2_EDIT, and synthlibDefs.h gates TOP_BAR_HEIGHT, the
colour palette and several layout constants on it. Included the other way round, TOP_BAR_HEIGHT
silently becomes 0.0 (the non-G2 branch), which put the module band 80 units too high, hidden
behind the top bar and with the margin between them swallowed.

## 3. `action_about()`

THE APPLICATION'S OWN MENUS, minus the ones that describe hardware.

An earlier version here defined its own File and View menus. That was the same mistake the topbar
made: a plug-in that resembles the editor is the point, and a second set of menu definitions can
only drift from the first. appMenuBar.c's menus are now exposed individually so a bar can be
composed from a subset of them.

DROPPED ENTIRELY: Backup, Restore and Experimental. All three exist to talk to a G2 over USB or to
toggle the sound engine the plug-in IS. The bank entries inside File and Tools are dropped too,
via app_menu_set_device_capable(false) — the application GREYS those while offline because going
online is possible; here it never is, and a permanently greyed row is worse than no row.

## 4. `action_about()`

── Help menu ─────────────────────────────────────────────────────────────────
WHICH BUILD IS THIS. Version, compile time and the render backend in force. A .vst3 sitting in a host's plug-in folder carries nothing that says where it came
from, and the editor looks the same whether it was built this morning or three weeks ago —
which is exactly what prompted this (CT, 2026-08-29).

## 5. in `action_about()`

AND WHAT THE ENGINE THINKS IT IS DOING. Kept after the meters were reported dead in Ableton
and turned out to be drawing an honest zero - the track was not armed, so nothing was reaching
the plug-in at all. Nothing on the editor says that: the canvas looks identical whether the
engine is playing or idle, so a still meter reads as a broken meter. This line says which,
in the words the application already uses - "Playing 6 modules, 3/8 voices" against
"Playing 6 modules, 0/8 voices" or "Select a module, or patch something into an Out".

STATIC, not on the stack. show_alert() does copy what it is given (wrap_message() splits it
into its own lines), but this string outlives nothing and there is no reason to make that a
dependency of a dialog that appears a frame later.

## 6. in `g2_topbar_rect()`

Reserved, not yet drawn into. Sits directly below the menu bar and above the canvas, which is
where the application puts it — so when the variation buttons, patch name and cable toggles
arrive they land in the space the canvas has already been laid out around, rather than
shifting everything down at that point.
