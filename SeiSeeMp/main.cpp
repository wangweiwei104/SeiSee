#include "mainwindow.h"

#include <QApplication>
#include <QFontDatabase>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#include <string>

extern "C" HRESULT WINAPI
SetCurrentProcessExplicitAppUserModelID(PCWSTR AppID);

static bool setRegistryStringValue(HKEY rootKey, LPCWSTR subKey, LPCWSTR valueName, const QString &value)
{
    HKEY hKey = NULL;
    LONG res = RegCreateKeyExW(rootKey, subKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (res != ERROR_SUCCESS)
        return false;

    const std::wstring wVal = value.toStdWString();
    res = RegSetValueExW(hKey, valueName, 0, REG_SZ,
                         reinterpret_cast<const BYTE *>(wVal.c_str()),
                         static_cast<DWORD>((wVal.length() + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
    return res == ERROR_SUCCESS;
}

static QString getRegistryStringValue(HKEY rootKey, LPCWSTR subKey, LPCWSTR valueName)
{
    HKEY hKey = NULL;
    LONG res = RegOpenKeyExW(rootKey, subKey, 0, KEY_READ, &hKey);
    if (res != ERROR_SUCCESS)
        return QString();

    wchar_t buffer[1024];
    DWORD bufferSize = sizeof(buffer);
    DWORD type = 0;
    res = RegQueryValueExW(hKey, valueName, NULL, &type, reinterpret_cast<LPBYTE>(buffer), &bufferSize);
    RegCloseKey(hKey);
    if (res == ERROR_SUCCESS && type == REG_SZ)
        return QString::fromWCharArray(buffer);
    return QString();
}

static void registerWindowsFileAssociations()
{
    const QString appExe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    if (appExe.isEmpty())
        return;

    const QString appDir = QCoreApplication::applicationDirPath();
    QString iconPath;
    const QString subIco = QDir::toNativeSeparators(appDir + "/images/segyfile.ico");
    const QString rootIco = QDir::toNativeSeparators(appDir + "/segyfile.ico");
    if (QFile::exists(subIco)) {
        iconPath = QString("\"%1\",0").arg(subIco);
    } else if (QFile::exists(rootIco)) {
        iconPath = QString("\"%1\",0").arg(rootIco);
    } else {
        const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(appDataDir);
        const QString cachedIco = QDir::toNativeSeparators(appDataDir + "/segyfile.ico");
        if (!QFile::exists(cachedIco) && QFile::exists(":/images/segyfile.ico")) {
            QFile::copy(":/images/segyfile.ico", cachedIco);
        }
        if (QFile::exists(cachedIco)) {
            iconPath = QString("\"%1\",0").arg(cachedIco);
        } else {
            iconPath = QString("\"%1\",1").arg(appExe);
        }
    }

    const QString openCommand = QString("\"%1\" \"%2\"").arg(appExe, "%1");

    const QString currentCmd = getRegistryStringValue(HKEY_CURRENT_USER, L"Software\\Classes\\SeiSee.SegyFile\\shell\\open\\command", NULL);
    const QString currentIcon = getRegistryStringValue(HKEY_CURRENT_USER, L"Software\\Classes\\SeiSee.SegyFile\\DefaultIcon", NULL);
    if (currentCmd == openCommand && currentIcon == iconPath) {
        return;
    }

    bool changed = false;
    changed |= setRegistryStringValue(HKEY_CURRENT_USER, L"Software\\Classes\\.sgy", NULL, "SeiSee.SegyFile");
    changed |= setRegistryStringValue(HKEY_CURRENT_USER, L"Software\\Classes\\.sgy\\OpenWithProgids", L"SeiSee.SegyFile", "");

    changed |= setRegistryStringValue(HKEY_CURRENT_USER, L"Software\\Classes\\.segy", NULL, "SeiSee.SegyFile");
    changed |= setRegistryStringValue(HKEY_CURRENT_USER, L"Software\\Classes\\.segy\\OpenWithProgids", L"SeiSee.SegyFile", "");

    changed |= setRegistryStringValue(HKEY_CURRENT_USER, L"Software\\Classes\\SeiSee.SegyFile", NULL, "SEG-Y Seismic Data File");
    changed |= setRegistryStringValue(HKEY_CURRENT_USER, L"Software\\Classes\\SeiSee.SegyFile\\DefaultIcon", NULL, iconPath);
    changed |= setRegistryStringValue(HKEY_CURRENT_USER, L"Software\\Classes\\SeiSee.SegyFile\\shell\\open\\command", NULL, openCommand);

    changed |= setRegistryStringValue(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\SeiSeeMp.exe", L"FriendlyAppName", "SeiSee");
    changed |= setRegistryStringValue(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\SeiSeeMp.exe\\SupportedTypes", L".sgy", "");
    changed |= setRegistryStringValue(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\SeiSeeMp.exe\\SupportedTypes", L".segy", "");
    changed |= setRegistryStringValue(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\SeiSeeMp.exe\\DefaultIcon", NULL, iconPath);
    changed |= setRegistryStringValue(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\SeiSeeMp.exe\\shell\\open\\command", NULL, openCommand);

    if (changed) {
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    }
}
#endif

#include "gfxstyle.h"

bool setDefaultFont(QApplication *app)
{
    const int plotFontId =
        QFontDatabase::addApplicationFont(GfxStyle::LabelFontResource);
    if (plotFontId < 0) {
        qCritical() << "Unable to load plot font:"
                    << GfxStyle::LabelFontResource;
        return false;
    }

    QStringList plotFontFamilies =
        QFontDatabase::applicationFontFamilies(plotFontId);
    if (plotFontFamilies.isEmpty()) {
        qCritical() << "No font family found in plot font resource:"
                    << GfxStyle::LabelFontResource;
        return false;
    }

    int loadedFontId =
        QFontDatabase::addApplicationFont(GfxStyle::UiFontResource);
    if (loadedFontId < 0) {
        qCritical() << "Unable to load UI font:" << GfxStyle::UiFontResource;
        return false;
    }

    QStringList loadedFontFamilies =
        QFontDatabase::applicationFontFamilies(loadedFontId);
    if (loadedFontFamilies.isEmpty()) {
        qCritical() << "No font family found in UI font resource:"
                    << GfxStyle::UiFontResource;
        return false;
    }

    QFont defaultFont = app->font();
    defaultFont.setFamily(loadedFontFamilies.first());
    defaultFont.setPointSizeF(GfxStyle::UiFontPointSize);
    app->setFont(defaultFont);
    return true;
}

int main(int argc, char *argv[])
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

#ifdef Q_OS_WIN
    const HRESULT appIdResult =
        SetCurrentProcessExplicitAppUserModelID(
            L"wangweiwei104.SeiSee.SeiSeeMp");
    if (FAILED(appIdResult))
        qWarning() << "Unable to set Windows application ID:"
                   << Qt::hex << appIdResult;
#endif

    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/images/SeiSeeMp.ico"));
    if (!setDefaultFont(&a))
        return 1;

#ifdef Q_OS_WIN
    registerWindowsFileAssociations();
#endif

    MainWindow w;
    w.show();

    const QStringList args = a.arguments();
    QString fileToOpen;
    for (int i = 1; i < args.size(); ++i) {
        const QString &arg = args.at(i);
        if (!arg.startsWith('-') && !arg.startsWith('/')) {
            fileToOpen = arg;
            break;
        }
    }
    if (!fileToOpen.isEmpty()) {
        QTimer::singleShot(0, &w, [&w, fileToOpen]() {
            w.openFile(fileToOpen);
        });
    }

    return a.exec();
}
