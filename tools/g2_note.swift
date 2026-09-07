// Sends MIDI note-on/note-off to a destination — for testing what the instrument does on FAST
// REPEATED NOTES over its real playing path.
//
// It exists because the editor's virtual-keyboard path (backdoor DEVNOTE, USB command 0x56) turned
// out NOT to represent playing: a second note there waits for the previous release to finish, even
// with eight voices and a different pitch. Whether that is the instrument or that path is a
// question only MIDI can answer, and answering it decides what the sound engine should copy.
//
// usage: g2_note <destination-substring> <channel 1-indexed> <note> <velocity>   (velocity 0 = off)
import CoreMIDI
import Foundation

func err(_ s: String) { FileHandle.standardError.write((s + "\n").data(using: .utf8)!) }
func check(_ status: OSStatus, _ what: String) {
    if status != 0 { err("\(what) failed: status=\(status)"); exit(1) }
}

guard CommandLine.arguments.count == 5,
      let channel1 = UInt8(CommandLine.arguments[2]),
      let note = UInt8(CommandLine.arguments[3]),
      let velocity = UInt8(CommandLine.arguments[4]) else {
    err("usage: g2_note <destination-substring> <channel 1-indexed> <note> <velocity>")
    exit(1)
}
let sub = CommandLine.arguments[1].lowercased()
let ch = (channel1 - 1) & 0x0F

var client = MIDIClientRef()
check(MIDIClientCreate("g2_note" as CFString, nil, nil, &client), "MIDIClientCreate")
var outPort = MIDIPortRef()
check(MIDIOutputPortCreate(client, "out" as CFString, &outPort), "MIDIOutputPortCreate")

func findDestination(matching s: String) -> MIDIEndpointRef? {
    for i in 0..<MIDIGetNumberOfDestinations() {
        let ep = MIDIGetDestination(i)
        var cfName: Unmanaged<CFString>?
        MIDIObjectGetStringProperty(ep, kMIDIPropertyDisplayName, &cfName)
        if let name = cfName?.takeRetainedValue() as String?, name.lowercased().contains(s) { return ep }
    }
    return nil
}
guard let dest = findDestination(matching: sub) else {
    err("no MIDI destination matching '\(sub)'")
    for i in 0..<MIDIGetNumberOfDestinations() {
        let ep = MIDIGetDestination(i)
        var cfName: Unmanaged<CFString>?
        MIDIObjectGetStringProperty(ep, kMIDIPropertyDisplayName, &cfName)
        if let n = cfName?.takeRetainedValue() as String? { err("  available: \(n)") }
    }
    exit(1)
}
// Velocity 0 is a note-off by convention, and is what a release should be sent as.
let bytes: [UInt8] = [(velocity == 0 ? 0x80 : 0x90) | ch, note, velocity]
var packetList = MIDIPacketList()
let p = MIDIPacketListInit(&packetList)
_ = MIDIPacketListAdd(&packetList, 1024, p, 0, bytes.count, bytes)
check(MIDISend(outPort, dest, &packetList), "MIDISend")
