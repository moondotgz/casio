#include "gui/mainwindow.h"
#include <QApplication>
#include <QStyleFactory>
#include <QTimer>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("SciCalc991");
    QApplication::setOrganizationName("SciCalc991");
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    // Optional: --font "<family>" sets the interface font (keys, menus, tables)
    const QStringList pre = app.arguments();
    if (int fi = int(pre.indexOf("--font")); fi > 0 && fi + 1 < pre.size()) {
        QFont f(pre[fi + 1]);
        f.setPointSizeF(QApplication::font().pointSizeF());
        QApplication::setFont(f);
    }
    MainWindow w;
#ifdef Q_OS_IOS
    w.showMaximized();
#else
    w.show();
#endif

    // Optional automation: --script "<steps>" --shot <file.png>
    const QStringList args = app.arguments();
    int si = int(args.indexOf("--script")), shi = int(args.indexOf("--shot"));
    if (si > 0 && si + 1 < args.size()) {
        QString script = args[si + 1];
        QTimer::singleShot(50, &w, [&w, script] { w.runScript(script); });
    }
    if (shi > 0 && shi + 1 < args.size()) {
        QString file = args[shi + 1];
        QTimer::singleShot(400, &w, [&w, file] {
            QWidget *target = QApplication::activeModalWidget();
            (target ? target : static_cast<QWidget *>(&w))->grab().save(file);
            QApplication::quit();
        });
    }
    return app.exec();
}
