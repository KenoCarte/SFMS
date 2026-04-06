#include "MainWindow.h"

#include <QAction>
#include <QAbstractItemView>
#include <QApplication>
#include <QByteArray>
#include <QComboBox>
#include <QDataStream>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QStandardPaths>
#include <QTableView>
#include <QToolBar>
#include <QUuid>
#include <QUrl>
#include <QVBoxLayout>
#include <QStringList>

namespace {
    constexpr int kColumnName = 0;
    constexpr int kColumnExt = 1;
    constexpr int kColumnSize = 2;
    constexpr int kColumnModified = 3;
    constexpr int kColumnPath = 4;

    // 返回用于保存目录列表的二进制文件路径。
    QString persistedDirectoryFilePath() {
        const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir dir(appDataDir);
        dir.mkpath(".");
        return dir.filePath("directories.bin");
    }

    // 返回用于保存窗口状态的二进制文件路径。
    QString persistedWindowStateFilePath() {
        const QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir dir(appDataDir);
        dir.mkpath(".");
        return dir.filePath("window_state.bin");
    }

    // 为导入文件生成一个不会与现有文件重名的目标路径。
    QString buildUniqueTargetPath(const QString& targetDir, const QFileInfo& sourceInfo) {
        QString candidate = QDir(targetDir).filePath(sourceInfo.fileName());
        if (!QFile::exists(candidate)) {
            return candidate;
        }

        const QString stem = sourceInfo.completeBaseName();
        const QString suffix = sourceInfo.suffix();
        for (int i = 1; i <= 9999; ++i) {
            const QString suffixText = suffix.isEmpty() ? "" : "." + suffix;
            const QString name = QString("%1_%2%3").arg(stem).arg(i).arg(suffixText);
            candidate = QDir(targetDir).filePath(name);
            if (!QFile::exists(candidate)) {
                return candidate;
            }
        }

        return QDir(targetDir).filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + "_" + sourceInfo.fileName());
    }
}

// 自定义代理模型：在文件名关键字基础上，再叠加扩展名过滤条件。
class FileFilterProxyModel : public QSortFilterProxyModel {
public:
    // 构造一个附着到指定父对象的过滤代理模型。
    explicit FileFilterProxyModel(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent) {
    }

    // 设置扩展名过滤条件，并通知视图重新筛选。
    void setExtensionFilter(const QString& ext) {
        extensionFilter_ = ext;
        // 重新计算
    #if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange();
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
    #else
        invalidateFilter();
    #endif
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override {
        const QModelIndex nameIndex = sourceModel()->index(sourceRow, kColumnName, sourceParent);
        const QModelIndex extIndex = sourceModel()->index(sourceRow, kColumnExt, sourceParent);

        const QString fileName = sourceModel()->data(nameIndex).toString();
        const QString fileExt = sourceModel()->data(extIndex).toString();

        const bool keywordPass = filterRegularExpression().pattern().isEmpty()
            || fileName.contains(filterRegularExpression());

        const bool extPass = extensionFilter_ == "*"
            || fileExt.compare(extensionFilter_, Qt::CaseInsensitive) == 0;

        return keywordPass && extPass;
    }

private:
    QString extensionFilter_ = "*";
};

// 构造主窗口，并恢复保存的目录列表和窗口状态。
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    buildUi();
    loadPersistedDirectories();
    loadWindowState();
    connectSignals();
    setupToolbar();
}

// 创建主界面布局、控件、模型和样式。
void MainWindow::buildUi() {
    setWindowTitle(QStringLiteral("SFMS简单文件管理系统"));
    resize(1100, 700);
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    central->setObjectName("centralPanel");
    auto* mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(20, 20, 20, 16);
    mainLayout->setSpacing(16);

    // 目录列表
    auto* leftCard = new QFrame(this);
    leftCard->setObjectName("panelCard");
    auto* leftPanel = new QVBoxLayout(leftCard);
    leftPanel->setSpacing(10);
    leftPanel->setContentsMargins(14, 14, 14, 14);
    auto* leftTitle = new QLabel(QStringLiteral("索引目录"), this);
    directoryList_ = new QListWidget(this);
    directoryList_->setSpacing(6);

    leftPanel->addWidget(leftTitle);
    leftPanel->addWidget(directoryList_, 1);

    // 结果统计、过滤行、表格。
    auto* rightCard = new QFrame(this);
    rightCard->setObjectName("panelCard");
    auto* rightPanel = new QVBoxLayout(rightCard);
    rightPanel->setSpacing(10);
    rightPanel->setContentsMargins(14, 14, 14, 14);

    auto* resultHeaderRow = new QHBoxLayout();
    resultHeaderRow->setSpacing(10);
    auto* resultTitle = new QLabel(QStringLiteral("文件结果"), this);
    resultStatsLabel_ = new QLabel(QStringLiteral("当前显示 0 / 总计 0"), this);
    resultStatsLabel_->setObjectName("resultStatsLabel");
    resultHeaderRow->addWidget(resultTitle);
    resultHeaderRow->addStretch(1);
    resultHeaderRow->addWidget(resultStatsLabel_);

    auto* filterRow = new QHBoxLayout();
    filterRow->setSpacing(10);
    auto* searchLabel = new QLabel(QStringLiteral("文件名搜索:"), this);
    searchEdit_ = new QLineEdit(this);
    searchEdit_->setPlaceholderText(QStringLiteral("输入关键字，例如 report 或 作业"));

    auto* extLabel = new QLabel(QStringLiteral("扩展名:"), this);
    extensionCombo_ = new QComboBox(this);
    extensionCombo_->addItem("*");

    filterRow->addWidget(searchLabel);
    filterRow->addWidget(searchEdit_, 1);
    filterRow->addWidget(extLabel);
    filterRow->addWidget(extensionCombo_);

    sourceModel_ = new QStandardItemModel(this);
    sourceModel_->setHorizontalHeaderLabels({
        QStringLiteral("文件名"),
        QStringLiteral("扩展名"),
        QStringLiteral("大小"),
        QStringLiteral("修改时间"),
        QStringLiteral("所在目录")
        });

    auto* typedProxy = new FileFilterProxyModel(this);
    typedProxy->setSourceModel(sourceModel_);
    typedProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    typedProxy->setFilterKeyColumn(kColumnName);

    proxyModel_ = typedProxy;

    tableView_ = new QTableView(this);
    tableView_->setModel(proxyModel_);
    tableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableView_->setSortingEnabled(true);
    tableView_->setAlternatingRowColors(true);
    tableView_->setShowGrid(false);
    tableView_->verticalHeader()->setVisible(false);
    tableView_->verticalHeader()->setDefaultSectionSize(38);
    tableView_->horizontalHeader()->setStretchLastSection(true);
    tableView_->horizontalHeader()->setSectionResizeMode(kColumnPath, QHeaderView::Stretch);
    tableView_->horizontalHeader()->setSectionResizeMode(kColumnName, QHeaderView::ResizeToContents);
    tableView_->horizontalHeader()->setSectionResizeMode(kColumnExt, QHeaderView::ResizeToContents);

    rightPanel->addLayout(resultHeaderRow);
    rightPanel->addLayout(filterRow);
    rightPanel->addWidget(tableView_);

    leftCard->setMinimumWidth(330);
    mainLayout->addWidget(leftCard, 4);
    mainLayout->addWidget(rightCard, 6);

    setStyleSheet(
        "QWidget#centralPanel {"
        "  background: #eef3f9;"
        "}"
        "QFrame#panelCard {"
        "  background: #ffffff;"
        "  border: 1px solid #d3dce8;"
        "  border-radius: 16px;"
        "}"
        "QLabel {"
        "  color: #22364f;"
        "  font-family: 'Microsoft YaHei UI', 'Segoe UI', sans-serif;"
        "  font-size: 14px;"
        "  font-weight: 600;"
        "}"
        "QLabel#resultStatsLabel {"
        "  color: #2e4d74;"
        "  font-family: 'Microsoft YaHei UI', 'Segoe UI', sans-serif;"
        "  font-size: 13px;"
        "  font-weight: 500;"
        "  padding: 5px 12px;"
        "  background: #e8f1ff;"
        "  border: 1px solid #c7daf8;"
        "  border-radius: 999px;"
        "}"
        "QListWidget, QLineEdit, QComboBox, QTableView {"
        "  background: #fbfdff;"
        "  border: 1px solid #cfd9e6;"
        "  border-radius: 12px;"
        "  color: #1a2e44;"
        "  font-family: 'Microsoft YaHei UI', 'Segoe UI', sans-serif;"
        "  padding: 7px 10px;"
        "  font-size: 14px;"
        "}"
        "QListWidget::item {"
        "  padding: 9px 11px;"
        "  border-radius: 11px;"
        "  border: 1px solid #d8e2ef;"
        "  background: #f5f9ff;"
        "  margin: 3px 0px;"
        "}"
        "QListWidget::item:hover {"
        "  background: #eaf3ff;"
        "  border: 1px solid #b9d2f6;"
        "}"
        "QListWidget::item:selected {"
        "  background: #d8e9ff;"
        "  color: #18396b;"
        "  border: 1px solid #7cb5f5;"
        "}"
        "QLineEdit:focus, QComboBox:focus, QListWidget:focus, QTableView:focus {"
        "  border: 1px solid #3f85e6;"
        "}"
        "QTableView {"
        "  selection-background-color: #d4e6ff;"
        "  selection-color: #0f2941;"
        "  alternate-background-color: #f2f7ff;"
        "}"
        "QHeaderView::section {"
        "  background: #e9f0fa;"
        "  border: none;"
        "  border-bottom: 1px solid #cfd9e6;"
        "  padding: 9px;"
        "  color: #2a4362;"
        "  font-family: 'Microsoft YaHei UI', 'Segoe UI', sans-serif;"
        "  font-size: 13px;"
        "  font-weight: 700;"
        "}"
        "QStatusBar {"
        "  background: #edf3fc;"
        "  color: #2d4768;"
        "  border-top: 1px solid #cfd9e6;"
        "  font-family: 'Microsoft YaHei UI', 'Segoe UI', sans-serif;"
        "  font-size: 13px;"
        "}"
        "QToolBar {"
        "  background: #ffffff;"
        "  border: none;"
        "  border-bottom: 1px solid #d3dce8;"
        "  spacing: 8px;"
        "  padding: 8px 12px;"
        "}"
        "QToolButton {"
        "  background: #e6eefb;"
        "  color: #1f3f63;"
        "  border: 1px solid #c6d6ec;"
        "  border-radius: 10px;"
        "  padding: 7px 12px;"
        "  font-family: 'Microsoft YaHei UI', 'Segoe UI', sans-serif;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "}"
        "QToolButton:hover {"
        "  background: #d7e5f9;"
        "}"
        "QToolButton:pressed {"
        "  background: #c8dbf6;"
        "}"
    );

    updateResultStats();
    statusBar()->showMessage(QStringLiteral("请先添加目录并重建索引"));
}

// 连接所有信号与槽，组织目录、搜索和结果表的交互。
void MainWindow::connectSignals() {
    connect(searchEdit_, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    connect(extensionCombo_, &QComboBox::currentTextChanged, this, &MainWindow::onExtensionFilterChanged);
    connect(tableView_, &QTableView::doubleClicked, this, &MainWindow::onOpenFromTable);
    connect(directoryList_, &QListWidget::itemDoubleClicked, this, [this](auto* item) {
        if (!item) {
            return;
        }
        const QString selectedDir = item->data(Qt::UserRole).toString().isEmpty()
            ? item->text()
            : item->data(Qt::UserRole).toString();
        statusBar()->showMessage(QStringLiteral("正在重建目录: %1").arg(selectedDir), 1500);
        rebuildIndexForDirectories(QStringList{ selectedDir });
        });
    connect(directoryList_, &QListWidget::currentRowChanged, this, [this](int) {
        persistWindowState();
        });

    connect(sourceModel_, &QStandardItemModel::modelReset, this, &MainWindow::updateResultStats);
    connect(sourceModel_, &QStandardItemModel::rowsInserted, this, &MainWindow::updateResultStats);
    connect(sourceModel_, &QStandardItemModel::rowsRemoved, this, &MainWindow::updateResultStats);
    connect(proxyModel_, &QSortFilterProxyModel::modelReset, this, &MainWindow::updateResultStats);
    connect(proxyModel_, &QSortFilterProxyModel::rowsInserted, this, &MainWindow::updateResultStats);
    connect(proxyModel_, &QSortFilterProxyModel::rowsRemoved, this, &MainWindow::updateResultStats);
}

// 创建顶部工具栏，并绑定快捷操作。
void MainWindow::setupToolbar() {
    auto* toolbar = addToolBar(QStringLiteral("快速操作"));
    toolbar->setMovable(false);
    toolbar->setFloatable(false);

    auto* addDirAction = toolbar->addAction(QStringLiteral("添加目录"));
    auto* removeDirAction = toolbar->addAction(QStringLiteral("移除目录"));
    auto* rebuildAction = toolbar->addAction(QStringLiteral("重建索引"));
    auto* importFilesAction = toolbar->addAction(QStringLiteral("导入文件到选中目录"));
    auto* focusSearchAction = toolbar->addAction(QStringLiteral("聚焦搜索"));
    auto* resetFilterAction = toolbar->addAction(QStringLiteral("清空筛选"));

    addDirAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+L")));
    removeDirAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+L")));
    rebuildAction->setShortcut(QKeySequence::Refresh);
    importFilesAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+I")));
    focusSearchAction->setShortcut(QKeySequence::Find);
    resetFilterAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));

    connect(addDirAction, &QAction::triggered, this, &MainWindow::onAddDirectory);
    connect(removeDirAction, &QAction::triggered, this, &MainWindow::onRemoveSelectedDirectory);
    connect(rebuildAction, &QAction::triggered, this, &MainWindow::onBuildIndex);
    connect(importFilesAction, &QAction::triggered, this, &MainWindow::onImportFilesToSelectedDirectory);
    connect(focusSearchAction, &QAction::triggered, this, [this]() {
        searchEdit_->setFocus();
        searchEdit_->selectAll();
        });
    connect(resetFilterAction, &QAction::triggered, this, [this]() {
        searchEdit_->clear();
        extensionCombo_->setCurrentIndex(0);
        updateResultStats();
        statusBar()->showMessage(QStringLiteral("筛选条件已清空"), 2000);
        });
}

// 刷新右侧结果统计文本。
void MainWindow::updateResultStats() {
    if (!resultStatsLabel_) {
        return;
    }

    const int total = sourceModel_ ? sourceModel_->rowCount() : 0;
    const int visible = proxyModel_ ? proxyModel_->rowCount() : 0;
    resultStatsLabel_->setText(QStringLiteral("当前显示 %1 / 总计 %2").arg(visible).arg(total));
}

// 将窗口几何和当前选中的目录保存到本地。
void MainWindow::persistWindowState() const {
    QFile outFile(persistedWindowStateFilePath());
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }

    const QString selectedDir = directoryPathAt(directoryList_ ? directoryList_->currentRow() : -1);
    QDataStream out(&outFile);
    out.setVersion(QDataStream::Qt_6_5);
    out << static_cast<quint32>(0x53464D53)
        << static_cast<quint16>(2)
        << saveGeometry()
        << selectedDir;
}

// 从本地恢复窗口几何和上次选中的目录。
void MainWindow::loadWindowState() {
    QFile inFile(persistedWindowStateFilePath());
    if (!inFile.exists() || !inFile.open(QIODevice::ReadOnly)) {
        return;
    }

    QDataStream in(&inFile);
    in.setVersion(QDataStream::Qt_6_5);

    quint32 magic = 0;
    quint16 version = 0;
    QByteArray geometry;
    QString selectedDir;
    in >> magic >> version >> geometry >> selectedDir;

    if (in.status() != QDataStream::Ok || magic != 0x53464D53 || version != 2) {
        return;
    }

    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }

    if (!selectedDir.isEmpty()) {
        for (int i = 0; i < directoryList_->count(); ++i) {
            if (directoryPathAt(i).compare(selectedDir, Qt::CaseInsensitive) == 0) {
                directoryList_->setCurrentRow(i);
                break;
            }
        }
    }
}

// 在关闭窗口前保存目录列表和窗口状态。
void MainWindow::closeEvent(QCloseEvent* event) {
    persistDirectories();
    persistWindowState();
    QMainWindow::closeEvent(event);
}

// 读取左侧列表中某一项对应的真实目录路径。
QString MainWindow::directoryPathAt(int index) const {
    if (index < 0 || index >= directoryList_->count()) {
        return QString();
    }

    const QListWidgetItem* item = directoryList_->item(index);
    const QString stored = item->data(Qt::UserRole).toString();
    return stored.isEmpty() ? item->text() : stored;
}

// 从磁盘恢复上次保存的目录列表。
void MainWindow::loadPersistedDirectories() {
    QFile inFile(persistedDirectoryFilePath());
    if (!inFile.exists()) {
        return;
    }

    if (!inFile.open(QIODevice::ReadOnly)) {
        statusBar()->showMessage(QStringLiteral("目录缓存读取失败"), 2000);
        return;
    }

    QDataStream in(&inFile);
    in.setVersion(QDataStream::Qt_6_5);

    quint32 magic = 0;
    quint16 version = 0;
    QStringList directories;
    in >> magic >> version >> directories;

    if (in.status() != QDataStream::Ok || magic != 0x53464D53 || version != 1) {
        statusBar()->showMessage(QStringLiteral("目录缓存格式不匹配，已忽略"), 2500);
        return;
    }

    for (const QString& dirPathRaw : directories) {
        const QString dirPath = QDir::toNativeSeparators(QDir(dirPathRaw).absolutePath());
        if (!QFileInfo::exists(dirPath) || !QFileInfo(dirPath).isDir()) {
            continue;
        }

        QString displayName = QFileInfo(dirPath).fileName();
        if (displayName.isEmpty()) {
            displayName = dirPath;
        }

        auto* item = new QListWidgetItem(displayName);
        item->setData(Qt::UserRole, dirPath);
        item->setToolTip(dirPath);
        directoryList_->addItem(item);
    }
}

// 将当前目录列表以二进制格式保存到本地。
void MainWindow::persistDirectories() const {
    QStringList directories;
    directories.reserve(directoryList_->count());
    for (int i = 0; i < directoryList_->count(); ++i) {
        const QString dir = directoryPathAt(i);
        if (!dir.isEmpty()) {
            directories.push_back(dir);
        }
    }

    QFile outFile(persistedDirectoryFilePath());
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }

    QDataStream out(&outFile);
    out.setVersion(QDataStream::Qt_6_5);
    out << static_cast<quint32>(0x53464D53) << static_cast<quint16>(1) << directories;
}

// 扫描指定目录集合并重建索引结果表。
void MainWindow::rebuildIndexForDirectories(const QStringList& directories) {
    if (directories.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先添加至少一个目录。"));
        return;
    }

    sourceModel_->clear();
    sourceModel_->setHorizontalHeaderLabels({
        QStringLiteral("文件名"),
        QStringLiteral("扩展名"),
        QStringLiteral("大小"),
        QStringLiteral("修改时间"),
        QStringLiteral("所在目录")
        });

    QStringList allExtensions;
    int fileCount = 0;

    QApplication::setOverrideCursor(Qt::WaitCursor);

    for (const QString& rootDir : directories) {
        QDirIterator it(rootDir,
            QDir::Files | QDir::NoDotAndDotDot,
            QDirIterator::Subdirectories);

        while (it.hasNext()) {
            it.next();
            const QFileInfo info = it.fileInfo();

            FileRecord file;
            file.name = info.fileName();
            file.extension = info.suffix().isEmpty() ? "(none)" : "." + info.suffix().toLower();
            file.path = info.absoluteFilePath();
            file.size = info.size();
            file.lastModified = info.lastModified();

            appendFileToModel(file);
            allExtensions.push_back(file.extension);
            ++fileCount;
        }
    }

    QApplication::restoreOverrideCursor();

    allExtensions.removeDuplicates();
    allExtensions.sort(Qt::CaseInsensitive);

    extensionCombo_->blockSignals(true);
    extensionCombo_->clear();
    extensionCombo_->addItem("*");
    for (const QString& ext : allExtensions) {
        extensionCombo_->addItem(ext);
    }
    extensionCombo_->setCurrentIndex(0);
    extensionCombo_->blockSignals(false);

    updateResultStats();
    statusBar()->showMessage(QStringLiteral("索引完成，共 %1 个文件").arg(fileCount));
}

// 让用户选择一个新目录并追加到左侧目录列表中。
void MainWindow::onAddDirectory() {
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择要索引的目录"));
    if (dir.isEmpty()) return;

    const QString normalizedDir = QDir::toNativeSeparators(QDir(dir).absolutePath());
    for (int i = 0; i < directoryList_->count(); i++) {
        const QListWidgetItem* it = directoryList_->item(i);
        const QString storedDir = it->data(Qt::UserRole).toString().isEmpty()
            ? it->text()
            : it->data(Qt::UserRole).toString();
        if (storedDir.compare(normalizedDir, Qt::CaseInsensitive) == 0) {
            statusBar()->showMessage(QStringLiteral("目录已存在"), 2000);
            return;
        }
    }

    QString displayName = QFileInfo(normalizedDir).fileName();
    if (displayName.isEmpty()) {
        displayName = normalizedDir;
    }

    auto* item = new QListWidgetItem(displayName);
    item->setData(Qt::UserRole, normalizedDir);
    item->setToolTip(normalizedDir);
    directoryList_->addItem(item);
    persistDirectories();
    statusBar()->showMessage(QStringLiteral("已添加目录"), 2000);
}

// 删除左侧当前选中的目录。
void MainWindow::onRemoveSelectedDirectory() {
    const int row = directoryList_->currentRow();
    if (row < 0) return;
    delete directoryList_->takeItem(row);
    persistDirectories();
    statusBar()->showMessage(QStringLiteral("已移除目录"), 2000);
}

// 根据左侧所有目录重新建立完整索引。
void MainWindow::onBuildIndex() {
    QStringList dirs;
    dirs.reserve(directoryList_->count());
    for (int i = 0; i < directoryList_->count(); i++) {
        const QString dir = directoryPathAt(i);
        dirs.push_back(dir);
    }
    rebuildIndexForDirectories(dirs);
}

// 让用户选择文件，并复制到左侧当前选中的目录。
void MainWindow::onImportFilesToSelectedDirectory() {
    const int selectedIndex = directoryList_->currentRow();
    const QString targetDir = directoryPathAt(selectedIndex);
    if (targetDir.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先在左侧选中一个目录。"));
        return;
    }

    const QStringList sourceFiles = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("选择要导入的文件"),
        QString(),
        QStringLiteral("所有文件 (*.*)"));
    if (sourceFiles.isEmpty()) {
        return;
    }

    int successCount = 0;
    int failCount = 0;
    for (const QString& src : sourceFiles) {
        const QFileInfo srcInfo(src);
        if (!srcInfo.exists() || !srcInfo.isFile()) {
            ++failCount;
            continue;
        }

        const QString targetPath = buildUniqueTargetPath(targetDir, srcInfo);
        if (QDir::toNativeSeparators(srcInfo.absoluteFilePath()).compare(QDir::toNativeSeparators(targetPath), Qt::CaseInsensitive) == 0) {
            ++failCount;
            continue;
        }

        if (QFile::copy(srcInfo.absoluteFilePath(), targetPath)) {
            ++successCount;
        }
        else {
            ++failCount;
        }
    }

    statusBar()->showMessage(QStringLiteral("导入完成：成功 %1，失败 %2").arg(successCount).arg(failCount), 3000);
    rebuildIndexForDirectories(QStringList{ targetDir });
}

// 根据关键字更新文件名过滤条件。
void MainWindow::onSearchTextChanged(const QString& text) {
    auto* typedProxy = static_cast<FileFilterProxyModel*>(proxyModel_);
    typedProxy->setFilterRegularExpression(QRegularExpression(
        QRegularExpression::escape(text),
        QRegularExpression::CaseInsensitiveOption));
    updateResultStats();
}

// 根据下拉框更新扩展名过滤条件。
void MainWindow::onExtensionFilterChanged(const QString& ext) {
    auto* typedProxy = static_cast<FileFilterProxyModel*>(proxyModel_);
    typedProxy->setExtensionFilter(ext);
    updateResultStats();
}

// 双击结果项后，使用系统默认程序打开文件。
void MainWindow::onOpenFromTable(const QModelIndex& proxyIndex) {
    if (!proxyIndex.isValid()) {
        return;
    }

    const QModelIndex sourceIndex = proxyModel_->mapToSource(proxyIndex);
    const QStandardItem* pathItem = sourceModel_->item(sourceIndex.row(), kColumnPath);
    const QString path = pathItem ? pathItem->data(Qt::UserRole + 1).toString() : QString();

    const bool ok = QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("打开失败"), QStringLiteral("无法打开文件:\n%1").arg(path));
    }
}

// 将一条文件记录写入表格模型。
void MainWindow::appendFileToModel(const FileRecord& file) {
    QList<QStandardItem*> row;

    const QFileInfo fileInfo(file.path);
    const QString parentDirName = fileInfo.dir().dirName().isEmpty()
        ? fileInfo.dir().absolutePath()
        : fileInfo.dir().dirName();

    auto* nameItem = new QStandardItem(file.name);
    auto* extItem = new QStandardItem(file.extension);
    auto* sizeItem = new QStandardItem(humanReadableSize(file.size));
    auto* timeItem = new QStandardItem(file.lastModified.toString("yyyy-MM-dd HH:mm:ss"));
    auto* pathItem = new QStandardItem(parentDirName);

    sizeItem->setData(file.size, Qt::UserRole + 1);
    timeItem->setData(file.lastModified, Qt::UserRole + 1);
    pathItem->setData(file.path, Qt::UserRole + 1);

    row << nameItem << extItem << sizeItem << timeItem << pathItem;
    sourceModel_->appendRow(row);
}

// 将字节数转换成便于阅读的大小文本。
QString MainWindow::humanReadableSize(qint64 bytes) {
    const qint64 kb = 1024;
    const qint64 mb = kb * 1024;
    const qint64 gb = mb * 1024;
    if (bytes >= gb) {
        return QString::number(bytes / static_cast<double>(gb), 'f', 2) + " GB";
    }
    if (bytes >= mb) {
        return QString::number(bytes / static_cast<double>(mb), 'f', 2) + " MB";
    }
    if (bytes >= kb) {
        return QString::number(bytes / static_cast<double>(kb), 'f', 2) + " KB";
    }
    return QString::number(bytes) + " B";
}
