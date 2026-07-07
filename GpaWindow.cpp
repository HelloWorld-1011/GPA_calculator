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
    setWindowTitle("GPA 计算器");
    resize(520, 560);

    // ---- 学期选择栏 ----
    semesterCombo = new QComboBox(this);
    auto *addSemBtn    = new QPushButton("新建学期", this);
    auto *renameSemBtn = new QPushButton("重命名", this);
    auto *delSemBtn    = new QPushButton("删除学期", this);

    auto *semLayout = new QHBoxLayout;
    semLayout->addWidget(new QLabel("学期：", this));
    semLayout->addWidget(semesterCombo, 1);
    semLayout->addWidget(addSemBtn);
    semLayout->addWidget(renameSemBtn);
    semLayout->addWidget(delSemBtn);

    // ---- 课程表格：课程名称 | 学分 | 成绩 ----
    table = new QTableWidget(this);
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({"课程名称", "学分", "成绩", "绩点"});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table->verticalHeader()->setDefaultSectionSize(28);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);

    // ---- 课程操作按钮 ----
    auto *addBtn    = new QPushButton("添加课程", this);
    auto *removeBtn = new QPushButton("删除选中", this);
    auto *clearBtn  = new QPushButton("清空本学期", this);
    auto *calcBtn   = new QPushButton("计算 GPA", this);

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

    // ---- 主布局 ----
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(semLayout);
    mainLayout->addWidget(new QLabel("输入每门课程的名称、学分与成绩（百分制；P/NP 课程成绩填 P 或 NP）：", this));
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

    // 编辑学分/成绩后即时刷新绩点列
    connect(table, &QTableWidget::cellChanged, this, [this](int, int col) {
        if (loadingTable || col == 3)   // 跳过载入过程及绩点列自身的写入
            return;
        updateGpaColumn();
    });

    // ---- 载入已保存的数据 ----
    load();
    rebuildSemesterCombo();
    loadModelToTable();
    calculate();
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
        Course c;
        c.name   = cellText(0);
        c.credit = cellText(1);
        c.score  = cellText(2);
        // 跳过完全空白的行
        if (c.name.isEmpty() && c.credit.isEmpty() && c.score.isEmpty())
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
            table->setItem(row, 0, new QTableWidgetItem(c.name));
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
        semesterCombo->addItem(s.name);
    if (currentIndex >= 0 && currentIndex < semesters.size())
        semesterCombo->setCurrentIndex(currentIndex);
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

QString GpaWindow::askSemesterName(const QString &title, int initYear, int initTerm)
{
    QDialog dlg(this);
    dlg.setWindowTitle(title);

    // 学年（自行输入，如 2024-2025）
    auto *yearEdit = new QLineEdit(&dlg);
    int defYear = initYear;
    if (defYear < 0) {
        QDate d = QDate::currentDate();
        defYear = (d.month() >= 8) ? d.year() : d.year() - 1;   // 8 月起算新学年
    }
    yearEdit->setText(QString("%1-%2").arg(defYear).arg(defYear + 1));
    yearEdit->setPlaceholderText("例如 2024-2025");

    // 学期
    auto *termCombo = new QComboBox(&dlg);
    termCombo->addItems({"第1学期", "第2学期", "第3学期（小学期）"});
    if (initTerm >= 0 && initTerm < termCombo->count())
        termCombo->setCurrentIndex(initTerm);

    auto *form = new QFormLayout;
    form->addRow("学年：", yearEdit);
    form->addRow("学期：", termCombo);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto *lay = new QVBoxLayout(&dlg);
    lay->addLayout(form);
    lay->addWidget(box);

    if (dlg.exec() != QDialog::Accepted)
        return QString();

    QString year = yearEdit->text().trimmed();
    if (year.isEmpty())
        return QString();
    return QString("%1学年%2").arg(year).arg(termCombo->currentText());
}

void GpaWindow::addSemester()
{
    QString name = askSemesterName("新建学期");
    if (name.isEmpty())
        return;

    commitTableToModel();
    semesters.push_back({name, {}});
    currentIndex = semesters.size() - 1;
    rebuildSemesterCombo();
    loadModelToTable();
    calculate();
}

void GpaWindow::renameSemester()
{
    if (semesters.isEmpty())
        return;
    QString name = askSemesterName("重命名学期");
    if (name.isEmpty())
        return;
    semesters[currentIndex].name = name;
    rebuildSemesterCombo();
}

void GpaWindow::removeSemester()
{
    if (semesters.size() <= 1) {
        QMessageBox::information(this, "提示", "至少需要保留一个学期。");
        return;
    }
    auto ret = QMessageBox::question(this, "删除学期",
        QString("确定删除学期「%1」吗？").arg(semesters[currentIndex].name));
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
                QMessageBox::warning(this, "输入错误",
                    QString("学期「%1」第 %2 门课程：%3")
                        .arg(sem.name).arg(i + 1).arg(msg));
        };

        bool okCredit = false;
        double credit = c.credit.toDouble(&okCredit);

        // P/NP 课程：不计绩点与加权均分，仅 P（通过）计入已修学分
        QString grade = c.score.trimmed().toUpper();
        if (grade == "P" || grade == "NP") {
            if (!okCredit)   { warn("学分不是有效数字。"); return false; }
            if (credit <= 0) { warn("学分必须大于 0。");   return false; }
            if (grade == "P")
                totalCredit += credit;
            continue;
        }

        // 百分制课程
        bool okScore = false;
        double score = c.score.toDouble(&okScore);

        if (!okCredit || !okScore) { warn("学分或成绩不是有效数字（P/NP 课程请填写 P 或 NP）。"); return false; }
        if (credit <= 0)           { warn("学分必须大于 0。");        return false; }
        if (score < 0 || score > 100) { warn("成绩应在 0~100 之间。"); return false; }

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
        currentResult->setText(QString("本学期    GPA：%1    加权均分：%2    学分：%3")
            .arg(curGpa / curGpaCredit, 0, 'f', 3)
            .arg(curScore / curGpaCredit, 0, 'f', 2)
            .arg(curCredit, 0, 'g', -1));
    } else if (curCredit > 0) {
        currentResult->setText(QString("本学期    学分：%1（均为 P 课程，不计 GPA）")
            .arg(curCredit, 0, 'g', -1));
    } else {
        currentResult->setText("本学期    暂无有效课程");
    }

    // 总计（所有学期）
    double allCredit = 0, allGpaCredit = 0, allGpa = 0, allScore = 0;
    for (const Semester &s : semesters)
        accumulate(s, allCredit, allGpaCredit, allGpa, allScore, false);

    if (allGpaCredit > 0) {
        totalResult->setText(QString("总计      GPA：%1    加权均分：%2    总学分：%3")
            .arg(allGpa / allGpaCredit, 0, 'f', 3)
            .arg(allScore / allGpaCredit, 0, 'f', 2)
            .arg(allCredit, 0, 'g', -1));
    } else if (allCredit > 0) {
        totalResult->setText(QString("总计      总学分：%1（均为 P 课程，不计 GPA）")
            .arg(allCredit, 0, 'g', -1));
    } else {
        totalResult->setText("总计      暂无有效课程");
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
            sem.name = so.value("name").toString();
            for (const QJsonValue &cv : so.value("courses").toArray()) {
                QJsonObject co = cv.toObject();
                Course c;
                c.name   = co.value("name").toString();
                c.credit = co.value("credit").toString();
                c.score  = co.value("score").toString();
                sem.courses.push_back(c);
            }
            semesters.push_back(sem);
        }
        currentIndex = doc.object().value("currentIndex").toInt(0);
    }

    // 没有任何数据时，建立一个默认学期（按学年格式命名）
    if (semesters.isEmpty()) {
        QDate d = QDate::currentDate();
        int y = (d.month() >= 8) ? d.year() : d.year() - 1;
        int term = (d.month() >= 8 || d.month() <= 1) ? 1 : 2;   // 秋季学期记为第1学期
        semesters.push_back({QString("%1-%2学年第%3学期").arg(y).arg(y + 1).arg(term), {}});
    }
    if (currentIndex < 0 || currentIndex >= semesters.size())
        currentIndex = 0;
}

void GpaWindow::save()
{
    commitTableToModel();

    QJsonArray semArr;
    for (const Semester &sem : semesters) {
        QJsonObject so;
        so["name"] = sem.name;
        QJsonArray courseArr;
        for (const Course &c : sem.courses) {
            QJsonObject co;
            co["name"]   = c.name;
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

    QFile file(dataFilePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
        file.close();
    }
}

void GpaWindow::closeEvent(QCloseEvent *event)
{
    save();
    event->accept();
}
