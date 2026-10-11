#pragma once

#include <array>

// The product's name in every place the OS or another program sees it.
// heap → lowkey (APP-280, 0.8.0) → Tabless (APP-283, owner's call
// 2026-10-11). Tabless is a proper noun with a capital T; what the user types
// (the binary, the CLI, the URL scheme) is lowercase. The legacy names stay
// readable so a heap 0.7 or a lowkey 0.8 install moves over without losing
// anything: its data folder, its keychain entries, its links.
namespace heap::brand {

// Application and organisation name: the data folder is <AppData>/Tabless/Tabless.
inline constexpr char kName[] = "Tabless";
// The binary and the CLI command.
inline constexpr char kCommand[] = "tabless";

// Where integration tokens live in the OS keychain.
inline constexpr char kKeychainService[] = "tabless.integrations";

// The OS URL scheme toast clicks come back through.
inline constexpr char kUrlScheme[] = "tabless";

// The lock that makes one running copy per data folder.
inline constexpr char kLockFile[] = "tabless.lock";

// Left in an old data folder once its contents were copied to the new one.
inline constexpr char kMovedMarker[] = "MOVED-TO-TABLESS.txt";

// One earlier name of the product and everything an install of it left on
// the machine.
struct Legacy {
  const char* name;             // its data folder: <AppData>/<name>/<name>
  const char* keychainService;  // its keychain entries
  const char* urlScheme;        // a toast it showed may still sit in the Action Center
  const char* lockFile;         // held while it runs
  const char* movedMarker;      // what it left in the folder before it, if anything
};

// Newest first: a lowkey folder is the one to move when both are there (a
// heap folder beside it was already copied into it by 0.8).
inline constexpr std::array<Legacy, 2> kLegacy{{
    {"lowkey", "lowkey.integrations", "lowkey", "lowkey.lock", "MOVED-TO-LOWKEY.txt"},
    {"heap", "heap.integrations", "heap", "heap.lock", nullptr},
}};

}  // namespace heap::brand
