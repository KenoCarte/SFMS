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

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    // 添加新的扫描根目录到左侧列表
    void onAddDirectory();
    // 移除当前选中的目录
    void onRemoveSelectedDirectory();
    // 扫描全部根目录并重建索引
    void onBuildIndex();
    // 根据关键词更新文件名过滤条件
    void onSearchTextChanged(const QString& text);
    // 根据扩展名更新扩展名过滤条件
    void onExtensionFilterChanged(const QString& ext);
    // 双击结果行后使用系统默认程序打开文件
    void onOpenFromTable(const QModelIndex& proxyIndex);

private:
    // 保存单条文件索引记录
    struct FileRecord {
        QString name;
        QString extension;
        QString path;
        qint64 size;
        QDateTime lastModified;
    };

    // 构建主界面控件和布局
    void buildUi();
    // 连接控件信号和槽函数
    void connectSignals();
    // 追加一条文件记录到表格模型
    void appendFileToModel(const FileRecord& file);
    // 将字节数转换为可读文本
    static QString humanReadableSize(qint64 bytes);
    // 构建顶部工具栏和快捷键
    void setupToolbar();
    // 刷新右侧结果统计文本
    void updateResultStats();
    // 按指定目录列表重建索引
    void rebuildIndexForDirectories(const QStringList& directories);
    // 从本地二进制文件加载目录列表
    void loadPersistedDirectories();
    // 将目录列表以二进制形式保存到本地
    void persistDirectories() const;
    // 将窗口几何和选中目录保存到本地
    void persistWindowState() const;
    // 从本地二进制文件恢复窗口几何和选中目录
    void loadWindowState();
    // 获取左侧列表指定项的真实目录路径
    QString directoryPathAt(int index) const;
    // 选择文件并复制到当前选中的目录
    void onImportFilesToSelectedDirectory();

protected:
    void closeEvent(QCloseEvent* event) override;

    // 左侧仅保留目录列表
    QListWidget* directoryList_ = nullptr;

    QLineEdit* searchEdit_ = nullptr;
    QComboBox* extensionCombo_ = nullptr;
    QLabel* resultStatsLabel_ = nullptr;

    QTableView* tableView_ = nullptr;
    QStandardItemModel* sourceModel_ = nullptr;
    QSortFilterProxyModel* proxyModel_ = nullptr;
};
