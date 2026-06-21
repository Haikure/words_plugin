#include "WordController.h"

#include <QFileInfo>
#include <QQmlContext>
#include <QQmlEngine>
#include <QString>

#include <dlfcn.h>
#include <cstdio>

// 通过 dladdr 反查本 .so 的绝对路径，得到插件根目录
// （/userdisk/PenMods/plugins/word_plugin），用于定位 dicts/ 与 userdata/。
static QString resolvePluginDir() {
    Dl_info info;
    if (dladdr(reinterpret_cast<void*>(&resolvePluginDir), &info) && info.dli_fname) {
        return QFileInfo(QString::fromLocal8Bit(info.dli_fname)).absolutePath();
    }
    return QString();
}

extern "C" {

void init_plugin() {
    const QString dir = resolvePluginDir();
    std::printf("[word_plugin] init_plugin, dir=%s\n", dir.toUtf8().constData());
    if (!dir.isEmpty())
        word::WordController::instance()->setBaseDir(dir);
}

void attach_engine(QQmlEngine* engine) {
    if (engine && engine->rootContext()) {
        auto* controller = word::WordController::instance();
        QQmlEngine::setObjectOwnership(controller, QQmlEngine::CppOwnership);
        engine->rootContext()->setContextProperty(QStringLiteral("wordController"), controller);
    }
    std::printf("[word_plugin] attach_engine\n");
}

void destroy_plugin() {
    // 插件卸载/退出：落盘当前会话，避免进度丢失。
    word::WordController::instance()->saveState();
    std::printf("[word_plugin] destroy_plugin\n");
}

} // extern "C"
