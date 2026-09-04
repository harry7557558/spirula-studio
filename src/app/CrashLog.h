#pragma once

// The report every tool leaves behind when it dies.
//
// A desktop launch has no terminal, GuiMain releases the console Windows hands
// it, and a reconstruction is a child process whose parent sees only an exit
// status -- so a fault otherwise ends the run with nothing to send. This
// appends a stack trace to <config>/crash.log and, on Windows, says where it
// went in a message box.
//
// Frames are printed as `module+0xRVA`, which addr2line or a matching PDB
// resolves; names and file:line appear too when the build carries symbols.

#include <string>

namespace app {

// Arm the handlers; `dir` is where crash.log goes. What faults before this
// call is still lost, so call it early. SS_CRASH_TEST=segv|throw|worker faults
// on purpose -- the only way to check the handler on a machine it must work on.
void install_crash_log(const std::string& dir);

// Windows: also name the file in a message box. Only the window asks for it --
// a tool run from a terminal or spawned by the GUI has somewhere to print, and
// a modal in a child process stalls the run behind an OK nobody expected.
void set_crash_dialog(bool on);

// <dir>/crash.log; empty before install_crash_log().
std::string crash_log_path();

// What the program is doing, quoted in the report. Without it an unsymbolized
// Windows trace says only which module faulted; with it, which dataset was
// being opened. A racing update costs a garbled line, not a crash.
void set_crash_note(const std::string& what);

}  // namespace app
