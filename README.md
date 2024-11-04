# G2 Editor

Work in-progress. So far implements basic USB comms. in 'C'.
Init sequence followed, with reponse values output to console.

Very much a work-in progress and experiment with MacOS user
level IOKit.

To do:

Storage of values in structures.

GUI work and programming language to be decided - possibly Swift.

Write commands to be implemented.

Possible wholesale conversion to Swift, Objective C or C++ (possibly using JUCE).

Patch file reading and writing.

Deliberately implemented using polling/synchronous access. Possible shift to
asynchronous, depending on eventual GUI events mechanism.
