#include "AdtConsistencyCheckApp.h"

AdtConsistencyCheckApp::AdtConsistencyCheckApp(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::AdtConsistencyCheckApp) {
    ui->setupUi(this);
    this->dataTypeChecker = std::make_unique<DataTypeConsistencyCheck>();
}

void AdtConsistencyCheckApp::on_pb_open_clicked() {
    //获取上次打开的位置
    const QString open_dir = utils.getLastOpenDir();
    fileName = QFileDialog::getOpenFileName(this, "Open the File", open_dir,
                                            "Excel (*.xlsx);;All Files (*)");
    if (fileName.isEmpty()) return;

    //存储新位置
    utils.getLastOpenDir(fileName);
    setWindowTitle(fileName);

    ui->lineEdit->setText(fileName);
    this->dataTypeChecker->set_file_path(fileName);
    this->dataTypeChecker->set_text_edit(ui->textEdit);
    this->dataTypeChecker->readDataType();
}

void AdtConsistencyCheckApp::on_actionOpen_triggered() {
    on_pb_open_clicked();
}

void AdtConsistencyCheckApp::on_actionSave_triggered() const {
    //将错误信息保存到text文件，与校验文件位置相同
    if (!this->fileName.isEmpty()) {
        const QString logName = QFileInfo(this->fileName).absolutePath() + "/"
                                + QFileInfo(this->fileName).completeBaseName() + "_check_result.txt";
        QFile file(logName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            //如果文件存在，先清除原来的内容后再写入
            if (file.exists())
                file.resize(0);

            QTextStream out(&file);
            out << ui->textEdit->toPlainText();
            file.close();
            // 在textEdit中提示保存成功，textEdit跳转到最后一行
            ui->textEdit->moveCursor(QTextCursor::End);
            const QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
            ui->textEdit->insertPlainText("\n[" + currentTime + "] 检查结果保存成功，与输入文件同目录！");
        }
    }
}

void AdtConsistencyCheckApp::on_actionClear_triggered() const {
    //检查textEdit是否为空
    if (!ui->textEdit->toPlainText().isEmpty())
        ui->textEdit->clear();
}

void AdtConsistencyCheckApp::on_actionRefresh_triggered() const {
    if (!this->fileName.isEmpty())
        this->dataTypeChecker->readDataType();
    // readApplicationDataType(this->fileName.toStdString());
}
