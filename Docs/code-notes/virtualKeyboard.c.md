# virtualKeyboard.c notes

The longer comments from `virtualKeyboard.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `VKB_BLACK_W_FRAC`

How far a black key sits from the left edge of the white key it follows, as a fraction of a white
key's width. A real keyboard doesn't centre them on the gap — C# and D# sit slightly left and
right of centre respectively, and the same within the F-A# group — but centring is what every
software keyboard does and it keeps the hit rects honest against what's drawn.

## 2. `VKB_REPEAT_MS`

Repeat's rate. A GUESS: the manual says only "play repeatedly" and gives no rate, and the
original offers no control over it either. 250ms is a musically plausible eighth-note-ish pulse
and is slow enough that the note is clearly re-struck rather than buzzing. If the real editor
turns out to lock this to the master clock, this is the constant to replace.

## 3. in `send_note()`

WITH THE LOCAL ENGINE SOUNDING, THE G2 DOES NOT ALSO GET THE NOTE. Otherwise one key press
plays twice — once here and once on the instrument — which is exactly the comparison the
engine exists to make, ruined by doing both at once.

NOTE-OFFS ALWAYS GO THROUGH. Enabling the engine while a key is held would otherwise swallow
the release and leave the G2 droning with no way to stop it short of a panic. A release sent
for a note the instrument is not playing is harmless, so this is the safe asymmetry rather
than a tidy one.

## 4. in `render_virtual_keyboard_panel()`

── The button bar ────────────────────────────────────────────────────
Four scroll buttons on the left as in the original — double arrows an octave, singles a note
— then Drone and Repeat, which are toggles and so carry the lit/unlit colour every other
toggle in the app uses.

## 5. in `handle_virtual_keyboard_mouse()`

The key is no longer held, so the note goes off — UNLESS Drone is engaged, which is
exactly what that button means: "make the next played note start sounding infinitely".
Repeat holds the note on too, since it is about to re-strike it anyway and releasing
here would just make the first gap longer than the rest.

## 6. `note_offset_for_key()`

The computer keyboard as note entry: the home row is the white keys and the row above holds the
blacks, the layout every tracker and DAW uses — a = C, w = C#, s = D, e = D#, d = E, f = F and so
on, with k carrying on into the octave above where the home row runs out. L is the one gap, and
deliberately so — see its case below.

Returns the semitone offset from the leftmost DRAWN note, or -1 for a key that is not a note.
Offsets rather than absolute notes so the played octave follows gVirtualKeyboard.firstNote, which
is always a C: what you play is then always what the panel is showing.

## 7. in `note_offset_for_key()`

L IS DELIBERATELY NOT A NOTE. It is MIDI Learn - the original editor's only bare-key
shortcut, and the reason gParamFocus exists at all. Note entry is dispatched BEFORE the
shortcuts in key_callback() and returns once it claims a key, so while L mapped to D here
a bare L played a note and midi_learn_focused_param() was simply unreachable. The guard
below suppresses note entry for Cmd/Ctrl/Alt so a modified shortcut can never also play;
that cannot help a shortcut which is bare by design, so this one is dropped from the map.
Costs the D above the home row's octave; K and O either side of it still play.

## 8. in `handle_note_entry_key()`

A RELEASE BELONGS TO ITS KEY: it sends the off for the note that key started and nothing else.
Roll from one key to the next without lifting the first and the releases arrive out of order - a
release that silenced whatever happened to be sounding would cut the note still being held.

Drone and Repeat keep the note deliberately, as they do for a mouse release, and so does a shift
latch on this same note. It is left as `ringingNote` and released when the next key goes down, as
a mouse note rings until the next is played.

## 9. in `set_sounding_note()`

THE NEW NOTE GOES OUT BEFORE THE OLD ONE IS RELEASED. Sent the other way round, the G2 sees a
moment with no key down between every pair of notes, so a Legato patch retriggers its envelopes
on each change, which is exactly what Legato exists not to do. Overlapping them is what a
player's hand does on a real keyboard. Mono and Poly behave the same either way: in Mono the
late release is for a note already replaced, and in Poly each note has its own voice.

## 10. `key_note()` and the held-key list

THE COMPUTER KEYBOARD IS A KEYBOARD. Each key sends its own note-on and note-off, to the G2 and to
the engine, and each decides what sounds: a Poly patch plays chords from it, and a Mono or Legato
patch goes back to the HIGHEST key still held when the sounding one is let go (sound engine reference
§15.2), exactly as the G2 plays its own keyboard. Until 2026-09-13 note entry had one sounding note,
so the release of a key silenced everything with another still down; for a few hours after that it
chose the return itself, the NEWEST key held, which is not the G2's choice.

The list is keyed by the PHYSICAL KEY and stores the note each press started. Z and X move the
octave while keys are down, so a note recomputed at release time can be a different note from
the one the key started, and that release would then find nothing to stop.

The panel highlights the key pressed last, and after a release the highest one still down. The
mouse keeps its one-note model through set_sounding_note().

A key whose release never arrives would stay in the list. GLFW releases every held key itself
when the window loses focus, so in the application that cannot happen. Closing the panel releases
every key in it.
