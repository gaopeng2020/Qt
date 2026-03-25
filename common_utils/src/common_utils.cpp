//
// Created by gaopeng on 2026/3/15.
//

#include "CommonUtils/common_utils.h"
//匿名命名空间，存放私有函数
namespace {
    QString doublePrecisionHandler(const double d, const int precision) {
        if (precision >= 0) {
            return QString::number(d, 'f', precision);
        } else {
            // 自动判断小数位数
            QString str = QString::number(d, 'f', 12);
            // 去除末尾多余的0
            while (str.contains('.') && str.endsWith('0')) {
                str.chop(1);
            }
            if (str.endsWith('.')) str.chop(1);
            return str;
        }
    }
}

std::string common_utils::stringFormat(const std::string &str) {
    if (str.empty()) {
        return "";
    }
    // 正则表达式匹配所有空白字符：\s 包含空格、制表符(\t)、回车(\r)、换行(\n)
    std::regex pattern("\\s");
    return std::regex_replace(str, pattern, "");
}

std::string common_utils::cellNumberToLetter(const int rowNum, int colNum) {
    if (colNum <= 0) return "";

    std::string result;
    while (colNum > 0) {
        // 将列号转换为0-indexed以便计算
        colNum--; // 因为A对应1，但模运算需要0对应A
        const int remainder = colNum % 26;
        result.push_back('A' + remainder);
        colNum /= 26;
    }
    // 上述循环得到的字母顺序是从低位到高位，需要反转
    std::reverse(result.begin(), result.end());
    return result + std::to_string(rowNum);
}

QString common_utils::getLastOpenDir(const QString &newPath) {
    // 存储文件路径：exe 所在目录/conf/lastdir.txt
    const QString exeDir = QCoreApplication::applicationDirPath();
    const QDir confDir(exeDir);
    if (!confDir.exists("conf")) {
        confDir.mkdir("conf"); // 保存模式下，需要确保文件夹存在；读取模式下，如果不存在后续也会判断
    }
    QString filePath = confDir.absoluteFilePath("conf/lastdir.txt");

    //传输参数则执行保存:提取入参中的目录并写入配置文件
    if (!newPath.isEmpty()) {
        const QFileInfo info(newPath);
        QString dirToSave;
        if (info.isFile()) {
            dirToSave = info.absolutePath(); // 文件 → 取所在目录
        } else {
            dirToSave = info.absoluteFilePath(); // 目录 → 取自身绝对路径
        }
        dirToSave = QDir::cleanPath(dirToSave); // 标准化路径

        // 确保目录存在（上面已经创建了）
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << dirToSave;
            file.close();
        }
        return dirToSave;
    } else {
        //不传参数则执行读取：从文件读取上次保存的目录，若文件不存在则返回用户目录
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            QString dir = in.readLine().trimmed();
            file.close();
            if (!dir.isEmpty()) {
                return dir;
            }
        }
        // 文件不存在或内容为空，返回用户目录
        return QDir::homePath();
    }
}

bool common_utils::isNumber(const std::string& str) {
    if (str.empty()) return false;

    // 正则表达式匹配数字（整数、小数、负数、科学计数法）
    const std::regex numberPattern(R"(^[+-]?(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?$)");

    // 去除首尾空格后匹配
    const std::string trimmed = std::regex_replace(str, std::regex("^\\s+|\\s+$"), "");
    return std::regex_match(trimmed, numberPattern);
}

bool common_utils::isValidCVariableName(const std::string &name, const int length) {
    // 1. 空字符串直接返回false
    if (name.empty()) return false;

    // 2. 检查是否为C语言关键字
    if (C_KEYWORDS.count(name)) return false;

    // 3. 检查名称长度
    if (name.length() > static_cast<size_t>(length)) return false;

    // 4. 正则表达式匹配：首字符必须为大写字母，后续为字母、数字或下划线
    const std::regex pattern("^[A-Z][a-zA-Z0-9_]*$");
    return std::regex_match(name, pattern);
}

int common_utils::getColumnByHeader(const QXlsx::Worksheet *sheet, const std::string &headerName) {
    if (!sheet) return -1;

    // 获取工作表的列范围（最大列数）
    int maxColumn = sheet->dimension().columnCount();
    if (maxColumn == 0) return -1;

    // 遍历第一行的所有单元格
    for (int col = 1; col <= maxColumn; ++col) {
        QVariant cellValue = sheet->read(1, col); // 第一行（行号1），第col列
        if (cellValue.toString() == headerName) {
            return col; // 找到匹配，返回列号
        }
    }

    return -1; // 未找到匹配
}

int common_utils::getColumnByHeader(const QXlsx::Worksheet *sheet, const std::string &headerName, const int rowNum) {
    if (!sheet) return -1;

    // 获取工作表的列范围（最大列数）
    int maxColumn = sheet->dimension().columnCount();
    if (maxColumn == 0) return -1;

    // 遍历第一行的所有单元格
    for (int col = 1; col <= maxColumn; ++col) {
        if (QVariant cellValue = sheet->read(rowNum, col); cellValue.toString() == headerName) {
            return col; // 找到匹配，返回列号
        }
    }

    return -1; // 未找到匹配
}

bool common_utils::isCellMerged(const QXlsx::Worksheet *worksheet, const int targetRow, const int targetCol) {
    if (!worksheet) return false;

    // 1. 获取当前工作表的所有合并区域 [citation:2][citation:8]
    QList<QXlsx::CellRange> mergedRanges = worksheet->mergedCells();

    // 2. 遍历所有合并区域，检查目标单元格是否在任何一个区域内
    foreach(const QXlsx::CellRange &range, mergedRanges) {
        // 获取当前合并区域的边界 (已验证存在的方法)
        const int firstRow = range.firstRow();
        const int lastRow = range.lastRow();
        const int firstCol = range.firstColumn();

        // 3. 手动判断目标单元格是否在此区域内
        if (const int lastCol = range.lastColumn(); targetRow >= firstRow && targetRow <= lastRow &&
                                                    targetCol >= firstCol && targetCol <= lastCol) {
            return true; // 目标单元格在此合并区域内
        }
    }
    return false; // 不在任何合并区域内
}

int common_utils::getLastRowNum(const QXlsx::Worksheet *worksheet, const int targetRow, const int targetCol) {
    if (!worksheet) return targetRow; // 无效工作表，返回自身

    // 1. 检查是否在合并区域内
    QList<QXlsx::CellRange> mergedRanges = worksheet->mergedCells();
    foreach(const QXlsx::CellRange &range, mergedRanges) {
        if (targetRow >= range.firstRow() && targetRow <= range.lastRow() &&
            targetCol >= range.firstColumn() && targetCol <= range.lastColumn()) {
            // 在合并区域内，返回合并区域的最后一行
            return range.lastRow();
        }
    }

    // 2. 获取目标单元格的值
    const QVariant targetValue = worksheet->read(targetRow, targetCol);
    // 如果目标单元格为空，没有连续相同值区域，返回自身
    if (targetValue.isNull()) {
        return targetRow;
    }

    // 3. 向下查找连续相同值的最后一行
    int lastRow = targetRow;
    int currentRow = targetRow;
    constexpr int MAX_ROW = 1048576; // Excel 最大行号
    while (currentRow < MAX_ROW) {
        int nextRow = currentRow + 1;
        QVariant nextValue = worksheet->read(nextRow, targetCol);
        // 如果下一行为空，停止
        if (nextValue.isNull()) {
            break;
        }
        // 如果值相同，继续
        if (targetValue == nextValue) {
            lastRow = nextRow;
            currentRow = nextRow;
        } else {
            break;
        }
    }
    return lastRow;
}

// QString common_utils::readCellValue(const QXlsx::Worksheet *sheet, const int row, const int col, const int precision) {
QString common_utils::readCellValue(const std::shared_ptr<QXlsx::Cell>& cell, const int precision) {
    // std::shared_ptr<QXlsx::Cell> cell = sheet->cellAt(row, col);
    if (!cell) return {};

    const QVariant value = cell->value();
    // 日期处理
    if (cell->isDateTime()) {
        if (value.typeId() == QMetaType::Double || value.typeId() == QMetaType::Int) {
            double serialDate = value.toDouble();
            if (serialDate > 40000) {
                const QDate epoch(1899, 12, 30);
                int days = static_cast<int>(serialDate);
                if (days >= 60) days -= 1;
                const QDate date = epoch.addDays(days);
                return date.toString("yyyy-MM-dd");
            }
        }
        return cell->dateTime().toDateTime().toString("yyyy-MM-dd");
    }

    // 浮点数处理
    if (value.typeId() == QMetaType::Double) {
        const double d = value.toDouble();
        return doublePrecisionHandler(d, precision);
    }

    // 整数
    if (value.typeId() == QMetaType::Int) {
        return QString::number(value.toInt());
    }

    // 获取字符串形式的值
    QString str = value.toString();
    // 检查是否是数字字符串（如 "2.2"）
    bool ok;
    const double d = str.toDouble(&ok);
    if (ok) {
        // 如果是整数，返回整数形式
        if (d == static_cast<int>(d)) {
            return QString::number(static_cast<int>(d));
        }
        // 如果是浮点数，格式化输出
        return doublePrecisionHandler(d, precision);
    }
    return str;;
}
