//
// Created by gaopeng on 2026/3/22.
//

#include "DataTypeConsistencyCheck.h"

namespace {
    enum class compuMethodType {
        TEXT_TABLE,
        BIT_FIELD,
        LINEAR,
        IDENTICAL,
        NOT_SUPPORTED,
        ERROR
    };

    QString checkUnusedCell(QString value, const std::string &cellAddress) {
        value = value.trimmed();
        if (value.isEmpty() || value == "/" || value == "\\") {
            return nullptr;
        } else {
            return QString::fromStdString(cellAddress) +
                   QStringLiteral(u" Value类型此处应为空，或用\\,/填充；实际为:") +
                   value;
        }
    }

    compuMethodType compumethodType(const QString &valueTable, const QString &factor, const QString &offset) {
        // 去除空格
        QString vt = valueTable.trimmed();
        QString f = factor.trimmed();
        QString o = offset.trimmed();

        // 静态正则表达式
        static QRegularExpression textTablePattern("^0x[0-9a-f]{1,2}[=:：]\\s*",
                                                   QRegularExpression::CaseInsensitiveOption);
        static QRegularExpression bitFieldPattern("^bit[0-9]{1,2}[=:：]\\s*",
                                                  QRegularExpression::CaseInsensitiveOption);

        const bool vtIsEmpty = vt.isEmpty();
        const bool isTextTableFormat = textTablePattern.match(vt).hasMatch();
        const bool isBitFieldFormat = bitFieldPattern.match(vt).hasMatch();

        // 解析 factor 和 offset
        bool factorOk, offsetOk;
        const double factorVal = f.toDouble(&factorOk);
        const double offsetVal = o.toDouble(&offsetOk);
        const bool factorIsOne = qFuzzyCompare(factorVal, 1.0);
        const bool offsetIsZero = qFuzzyIsNull(offsetVal);

        // 规则5: valueTable满足1或2，但factor!=1 || offset!=0但必须是数字 → NOT_SUPPORTED
        if ((isTextTableFormat || isBitFieldFormat) && factorOk && offsetOk && (!factorIsOne || !offsetIsZero)) {
            return compuMethodType::NOT_SUPPORTED;
        }

        // 规则1: valueTable以0x开头，紧跟=或: → TEXT_TABLE
        if (isTextTableFormat) {
            return compuMethodType::TEXT_TABLE;
        }

        // 规则2: valueTable以Bit开头，紧跟=或: → BIT_FIELD
        if (isBitFieldFormat) {
            return compuMethodType::BIT_FIELD;
        }

        // 如果 factor 或 offset 不是有效数字，后面的规则无法判断，返回 ERROR
        if (!factorOk || !offsetOk) {
            return compuMethodType::ERROR;
        }

        // 规则3: valueTable为空 & factor=1 & offset=0 → IDENTICAL
        if (vtIsEmpty && factorIsOne && offsetIsZero) {
            return compuMethodType::IDENTICAL;
        }

        // 规则4: valueTable为空但(factor!=1 || offset!=0) → LINEAR
        if (vtIsEmpty && (!factorIsOne || !offsetIsZero)) {
            return compuMethodType::LINEAR;
        }

        // 默认情况返回 ERROR
        return compuMethodType::ERROR;
    }

    QList<int> extractNumbers(const QString &input, const QString &prefix) {
        QList<int> numbers;
        // 构建动态正则表达式，支持自定义前缀（如 "0x" 或 "Bit"）
        // 匹配模式：前缀 + 十六进制/十进制数字 (可选范围 -前缀 + 数字)
        // 注意：Bit 后面通常是十进制，0x 后面是十六进制，这里根据前缀自动选择进制
        const bool isHex = prefix.compare("0x", Qt::CaseInsensitive) == 0;
        QString numPattern = isHex ? "([0-9A-F]+)" : "([0-9]+)";
        const QString patternStr = QString(R"(%1%2(?:-%1%2)?)").arg(prefix, numPattern);
        QRegularExpression regex(patternStr, QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator it = regex.globalMatch(input);

        const int base = isHex ? 16 : 10;
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();

            bool ok1, ok2;
            // captured(1) 是第一个数字部分
            const int first = match.captured(1).toInt(&ok1, base);

            // captured(2) 是范围结束的数字部分（如果有）
            if (match.hasCaptured(2) && !match.captured(2).isEmpty()) {
                const int second = match.captured(2).toInt(&ok2, base);
                if (ok1 && ok2) {
                    // 添加从 first 到 second 的所有数字（不去重）
                    for (int i = first; i <= second; ++i) {
                        numbers.append(i);
                    }
                }
            } else if (ok1) {
                numbers.append(first);
            }
        }

        // 只排序，不去重
        std::sort(numbers.begin(), numbers.end());
        return numbers;
    }

    void checkDuplicatedNums(QList<int> &numbers, QStringList &errors, const QString &cellAddress) {
        QSet<int> seen;
        for (int value: numbers) {
            if (seen.contains(value)) {
                errors.append(cellAddress + QString(": 发现重复的值：%1").arg(value));
            }
            seen.insert(value);
        }
    }

    void checkIncontinuityNums(QList<int> &numbers, QStringList &errors, const QString &cellAddress) {
        int expected = numbers.first();
        for (int value: numbers) {
            if (value != expected) {
                errors.append(cellAddress + QString(": 发现不连续的值：%1，期望值为：%2")
                              .arg(value).arg(expected));
            }
            expected++;
        }
    }

    QStringList extractBitFieldVariable(const QStringList &bitFieldVars) {
        QStringList result;
        // 最简单的匹配：分隔符后面跟着非分隔符字符
        static const QRegularExpression regex(R"([=:：][^=:：]+)");
        for (const QString &line: bitFieldVars) {
            QRegularExpressionMatchIterator it = regex.globalMatch(line);
            while (it.hasNext()) {
                QRegularExpressionMatch match = it.next();
                QString captured = match.captured(0); // 获取整个匹配
                // 去掉开头的分隔符
                if (captured.startsWith('=') || captured.startsWith(':') || captured.startsWith("：")) {
                    captured = captured.mid(1);
                }
                captured = captured.trimmed();
                if (!captured.isEmpty()) {
                    result.append(captured);
                }
            }
        }

        return result;
    }

    void checkBitFieldInside(QStringList &bitFieldVars, QStringList &errors, const QString &cellAddress) {
        // 检查括号内的位域值定义 (例如：00=no, 01=yes) 是否为二进制
        QList<int> list;
        for (const QString &varDef: bitFieldVars) {
            QString trimmed = varDef.trimmed();
            if (trimmed.isEmpty()) continue;

            // 使用正则提取开头的二进制部分，支持 1~3 位二进制
            static const QRegularExpression binRegex(R"(^([01]{1,3})\s*[=:：]\s*(.+)$)");
            QRegularExpressionMatch match = binRegex.match(trimmed);

            if (!match.hasMatch()) {
                errors.append(cellAddress + QString(": 位域值定义格式错误：'%1'（应为：二进制值=变量名，如 00=no）").arg(trimmed));
            } else {
                bool ok;
                int value = match.captured(1).toInt(&ok, 2); // 使用二进制转换
                if (ok) {
                    list.push_back(value);  // 对于临时变量，使用 push_back 代替 emplace，否则emplace可能会构造成0
                } else {
                    errors.append(cellAddress + QString(": 二进制值转换失败：'%1'").arg(match.captured(1)));
                }
            }
        }
        // 检查位域值是否重复
        std::sort(list.begin(), list.end());
        checkDuplicatedNums(list, errors, cellAddress);
        // 检查位域值是否连续
        checkIncontinuityNums(list, errors, cellAddress);
    }
}

DataTypeConsistencyCheck::DataTypeConsistencyCheck()
    : dataTypeNameCol(-1), categoryCol(-1),
      memPosiCol(-1), memNameCol(-1), memTypeRefCol(-1),
      lenTypeCol(-1), minLenCol(-1), maxLenCol(-1),
      baseTypeCol(-1), sigLenCol(-1), factorCol(-1),
      offsetCol(-1), minPhyCol(-1), maxPhyCol(-1), unitCol(-1),
      tableCol(-1), textEdit(nullptr) {
}

DataTypeConsistencyCheck::DataTypeConsistencyCheck(QString &filePath, QTextEdit *textEdit)
    : dataTypeNameCol(-1), categoryCol(-1),
      memPosiCol(-1), memNameCol(-1), memTypeRefCol(-1),
      lenTypeCol(-1), minLenCol(-1), maxLenCol(-1),
      baseTypeCol(-1), sigLenCol(-1), factorCol(-1), offsetCol(-1),
      minPhyCol(-1), maxPhyCol(-1), unitCol(-1), tableCol(-1),
      filePath(std::move(filePath)), textEdit(textEdit) {
    readDataType();
}

void DataTypeConsistencyCheck::readDataType() {
    errors.clear();
    textEdit->clear();
    QXlsx::Document doc(filePath);
    if (!doc.load())
        return;

    doc.selectSheet(4);
    QXlsx::Worksheet *sheet = doc.currentWorksheet();
    textEdit->append("currentWorksheet is： " + sheet->sheetName());

    //根据表头（存储在config.ini中允许编译后修改表头）获取列号
    QString configPath = QCoreApplication::applicationDirPath() + "/conf/header_config.ini";
    if (!QFile::exists(configPath)) {
        LOG(ERROR) << "Configuration file not found:" << configPath.toStdString();
        return;
    }
    QSettings settings(configPath, QSettings::IniFormat);
    settings.beginGroup("AdtColumnHeaders");

    dataTypeNameCol = utils.getColumnByHeader(sheet, settings.value("DataTypeName").toString().toStdString());
    categoryCol = utils.getColumnByHeader(sheet, settings.value("DataTypeCategory").toString().toStdString());

    memPosiCol = utils.getColumnByHeader(sheet, settings.value("MemberPosition").toString().toStdString());
    memNameCol = utils.getColumnByHeader(sheet, settings.value("MemberName").toString().toStdString());
    memTypeRefCol = utils.getColumnByHeader(sheet, settings.value("MemberDataTypeReference").toString().toStdString());

    lenTypeCol = utils.getColumnByHeader(sheet, settings.value("StringLengthType").toString().toStdString());
    minLenCol = utils.getColumnByHeader(sheet, settings.value("StringLengthMin").toString().toStdString());
    maxLenCol = utils.getColumnByHeader(sheet, settings.value("StringLengthMax").toString().toStdString());

    baseTypeCol = utils.getColumnByHeader(sheet, settings.value("BasicDataType").toString().toStdString());
    sigLenCol = utils.getColumnByHeader(sheet, settings.value("SignalLength").toString().toStdString());
    factorCol = utils.getColumnByHeader(sheet, settings.value("Resolution").toString().toStdString());
    offsetCol = utils.getColumnByHeader(sheet, settings.value("Offset").toString().toStdString());
    minPhyCol = utils.getColumnByHeader(sheet, settings.value("MinPhysicalValue").toString().toStdString());
    maxPhyCol = utils.getColumnByHeader(sheet, settings.value("MaxPhysicalValue").toString().toStdString());
    unitCol = utils.getColumnByHeader(sheet, settings.value("Unit").toString().toStdString());
    tableCol = utils.getColumnByHeader(sheet, settings.value("TableValue").toString().toStdString());
    settings.endGroup();

    //检查输入的表格是否有效
    if (dataTypeNameCol == -1 || categoryCol == -1
        || memPosiCol == -1 || memNameCol == -1 || memTypeRefCol == -1
        || lenTypeCol == -1 || minLenCol == -1 || maxLenCol == -1
        || baseTypeCol == -1 || sigLenCol == -1 || factorCol == -1 || offsetCol == -1
        || minPhyCol == -1 || maxPhyCol == -1 || unitCol == -1 || tableCol == -1) {
        auto msg = "输入文件无效，请检查输入的Excel是否为Application Data Type表格，且不能修改模板的表头,自定义的ADT必须放在第一个sheet中";
        LOG(ERROR) << msg;
        textEdit->append(msg);
        return;
    }
    //提示开始检查
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    LOG(INFO) << QFileInfo(filePath).fileName().toStdString() + " 正在执行检查，请等待...";
    textEdit->append("[" + currentTime + "]" + QFileInfo(filePath).fileName() + " 正在执行检查，请等待...");

    //获取所有的data name
    for (int row = 2; row <= sheet->dimension().rowCount(); row++) {
        auto name = utils.readCellValue(sheet->cellAt(row, dataTypeNameCol));
        if (name.isEmpty()) continue;
        dataTypeNames.emplace(name);
    }
    checkDataType(sheet, currentTime);
}

void DataTypeConsistencyCheck::checkDataType(const QXlsx::Worksheet *sheet, QString &currentTime) {
    int row = 2;
    while (row <= sheet->dimension().rowCount()) {
        const int endRow = utils.getLastRowNum(sheet, row, dataTypeNameCol);

        auto name = utils.readCellValue(sheet->cellAt(row, dataTypeNameCol));
        auto category = utils.readCellValue(sheet->cellAt(row, categoryCol));

        //data type name check
        if (!utils.isValidCVariableName(name.toStdString()))
            errors.append(QString::fromStdString(utils.cellNumberToLetter(row, dataTypeNameCol))
                          + ": " + name + ": 数据类型名称不符合要求（大写字母开头，只能包含字母、数字、下划线，长度不超过32个字符）");
        if (category == "Value") {
            valueConsistencyCheck(sheet, row);
        } else if (category == "Struct") {
            recordConsistencyCheck(sheet, row, endRow);
        } else if (category == "Array") {
        } else {
        }

        //结构体
        // if (utils.isCellMerged(sheet, row, dataTypeNameCol)) {
        //     // qDebug().noquote() << name << "is merged,end row num =" << end_row_num;
        //     row = end_row_num + 1;
        // } else {
        //     //基础类型
        //     row++;
        // }
        row = endRow + 1;
    }
    // 将错误信息输出到textEdit控件
    if (!errors.isEmpty()) {
        //输出检查完成的提示
        currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
        textEdit->append(errors.join("\n"));
        textEdit->append("[" + currentTime + "]" + QFileInfo(filePath).fileName() + " 已完成检查，请按照提示修改！");
    } else {
        textEdit->setPlainText("没有发现错误");
    }
}

/*=================================================Value check===================================================*/

void DataTypeConsistencyCheck::valueConsistencyCheck(const QXlsx::Worksheet *sheet, const int row) {
    //无需填写的单元格检查（不适用Value）
    const auto memPosition = utils.readCellValue(sheet->cellAt(row, memPosiCol));
    QString result = checkUnusedCell(memPosition, utils.cellNumberToLetter(row, memPosiCol));
    if (result != nullptr)
        errors.append(result);

    const auto memName = utils.readCellValue(sheet->cellAt(row, memNameCol));
    result = checkUnusedCell(memName, utils.cellNumberToLetter(row, memNameCol));
    if (result != nullptr)
        errors.append(result);

    const auto memType = utils.readCellValue(sheet->cellAt(row, memTypeRefCol));
    result = checkUnusedCell(memType, utils.cellNumberToLetter(row, memTypeRefCol));
    if (result != nullptr)
        errors.append(result);

    const auto arrayType = utils.readCellValue(sheet->cellAt(row, lenTypeCol));
    result = checkUnusedCell(arrayType, utils.cellNumberToLetter(row, lenTypeCol));
    if (result != nullptr)
        errors.append(result);

    const auto arrayMinLen = utils.readCellValue(sheet->cellAt(row, minLenCol));
    result = checkUnusedCell(arrayMinLen, utils.cellNumberToLetter(row, minLenCol));
    if (result != nullptr)
        errors.append(result);

    const auto arrayMaxLen = utils.readCellValue(sheet->cellAt(row, maxLenCol));
    result = checkUnusedCell(arrayMaxLen, utils.cellNumberToLetter(row, maxLenCol));
    if (result != nullptr)
        errors.append(result);
    //必须填写的单元格检查
    checkMustDefinedCell(sheet, row);
}

void DataTypeConsistencyCheck::checkMustDefinedCell(const QXlsx::Worksheet *sheet, const int row) {
    if (const auto baseType = utils.readCellValue(sheet->cellAt(row, baseTypeCol));
        !BASIC_TYPES.count(baseType.toStdString()))
        errors.append(QString::fromStdString(utils.cellNumberToLetter(row, baseTypeCol)) + "基础数据类型不在支持的范围内,"
                      "仅支持{bool,uint8,uint16,uint32,uint64,sint8," "sint16,sint32,sint64,float32,float64,\\}");

    bool isSignal;
    const int sigLen = utils.readCellValue(sheet->cellAt(row, sigLenCol)).toInt(&isSignal);
    if (!isSignal)
        errors.append(QString::fromStdString(utils.cellNumberToLetter(row, sigLenCol)) + " Value类型的ADT，信号长度必须填写");

    const auto valueTable = utils.readCellValue(sheet->cellAt(row, tableCol));
    const auto factor = utils.readCellValue(sheet->cellAt(row, factorCol));
    const auto offset = utils.readCellValue(sheet->cellAt(row, offsetCol));

    const auto minPhy = utils.readCellValue(sheet->cellAt(row, minPhyCol));
    const auto maxPhy = utils.readCellValue(sheet->cellAt(row, maxPhyCol));
    const auto unit = utils.readCellValue(sheet->cellAt(row, unitCol));

    const auto tableCellAddr = QString::fromStdString(utils.cellNumberToLetter(row, tableCol));

    if (const auto methodType = compumethodType(valueTable, factor, offset);
        methodType == compuMethodType::TEXT_TABLE || methodType == compuMethodType::BIT_FIELD) {
        QString result = checkUnusedCell(factor, utils.cellNumberToLetter(row, factorCol));
        if (result != nullptr)
            errors.append(QString::fromStdString(utils.cellNumberToLetter(row, factorCol))
                          + ": 枚举或位域，此处建议保持为空，或者填写/或\\");

        result = checkUnusedCell(offset, utils.cellNumberToLetter(row, offsetCol));
        if (result != nullptr)
            errors.append(QString::fromStdString(utils.cellNumberToLetter(row, offsetCol))
                          + ": 枚举或位域，此处建议保持为空，或者填写/或\\");

        result = checkUnusedCell(minPhy, utils.cellNumberToLetter(row, minPhyCol));
        if (result != nullptr)
            errors.append(QString::fromStdString(utils.cellNumberToLetter(row, minPhyCol))
                          + ": 枚举或位域，此处建议保持为空，或者填写/或\\");

        result = checkUnusedCell(maxPhy, utils.cellNumberToLetter(row, maxPhyCol));
        if (result != nullptr)
            errors.append(QString::fromStdString(utils.cellNumberToLetter(row, maxPhyCol))
                          + ": 枚举或位域，此处建议保持为空，或者填写/或\\");

        result = checkUnusedCell(unit, utils.cellNumberToLetter(row, unitCol));
        if (result != nullptr)
            errors.append(QString::fromStdString(utils.cellNumberToLetter(row, unitCol))
                          + ": 枚举或位域，此处建议保持为空，或者填写/或\\");

        //枚举和位域校验
        int maxRaw = -1;
        if (methodType == compuMethodType::TEXT_TABLE)
            maxRaw = checkValueTable(valueTable, tableCellAddr);

        if (methodType == compuMethodType::BIT_FIELD)
            maxRaw = checkBitField(valueTable, tableCellAddr);

        if (isSignal && maxRaw != -1)
            checkSignalLength(maxRaw, sigLen, row);
    } else if (methodType == compuMethodType::IDENTICAL || methodType == compuMethodType::LINEAR) {
        bool isMinNum, isMaxNum, isFactor, isOffset;
        const double min = minPhy.toDouble(&isMinNum);
        const double max = minPhy.toDouble(&isMaxNum);
        const double f = factor.toDouble(&isFactor);
        const double o = offset.toDouble(&isOffset);
        if (!isMinNum)
            errors.append(QString::fromStdString(utils.cellNumberToLetter(row, minPhyCol))
                          + "Value类型如果没有枚举或位域，物理最小值必须填写数字");
        if (!isMaxNum) {
            errors.append(QString::fromStdString(utils.cellNumberToLetter(row, maxPhyCol))
                          + "Value类型如果没有枚举或位域，物理最大值必须填写数字");
        }
        if (!isFactor) {
            errors.append(QString::fromStdString(utils.cellNumberToLetter(row, maxPhyCol))
                          + "Value类型如果没有枚举或位域，物理最大值必须填写数字");
        }
        if (!isOffset) {
            errors.append(QString::fromStdString(utils.cellNumberToLetter(row, maxPhyCol))
                          + "Value类型如果没有枚举或位域，物理最大值必须填写数字");
        }
        if (isMaxNum && isMinNum && max <= min)
            errors.append(QString::fromStdString(utils.cellNumberToLetter(row, maxPhyCol))
                          + "物理最大值应大于物理最小值>");
        if (isFactor && isOffset && isMaxNum && isSignal) {
            //phy to raw value (phy = raw*factor + offset)
            const double rawMax = (max - o) / f;
            checkSignalLength(rawMax, sigLen, row);
        }
    } else if (methodType == compuMethodType::NOT_SUPPORTED) {
        errors.append(tableCellAddr + "不支持的类型，valueTable中有枚举或位域的定义，但factor!=1 或 offset!=0");
    }
}

int DataTypeConsistencyCheck::checkValueTable(const QString &valueTable, const QString &cellAddress) {
    QList<int> numbers = extractNumbers(valueTable, "0x");
    //检查枚举值是否有重复
    checkDuplicatedNums(numbers, errors, cellAddress);
    //检查枚举值是否连续，比如1，2，3，4不能1，3，4
    checkIncontinuityNums(numbers, errors, cellAddress);

    // 分隔符正则：等号、英文冒号、中文冒号
    QStringList list = valueTable.split('\n', Qt::SkipEmptyParts);
    static const QRegularExpression regex("[=:：]");
    int lineNumber = 0;
    for (const QString &str: list) {
        lineNumber++;
        QString trimmedStr = str.trimmed();

        // 跳过空行
        if (trimmedStr.isEmpty()) continue;

        // 查找分隔符
        QRegularExpressionMatch match = regex.match(trimmedStr);
        const qsizetype separatorPos = match.capturedStart();
        QString enumName = trimmedStr.mid(separatorPos + 1).trimmed();

        //检查是否以0x开头
        if (!trimmedStr.startsWith("0x", Qt::CaseInsensitive)) {
            errors.append(cellAddress + QString(": 第%1行：'%2' 没有以0x开头（枚举值必须是十六进制，以0x开头）")
                          .arg(lineNumber).arg(enumName));
        }

        // 检查是否符合C变量命名规范
        if (!utils.isValidCVariableName(utils.stringFormat(enumName.toStdString()), 16)) {
            errors.append(cellAddress + QString(": 第%1行：'%2' 枚举名称不符合要求（大写字母开头，只能包含字母、数字、下划线，长度不超过16个字符%3）")
                          .arg(lineNumber).arg(
                              enumName, enumName.size() > 16 ? QString(",实际:%1").arg(enumName.size()) : ""));
        }
    }
    return *std::max_element(numbers.begin(), numbers.end());
}

int DataTypeConsistencyCheck::checkBitField(const QString &valueTable, const QString &cellAddress) {
    QList<int> numbers = extractNumbers(valueTable, "Bit");
    //检查位域值是否有重复
    checkDuplicatedNums(numbers, errors, cellAddress);
    //检查位域值是否连续，比如1，2，3，4不能1，3，4
    checkIncontinuityNums(numbers, errors, cellAddress);

    QStringList list = valueTable.split('\n', Qt::SkipEmptyParts);
    int lineNumber = 0;
    for (const QString &str: list) {
        QString trimmedStr = str.trimmed();

        // 跳过空行
        if (trimmedStr.isEmpty()) continue;

        //检查是否以Bit开头
        if (!trimmedStr.startsWith("Bit", Qt::CaseInsensitive)) {
            errors.append(QString(cellAddress + ": 第%1行：'%2' 没有以Bit开头（位域必须以Bit开头，如Bit1:front_right(00=no,01=yes)）")
                .arg(lineNumber).arg(trimmedStr));
        }

        //检查是否包含括号
        if (const qint64 index = trimmedStr.indexOf("("); index == -1) {
            errors.append(QString(cellAddress + ": 第%1行：'%2' 没有找到左括号（位域定义必须有英文括号，如Bit1:front_right(00=no,01=yes)）")
                .arg(lineNumber).arg(trimmedStr));
        } else {
            QString variable = trimmedStr.mid(index + 1);
            variable = variable.left(variable.indexOf(")"));
            if (variable.contains(")"))
                variable = variable.left(variable.indexOf(")"));

            //检查括号内的位域值定义是否为二进制，是否重复，是否连续
            if (variable.contains(",")) variable.replace(",", ",");
            QStringList bitFieldVars = variable.split(',');
            checkBitFieldInside(bitFieldVars, errors, cellAddress);

            //检查冒号或等号后，括号前的变量是否符合 C 变量命名规范
            static const QRegularExpression varNameRegex(R"([=:：]\s*([^\(]+)\s*\()");
            QRegularExpressionMatch match = varNameRegex.match(trimmedStr);
            if (match.hasMatch()) {
                QString varName = match.captured(1).trimmed();
                if (!varName.isEmpty() && !utils.isValidCVariableName(utils.stringFormat(varName.toStdString()), 16)) {
                    errors.append(cellAddress + QString(": 第%1行：'%2' 位域名不符合要求（大写字母开头，只能包含字母、数字、下划线，长度不超过 16 个字符%3）")
                                  .arg(lineNumber).arg(varName, varName.size() > 16 ? QString(",实际:%1").arg(varName.size()) : ""));
                }
            }

            //检查括号内变量是否符合 C 变量命名规范
            QStringList variables = extractBitFieldVariable(bitFieldVars);
            for (const QString &var: variables) {
                if (!utils.isValidCVariableName(utils.stringFormat(var.toStdString()), 16)) {
                    errors.append(cellAddress + QString(": 第%1行：'%2' 括号内定义的名称不符合要求（大写字母开头，只能包含字母、数字、下划线，长度不超过16个字符%3）")
                                  .arg(lineNumber).arg(var, var.size() > 16 ? QString(",实际:%1").arg(var.size()) : ""));
                }
            }
        }

        lineNumber++;
    }
    return *std::max_element(numbers.begin(), numbers.end());
}

void DataTypeConsistencyCheck::checkSignalLength(const double maxValue, const int len, const int row) {
    if (pow(2, len) - 1 < maxValue)
        errors.append(QString::fromStdString(utils.cellNumberToLetter(row, sigLenCol))
                      + ": " + "信号长度太短或无效，最小应该为：" + QString::number(ceil(log2(maxValue + 1))));
}


/*=================================================Struct check===================================================*/
void DataTypeConsistencyCheck::recordConsistencyCheck(const QXlsx::Worksheet *sheet, const int startRow,
                                                      const int endRow) {
    QList<int> numbers;
    for (int row = startRow; row <= endRow; row++) {
        //检查成员名称检查
        const auto memName = utils.readCellValue(sheet->cellAt(row, memNameCol));
        if (!utils.isValidCVariableName(memName.toStdString())) {
            errors.append(QString::fromStdString(utils.cellNumberToLetter(row, memNameCol)) + ": " + memName
                          + " 结构体成员名称不符合要求（大写字母开头，只能包含字母、数字、下划线，长度不超过32个字符）");
        }
        //检查成员序号
        const auto memPosi = utils.readCellValue(sheet->cellAt(row, memPosiCol));
        if (bool ok = memPosi.toInt(&ok); !ok && memPosi.toInt() < 0 && memPosi != "0") {
            errors.append(QString::fromStdString(utils.cellNumberToLetter(row, memPosiCol)) + ": " + memPosi
                          + ": 结构体成员位置必须是正整数");
        } else {
            numbers.append(memPosi.toInt());
        }

        QString memTypeRef = utils.readCellValue(sheet->cellAt(row, memTypeRefCol));
        // 直接在成员所在行定义结构体成员数据类型
        if (checkUnusedCell(memTypeRef, utils.cellNumberToLetter(row, memTypeRefCol)) == nullptr) {
            recordElementTypeDirectDefine(sheet, row, memTypeRef);
            //引用数据类型
        } else {
            recordElementTypeRefDefine(sheet, row, memTypeRef);
        }
    }

    //检查结构体成员位置是否有重复，不连续
    QString msg = utils.readCellValue(sheet->cellAt(startRow, dataTypeNameCol)) + ": 结构体成员";
    checkDuplicatedNums(numbers, errors, msg);
    checkIncontinuityNums(numbers, errors, msg);
}


void DataTypeConsistencyCheck::recordElementTypeDirectDefine(const QXlsx::Worksheet *sheet, const int row,
                                                             const QString &memTypeRef) {
    //无需填写的单元格检查（不适用Struct）
    QString result = checkUnusedCell(memTypeRef, utils.cellNumberToLetter(row, memTypeRefCol));
    if (result != nullptr)
        errors.append(result);

    const auto arrayType = utils.readCellValue(sheet->cellAt(row, lenTypeCol));
    result = checkUnusedCell(arrayType, utils.cellNumberToLetter(row, lenTypeCol));
    if (result != nullptr)
        errors.append(result);

    const auto arrayMinLen = utils.readCellValue(sheet->cellAt(row, minLenCol));
    result = checkUnusedCell(arrayMinLen, utils.cellNumberToLetter(row, minLenCol));
    if (result != nullptr)
        errors.append(result);

    const auto arrayMaxLen = utils.readCellValue(sheet->cellAt(row, maxLenCol));
    result = checkUnusedCell(arrayMaxLen, utils.cellNumberToLetter(row, maxLenCol));
    if (result != nullptr)
        errors.append(result);

    checkMustDefinedCell(sheet, row);
}

void DataTypeConsistencyCheck::recordElementTypeRefDefine(const QXlsx::Worksheet *sheet, const int row,
                                                          const QString &memTypeRef) {
    //检查引用是否有定义
    if (dataTypeNames.find(memTypeRef) == dataTypeNames.end())
        errors.append(QString::fromStdString(utils.cellNumberToLetter(row, memTypeRefCol))
                      + ": " + memTypeRef + " 结构体成员引用数据类型未在表格中定义");

    //剩余皆是不需要定义的
    const auto arrayType = utils.readCellValue(sheet->cellAt(row, lenTypeCol));
    QString result = checkUnusedCell(arrayType, utils.cellNumberToLetter(row, lenTypeCol));
    if (result != nullptr)
        errors.append(result);

    const auto arrayMinLen = utils.readCellValue(sheet->cellAt(row, minLenCol));
    result = checkUnusedCell(arrayMinLen, utils.cellNumberToLetter(row, minLenCol));
    if (result != nullptr)
        errors.append(result);

    const auto arrayMaxLen = utils.readCellValue(sheet->cellAt(row, maxLenCol));
    result = checkUnusedCell(arrayMaxLen, utils.cellNumberToLetter(row, maxLenCol));
    if (result != nullptr)
        errors.append(result);

    unusedCellForBasicType(sheet, row);
}

/*=================================================Array check===================================================*/
void DataTypeConsistencyCheck::arrayConsistencyCheck(const QXlsx::Worksheet *sheet, const int row) {
    //检查数组类型
    if (QString arrayType = utils.readCellValue(sheet->cellAt(row, lenTypeCol)); arrayType != "Fix") {
        errors.append(QString::fromStdString(utils.cellNumberToLetter(row, lenTypeCol))
                      + " 仅支持定长数组，数组类长度型必须为Fixed，实际为：" + arrayType);
    }

    //数组长度最小值，必须是数字
    const auto minnLenCellAddr = utils.cellNumberToLetter(row, minLenCol);
    const QString arrayMinLen = utils.readCellValue(sheet->cellAt(row, minLenCol));
    bool ok;
    int minLen = arrayMinLen.toInt(&ok);
    if (checkUnusedCell(arrayMinLen, minnLenCellAddr) == nullptr || !ok || minLen < 0) {
        errors.append(QString::fromStdString(minnLenCellAddr)
                      + " 定长数组类长度最小值必须为正整数，实际为：" + arrayMinLen);
    }
    //数组长度最大值，必须是数字
    const auto maxLenCellAddr = utils.cellNumberToLetter(row, maxLenCol);
    QString arrayMaxLen = utils.readCellValue(sheet->cellAt(row, maxLenCol));
    ok = false;
    int maxLen = arrayMaxLen.toInt(&ok);
    if (checkUnusedCell(arrayMaxLen, maxLenCellAddr) == nullptr || !ok || maxLen < 0) {
        errors.append(QString::fromStdString(maxLenCellAddr)
                      + " 定长数组类长度最大值必须为正整数，实际为：" + arrayMaxLen);
    }
    if (maxLen != minLen)
        errors.append(QString::fromStdString(maxLenCellAddr)
                      + " 定长数组类长度最小值应与最大值保持一致");

    //检查引用是否有定义
    QString memTypeRef = utils.readCellValue(sheet->cellAt(row, memTypeRefCol));
    // 直接在成员所在行定义结构体成员数据类型
    if (checkUnusedCell(memTypeRef, utils.cellNumberToLetter(row, memTypeRefCol)) == nullptr) {
        //直接定义的数据类型
        arrayElementTypeDirectDefine(sheet, row, memTypeRef);
    } else {
        //引用数据类型
        arrayElementTypeRefDefine(sheet, row, memTypeRef);
    }
}

void DataTypeConsistencyCheck::arrayElementTypeDirectDefine(const QXlsx::Worksheet *sheet, const int row,
                                                            const QString &memTypeRef) {
    //无需填写的单元格检查（不适用Struct）
    QString result = checkUnusedCell(memTypeRef, utils.cellNumberToLetter(row, memTypeRefCol));
    if (result != nullptr)
        errors.append(result);

    const auto memName = utils.readCellValue(sheet->cellAt(row, memNameCol));
    result = checkUnusedCell(memName, utils.cellNumberToLetter(row, memNameCol));
    if (result != nullptr)
        errors.append(result);

    const auto memPosi = utils.readCellValue(sheet->cellAt(row, memPosiCol));
    result = checkUnusedCell(memPosi, utils.cellNumberToLetter(row, memPosiCol));
    if (result != nullptr)
        errors.append(result);

    checkMustDefinedCell(sheet, row);
}


void DataTypeConsistencyCheck::arrayElementTypeRefDefine(const QXlsx::Worksheet *sheet, const int row,
                                                         const QString &memTypeRef) {
    //检查引用是否有定义
    if (dataTypeNames.find(memTypeRef) == dataTypeNames.end())
        errors.append(QString::fromStdString(utils.cellNumberToLetter(row, memTypeRefCol))
                      + ": " + memTypeRef + " 结构体成员引用数据类型未在表格中定义");

    //剩余皆是不需要定义的
    const auto memName = utils.readCellValue(sheet->cellAt(row, memNameCol));
    QString result = checkUnusedCell(memName, utils.cellNumberToLetter(row, memNameCol));
    if (result != nullptr)
        errors.append(result);

    const auto memPosi = utils.readCellValue(sheet->cellAt(row, memPosiCol));
    result = checkUnusedCell(memPosi, utils.cellNumberToLetter(row, memPosiCol));
    if (result != nullptr)
        errors.append(result);

    unusedCellForBasicType(sheet, row);
}

void DataTypeConsistencyCheck::unusedCellForBasicType(const QXlsx::Worksheet *sheet, const int row) {
    const auto baseType = utils.readCellValue(sheet->cellAt(row, baseTypeCol));
    QString result = checkUnusedCell(baseType, utils.cellNumberToLetter(row, baseTypeCol));
    if (result != nullptr)
        errors.append(result);

    const auto sigLen = utils.readCellValue(sheet->cellAt(row, sigLenCol));
    result = checkUnusedCell(sigLen, utils.cellNumberToLetter(row, sigLenCol));
    if (result != nullptr)
        errors.append(result);

    const auto valueTable = utils.readCellValue(sheet->cellAt(row, tableCol));
    result = checkUnusedCell(valueTable, utils.cellNumberToLetter(row, tableCol));
    if (result != nullptr)
        errors.append(result);

    const auto factor = utils.readCellValue(sheet->cellAt(row, factorCol));
    result = checkUnusedCell(factor, utils.cellNumberToLetter(row, factorCol));
    if (result != nullptr)
        errors.append(result);

    const auto offset = utils.readCellValue(sheet->cellAt(row, offsetCol));
    result = checkUnusedCell(offset, utils.cellNumberToLetter(row, offsetCol));
    if (result != nullptr)
        errors.append(result);

    const auto minPhy = utils.readCellValue(sheet->cellAt(row, minPhyCol));
    result = checkUnusedCell(minPhy, utils.cellNumberToLetter(row, minPhyCol));
    if (result != nullptr)
        errors.append(result);

    const auto maxPhy = utils.readCellValue(sheet->cellAt(row, maxPhyCol));
    result = checkUnusedCell(maxPhy, utils.cellNumberToLetter(row, maxPhyCol));
    if (result != nullptr)
        errors.append(result);

    const auto unit = utils.readCellValue(sheet->cellAt(row, unitCol));
    result = checkUnusedCell(unit, utils.cellNumberToLetter(row, unitCol));
    if (result != nullptr)
        errors.append(result);
}
