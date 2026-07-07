#ifndef GPAWINDOW_H
#define GPAWINDOW_H

#include <QWidget>
#include <QVector>
#include <QString>

class QTableWidget;
class QLabel;
class QComboBox;
class QPushButton;

// 一门课程（以文本保存，避免输入中途丢失）
struct Course {
    QString name;       // 中文课名
    QString nameEn;     // 英文课名
    QString credit;
    QString score;
};

// 一个学期
struct Semester {
    QString year;           // 学年，如 "2025-2026"
    int     term = 0;       // 学期：0=第1学期 1=第2学期 2=第3学期（小学期）
    QVector<Course> courses;
};

class GpaWindow : public QWidget
{
    Q_OBJECT

public:
    explicit GpaWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;   // 关闭时自动保存

private slots:
    void addRow();
    void removeSelectedRows();
    void clearCurrentSemester();
    void calculate();

    void addSemester();
    void renameSemester();
    void removeSemester();
    void onSemesterChanged(int index);

    void toggleLanguage();              // 中 / English 切换

private:
    // 绩点公式，与参考代码保持一致：
    // GPA = 4.0 - 3 * (100 - score)^2 / 1600
    static double scoreToGpa(double score);

    // 弹出「学年+学期」选择框，结果写入 year/term；确定返回 true，取消返回 false
    bool askSemesterName(const QString &title, QString &year, int &term);

    // 按当前语言返回学期显示名，如 "2025-2026学年第1学期" / "2025-2026 Term 1"
    QString semName(const Semester &s) const;

    void commitTableToModel();          // 把表格内容写回当前学期数据
    void loadModelToTable();            // 把当前学期数据载入表格
    void rebuildSemesterCombo();        // 刷新学期下拉框
    void updateGpaColumn();             // 根据成绩刷新每门课的绩点列

    // 按当前语言返回文案：中文 english=false / 英文 english=true
    QString L(const QString &zh, const QString &en) const { return english ? en : zh; }
    void retranslateUi();               // 按当前语言刷新所有界面文字

    // 累加一个学期的统计；返回是否有有效课程，弹窗报错则返回 false
    // totalCredit：已修总学分（含 P 通过的 P/NP 课程）
    // gpaCredit  ：参与 GPA/加权均分计算的学分（仅百分制课程）
    bool accumulate(const Semester &sem, double &totalCredit, double &gpaCredit,
                    double &weightedGpa, double &weightedScore,
                    bool showErrors);

    QString dataFilePath() const;       // 数据文件路径
    void load();                        // 从文件读取
    void save();                        // 写入文件

    QTableWidget *table;
    QComboBox    *semesterCombo;
    QLabel       *currentResult;        // 本学期结果
    QLabel       *totalResult;          // 总计结果

    // 需要随语言切换刷新文字的控件
    QPushButton *langBtn;
    QLabel      *semLabel;
    QPushButton *addSemBtn, *renameSemBtn, *delSemBtn;
    QLabel      *hintLabel;
    QPushButton *addBtn, *removeBtn, *clearBtn, *calcBtn;

    QVector<Semester> semesters;
    int  currentIndex = 0;
    bool loadingTable = false;          // 防止载入表格时触发提交
    bool english = false;               // 当前是否英文界面
};

#endif // GPAWINDOW_H
