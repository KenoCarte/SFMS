#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QMainWindow>

class QListWidget;
class QLineEdit;
class QStandardItemModel;
class QTableView;
class QSortFilterProxyModel;
class QComboBox;
class QModelIndex;
class QLabel;
class QCloseEvent;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    // 目录管理：添加一个新的扫描根目录到左侧列表。
    void onAddDirectory();
    // 目录管理：从列表中移除当前选中的目录。
    void onRemoveSelectedDirectory();
    // 扫描所有根目录，重新建立文件索引并刷新表格。
    void onBuildIndex();
    // 关键词输入变化时，实时更新文件名过滤条件。
    void onSearchTextChanged(const QString &text);
    // 扩展名下拉框变化时，实时更新扩展名过滤条件。
    void onExtensionFilterChanged(const QString &ext);
    // 双击结果行时，使用系统默认程序打开对应文件。
    void onOpenFromTable(const QModelIndex &proxyIndex);

private:
    // 索引中每一条记录对应一个文件，UI 直接从这些字段生成表格内容。
    struct FileRecord {
        QString name;
        QString extension;
        QString path;
        qint64 size;
        QDateTime lastModified;
    };

    // 创建窗口上的控件布局和表格模型。
    void buildUi();
    // 把按钮、输入框、表格等控件和槽函数连接起来。
    void connectSignals();
    // 将一个文件记录追加到表格模型中。
    void appendFileToModel(const FileRecord &file);
    // 把字节数转换成便于人类阅读的大小文本。
    static QString humanReadableSize(qint64 bytes);
    // 创建顶部工具栏和快捷键入口。
    void setupToolbar();
    // 刷新右侧结果统计文本（当前可见/总数）。
    void updateResultStats();
    // 按给定目录列表重建索引；可用于全量重建或单目录重建。
    void rebuildIndexForDirectories(const QStringList &directories);
    // 从本地二进制文件加载已保存的目录列表。
    void loadPersistedDirectories();
    // 将当前目录列表以二进制形式保存到本地。
    void persistDirectories() const;
    // 将窗口几何和当前选中目录以二进制形式保存到本地。
    void persistWindowState() const;
    // 从本地二进制文件恢复窗口几何和选中目录。
    void loadWindowState();
    // 获取左侧列表中第 i 项对应的真实目录路径。
    QString directoryPathAt(int index) const;
    // 允许选择文件并复制到当前选中的目录。
    void onImportFilesToSelectedDirectory();

protected:
    void closeEvent(QCloseEvent *event) override;

    // 左侧仅保留目录列表；添加/删除/重建通过顶部工具栏触发。
    QListWidget *directoryList_ = nullptr;

    QLineEdit *searchEdit_ = nullptr;
    QComboBox *extensionCombo_ = nullptr;
    QLabel *resultStatsLabel_ = nullptr;

    QTableView *tableView_ = nullptr;
    QStandardItemModel *sourceModel_ = nullptr;
    QSortFilterProxyModel *proxyModel_ = nullptr;
};
