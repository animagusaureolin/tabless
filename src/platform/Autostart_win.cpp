#include "platform/Autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

#include <memory>

namespace heap::platform::autostart::detail {

namespace {

constexpr char kRunKey[] = R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)";
constexpr char kValueName[] = "tabless";
// lowkey 0.8's and heap 0.7's values, newest first.
constexpr const char* kLegacyValueNames[] = {"lowkey", "heap"};

// The Run key, or an .ini stand-in under the test root.
std::unique_ptr<QSettings> openRunKey(const QString& root) {
  if(root.isEmpty()) {
    return std::make_unique<QSettings>(QLatin1String(kRunKey), QSettings::NativeFormat);
  }
  QDir().mkpath(root);
  return std::make_unique<QSettings>(root + QStringLiteral("/Run.ini"), QSettings::IniFormat);
}

}  // namespace

Entry readSystem(const QString& root) {
  const std::unique_ptr<QSettings> key = openRunKey(root);
  return parseWindowsRunCommand(key->value(QLatin1String(kValueName)).toString());
}

bool writeSystem(const QString& root, bool enabled, bool minimized) {
  const std::unique_ptr<QSettings> key = openRunKey(root);
  if(enabled) {
    key->setValue(QLatin1String(kValueName), windowsRunCommand(QCoreApplication::applicationFilePath(), minimized));
  } else {
    key->remove(QLatin1String(kValueName));
  }
  for(const char* old : kLegacyValueNames) {
    key->remove(QLatin1String(old));  // one entry, never two starts
  }
  key->sync();
  return key->status() == QSettings::NoError;
}

Entry readLegacySystem(const QString& root) {
  const std::unique_ptr<QSettings> key = openRunKey(root);
  for(const char* old : kLegacyValueNames) {
    const Entry e = parseWindowsRunCommand(key->value(QLatin1String(old)).toString());
    if(e.enabled) {
      return e;
    }
  }
  return {};
}

void removeLegacySystem(const QString& root) {
  const std::unique_ptr<QSettings> key = openRunKey(root);
  for(const char* old : kLegacyValueNames) {
    key->remove(QLatin1String(old));
  }
  key->sync();
}

}  // namespace heap::platform::autostart::detail
