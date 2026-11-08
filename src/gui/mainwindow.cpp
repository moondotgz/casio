#include "mainwindow.h"
#include "calcpage.h"
#include "customizedialog.h"
#include "richtext.h"
#include "theme.h"
#include "dialogs.h"
#include "distpage.h"
#include "equationpage.h"
#include "inequalitypage.h"
#include "sheetpage.h"
#include "statpage.h"
#include "tablepage.h"
#include <QApplication>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QWidget(parent)
{
    setWindowTitle("SciCalc 991");
    setMinimumSize(430, 860);
    resize(470, 960);
    loadState();
    ThemeManager::instance().load();

    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(18, 14, 18, 18);
    v->setSpacing(10);

    // header
    auto *head = new QHBoxLayout;
    m_brand = new QLabel(this);
    m_tagline = new QLabel(this);
    m_gear = new QToolButton(this);
    m_gear->setToolTip("Customize appearance  (F8, or ALPHA MENU)\nCtrl+T / Ctrl+Shift+T: next / previous theme");
    m_gear->setFocusPolicy(Qt::NoFocus);
    m_gear->setCursor(Qt::PointingHandCursor);
    m_gear->setAutoRaise(true);
    connect(m_gear, &QToolButton::clicked, this, &MainWindow::openCustomize);
    head->addWidget(m_brand);
    head->addStretch();
    head->addWidget(m_tagline);
    head->addWidget(m_gear);
    v->addLayout(head);

    // LCD
    m_lcd = new QWidget(this);
    m_lcd->setObjectName("lcd");
    m_lcd->setAttribute(Qt::WA_StyledBackground);
    m_lcd->setStyleSheet(ModePage::styleSheet());
    m_lcd->setMinimumHeight(340);
    auto *lv = new QVBoxLayout(m_lcd);
    lv->setContentsMargins(4, 3, 4, 4);
    lv->setSpacing(2);
    auto *status = new QHBoxLayout;
    status->setSpacing(7);
    status->setContentsMargins(6, 0, 6, 0);
    for (auto *&l : m_ind) {
        l = new QLabel(m_lcd);
        status->addWidget(l);
    }
    status->addStretch();
    m_modeLabel = new QLabel(m_lcd);
    status->addWidget(m_modeLabel);
    lv->addLayout(status);
    auto *sep = new QFrame(m_lcd);
    sep->setObjectName("sep");
    lv->addWidget(sep);

    m_pages = new QStackedWidget(m_lcd);
    m_menu = new MenuPage(m_pages);
    m_pages->addWidget(m_menu);
    m_modes = {new CalcPage(CalcKind::Calculate, &m_state), new CalcPage(CalcKind::Complex, &m_state),
               new CalcPage(CalcKind::BaseN, &m_state),     new CalcPage(CalcKind::Matrix, &m_state),
               new CalcPage(CalcKind::Vector, &m_state),    new StatPage(&m_state),
               new DistPage(&m_state),                      new SheetPage(&m_state),
               new TablePage(&m_state),                     new EquationPage(&m_state),
               new InequalityPage(&m_state),                new RatioPage(&m_state)};
    for (auto *p : m_modes) {
        m_pages->addWidget(p);
        connect(p, &ModePage::statusChanged, this, &MainWindow::updateStatus);
        connect(p, &ModePage::message, this, &MainWindow::toast);
    }
    lv->addWidget(m_pages, 1);
    m_toast = new QLabel(m_lcd);
    m_toast->setObjectName("errorBox");
    m_toast->setAlignment(Qt::AlignCenter);
    m_toast->hide();
    lv->addWidget(m_toast);
    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    connect(m_toastTimer, &QTimer::timeout, m_toast, &QWidget::hide);
    v->addWidget(m_lcd, 1);

    // keypad
    m_keypad = new Keypad(this);
    v->addWidget(m_keypad, 0);
    connect(m_keypad, &Keypad::keyPressed, this, &MainWindow::onKey);
    connect(m_keypad, &Keypad::nav, this, &MainWindow::onNav);
    connect(m_menu, &MenuPage::modeSelected, this, &MainWindow::selectMode);

    // keyboard shortcuts
    auto sc = [this](const QKeySequence &k, const QString &action) {
        auto *s = new QShortcut(k, this);
        connect(s, &QShortcut::activated, this, [this, action] { dispatch(action); });
    };
    sc(Qt::Key_Escape, "c:AC");
    sc(Qt::Key_F1, "c:MENU");
    sc(Qt::Key_F2, "c:SETUP");
    sc(Qt::Key_F3, "c:OPTN");
    sc(Qt::Key_F4, "c:CATALOG");
    sc(Qt::Key_F5, "c:SD");
    sc(Qt::Key_F6, "c:CALC");
    sc(Qt::Key_F7, "c:SOLVE");
    sc(Qt::Key_F8, "c:CUSTOMIZE");
    sc(QKeySequence(Qt::CTRL | Qt::Key_T), "c:THEMENEXT");
    sc(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T), "c:THEMEPREV");
    sc(QKeySequence(Qt::CTRL | Qt::Key_K), "c:KEYPAD");
    sc(QKeySequence(Qt::CTRL | Qt::Key_Return), "c:APPROX");
    sc(QKeySequence(Qt::CTRL | Qt::Key_Enter), "c:APPROX");

    selectMode(std::clamp(m_mode, 0, int(m_modes.size()) - 1));
    makeFontSafe(m_lcd);
    connect(&ThemeManager::instance(), &ThemeManager::changed, this, &MainWindow::applyTheme);
    applyTheme();
    updateLayer();
}

MainWindow::~MainWindow() = default;

void MainWindow::runScript(const QString &script)
{
    for (const QString &step : script.split('|', Qt::SkipEmptyParts)) {
        const QString kind = step.section(':', 0, 0), arg = step.section(':', 1);
        if (kind == "key") {
            for (const KeyDef &d : m_keypad->keys())
                if (d.id == arg) { onKey(d); break; }
        } else if (kind == "type") {
            dispatch("i:" + arg);
        } else if (kind == "act") {
            dispatch(arg);
        } else if (kind == "nav") {
            onNav(arg);
        } else if (kind == "app") {   // app:uiFont=Cantarell, app:mathFont=..., app:uiScale=1.3
            Appearance a = appearance();
            const QString key = arg.section('=', 0, 0), val = arg.section('=', 1);
            if (key == "uiFont") a.uiFont = val;
            else if (key == "mathFont") a.mathFont = val;
            else if (key == "uiScale") a.uiScale = val.toDouble();
            else if (key == "mathScale") a.mathScale = val.toDouble();
            ThemeManager::instance().setAppearance(a);
        } else if (kind == "mode") {
            selectMode(arg.toInt());
        }
        QApplication::processEvents();
    }
}

void MainWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient g(0, 0, 0, height());
    const QColor top = themeColor("bodyTop"), bottom = themeColor("bodyBottom");
    g.setColorAt(0, top);
    g.setColorAt(0.45, QColor::fromRgbF((top.redF() + bottom.redF() * 2) / 3, (top.greenF() + bottom.greenF() * 2) / 3,
                                        (top.blueF() + bottom.blueF() * 2) / 3));
    g.setColorAt(1, bottom);
    p.fillRect(rect(), g);
    // display bezel
    QRectF bez = QRectF(m_lcd->geometry()).adjusted(-7, -7, 7, 7);
    QPainterPath path;
    path.addRoundedRect(bez, 10, 10);
    const QColor bezel = themeColor("bezel");
    p.fillPath(path, bezel);
    p.setPen(QPen(bezel.lightness() > 128 ? bezel.darker(125) : bezel.lighter(260), 1));
    p.drawPath(path);
}

ModePage *MainWindow::page() const { return m_modes.value(m_mode); }

bool MainWindow::menuShown() const { return m_pages->currentWidget() == m_menu; }

// ------------------------------------------------------------------ key handling

void MainWindow::updateLayer()
{
    m_keypad->setLayer(m_shift ? KeyLayer::Shift : m_alpha ? KeyLayer::Alpha : KeyLayer::Main);
    m_keypad->setShiftLatched(m_shift);
    m_keypad->setAlphaLatched(m_alpha);
    updateStatus();
}

void MainWindow::onKey(const KeyDef &def)
{
    if (def.id == "shift") { m_shift = !m_shift; m_alpha = false; updateLayer(); return; }
    if (def.id == "alpha") { m_alpha = !m_alpha; m_shift = false; updateLayer(); return; }
    const KeyLayer layer = m_shift ? KeyLayer::Shift : m_alpha ? KeyLayer::Alpha : KeyLayer::Main;
    m_shift = m_alpha = false;
    updateLayer();
    const QString action = def.actionFor(layer);

    if (menuShown()) {
        if (layer == KeyLayer::Main && m_menu->handleKey(def.id)) return;
        if (action == "c:AC" || action == "c:MENU") { selectMode(m_mode); return; }
        if (action == "c:SETUP" || action == "c:OFF" || action == "c:CUSTOMIZE") dispatch(action);
        return;
    }

    if (m_sto) {
        m_sto = false;
        static const QMap<QString, QString> vars = {{"neg", "A"}, {"dms", "B"}, {"inv", "C"}, {"sin", "D"},
                                                    {"cos", "E"}, {"tan", "F"}, {"rp", "x"}, {"sd", "y"},
                                                    {"mplus", "M"}};
        updateStatus();
        m_toast->hide();
        if (vars.contains(def.id)) page()->command("STO:" + vars[def.id]);
        return;
    }
    if (layer == KeyLayer::Main && page()->handleKey(def.id)) return;
    if (!action.isEmpty()) dispatch(action);
}

void MainWindow::onNav(const QString &dir)
{
    m_shift = m_alpha = false;
    updateLayer();
    if (menuShown()) {
        m_menu->navigate(dir);
        return;
    }
    page()->navigate(dir);
}

void MainWindow::dispatch(const QString &action)
{
    static const QStringList global = {"c:SETUP", "c:OFF", "c:AC", "c:MENU", "c:CUSTOMIZE", "c:THEMENEXT", "c:THEMEPREV", "c:KEYPAD"};
    if (menuShown() && !global.contains(action)) return;
    if (action.startsWith("i:")) { page()->insertText(action.mid(2), false); return; }
    if (action.startsWith("n:")) { page()->insertText(action.mid(2), true); return; }
    if (!action.startsWith("c:")) { page()->insertText(action, false); return; }
    const QString cmd = action.mid(2);
    if (cmd == "AC") {
        if (menuShown()) { selectMode(m_mode); return; }
        m_sto = false;
        page()->allClear();
    } else if (cmd == "DEL") {
        page()->del();
    } else if (cmd == "UNDO") {
        if (auto *e = qobject_cast<QLineEdit *>(QApplication::focusWidget())) e->undo();
    } else if (cmd == "EXE") {
        page()->execute(false);
    } else if (cmd == "APPROX") {
        page()->execute(true);
    } else if (cmd == "MENU") {
        if (menuShown()) selectMode(m_mode);
        else showMenu();
    } else if (cmd == "SETUP") {
        openSetup();
    } else if (cmd == "CUSTOMIZE") {
        openCustomize();
    } else if (cmd == "THEMENEXT" || cmd == "THEMEPREV") {
        ThemeManager::instance().cycleTheme(cmd == "THEMENEXT" ? 1 : -1);
        toast("Theme: " + ThemeManager::instance().theme().name);
    } else if (cmd == "KEYPAD") {
        Appearance a = appearance();
        a.showKeypad = !a.showKeypad;
        ThemeManager::instance().setAppearance(a);
        ThemeManager::instance().save();
    } else if (cmd == "OPTN") {
        QList<MenuItem> items = page()->optnMenu();
        if (items.isEmpty()) toast("No options in this mode");
        else popup(items);
    } else if (cmd == "CATALOG") {
        popup(catalogMenu());
    } else if (cmd == "CONST") {
        popup(constantsMenu());
    } else if (cmd == "CONV") {
        popup(conversionsMenu());
    } else if (cmd == "STO") {
        m_sto = true;
        toast("STO \u2014 press a variable key: A\u2013F, x, y or M");
    } else if (cmd == "RCL") {
        openRecall();
    } else if (cmd == "RESET") {
        openReset();
    } else if (cmd == "OFF") {
        close();
    } else {
        page()->command(cmd);
    }
    updateStatus();
}

void MainWindow::popup(const QList<MenuItem> &items)
{
    QMenu menu(this);
    menu.setStyleSheet(ThemeManager::instance().menuStyleSheet());
    std::function<void(QMenu *, const QList<MenuItem> &)> build = [&](QMenu *m, const QList<MenuItem> &list) {
        int n = 1;
        for (const MenuItem &it : list) {
            QString label = list.size() <= 9 && !it.label.startsWith(QChar('0')) && !it.label.at(0).isDigit()
                                ? QString("&%1: %2").arg(n).arg(it.label)
                                : it.label;
            ++n;
            if (!it.children.isEmpty()) {
                QMenu *sub = m->addMenu(fontSafe(label, menu.font()));
                sub->setStyleSheet(menu.styleSheet());
                build(sub, it.children);
            } else {
                QAction *a = m->addAction(fontSafe(label, menu.font()));
                a->setData(it.insert);
            }
        }
    };
    build(&menu, items);
    QAction *chosen = menu.exec(m_lcd->mapToGlobal(QPoint(14, 28)));
    if (chosen && !chosen->data().toString().isEmpty()) {
        QString act = chosen->data().toString();
        if (!act.startsWith("i:") && !act.startsWith("c:") && !act.startsWith("n:")) act = "i:" + act;
        dispatch(act);
    }
}

// ------------------------------------------------------------------ modes

void MainWindow::showMenu()
{
    m_menu->setSelected(m_mode);
    m_pages->setCurrentWidget(m_menu);
    m_menu->setFocus();
    updateStatus();
}

void MainWindow::selectMode(int index)
{
    m_mode = index;
    m_pages->setCurrentWidget(m_modes[index]);
    m_modes[index]->activated();
    updateStatus();
}

void MainWindow::settingsChanged()
{
    for (auto *p : m_modes) p->settingsChanged();
    updateStatus();
    saveState();
}

void MainWindow::openSetup()
{
    SetupDialog d(m_state.settings(), this);
    if (d.exec() == QDialog::Accepted) settingsChanged();
}

void MainWindow::openRecall()
{
    RecallDialog d(m_state.ctx, this);
    if (d.exec() == QDialog::Accepted && !d.chosen().isEmpty() && !menuShown()) page()->insertText(d.chosen(), true);
}

void MainWindow::openCustomize()
{
    CustomizeDialog d(this);
    d.exec();
}

void MainWindow::applyTheme()
{
    auto &tm = ThemeManager::instance();
    const Appearance &a = tm.appearance();
    QApplication::setPalette(tm.dialogPalette());
    const QFont uiFont = tm.interfaceFont();
    if (QApplication::font() != uiFont) QApplication::setFont(uiFont);
    m_lcd->setStyleSheet(tm.lcdStyleSheet());

    m_brand->setText(QString("%1 <span style='color:%2; font-weight:normal'>%3</span>")
                         .arg(a.brand.toHtmlEscaped(), themeColor("brandDim").name(), a.model.toHtmlEscaped()));
    m_brand->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:bold; letter-spacing:2px; background:transparent;")
                               .arg(themeColor("brand").name()).arg(int(std::round(18 * a.uiScale))));
    m_tagline->setText(a.tagline);
    m_tagline->setStyleSheet(QString("color:%1; font-size:%2px; font-style:italic; background:transparent;")
                                 .arg(themeColor("brandDim").name()).arg(int(std::round(11 * a.uiScale))));
    // gear icon drawn with a path so it does not depend on the font having ⚙
    auto gear = [](const QColor &c, int px) {
        QPixmap pm(px * 2, px * 2);
        pm.setDevicePixelRatio(2);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        const QPointF ctr(px / 2.0, px / 2.0);
        const qreal ro = px * 0.48, ri = px * 0.36;
        QPainterPath path;
        for (int i = 0; i < 16; ++i) {
            qreal a0 = (i - 0.5) * M_PI / 8, a1 = (i + 0.5) * M_PI / 8, r = (i % 2) ? ri : ro;
            QPointF p0 = ctr + QPointF(std::cos(a0) * r, std::sin(a0) * r), p1 = ctr + QPointF(std::cos(a1) * r, std::sin(a1) * r);
            if (i == 0) path.moveTo(p0); else path.lineTo(p0);
            path.lineTo(p1);
        }
        path.closeSubpath();
        path.addEllipse(ctr, px * 0.15, px * 0.15);
        path.setFillRule(Qt::OddEvenFill);
        p.fillPath(path, c);
        return QIcon(pm);
    };
    const int gearPx = int(std::round(16 * a.uiScale));
    QIcon icon = gear(themeColor("brandDim"), gearPx);
    icon.addPixmap(gear(themeColor("brand"), gearPx).pixmap(gearPx, gearPx), QIcon::Active);
    m_gear->setIcon(icon);
    m_gear->setIconSize(QSize(gearPx, gearPx));
    m_gear->setStyleSheet("QToolButton { border:0; background:transparent; padding:0 2px; }");
    QString title = (a.brand + " " + a.model).trimmed();
    setWindowTitle(title.isEmpty() ? "SciCalc 991" : title);

    // keypad: visibility, legend height, repaint every key
    for (QWidget *w : m_keypad->findChildren<QWidget *>()) {
        w->updateGeometry();
        w->update();
    }
    if (a.showKeypad != m_keypadShown) {
        m_keypadShown = a.showKeypad;
        m_keypad->setVisible(a.showKeypad);
        if (a.showKeypad) {
            setMinimumSize(430, 860);
            resize(std::max(width(), 470), std::max(height(), 960));
        } else {
            setMinimumSize(340, 360);
            resize(width(), 520);
        }
    }
    if (a.alwaysOnTop != m_onTop) {
        m_onTop = a.alwaysOnTop;
        bool vis = isVisible();
        setWindowFlag(Qt::WindowStaysOnTopHint, m_onTop);
        if (vis) show();
    }
    setWindowOpacity(a.opacity);

    makeFontSafe(m_lcd);   // the new font may lack glyphs the old one had
    for (auto *p : m_modes) p->settingsChanged();
    m_menu->update();
    updateStatus();
    update();
}

void MainWindow::openReset()
{
    QMessageBox box(this);
    box.setWindowTitle("RESET");
    box.setText("Reset what?");
    auto *setup = box.addButton("Setup Data", QMessageBox::ActionRole);
    auto *vars = box.addButton("Variable Memory", QMessageBox::ActionRole);
    auto *all = box.addButton("Initialize All", QMessageBox::DestructiveRole);
    box.addButton(QMessageBox::Cancel);
    box.exec();
    auto *clicked = box.clickedButton();
    if (clicked == setup || clicked == all) m_state.ctx.settings = Settings{};
    if (clicked == vars || clicked == all) {
        Settings keep = m_state.ctx.settings;
        m_state.ctx.vars.clear();
        Context fresh;
        m_state.ctx.vars = fresh.vars;
        m_state.ctx.settings = keep;
    }
    if (clicked == setup || clicked == vars || clicked == all) {
        settingsChanged();
        toast(clicked == setup ? "Setup reset" : clicked == vars ? "Variables cleared" : "All reset");
    }
}

// ------------------------------------------------------------------ status

void MainWindow::updateStatus()
{
    const Settings &s = m_state.settings();
    const QString ink = themeColor("lcdText").name();
    const int statusPx = int(std::round(11 * appearance().uiScale));
    const QString on = QString("font-size:%3px; font-weight:bold; color:%1; background:%2; padding:0 3px; border-radius:2px;")
                           .arg(themeColor("lcdAccentText").name(), themeColor("lcdAccent").name()).arg(statusPx);
    const QString off = QString("font-size:%2px; font-weight:bold; color:%1;").arg(ink).arg(statusPx);
    m_ind[0]->setText(m_shift ? "S" : "");
    m_ind[0]->setStyleSheet(m_shift ? on : off);
    m_ind[1]->setText(m_alpha ? "A" : "");
    m_ind[1]->setStyleSheet(m_alpha ? on : off);
    for (int i = 2; i < 8; ++i) m_ind[i]->setStyleSheet(off);
    m_modeLabel->setStyleSheet(QString("font-size:%2px; color:%1;").arg(themeColor("lcdDim").name()).arg(statusPx));
    m_ind[2]->setText(m_state.ctx.vars.value("M").z != cplx(0, 0) ? "M" : "");
    m_ind[3]->setText(m_sto ? "STO" : "");
    m_ind[4]->setText(s.angle == AngleUnit::Deg ? "D" : s.angle == AngleUnit::Rad ? "R" : "G");
    m_ind[5]->setText(s.format == NumFormat::Fix ? QString("FIX%1").arg(s.digits)
                      : s.format == NumFormat::Sci ? QString("SCI%1").arg(s.digits)
                                                   : QString());
    m_ind[6]->setText(s.io == IOMode::MathMath ? "Math▲" : s.io == IOMode::MathDec ? "Math▲ ≈" : s.io == IOMode::LineLine ? "Line" : "Line ≈");
    m_ind[7]->setText(menuShown() ? QString() : page()->indicator());
    m_modeLabel->setText(menuShown() ? "MENU" : page()->modeName());
}

void MainWindow::toast(const QString &text)
{
    m_toast->setText(text);
    m_toast->show();
    m_toastTimer->start(text.size() > 40 ? 4000 : 2200);
}

// ------------------------------------------------------------------ persistence

void MainWindow::closeEvent(QCloseEvent *e)
{
    saveState();
    e->accept();
}

void MainWindow::loadState()
{
    QSettings q("SciCalc991", "SciCalc991");
    Settings &s = m_state.settings();
    s.angle = AngleUnit(q.value("setup/angle", int(s.angle)).toInt());
    s.format = NumFormat(q.value("setup/format", int(s.format)).toInt());
    s.digits = q.value("setup/digits", s.digits).toInt();
    s.io = IOMode(q.value("setup/io", int(s.io)).toInt());
    s.frac = FracFormat(q.value("setup/frac", int(s.frac)).toInt());
    s.complexForm = ComplexForm(q.value("setup/complex", int(s.complexForm)).toInt());
    s.statFrequency = q.value("setup/statFreq", s.statFrequency).toBool();
    s.sheetAutoCalc = q.value("setup/sheetAuto", s.sheetAutoCalc).toBool();
    s.decimalComma = q.value("setup/decimalComma", s.decimalComma).toBool();
    s.digitSeparator = q.value("setup/digitSep", s.digitSeparator).toBool();
    s.tableUseG = q.value("setup/tableG", s.tableUseG).toBool();
    m_mode = q.value("mode", 0).toInt();
    q.beginGroup("vars");
    for (const QString &k : q.childKeys()) {
        QVariantList l = q.value(k).toList();
        if (l.size() < 3) continue;
        int kind = l[0].toInt(), rows = l[1].toInt(), cols = l[2].toInt();
        if (kind == 0 && l.size() >= 5) {
            m_state.ctx.vars[k] = Value(cplx(l[3].toDouble(), l[4].toDouble()));
        } else if ((kind == 1 || kind == 2) && rows > 0 && cols > 0 && rows <= 4 && cols <= 4 &&
                   l.size() >= 3 + 2 * rows * cols) {
            Value v = kind == 1 ? Value::matrix(rows, cols) : Value::vector(rows);
            for (int i = 0; i < rows * cols; ++i) v.m[size_t(i)] = cplx(l[3 + 2 * i].toDouble(), l[4 + 2 * i].toDouble());
            m_state.ctx.vars[k] = v;
        }
    }
    q.endGroup();
}

void MainWindow::saveState()
{
    QSettings q("SciCalc991", "SciCalc991");
    const Settings &s = m_state.settings();
    q.setValue("setup/angle", int(s.angle));
    q.setValue("setup/format", int(s.format));
    q.setValue("setup/digits", s.digits);
    q.setValue("setup/io", int(s.io));
    q.setValue("setup/frac", int(s.frac));
    q.setValue("setup/complex", int(s.complexForm));
    q.setValue("setup/statFreq", s.statFrequency);
    q.setValue("setup/sheetAuto", s.sheetAutoCalc);
    q.setValue("setup/decimalComma", s.decimalComma);
    q.setValue("setup/digitSep", s.digitSeparator);
    q.setValue("setup/tableG", s.tableUseG);
    q.setValue("mode", m_mode);
    q.remove("vars");
    q.beginGroup("vars");
    for (auto it = m_state.ctx.vars.begin(); it != m_state.ctx.vars.end(); ++it) {
        const Value &v = it.value();
        QVariantList l;
        l << (v.isScalar() ? 0 : v.isMatrix() ? 1 : 2) << v.rows << v.cols;
        if (v.isScalar()) l << v.z.real() << v.z.imag();
        else for (const auto &e : v.m) l << e.real() << e.imag();
        q.setValue(it.key(), l);
    }
    q.endGroup();
}
