#include "GpaWindow.h"

#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QInputDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QDate>
#include <QFont>
#include <QCloseEvent>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>
#include <functional>

GpaWindow::GpaWindow(QWidget *parent)
    : QWidget(parent)
{
    // 标题、表头等文字统一由 retranslateUi() 按语言设置
    resize(520, 560);

    // ---- 语言切换栏 ----
    langBtn = new QPushButton(this);
    auto *langLayout = new QHBoxLayout;
    langLayout->addStretch(1);
    langLayout->addWidget(langBtn);

    // ---- 学期选择栏 ----
    semesterCombo = new QComboBox(this);
    semLabel     = new QLabel(this);
    addSemBtn    = new QPushButton(this);
    renameSemBtn = new QPushButton(this);
    delSemBtn    = new QPushButton(this);

    auto *semLayout = new QHBoxLayout;
    semLayout->addWidget(semLabel);
    semLayout->addWidget(semesterCombo, 1);
    semLayout->addWidget(addSemBtn);
    semLayout->addWidget(renameSemBtn);
    semLayout->addWidget(delSemBtn);

    // ---- 课程表格：课程名称 | 学分 | 成绩 | 绩点 ----
    table = new QTableWidget(this);
    table->setColumnCount(4);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table->verticalHeader()->setDefaultSectionSize(28);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);

    // ---- 课程操作按钮 ----
    addBtn    = new QPushButton(this);
    removeBtn = new QPushButton(this);
    clearBtn  = new QPushButton(this);
    calcBtn   = new QPushButton(this);

    auto *btnLayout = new QHBoxLayout;
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(removeBtn);
    btnLayout->addWidget(clearBtn);

    // ---- 结果显示 ----
    currentResult = new QLabel("本学期 —", this);
    totalResult   = new QLabel("总计 —", this);
    QFont f = currentResult->font();
    f.setPointSize(14);
    f.setBold(true);
    currentResult->setFont(f);
    totalResult->setFont(f);
    currentResult->setAlignment(Qt::AlignCenter);
    totalResult->setAlignment(Qt::AlignCenter);

    hintLabel = new QLabel(this);
    hintLabel->setWordWrap(true);

    // ---- 主布局 ----
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(langLayout);
    mainLayout->addLayout(semLayout);
    mainLayout->addWidget(hintLabel);
    mainLayout->addWidget(table);
    mainLayout->addLayout(btnLayout);
    mainLayout->addWidget(calcBtn);
    mainLayout->addWidget(currentResult);
    mainLayout->addWidget(totalResult);

    // ---- 信号槽 ----
    connect(addBtn,    &QPushButton::clicked, this, &GpaWindow::addRow);
    connect(removeBtn, &QPushButton::clicked, this, &GpaWindow::removeSelectedRows);
    connect(clearBtn,  &QPushButton::clicked, this, &GpaWindow::clearCurrentSemester);
    connect(calcBtn,   &QPushButton::clicked, this, &GpaWindow::calculate);

    connect(addSemBtn,    &QPushButton::clicked, this, &GpaWindow::addSemester);
    connect(renameSemBtn, &QPushButton::clicked, this, &GpaWindow::renameSemester);
    connect(delSemBtn,    &QPushButton::clicked, this, &GpaWindow::removeSemester);
    connect(semesterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GpaWindow::onSemesterChanged);

    connect(langBtn, &QPushButton::clicked, this, &GpaWindow::toggleLanguage);

    // 编辑学分/成绩后即时刷新绩点列
    connect(table, &QTableWidget::cellChanged, this, [this](int, int col) {
        if (loadingTable || col == 3)   // 跳过载入过程及绩点列自身的写入
            return;
        updateGpaColumn();
    });

    // ---- 载入已保存的数据 ----
    load();
    rebuildSemesterCombo();
    loadModelToTable();    // 必须先把数据填进表格，之后 calculate 才不会用空表覆盖数据
    retranslateUi();       // 按载入的语言设置所有文字（含表头）
    calculate();
}

// 按当前语言刷新所有固定界面文字
void GpaWindow::retranslateUi()
{
    setWindowTitle(L("GPA 计算器", "GPA Calculator"));
    langBtn->setText(L("English", "中文"));   // 显示切换后的目标语言

    semLabel->setText(L("学期：", "Term:"));
    addSemBtn->setText(L("新建学期", "New Term"));
    renameSemBtn->setText(L("重命名", "Rename"));
    delSemBtn->setText(L("删除学期", "Delete Term"));

    hintLabel->setText(L(
        "输入每门课程的名称、学分与成绩（百分制；P/NP 课程成绩填 P 或 NP）：",
        "Enter each course's name, credits and score (0-100; use P or NP for pass/fail courses):"));

    table->setHorizontalHeaderLabels({
        L("课程名称", "Course"),
        L("学分", "Credits"),
        L("成绩", "Score"),
        L("绩点", "GPA")});

    addBtn->setText(L("添加课程", "Add Course"));
    removeBtn->setText(L("删除选中", "Remove Selected"));
    clearBtn->setText(L("清空本学期", "Clear Term"));
    calcBtn->setText(L("计算 GPA", "Calculate GPA"));
    // 注意：本函数只负责改文字，不得调用 calculate()（那会在表格尚未载入时用空表覆盖数据）
}

void GpaWindow::toggleLanguage()
{
    commitTableToModel();   // 先按当前语言把课名存回对应字段
    english = !english;
    loadModelToTable();     // 课名列切换到另一种语言（先载入，再算）
    rebuildSemesterCombo(); // 学期下拉框也切换语言
    retranslateUi();        // 刷新固定文字与表头
    calculate();            // 刷新结果行文字
}

double GpaWindow::scoreToGpa(double score)
{
    return 4.0 - 3.0 * (100.0 - score) * (100.0 - score) / 1600.0;
}

// ---------------- 表格 <-> 数据 ----------------

void GpaWindow::commitTableToModel()
{
    if (loadingTable || semesters.isEmpty())
        return;

    Semester &sem = semesters[currentIndex];
    sem.courses.clear();
    for (int row = 0; row < table->rowCount(); ++row) {
        auto cellText = [&](int col) {
            QTableWidgetItem *it = table->item(row, col);
            return it ? it->text().trimmed() : QString();
        };
        // 课名列：当前语言存于文本，另一种语言存于 UserRole
        QTableWidgetItem *nameItem = table->item(row, 0);
        QString curName   = nameItem ? nameItem->text().trimmed() : QString();
        QString otherName = nameItem ? nameItem->data(Qt::UserRole).toString().trimmed() : QString();

        Course c;
        if (english) { c.nameEn = curName; c.name = otherName; }
        else         { c.name = curName;   c.nameEn = otherName; }
        c.credit = cellText(1);
        c.score  = cellText(2);
        // 跳过完全空白的行（两种语言课名与学分、成绩都为空）
        if (c.name.isEmpty() && c.nameEn.isEmpty() && c.credit.isEmpty() && c.score.isEmpty())
            continue;
        sem.courses.push_back(c);
    }
}

void GpaWindow::loadModelToTable()
{
    loadingTable = true;
    table->setRowCount(0);
    if (!semesters.isEmpty()) {
        const Semester &sem = semesters[currentIndex];
        for (const Course &c : sem.courses) {
            int row = table->rowCount();
            table->insertRow(row);
            auto *nameItem = new QTableWidgetItem(english ? c.nameEn : c.name);
            nameItem->setData(Qt::UserRole, english ? c.name : c.nameEn);
            table->setItem(row, 0, nameItem);
            table->setItem(row, 1, new QTableWidgetItem(c.credit));
            table->setItem(row, 2, new QTableWidgetItem(c.score));

            auto *gpaItem = new QTableWidgetItem("");
            gpaItem->setFlags(gpaItem->flags() & ~Qt::ItemIsEditable);
            gpaItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, 3, gpaItem);
        }
    }
    // 至少留两行空白方便输入
    while (table->rowCount() < 2)
        addRow();
    loadingTable = false;
    updateGpaColumn();
}

void GpaWindow::rebuildSemesterCombo()
{
    QSignalBlocker blocker(semesterCombo);
    semesterCombo->clear();
    for (const Semester &s : semesters)
        semesterCombo->addItem(semName(s));
    if (currentIndex >= 0 && currentIndex < semesters.size())
        semesterCombo->setCurrentIndex(currentIndex);
}

// 按当前语言拼出学期显示名
QString GpaWindow::semName(const Semester &s) const
{
    static const char *zhTerm[] = {"第1学期", "第2学期", "第3学期（小学期）"};
    static const char *enTerm[] = {"Term 1", "Term 2", "Term 3 (Summer)"};
    int t = (s.term >= 0 && s.term <= 2) ? s.term : 0;
    return english
        ? QString("%1 %2").arg(s.year).arg(enTerm[t])
        : QString("%1学年%2").arg(s.year).arg(zhTerm[t]);
}

// ---------------- 行操作 ----------------

void GpaWindow::addRow()
{
    int row = table->rowCount();
    table->insertRow(row);
    table->setItem(row, 0, new QTableWidgetItem(""));
    table->setItem(row, 1, new QTableWidgetItem(""));
    table->setItem(row, 2, new QTableWidgetItem(""));

    // 绩点列：只读，由成绩自动计算
    auto *gpaItem = new QTableWidgetItem("");
    gpaItem->setFlags(gpaItem->flags() & ~Qt::ItemIsEditable);
    gpaItem->setTextAlignment(Qt::AlignCenter);
    table->setItem(row, 3, gpaItem);
}

// 依据每行成绩填写绩点列（P/NP 及无效成绩显示 —）
void GpaWindow::updateGpaColumn()
{
    QSignalBlocker blocker(table);   // 避免写入绩点列再次触发 cellChanged
    for (int row = 0; row < table->rowCount(); ++row) {
        QTableWidgetItem *gpaItem = table->item(row, 3);
        if (!gpaItem) {
            gpaItem = new QTableWidgetItem("");
            gpaItem->setFlags(gpaItem->flags() & ~Qt::ItemIsEditable);
            gpaItem->setTextAlignment(Qt::AlignCenter);
            table->setItem(row, 3, gpaItem);
        }

        QTableWidgetItem *scoreItem = table->item(row, 2);
        QString score = scoreItem ? scoreItem->text().trimmed() : QString();

        QString text;
        QString up = score.toUpper();
        if (score.isEmpty()) {
            text = "";
        } else if (up == "P" || up == "NP") {
            text = "—";
        } else {
            bool ok = false;
            double s = score.toDouble(&ok);
            if (ok && s >= 0 && s <= 100)
                text = QString::number(scoreToGpa(s), 'f', 3);
            else
                text = "—";
        }
        gpaItem->setText(text);
    }
}

void GpaWindow::removeSelectedRows()
{
    auto ranges = table->selectedRanges();
    QList<int> rows;
    for (const auto &r : ranges)
        for (int i = r.topRow(); i <= r.bottomRow(); ++i)
            rows << i;
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int r : rows)
        table->removeRow(r);
}

void GpaWindow::clearCurrentSemester()
{
    table->setRowCount(0);
    addRow();
    addRow();
    calculate();
}

// ---------------- 学期操作 ----------------

bool GpaWindow::askSemesterName(const QString &title, QString &year, int &term)
{
    QDialog dlg(this);
    dlg.setWindowTitle(title);

    // 学年（自行输入，如 2024-2025）
    auto *yearEdit = new QLineEdit(&dlg);
    if (year.isEmpty()) {
        QDate d = QDate::currentDate();
        int defYear = (d.month() >= 8) ? d.year() : d.year() - 1;   // 8 月起算新学年
        yearEdit->setText(QString("%1-%2").arg(defYear).arg(defYear + 1));
    } else {
        yearEdit->setText(year);
    }
    yearEdit->setPlaceholderText(L("例如 2024-2025", "e.g. 2024-2025"));

    // 学期
    auto *termCombo = new QComboBox(&dlg);
    termCombo->addItems(english
        ? QStringList{"Term 1", "Term 2", "Term 3 (Summer)"}
        : QStringList{"第1学期", "第2学期", "第3学期（小学期）"});
    if (term >= 0 && term < termCombo->count())
        termCombo->setCurrentIndex(term);

    auto *form = new QFormLayout;
    form->addRow(L("学年：", "Academic Year:"), yearEdit);
    form->addRow(L("学期：", "Term:"), termCombo);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto *lay = new QVBoxLayout(&dlg);
    lay->addLayout(form);
    lay->addWidget(box);

    if (dlg.exec() != QDialog::Accepted)
        return false;

    QString y = yearEdit->text().trimmed();
    if (y.isEmpty())
        return false;
    year = y;
    term = termCombo->currentIndex();
    return true;
}

void GpaWindow::addSemester()
{
    QString year; int term = 0;
    if (!askSemesterName(L("新建学期", "New Term"), year, term))
        return;

    commitTableToModel();
    Semester s;
    s.year = year;
    s.term = term;
    semesters.push_back(s);
    currentIndex = semesters.size() - 1;
    rebuildSemesterCombo();
    loadModelToTable();
    calculate();
}

void GpaWindow::renameSemester()
{
    if (semesters.isEmpty())
        return;
    QString year = semesters[currentIndex].year;
    int term     = semesters[currentIndex].term;
    if (!askSemesterName(L("重命名学期", "Rename Term"), year, term))
        return;
    semesters[currentIndex].year = year;
    semesters[currentIndex].term = term;
    rebuildSemesterCombo();
}

void GpaWindow::removeSemester()
{
    if (semesters.size() <= 1) {
        QMessageBox::information(this, L("提示", "Notice"),
            L("至少需要保留一个学期。", "At least one term must remain."));
        return;
    }
    auto ret = QMessageBox::question(this, L("删除学期", "Delete Term"),
        L("确定删除学期「%1」吗？", "Delete term \"%1\"?").arg(semName(semesters[currentIndex])));
    if (ret != QMessageBox::Yes)
        return;

    semesters.remove(currentIndex);
    if (currentIndex >= semesters.size())
        currentIndex = semesters.size() - 1;
    rebuildSemesterCombo();
    loadModelToTable();
    calculate();
}

void GpaWindow::onSemesterChanged(int index)
{
    if (index < 0 || index == currentIndex)
        return;
    commitTableToModel();      // 保存旧学期
    currentIndex = index;
    loadModelToTable();        // 载入新学期
    calculate();
}

// ---------------- 计算 ----------------

bool GpaWindow::accumulate(const Semester &sem, double &totalCredit, double &gpaCredit,
                           double &weightedGpa, double &weightedScore,
                           bool showErrors)
{
    for (int i = 0; i < sem.courses.size(); ++i) {
        const Course &c = sem.courses[i];
        if (c.credit.isEmpty() && c.score.isEmpty())
            continue;

        auto warn = [&](const QString &msg) {
            if (showErrors)
                QMessageBox::warning(this, L("输入错误", "Invalid Input"),
                    L("学期「%1」第 %2 门课程：%3",
                      "Term \"%1\", course #%2: %3")
                        .arg(semName(sem)).arg(i + 1).arg(msg));
        };

        bool okCredit = false;
        double credit = c.credit.toDouble(&okCredit);

        // P/NP 课程：不计绩点与加权均分，仅 P（通过）计入已修学分
        QString grade = c.score.trimmed().toUpper();
        if (grade == "P" || grade == "NP") {
            if (!okCredit)   { warn(L("学分不是有效数字。", "Credits is not a valid number.")); return false; }
            if (credit <= 0) { warn(L("学分必须大于 0。", "Credits must be greater than 0."));   return false; }
            if (grade == "P")
                totalCredit += credit;
            continue;
        }

        // 百分制课程
        bool okScore = false;
        double score = c.score.toDouble(&okScore);

        if (!okCredit || !okScore) { warn(L("学分或成绩不是有效数字（P/NP 课程请填写 P 或 NP）。",
                                            "Credits or score is not a valid number (use P or NP for pass/fail courses).")); return false; }
        if (credit <= 0)           { warn(L("学分必须大于 0。", "Credits must be greater than 0."));        return false; }
        if (score < 0 || score > 100) { warn(L("成绩应在 0~100 之间。", "Score must be between 0 and 100.")); return false; }

        totalCredit   += credit;
        gpaCredit     += credit;
        weightedGpa   += credit * scoreToGpa(score);
        weightedScore += credit * score;
    }
    return true;
}

void GpaWindow::calculate()
{
    commitTableToModel();
    updateGpaColumn();
    if (semesters.isEmpty())
        return;

    // 本学期
    double curCredit = 0, curGpaCredit = 0, curGpa = 0, curScore = 0;
    if (!accumulate(semesters[currentIndex], curCredit, curGpaCredit, curGpa, curScore, true))
        return;

    if (curGpaCredit > 0) {
        currentResult->setText(L("本学期    GPA：%1    加权均分：%2    学分：%3",
                                 "Term    GPA: %1    Weighted Avg: %2    Credits: %3")
            .arg(curGpa / curGpaCredit, 0, 'f', 3)
            .arg(curScore / curGpaCredit, 0, 'f', 2)
            .arg(curCredit, 0, 'g', -1));
    } else if (curCredit > 0) {
        currentResult->setText(L("本学期    学分：%1（均为 P 课程，不计 GPA）",
                                 "Term    Credits: %1 (all P/NP, excluded from GPA)")
            .arg(curCredit, 0, 'g', -1));
    } else {
        currentResult->setText(L("本学期    暂无有效课程", "Term    No valid courses yet"));
    }

    // 总计（所有学期）
    double allCredit = 0, allGpaCredit = 0, allGpa = 0, allScore = 0;
    for (const Semester &s : semesters)
        accumulate(s, allCredit, allGpaCredit, allGpa, allScore, false);

    if (allGpaCredit > 0) {
        totalResult->setText(L("总计      GPA：%1    加权均分：%2    总学分：%3",
                               "Overall    GPA: %1    Weighted Avg: %2    Total Credits: %3")
            .arg(allGpa / allGpaCredit, 0, 'f', 3)
            .arg(allScore / allGpaCredit, 0, 'f', 2)
            .arg(allCredit, 0, 'g', -1));
    } else if (allCredit > 0) {
        totalResult->setText(L("总计      总学分：%1（均为 P 课程，不计 GPA）",
                               "Overall    Total Credits: %1 (all P/NP, excluded from GPA)")
            .arg(allCredit, 0, 'g', -1));
    } else {
        totalResult->setText(L("总计      暂无有效课程", "Overall    No valid courses yet"));
    }
}

// ---------------- 持久化 ----------------

QString GpaWindow::dataFilePath() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/gpa_data.json";
}

void GpaWindow::load()
{
    QFile file(dataFilePath());
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        QJsonArray semArr = doc.object().value("semesters").toArray();
        for (const QJsonValue &sv : semArr) {
            QJsonObject so = sv.toObject();
            Semester sem;
            if (so.contains("year")) {
                sem.year = so.value("year").toString();
                sem.term = so.value("term").toInt(0);
            } else {
                // 兼容旧存档：从整串学期名里解析出学年与第几学期
                const QString n = so.value("name").toString();
                int idx = n.indexOf("学年");
                if (idx < 0) idx = n.indexOf(" Term ");
                sem.year = (idx > 0) ? n.left(idx).trimmed() : n;
                if (n.contains("第3") || n.contains("Term 3"))      sem.term = 2;
                else if (n.contains("第2") || n.contains("Term 2")) sem.term = 1;
                else                                                sem.term = 0;
            }
            for (const QJsonValue &cv : so.value("courses").toArray()) {
                QJsonObject co = cv.toObject();
                Course c;
                c.name   = co.value("name").toString();
                c.nameEn = co.value("nameEn").toString();
                c.credit = co.value("credit").toString();
                c.score  = co.value("score").toString();
                sem.courses.push_back(c);
            }
            semesters.push_back(sem);
        }
        currentIndex = doc.object().value("currentIndex").toInt(0);
        english      = doc.object().value("english").toBool(false);
    }

    // 没有任何数据时，建立一个默认学期
    if (semesters.isEmpty()) {
        QDate d = QDate::currentDate();
        int y = (d.month() >= 8) ? d.year() : d.year() - 1;
        Semester s;
        s.year = QString("%1-%2").arg(y).arg(y + 1);
        s.term = (d.month() >= 8 || d.month() <= 1) ? 0 : 1;   // 秋季记为第1学期(下标0)
        semesters.push_back(s);
    }
    if (currentIndex < 0 || currentIndex >= semesters.size())
        currentIndex = 0;
}

void GpaWindow::save()
{
    commitTableToModel();

    static const char *zhTerm[] = {"第1学期", "第2学期", "第3学期（小学期）"};
    QJsonArray semArr;
    for (const Semester &sem : semesters) {
        QJsonObject so;
        so["year"] = sem.year;
        so["term"] = sem.term;
        // name 仅为可读性/向下兼容而写（固定中文），实际显示由 year+term 决定
        int t = (sem.term >= 0 && sem.term <= 2) ? sem.term : 0;
        so["name"] = QString("%1学年%2").arg(sem.year).arg(zhTerm[t]);
        QJsonArray courseArr;
        for (const Course &c : sem.courses) {
            QJsonObject co;
            co["name"]   = c.name;
            co["nameEn"] = c.nameEn;
            co["credit"] = c.credit;
            co["score"]  = c.score;
            courseArr.append(co);
        }
        so["courses"] = courseArr;
        semArr.append(so);
    }

    QJsonObject root;
    root["semesters"]   = semArr;
    root["currentIndex"] = currentIndex;
    root["english"]      = english;

    const QByteArray data = QJsonDocument(root).toJson();

    // 安全网：覆盖前先把现有存档另存一份 .bak，误存也能回滚
    const QString path = dataFilePath();
    if (QFile::exists(path)) {
        const QString bak = path + ".bak";
        QFile::remove(bak);
        QFile::copy(path, bak);
    }

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data);
        file.close();
    }
}

void GpaWindow::closeEvent(QCloseEvent *event)
{
    save();
    event->accept();
}
