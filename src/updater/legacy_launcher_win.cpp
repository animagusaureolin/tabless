// lowkey.exe in a Tabless install (APP-283): lowkey was renamed to Tabless,
// and a few things outside Tabless still start lowkey.exe by its old path — the
// 0.8 updater, which restarts the program it updated; Start-menu, desktop
// and taskbar shortcuts made by 0.8; a login entry 0.8 wrote. This starts
// tabless.exe from the same folder with the same arguments and exits. Tabless
// itself rewrites the login entry on its first start; the rest fade out with
// the next installer. No Qt: it only has to find one file and start it.

#include <string>

#include <windows.h>

namespace {

std::wstring siblingExe(const wchar_t* name) {
  std::wstring self(MAX_PATH, L'\0');
  for(;;) {
    const DWORD n = GetModuleFileNameW(nullptr, self.data(), static_cast<DWORD>(self.size()));
    if(n == 0) {
      return {};
    }
    if(n < self.size()) {
      self.resize(n);
      break;
    }
    self.resize(self.size() * 2);
  }
  const auto slash = self.find_last_of(L"\\/");
  return (slash == std::wstring::npos ? std::wstring() : self.substr(0, slash + 1)) + name;
}

// The arguments as the shell passed them, minus this program's own name.
std::wstring forwardedArguments() {
  const wchar_t* cmd = GetCommandLineW();
  bool quoted = false;
  for(; *cmd != L'\0'; ++cmd) {
    if(*cmd == L'"') {
      quoted = !quoted;
    } else if(!quoted && (*cmd == L' ' || *cmd == L'\t')) {
      break;
    }
  }
  while(*cmd == L' ' || *cmd == L'\t') {
    ++cmd;
  }
  return cmd;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE /*instance*/, HINSTANCE /*previous*/, PWSTR /*cmdLine*/, int /*show*/) {
  const std::wstring exe = siblingExe(L"tabless.exe");
  std::wstring cmd = L"\"" + exe + L"\"";
  const std::wstring args = forwardedArguments();
  if(!args.empty()) {
    cmd += L" " + args;
  }
  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  if(!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
    MessageBoxW(nullptr, L"lowkey is now Tabless, but tabless.exe was not found next to lowkey.exe.", L"Tabless", MB_ICONWARNING | MB_OK);
    return 1;
  }
  // Let Tabless take the foreground: the shell gave it to this process.
  AllowSetForegroundWindow(pi.dwProcessId);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return 0;
}
