#ifndef GPAWINDOW_H
#define GPAWINDOW_H

#include <QWidget>
#include <QVector>
#include <QString>

class QTableWidget;
class QLabel;
class QComboBox;

// 一门课程（以文本保存，避免输入中途丢失）
struct Course {
    QString name;
    QString credit;
    QString score;
};

// 一个学期
struct Semester {
    QString name;
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

private:
    // 绩点公式，与参考代码保持一致：
    // GPA = 4.0 - 3 * (100 - score)^2 / 1600
    static double scoreToGpa(double score);

    // 弹出「学年+学期」选择框，返回形如 "2024-2025学年第1学期"；取消返回空串
    QString askSemesterName(const QString &title, int initYear = -1, int initTerm = 0);

    void commitTableToModel();          // 把表格内容写回当前学期数据
    void loadModelToTable();            // 把当前学期数据载入表格
    void rebuildSemesterCombo();        // 刷新学期下拉框

    // 累加一个学期的统计；返回是否有有效课程，弹窗报错则返回 false
    bool accumulate(const Semester &sem, double &totalCredit,
                    double &weightedGpa, double &weightedScore,
                    bool showErrors);

    QString dataFilePath() const;       // 数据文件路径
    void load();                        // 从文件读取
    void save();                        // 写入文件

    QTableWidget *table;
    QComboBox    *semesterCombo;
    QLabel       *currentResult;        // 本学期结果
    QLabel       *totalResult;          // 总计结果

    QVector<Semester> semesters;
    int  currentIndex = 0;
    bool loadingTable = false;          // 防止载入表格时触发提交
};

#endif // GPAWINDOW_H
