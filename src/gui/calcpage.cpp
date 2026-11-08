#include "calcpage.h"
#include "core/numeric.h"
#include "theme.h"
#include <QButtonGroup>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <cmath>

namespace {

bool isContinuation(const QString &t)
{
    static const QStringList ops = {"+", "\u2212", "-", "\u00D7", "*", "\u00F7", "/", "^", "\u00B2", "\u00B3",
                                    "\u207B\u00B9", "!", "%", "\u1D07", "nPr", "nCr", "\u2220", "\u00F7R", "\u2192",
                                    "\u00B0", "\u02B3", "\u1D4D", "and", "or", "xor", "xnor", "\u25B6", "\u231F"};
    for (const auto &o : ops)
        if (t.startsWith(o)) return true;
    if (t.startsWith(QChar(0x2039)) && t.contains(QChar(0x25B6))) return true;   // unit conversion
    return false;
}

QString editableValue(const cplx &z, const Settings &s)
{
    Settings t = s;
    t.format = NumFormat::Norm1;
    return formatComplex(z, t);
}

} // namespace

CalcPage::CalcPage(CalcKind kind, AppState *st, QWidget *parent) : ModePage(st, parent), m_kind(kind)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(buildCalcView());
    m_stack->addWidget(buildPromptPanel());
    m_stack->addWidget(buildEditorPanel());
    v->addWidget(m_stack);
}

// ------------------------------------------------------------------ construction

QWidget *CalcPage::buildToolbar()
{
    auto *bar = new QWidget;
    auto *h = new QHBoxLayout(bar);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);
    if (m_kind == CalcKind::BaseN) {
        m_baseButtons = new QButtonGroup(bar);
        m_baseButtons->setExclusive(true);
        const QList<QPair<QString, int>> bases = {{"DEC", 10}, {"HEX", 16}, {"BIN", 2}, {"OCT", 8}};
        for (const auto &b : bases) {
            auto *btn = new QPushButton(b.first, bar);
            btn->setCheckable(true);
            btn->setFocusPolicy(Qt::NoFocus);
            btn->setChecked(b.second == m_base);
            m_baseButtons->addButton(btn, b.second);
            h->addWidget(btn);
        }
        connect(m_baseButtons, &QButtonGroup::idClicked, this, [this](int id) { setBase(id); });
        h->addStretch();
    } else {
        const QString prefix = m_kind == CalcKind::Matrix ? "Mat" : "Vct";
        for (const QString s : {"A", "B", "C", "D"}) {
            auto *btn = new QPushButton(prefix + s, bar);
            btn->setFocusPolicy(Qt::NoFocus);
            btn->setToolTip("Define / edit " + prefix + s);
            connect(btn, &QPushButton::clicked, this, [this, prefix, s] { openEditor(prefix + s); });
            m_defineButtons << btn;
            h->addWidget(btn);
        }
        h->addStretch();
    }
    return bar;
}

QWidget *CalcPage::buildCalcView()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(6, 4, 6, 6);
    v->setSpacing(3);
    if (m_kind == CalcKind::BaseN || m_kind == CalcKind::Matrix || m_kind == CalcKind::Vector) v->addWidget(buildToolbar());

    m_scroll = new QScrollArea(w);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_historyHost = new QWidget;
    m_historyLayout = new QVBoxLayout(m_historyHost);
    m_historyLayout->setContentsMargins(0, 0, 0, 0);
    m_historyLayout->setSpacing(0);
    m_historyLayout->addStretch();
    m_scroll->setWidget(m_historyHost);
    v->addWidget(m_scroll, 1);
    m_historySpacer = new QWidget(w);   // takes the space when history is hidden
    m_historySpacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_historySpacer->hide();
    v->addWidget(m_historySpacer, 1);

    auto *sep = new QFrame(w);
    sep->setObjectName("sep");
    v->addWidget(sep);

    m_preview = new MathView(w);
    m_preview->setFixedLines(2);
    v->addWidget(m_preview);

    m_result = new MathView(w);
    m_result->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_result->style().size = 26;
    m_result->setFixedLines(1);
    v->addWidget(m_result);

    m_error = new QLabel(w);
    m_error->setObjectName("errorBox");
    m_error->setAlignment(Qt::AlignCenter);
    m_error->hide();
    v->addWidget(m_error);

    m_note = new QLabel(w);
    m_note->setObjectName("hint");
    m_note->setAlignment(Qt::AlignRight);
    m_note->hide();
    v->addWidget(m_note);

    m_baseInfo = new QLabel(w);
    m_baseInfo->setObjectName("hint");
    m_baseInfo->setWordWrap(true);
    m_baseInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_baseInfo->hide();
    v->addWidget(m_baseInfo);

    m_edit = new ExprEdit(w);
    m_edit->setPlaceholderText(m_kind == CalcKind::BaseN ? "Enter an integer expression"
                                                         : "Enter an expression, then press =");
    v->addWidget(m_edit);

    connect(m_edit, &QLineEdit::textChanged, this, [this] { updatePreview(); });
    connect(m_edit, &QLineEdit::returnPressed, this, [this] { execute(false); });
    connect(m_edit, &ExprEdit::upPressed, this, [this] { navigate("UP"); });
    connect(m_edit, &ExprEdit::downPressed, this, [this] { navigate("DOWN"); });
    connect(m_edit, &ExprEdit::typedWhileFresh, this, [this](const QString &t) { insertText(t, false); });
    return w;
}

QWidget *CalcPage::buildPromptPanel()
{
    m_promptPanel = new QWidget;
    auto *v = new QVBoxLayout(m_promptPanel);
    v->setContentsMargins(10, 8, 10, 8);
    m_promptTitle = lcdLabel("CALC", m_promptPanel, true);
    v->addWidget(m_promptTitle);
    m_promptExpr = new MathView(m_promptPanel);
    v->addWidget(m_promptExpr);
    auto *formHost = new QWidget(m_promptPanel);
    m_promptForm = new QFormLayout(formHost);
    m_promptForm->setContentsMargins(0, 4, 0, 4);
    v->addWidget(formHost);
    v->addStretch();
    auto *hint = new QLabel("[=] : Execute      [AC] : Cancel      [\u25B2][\u25BC] : Move", m_promptPanel);
    hint->setObjectName("hint");
    v->addWidget(hint);
    return m_promptPanel;
}

QWidget *CalcPage::buildEditorPanel()
{
    m_editorPanel = new QWidget;
    auto *v = new QVBoxLayout(m_editorPanel);
    v->setContentsMargins(8, 6, 8, 6);
    m_editorTitle = lcdLabel("MatA", m_editorPanel, true);
    v->addWidget(m_editorTitle);
    auto *dims = new QHBoxLayout;
    bool vec = m_kind == CalcKind::Vector;
    dims->addWidget(lcdLabel(vec ? "Dimension" : "Rows", m_editorPanel));
    m_rows = new QSpinBox(m_editorPanel);
    m_rows->setRange(vec ? 2 : 1, vec ? 3 : 4);
    m_rows->setValue(vec ? 3 : 2);
    dims->addWidget(m_rows);
    m_cols = new QSpinBox(m_editorPanel);
    m_cols->setRange(1, 4);
    m_cols->setValue(2);
    if (!vec) {
        dims->addSpacing(12);
        dims->addWidget(lcdLabel("Columns", m_editorPanel));
        dims->addWidget(m_cols);
    } else {
        m_cols->hide();
    }
    dims->addStretch();
    v->addLayout(dims);
    m_table = new QTableWidget(m_editorPanel);
    m_table->setItemDelegate(new ExprDelegate(m_table));
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::AllEditTriggers);
    v->addWidget(m_table, 1);
    auto *btns = new QHBoxLayout;
    auto *done = new QPushButton("Store", m_editorPanel);
    auto *clear = new QPushButton("Clear", m_editorPanel);
    auto *back = new QPushButton("Back", m_editorPanel);
    for (auto *b : {done, clear, back}) b->setFocusPolicy(Qt::NoFocus);
    btns->addWidget(done);
    btns->addWidget(clear);
    btns->addStretch();
    btns->addWidget(back);
    v->addLayout(btns);
    auto *hint = new QLabel("[=] : Store      [AC] : Back      Cells accept expressions", m_editorPanel);
    hint->setObjectName("hint");
    v->addWidget(hint);
    connect(done, &QPushButton::clicked, this, [this] { commitEditor(); });
    connect(back, &QPushButton::clicked, this, [this] { m_stack->setCurrentIndex(0); m_edit->setFocus(); });
    connect(clear, &QPushButton::clicked, this, [this] {
        for (int r = 0; r < m_table->rowCount(); ++r)
            for (int c = 0; c < m_table->columnCount(); ++c) m_table->setItem(r, c, new QTableWidgetItem("0"));
    });
    connect(m_rows, &QSpinBox::valueChanged, this, [this] { resizeEditorTable(); });
    connect(m_cols, &QSpinBox::valueChanged, this, [this] { resizeEditorTable(); });
    return m_editorPanel;
}

// ------------------------------------------------------------------ basics

QString CalcPage::modeName() const
{
    switch (m_kind) {
    case CalcKind::Calculate: return "Calculate";
    case CalcKind::Complex: return "Complex";
    case CalcKind::BaseN: return "Base-N";
    case CalcKind::Matrix: return "Matrix";
    case CalcKind::Vector: return "Vector";
    }
    return {};
}

QString CalcPage::indicator() const
{
    switch (m_kind) {
    case CalcKind::Complex: return "CMPLX";
    case CalcKind::BaseN: return m_base == 10 ? "DEC" : m_base == 16 ? "HEX" : m_base == 2 ? "BIN" : "OCT";
    case CalcKind::Matrix: return "MAT";
    case CalcKind::Vector: return "VCT";
    default: return {};
    }
}

QWidget *CalcPage::editor() const { return m_edit; }

void CalcPage::prepareContext()
{
    ModePage::prepareContext();
    ctx().complexMode = m_kind == CalcKind::Complex;
    ctx().base = m_kind == CalcKind::BaseN ? m_base : 0;
}

void CalcPage::activated()
{
    if (m_stack->currentIndex() == 0) m_edit->setFocus();
    updateDefineButtons();
    updatePreview();
}

void CalcPage::settingsChanged()
{
    m_scroll->setVisible(appearance().showHistory);
    m_historySpacer->setVisible(!appearance().showHistory);
    rebuildHistory();
    renderCurrent();
    updatePreview();
}

QString CalcPage::baseString(qint64 v, int base) const
{
    QString s = formatBaseN(v, base);
    if (base == 2) {
        int pad = (4 - int(s.size()) % 4) % 4;
        s = QString(pad, '0') + s;
        QString g;
        for (int i = 0; i < s.size(); ++i) {
            if (i && i % 4 == 0) g += ' ';
            g += s[i];
        }
        return g;
    }
    return s;
}

void CalcPage::setBase(int base)
{
    m_base = base;
    if (m_baseButtons && m_baseButtons->button(base)) m_baseButtons->button(base)->setChecked(true);
    if (m_current && !m_current->error) m_current->base = base;
    for (auto &e : m_entries) e.base = base;
    renderCurrent();
    rebuildHistory();
    updatePreview();
    emit statusChanged();
}

// ------------------------------------------------------------------ rendering

void CalcPage::updatePreview()
{
    LexOptions lex;
    lex.base = m_kind == CalcKind::BaseN ? m_base : 0;
    m_preview->setBox(layoutSyntax(m_edit->text(), mathStyle(20), lex));
}

BoxPtr CalcPage::resultBox(const CalcEntry &e, qreal size)
{
    MathStyle s = mathStyle(size);
    if (!e.special.isEmpty()) return layoutSyntax(e.special, s);
    if (m_kind == CalcKind::BaseN) return layoutText(baseString(qint64(e.value.re()), e.base ? e.base : m_base), s);
    if (!e.outputs.isEmpty()) {
        std::vector<BoxPtr> lines;
        for (const auto &o : e.outputs) lines.push_back(layoutLabeled(o.first, layoutValue(o.second, settings(), e.opts, s), s));
        return layoutLines(lines, true);
    }
    return layoutValue(e.value, settings(), e.opts, s);
}

void CalcPage::renderCurrent()
{
    m_error->hide();
    m_note->hide();
    m_baseInfo->hide();
    if (!m_current) {
        m_result->clear();
        return;
    }
    const CalcEntry &e = *m_current;
    if (e.error) {
        m_result->clear();
        QString hint = e.err.kind == ErrKind::Syntax || e.err.kind == ErrKind::Math || e.err.kind == ErrKind::Argument ||
                               e.err.kind == ErrKind::Dimension
                           ? "\n[AC] : Cancel      [\u25C0][\u25B6] : Goto"
                           : "\n[AC] : Cancel";
        m_error->setText(errorName(e.err.kind) + hint);
        m_error->show();
        return;
    }
    m_result->setBox(resultBox(e, 26));
    if (!e.note.isEmpty()) {
        m_note->setText(e.note);
        m_note->show();
    }
    if (m_kind == CalcKind::BaseN) {
        qint64 v = qint64(e.value.re());
        m_baseInfo->setText(QString("DEC %1     HEX %2     OCT %3\nBIN %4")
                                .arg(formatBaseN(v, 10), formatBaseN(v, 16), formatBaseN(v, 8), baseString(v, 2)));
        m_baseInfo->show();
    }
}

void CalcPage::addHistoryWidget(const CalcEntry &e)
{
    auto *w = new QWidget(m_historyHost);
    w->setObjectName("historyEntry");
    auto *l = new QVBoxLayout(w);
    l->setContentsMargins(2, 3, 2, 3);
    l->setSpacing(0);
    LexOptions lex;
    lex.base = m_kind == CalcKind::BaseN ? m_base : 0;
    auto *in = new MathView(w);
    in->setBox(layoutSyntax(e.input, mathStyle(15), lex));
    in->setCursor(Qt::PointingHandCursor);
    in->setToolTip("Click to recall this expression");
    auto *out = new MathView(w);
    out->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    out->setBox(e.error ? layoutText(errorName(e.err.kind), mathStyle(15)) : resultBox(e, 17));
    l->addWidget(in);
    l->addWidget(out);
    QString input = e.input;
    connect(in, &MathView::clicked, this, [this, input] {
        m_edit->setText(input);
        m_edit->fresh = false;
        m_edit->setFocus();
    });
    m_historyLayout->addWidget(w);
    while (m_historyLayout->count() > 61) {   // stretch + 60 entries
        QLayoutItem *it = m_historyLayout->takeAt(1);
        delete it->widget();
        delete it;
    }
    QTimer::singleShot(0, this, [this] { m_scroll->verticalScrollBar()->setValue(m_scroll->verticalScrollBar()->maximum()); });
}

void CalcPage::rebuildHistory()
{
    while (m_historyLayout->count() > 1) {
        QLayoutItem *it = m_historyLayout->takeAt(1);
        delete it->widget();
        delete it;
    }
    for (const auto &e : m_entries) addHistoryWidget(e);
}

void CalcPage::archiveCurrent()
{
    if (!m_current) return;
    m_entries.append(*m_current);
    while (m_entries.size() > 60) m_entries.removeFirst();
    addHistoryWidget(*m_current);
    m_current.reset();
    renderCurrent();
}

// ------------------------------------------------------------------ evaluation

void CalcPage::evaluateInto(CalcEntry &e, bool approx)
{
    e.opts = DisplayOpts{};
    e.opts.approx = approx;
    e.base = m_kind == CalcKind::BaseN ? m_base : 0;
    try {
        prepareContext();
        ParseOptions po;
        po.lex.base = ctx().base;
        auto stmts = parseStatements(e.input, po);
        Evaluator ev(ctx());
        Value v;
        for (const auto &s : stmts) {
            v = ev.evaluate(s);
            ctx().pushAns(v);
        }
        e.value = v;
        e.outputs = ctx().outputs;
        e.opts.form = ctx().forcedForm;
        e.error = false;
    } catch (const CalcError &err) {
        e.error = true;
        e.err = err;
    }
}

void CalcPage::runExpression(const QString &text, bool approx)
{
    if (m_current) {
        if (m_edit->fresh) archiveCurrent();   // "= = =" repeats create new entries
        else m_current.reset();               // the edited expression replaces the shown one
    }
    CalcEntry e;
    e.input = text;
    evaluateInto(e, approx);
    m_current = e;
    renderCurrent();
    if (m_edit->text() != text) m_edit->setText(text);
    if (!e.error) {
        m_edit->fresh = true;
        m_edit->setCursorPosition(int(text.size()));
    } else {
        m_edit->fresh = false;
        if (e.err.pos >= 0) m_edit->setCursorPosition(std::min<int>(e.err.pos, int(text.size())));
    }
    m_recall = -1;
    emit statusChanged();
}

void CalcPage::execute(bool approx)
{
    if (m_stack->currentIndex() == 1) { finishPrompt(); return; }
    if (m_stack->currentIndex() == 2) { commitEditor(); return; }
    QString text = m_edit->text().trimmed();
    if (text.isEmpty()) {
        if (m_current && !m_current->error && approx) {
            m_current->opts.approx = true;
            renderCurrent();
        }
        return;
    }
    if (approx && m_current && m_edit->fresh && !m_current->error && text == m_current->input) {
        m_current->opts.approx = true;
        renderCurrent();
        return;
    }
    runExpression(text, approx);
}

bool CalcPage::ensureEvaluated()
{
    QString text = m_edit->text().trimmed();
    if (!text.isEmpty() && (!m_current || !m_edit->fresh)) runExpression(text, false);
    else if (text.isEmpty() && !m_current) runExpression("Ans", false);
    return m_current && !m_current->error;
}

void CalcPage::storeTo(const QString &var)
{
    if (!ensureEvaluated()) return;
    if (!m_current->value.isScalar() || m_kind == CalcKind::BaseN) {
        if (!m_current->value.isScalar()) {
            m_current->error = true;
            m_current->err = CalcError(ErrKind::Dimension);
            renderCurrent();
            return;
        }
    }
    ctx().setVar(var, m_current->value);
    m_current->note = QString("Ans \u2192 %1").arg(var);
    renderCurrent();
    emit statusChanged();
}

void CalcPage::memoryAdd(bool subtract)
{
    if (!ensureEvaluated()) return;
    if (!m_current->value.isScalar()) return;
    cplx m = ctx().vars.value("M").z;
    cplx v = m_current->value.z;
    ctx().setVar("M", Value(subtract ? m - v : m + v));
    m_current->note = subtract ? "M\u2212" : "M+";
    renderCurrent();
    emit statusChanged();
}

// ------------------------------------------------------------------ input handling

void CalcPage::insertText(const QString &text, bool startsNew)
{
    if (m_stack->currentIndex() == 2) beginCellInput(m_table);
    if (m_stack->currentIndex() != 0) {
        ModePage::insertText(text, startsNew);
        return;
    }
    if (m_current && m_current->error) {
        m_current.reset();
        renderCurrent();
    }
    if (m_edit->fresh) {
        bool cont = !startsNew && isContinuation(text);
        archiveCurrent();
        m_edit->clear();
        if (cont) m_edit->setText("Ans");
        m_edit->fresh = false;
    }
    m_edit->insertTemplate(text);
    m_edit->setFocus();
    m_recall = -1;
}

void CalcPage::allClear()
{
    if (m_stack->currentIndex() == 1) { cancelPrompt(); return; }
    if (m_stack->currentIndex() == 2) { m_stack->setCurrentIndex(0); m_edit->setFocus(); return; }
    m_edit->clear();
    m_edit->fresh = false;
    if (m_current) {
        if (m_current->error) m_current.reset();
        else archiveCurrent();
    }
    renderCurrent();
    m_recall = -1;
    m_edit->setFocus();
}

void CalcPage::del()
{
    if (m_stack->currentIndex() != 0) { ModePage::del(); return; }
    if (m_current && m_current->error) {
        m_current.reset();
        renderCurrent();
    }
    if (m_edit->fresh) {
        m_edit->fresh = false;
        m_edit->setCursorPosition(int(m_edit->text().size()));
    }
    m_edit->smartBackspace();
}

void CalcPage::navigate(const QString &dir)
{
    if (m_stack->currentIndex() == 1) {
        if (dir == "UP") focusPreviousChild();
        else if (dir == "DOWN") focusNextChild();
        else ModePage::navigate(dir);
        return;
    }
    if (m_stack->currentIndex() == 2) { navigateTable(m_table, dir); return; }

    if (dir == "LEFT" || dir == "RIGHT") {
        if (m_current && m_current->error) {
            int pos = m_current->err.pos;
            m_current.reset();
            renderCurrent();
            m_edit->setCursorPosition(pos >= 0 ? pos : int(m_edit->text().size()));
            m_edit->setFocus();
            return;
        }
        if (m_edit->fresh) {
            m_edit->fresh = false;
            m_edit->setCursorPosition(dir == "LEFT" ? int(m_edit->text().size()) : 0);
            return;
        }
        ModePage::navigate(dir);
        return;
    }
    // replay history
    if (m_recall == -1) {
        bool wasFresh = m_current && m_edit->fresh && !m_current->error;
        if (m_current && !m_current->error) archiveCurrent();
        else m_current.reset();
        if (m_entries.isEmpty()) return;
        if (dir == "DOWN") return;
        m_recall = int(m_entries.size()) - (wasFresh && m_entries.size() > 1 ? 2 : 1);
    } else {
        m_recall += dir == "UP" ? -1 : 1;
    }
    if (m_recall >= m_entries.size()) {
        m_recall = -1;
        m_edit->clear();
        return;
    }
    m_recall = std::max(0, m_recall);
    m_edit->setText(m_entries[m_recall].input);
    m_edit->fresh = false;
    m_edit->setFocus();
}

bool CalcPage::handleKey(const QString &keyId)
{
    if (m_kind != CalcKind::BaseN || m_stack->currentIndex() != 0) return false;
    static const QMap<QString, int> baseKeys = {{"sq", 10}, {"pow", 16}, {"log", 2}, {"ln", 8}};
    if (baseKeys.contains(keyId)) {
        setBase(baseKeys[keyId]);
        return true;
    }
    static const QMap<QString, QString> hexKeys = {{"neg", "A"}, {"dms", "B"}, {"inv", "C"},
                                                   {"sin", "D"}, {"cos", "E"}, {"tan", "F"}};
    if (hexKeys.contains(keyId)) {
        if (m_base == 16) insertText(hexKeys[keyId], true);
        return true;
    }
    // keys without meaning in Base-N
    static const QStringList dead = {"sqrt", "frac", "integ", "xvar", "exp", "dot", "sd", "eng"};
    return dead.contains(keyId);
}

QList<MenuItem> CalcPage::optnMenu() const
{
    switch (m_kind) {
    case CalcKind::Calculate:
        return calcMenuItems();
    case CalcKind::Complex: {
        QList<MenuItem> m = {{"Complex", {}, {
            {"i  (imaginary unit)", "i:i"}, {"Arg  (argument)", "i:Arg(\u00A7)"}, {"Conjg  (conjugate)", "i:Conjg(\u00A7)"},
            {"ReP  (real part)", "i:ReP(\u00A7)"}, {"ImP  (imaginary part)", "i:ImP(\u00A7)"}, {"Abs  (modulus)", "i:Abs(\u00A7)"},
            {"\u2220  (polar input)", "i:\u2220"}, {"\u25B6r\u2220\u03B8", "i:\u25B6r\u2220\u03B8"}, {"\u25B6a+bi", "i:\u25B6a+bi"}}}};
        m += calcMenuItems();
        return m;
    }
    case CalcKind::BaseN:
        return {{"Logical Operation", {}, {{"and", "i: and "}, {"or", "i: or "}, {"xor", "i: xor "}, {"xnor", "i: xnor "},
                                          {"Not", "i:Not(\u00A7)"}, {"Neg", "i:Neg(\u00A7)"}}},
                {"Base Prefix", {}, {{"d  (decimal)", "i:d"}, {"h  (hexadecimal)", "i:h"}, {"b  (binary)", "i:b"},
                                    {"o  (octal)", "i:o"}}},
                {"Set Base", {}, {{"DEC", "c:BASE10"}, {"HEX", "c:BASE16"}, {"BIN", "c:BASE2"}, {"OCT", "c:BASE8"}}}};
    case CalcKind::Matrix:
        return {{"Define Matrix", {}, {{"MatA", "c:DEFINE:MatA"}, {"MatB", "c:DEFINE:MatB"}, {"MatC", "c:DEFINE:MatC"},
                                      {"MatD", "c:DEFINE:MatD"}}},
                {"Matrix Variables", {}, {{"MatA", "i:MatA"}, {"MatB", "i:MatB"}, {"MatC", "i:MatC"}, {"MatD", "i:MatD"},
                                         {"MatAns", "i:MatAns"}}},
                {"Matrix Calc", {}, {{"Determinant", "i:Det(\u00A7)"}, {"Transposition", "i:Trn(\u00A7)"},
                                    {"Identity", "i:Identity(\u00A7)"}, {"Row Echelon Form", "i:Ref(\u00A7)"},
                                    {"Reduced Row Echelon Form", "i:Rref(\u00A7)"}, {"Abs", "i:Abs(\u00A7)"},
                                    {"Inverse  x\u207B\u00B9", "i:\u207B\u00B9"}}}};
    case CalcKind::Vector:
        return {{"Define Vector", {}, {{"VctA", "c:DEFINE:VctA"}, {"VctB", "c:DEFINE:VctB"}, {"VctC", "c:DEFINE:VctC"},
                                      {"VctD", "c:DEFINE:VctD"}}},
                {"Vector Variables", {}, {{"VctA", "i:VctA"}, {"VctB", "i:VctB"}, {"VctC", "i:VctC"}, {"VctD", "i:VctD"},
                                         {"VctAns", "i:VctAns"}}},
                {"Vector Calc", {}, {{"Dot Product", "i:DotP(\u00A7,)"}, {"Cross Product", "i:CrossP(\u00A7,)"},
                                    {"Angle", "i:Angle(\u00A7,)"}, {"Unit Vector", "i:UnitV(\u00A7)"},
                                    {"Abs  (magnitude)", "i:Abs(\u00A7)"}}}};
    }
    return {};
}

void CalcPage::command(const QString &cmd)
{
    if (cmd.startsWith("STO:")) { storeTo(cmd.mid(4)); return; }
    if (cmd == "M+") { memoryAdd(false); return; }
    if (cmd == "M-") { memoryAdd(true); return; }
    if (cmd == "CALC") { startPrompt(Prompt::Calc); return; }
    if (cmd == "SOLVE") {
        if (m_kind == CalcKind::Calculate) startPrompt(Prompt::Solve);
        else emit message("SOLVE is available in the Calculate mode");
        return;
    }
    if (cmd.startsWith("BASE")) { if (m_kind == CalcKind::BaseN) setBase(cmd.mid(4).toInt()); return; }
    if (cmd.startsWith("DEFINE:")) { openEditor(cmd.mid(7)); return; }
    if (cmd == "DMS") {
        bool editing = !m_edit->text().isEmpty() && !m_edit->fresh;
        if (editing || !m_current || m_current->error || !m_current->value.isReal() || !m_current->outputs.isEmpty()) {
            if (m_kind != CalcKind::BaseN) insertText(QStringLiteral("\u00B0"), false);
            return;
        }
        m_current->opts.dms = !m_current->opts.dms;
        m_current->opts.eng = false;
        renderCurrent();
        return;
    }
    if (!m_current || m_current->error || m_kind == CalcKind::BaseN) return;
    CalcEntry &e = *m_current;
    if (cmd == "SD") {
        if (e.opts.dms || e.opts.eng || !e.special.isEmpty()) {
            e.opts.dms = e.opts.eng = false;
            e.special.clear();
        } else if (e.opts.approx || hasExactForm(e.value, settings()) ||
                   std::any_of(e.outputs.begin(), e.outputs.end(), [&](const auto &o) { return hasExactForm(o.second, settings()); })) {
            e.opts.approx = !e.opts.approx;
        }
        renderCurrent();
    } else if (cmd == "FRACSWAP") {
        e.opts.swapFraction = !e.opts.swapFraction;
        renderCurrent();
    } else if (cmd == "ENG" || cmd == "ENGL") {
        if (!e.value.isReal() || !e.outputs.isEmpty()) return;
        if (!e.opts.eng) {
            e.opts.eng = true;
            e.opts.engShift = cmd == "ENG" ? 0 : -1;
        } else {
            e.opts.engShift += cmd == "ENG" ? 1 : -1;
        }
        e.opts.dms = false;
        renderCurrent();
    } else if (cmd == "FACT") {
        double v = e.value.isReal() ? e.value.re() : -1;
        if (!num::isInteger(v) || v < 1 || v >= 1e10 || !e.outputs.isEmpty()) {
            e.error = true;
            e.err = CalcError(ErrKind::Math);
            renderCurrent();
            return;
        }
        qint64 rest;
        auto f = num::primeFactors(qint64(v), rest);
        QStringList parts;
        for (const auto &p : f) parts << (p.second == 1 ? QString::number(p.first) : QString("%1^%2").arg(p.first).arg(p.second));
        if (rest > 1) parts << QString("(%1)").arg(rest);
        e.special = parts.isEmpty() ? "1" : parts.join(QStringLiteral("\u00D7"));
        renderCurrent();
    }
}

// ------------------------------------------------------------------ CALC / SOLVE

void CalcPage::startPrompt(Prompt p)
{
    QString text = m_edit->text().trimmed();
    if (text.isEmpty() && m_current) text = m_current->input;
    if (text.isEmpty()) return;
    NodePtr node;
    try {
        ParseOptions po;
        po.allowRelations = true;
        po.lex.base = m_kind == CalcKind::BaseN ? m_base : 0;
        node = parseExpression(text, po);
    } catch (const CalcError &) {
        m_edit->fresh = false;
        runExpression(text, false);   // shows the syntax error
        return;
    }
    QStringList vars;
    if (node->type == Node::Type::Binary && node->op == "=" && p == Prompt::Calc &&
        node->kids[0]->type == Node::Type::Var) {
        vars = referencedVariables(node->kids[1]);   // "y = f(x)" form: y receives the result
    } else {
        vars = referencedVariables(node);
    }
    if (p == Prompt::Solve && vars.isEmpty()) {
        CalcEntry e;
        e.input = text;
        e.error = true;
        e.err = CalcError(ErrKind::Variable);
        if (m_current && m_edit->fresh) archiveCurrent();
        m_current = e;
        renderCurrent();
        return;
    }
    if (p == Prompt::Calc && vars.isEmpty()) {
        m_edit->fresh = m_current.has_value();
        runExpression(text, false);
        return;
    }

    // build the form
    while (m_promptForm->rowCount() > 0) m_promptForm->removeRow(0);
    m_promptFields.clear();
    m_solveVar = nullptr;
    m_prompt = p;
    m_promptSource = text;
    m_promptTitle->setText(p == Prompt::Calc ? "CALC  \u2014  enter variable values" : "SOLVE  \u2014  solve the equation");
    m_promptExpr->setBox(layoutSyntax(text, mathStyle(18)));
    if (p == Prompt::Solve) {
        m_solveVar = new QComboBox;
        m_solveVar->addItems(vars);
        m_solveVar->setCurrentIndex(vars.contains("x") ? int(vars.indexOf("x")) : 0);
        m_solveVar->setFocusPolicy(Qt::NoFocus);
        m_promptForm->addRow(lcdLabel("Solve for", nullptr), m_solveVar);
        connect(m_solveVar, &QComboBox::currentIndexChanged, this, [this] { rebuildSolveFields(); });
    }
    for (const QString &v : vars) {
        auto *ed = new ExprEdit;
        ed->setText(editableValue(ctx().vars.value(v).z, settings()));
        connect(ed, &QLineEdit::returnPressed, this, [this, ed] {
            int i = 0;
            for (; i < m_promptFields.size(); ++i)
                if (m_promptFields[i].second == ed) break;
            if (i + 1 < m_promptFields.size()) m_promptFields[i + 1].second->setFocus();
            else finishPrompt();
        });
        m_promptForm->addRow(lcdLabel(v + " =", nullptr), ed);
        m_promptFields.push_back({v, ed});
    }
    rebuildSolveFields();
    m_stack->setCurrentIndex(1);
    if (!m_promptFields.isEmpty()) {
        m_promptFields[0].second->setFocus();
        m_promptFields[0].second->selectAll();
    }
}

void CalcPage::rebuildSolveFields()
{
    if (!m_solveVar) return;
    const QString sv = m_solveVar->currentText();
    for (int i = 0; i < m_promptFields.size(); ++i) {
        auto *lab = qobject_cast<QLabel *>(m_promptForm->itemAt(i + 1, QFormLayout::LabelRole)->widget());
        if (lab) lab->setText(m_promptFields[i].first == sv ? m_promptFields[i].first + " (start) =" : m_promptFields[i].first + " =");
    }
}

void CalcPage::cancelPrompt()
{
    m_prompt = Prompt::None;
    m_stack->setCurrentIndex(0);
    m_edit->setFocus();
}

void CalcPage::finishPrompt()
{
    // assign the entered values
    for (const auto &f : m_promptFields) {
        try {
            QString t = f.second->text().trimmed();
            Value v = evalString(t.isEmpty() ? "0" : t);
            ctx().setVar(f.first, v);
        } catch (const CalcError &e) {
            emit message(errorName(e.kind) + " in " + f.first);
            f.second->setFocus();
            return;
        }
    }
    const QString src = m_promptSource;
    const Prompt p = m_prompt;
    m_prompt = Prompt::None;
    m_stack->setCurrentIndex(0);
    m_edit->setText(src);
    m_edit->setFocus();

    if (m_current && !m_current->error) archiveCurrent();
    else m_current.reset();

    CalcEntry e;
    e.input = src;
    try {
        prepareContext();
        ParseOptions po;
        po.allowRelations = true;
        NodePtr node = parseExpression(src, po);
        Evaluator ev(ctx());
        if (p == Prompt::Calc) {
            if (node->type == Node::Type::Binary && node->op == "=" && node->kids[0]->type == Node::Type::Var) {
                Value v = ev.evaluate(node->kids[1]);
                ctx().setVar(node->kids[0]->op, v);
                ctx().pushAns(v);
                e.value = v;
                e.outputs = {{node->kids[0]->op, v}};
            } else if (node->type == Node::Type::Binary && node->op == "=") {
                throw CalcError(ErrKind::Syntax, node->pos);
            } else {
                Value v = ev.evaluate(node);
                ctx().pushAns(v);
                e.value = v;
                e.outputs = ctx().outputs;
                e.opts.form = ctx().forcedForm;
            }
        } else {
            const QString sv = m_solveVar ? m_solveVar->currentText() : QString("x");
            NodePtr lhs = node, rhs;
            if (node->type == Node::Type::Binary && node->op == "=") {
                lhs = node->kids[0];
                rhs = node->kids[1];
            } else if (node->type == Node::Type::Binary) {
                throw CalcError(ErrKind::Syntax, node->pos);
            }
            double guess = ctx().vars.value(sv).re();
            auto g = [&](double t) {
                double l = ev.evalRealAt(lhs, sv, t);
                double r = rhs ? ev.evalRealAt(rhs, sv, t) : 0.0;
                return l - r;
            };
            num::SolveResult r = num::solve(g, guess);
            double root = r.x;
            // snap to a nearby "nice" value when it satisfies the equation as well
            double rr = std::round(root * 1e9) / 1e9;
            try {
                if (rr != root && std::abs(g(rr)) <= std::abs(r.lr)) root = rr, r.lr = g(rr);
            } catch (...) {
            }
            if (std::abs(r.lr) < 1e-12) r.lr = 0;   // round-off below display precision
            ctx().setVar(sv, Value(root));
            ctx().pushAns(Value(root));
            e.value = Value(root);
            e.outputs = {{sv, Value(root)}, {QStringLiteral("L\u2212R"), Value(r.lr)}};
        }
    } catch (const CalcError &err) {
        e.error = true;
        e.err = err;
    }
    m_current = e;
    renderCurrent();
    m_edit->fresh = !e.error;
    emit statusChanged();
}

// ------------------------------------------------------------------ matrix / vector editor

void CalcPage::openEditor(const QString &name)
{
    if (m_kind != CalcKind::Matrix && m_kind != CalcKind::Vector) return;
    m_editing = name;
    m_editorTitle->setText(name + (m_kind == CalcKind::Matrix ? "  \u2014  define matrix" : "  \u2014  define vector"));
    Value v = ctx().vars.value(name);
    bool has = ctx().vars.contains(name);
    {
        QSignalBlocker b1(m_rows), b2(m_cols);
        if (has) {
            m_rows->setValue(v.rows);
            m_cols->setValue(v.cols);
        }
    }
    m_table->setRowCount(0);
    resizeEditorTable();
    if (has)
        for (int r = 0; r < v.rows; ++r)
            for (int c = 0; c < v.cols; ++c)
                m_table->setItem(r, c, new QTableWidgetItem(editableValue(v.at(r, c), settings())));
    m_stack->setCurrentIndex(2);
    m_table->setFocus();
    m_table->setCurrentCell(0, 0);
}

void CalcPage::resizeEditorTable()
{
    int R = m_rows->value(), C = m_kind == CalcKind::Vector ? 1 : m_cols->value();
    int oldR = m_table->rowCount(), oldC = m_table->columnCount();
    m_table->setRowCount(R);
    m_table->setColumnCount(C);
    for (int r = 0; r < R; ++r)
        for (int c = 0; c < C; ++c)
            if (r >= oldR || c >= oldC || !m_table->item(r, c)) m_table->setItem(r, c, new QTableWidgetItem("0"));
    QStringList h;
    for (int c = 0; c < C; ++c) h << QString::number(c + 1);
    m_table->setHorizontalHeaderLabels(h);
}

void CalcPage::commitEditor()
{
    flushTableEditor(m_table);
    bool vec = m_kind == CalcKind::Vector;
    int R = m_table->rowCount(), C = m_table->columnCount();
    Value v = vec ? Value::vector(R) : Value::matrix(R, C);
    for (int r = 0; r < R; ++r)
        for (int c = 0; c < C; ++c) {
            QString t = m_table->item(r, c) ? m_table->item(r, c)->text().trimmed() : QString();
            try {
                Value cell = evalString(t.isEmpty() ? "0" : t);
                if (!cell.isScalar()) throw CalcError(ErrKind::Dimension);
                v.at(r, c) = cell.z;
            } catch (const CalcError &e) {
                emit message(QString("%1 in row %2, column %3").arg(errorName(e.kind)).arg(r + 1).arg(c + 1));
                m_table->setCurrentCell(r, c);
                return;
            }
        }
    ctx().setVar(m_editing, v);
    emit message(m_editing + " stored");
    m_stack->setCurrentIndex(0);
    m_edit->setFocus();
    updateDefineButtons();
}

void CalcPage::updateDefineButtons()
{
    for (auto *b : m_defineButtons) {
        QString name = b->text().section(' ', 0, 0);
        if (ctx().vars.contains(name)) {
            const Value &v = ctx().vars[name];
            b->setText(m_kind == CalcKind::Vector ? QString("%1 [%2]").arg(name).arg(v.rows)
                                                  : QString("%1 [%2\u00D7%3]").arg(name).arg(v.rows).arg(v.cols));
        } else {
            b->setText(name);
        }
    }
}
