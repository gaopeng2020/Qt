//
// Created by gaopeng on 2026/3/22.
//

#ifndef DATATYPECONSISTENCYCHECK_H
#define DATATYPECONSISTENCYCHECK_H
#include <unordered_set>
#include <string>
#include <QSettings>
#include <QTextEdit>
#include <QRegularExpression>
#include <glog/logging.h>
#include "xlsxdocument.h"
#include "CommonUtils/common_utils.h"

class DataTypeConsistencyCheck {
    friend class ValueConsistencyCheck;

protected:
    const std::unordered_set<std::string> BASIC_TYPES = {
        "bool", "uint8", "uint16", "uint32", "uint64",
        "sint8", "sint16", "sint32", "sint64", "float32", "float64", "\\"
    };
    int dataTypeNameCol;
    int categoryCol;

    int memPosiCol;
    int memNameCol;
    int memTypeRefCol;

    int lenTypeCol;
    int minLenCol;
    int maxLenCol;

    int baseTypeCol;
    int sigLenCol;
    int factorCol;
    int offsetCol;
    int minPhyCol;
    int maxPhyCol;
    int unitCol;
    int tableCol;
    QString filePath;
    QTextEdit *textEdit;
    QStringList errors;
    common_utils utils;

public:
    void set_file_path(const QString &file_path) {
        filePath = file_path;
    }

    void set_text_edit(QTextEdit *text_edit) {
        textEdit = text_edit;
    }
    void readDataType();

    explicit DataTypeConsistencyCheck();
    explicit DataTypeConsistencyCheck(QString &filePath, QTextEdit *textEdit);
    ~DataTypeConsistencyCheck() = default;

private:
    std::set<QString> dataTypeNames;
    void recordConsistencyCheck(const QXlsx::Worksheet * sheet, int startRow,int endRow);
    void recordElementTypeRefDefine(const QXlsx::Worksheet *sheet, int row, const QString &memTypeRef);
    void recordElementTypeDirectDefine(const QXlsx::Worksheet *sheet, int row, const QString &memTypeRef);

    void arrayConsistencyCheck(const QXlsx::Worksheet *sheet, int row) ;
    void arrayElementTypeRefDefine(const QXlsx::Worksheet *sheet, int row, const QString &memTypeRef) ;
    void arrayElementTypeDirectDefine(const QXlsx::Worksheet *sheet, int row, const QString &memTypeRef);

    void unusedCellForBasicType(const QXlsx::Worksheet *sheet, int row);

    void checkDataType(const QXlsx::Worksheet *sheet, QString &currentTime);
    void valueConsistencyCheck(const QXlsx::Worksheet *sheet, int row) ;
    void checkMustDefinedCell(const QXlsx::Worksheet *sheet, int row);
    int checkValueTable(const QString &valueTable, const QString &cellAddress);
    int checkBitField(const QString &valueTable, const QString &cellAddress);
    void checkSignalLength(double maxValue, int len, int row);

};


#endif //DATATYPECONSISTENCYCHECK_H
