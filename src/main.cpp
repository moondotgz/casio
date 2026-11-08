#include "gui/mainwindow.h"
#include "gui/modepage.h"
#include <QAbstractItemView>
#include <QInputMethodQueryEvent>
#include <QApplication>
#include <QStyleFactory>
#include <QTimer>

#ifdef Q_OS_IOS
// On iOS the system keyboard pops up whenever the focused widget reports that it accepts
// text input, and it covers the calculator keypad. Expression fields and data tables are
// driven by the keypad, so they answer "no" (QLineEdit ignores WA_InputMethodEnabled for this).
class NoSystemKeyboard : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject *obj, QEvent *e) override
    {
        if (e->type() != QEvent::InputMethodQuery) return false;
        if (!qobject_cast<ExprEdit *>(obj) && !qobject_cast<QAbstractItemView *>(obj)) return false;
        auto *q = static_cast<QInputMethodQueryEvent *>(e);
        if (!(q->queries() & Qt::ImEnabled)) return false;
        obj->event(e);   // let the widget answer the other queries (cursor position etc.)
        q->setValue(Qt::ImEnabled, false);
        return true;
    }
};
#endif

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("SciCalc991");
    QApplication::setOrganizationName("SciCalc991");
    QApplication::setStyle(QStyleFactory::create("Fusion"));
#ifdef Q_OS_IOS
    app.installEventFilter(new NoSystemKeyboard(&app));
#endif
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
