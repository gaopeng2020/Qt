//
// Created by gaopeng on 2026/3/22.
//

#include "DataTypeConsistencyCheck.h"
#include <common-utils/core.h>
#include <common-utils/xlsx.h>
#include <glog/logging.h>
#include <regex>
#include <sstream>
#include <cmath>
#include <chrono>
#include <fstream>
#include <iomanip>

using namespace common_utils;

namespace{
    enum class compuMethodType{
        TEXT_TABLE,
        BIT_FIELD,
        LINEAR,
        IDENTICAL,
        NOT_SUPPORTED,
        ERROR
    };

    std::string checkUnusedCell(std::string value, const std::string& cellAddress) {
        // value = value.trimmed();
        value = Core::stringTrim(value);
        if (value.empty() || value == "/" || value == "\\") {
            return "";
        } else {
            return cellAddress + " Value 类型此处应为空，或用\\,/填充；实际为:" + value;
        }
    }

    bool tryParseDouble(const std::string& str, double& result) {
        try {
            size_t pos;
            result = std::stod(str, &pos);
            return pos == str.length();
        } catch (...) {
            return false;
        }
    }

    compuMethodType compuMethodType(const std::string& valueTable, const std::string& factor,
                                    const std::string& offset) {
        // 去除空格
        std::string vt = Core::stringTrim(valueTable);
        std::string f = Core::stringTrim(factor);
        std::string o = Core::stringTrim(offset);

        // 静态正则表达式
        static const std::regex textTablePattern("^0x[0-9a-f]{1,2}[=:：]\\s*",
                                                 std::regex_constants::icase);
        static const std::regex bitFieldPattern("^bit[0-9]{1,2}[=:：]\\s*",
                                                std::regex_constants::icase);

        const bool vtIsEmpty = vt.empty();
        const bool isTextTableFormat = std::regex_search(vt, textTablePattern);
        const bool isBitFieldFormat = std::regex_search(vt, bitFieldPattern);

        // 解析 factor 和 offset
        double factorVal, offsetVal;
        const bool factorOk = tryParseDouble(f, factorVal);
        const bool offsetOk = tryParseDouble(o, offsetVal);
        const bool factorIsOne = factorOk && std::abs(factorVal - 1.0) < 1e-12;
        const bool offsetIsZero = offsetOk && std::abs(offsetVal) < 1e-12;

        // 规则 5: valueTable 满足 1 或 2，但 factor!=1 || offset!=0 但必须是数字 → NOT_SUPPORTED
        if ((isTextTableFormat || isBitFieldFormat) && factorOk && offsetOk && (!factorIsOne || !offsetIsZero)) {
            return compuMethodType::NOT_SUPPORTED;
        }

        // 规则 1: valueTable 以 0x 开头，紧跟=或: → TEXT_TABLE
        if (isTextTableFormat) {
            return compuMethodType::TEXT_TABLE;
        }

        // 规则 2: valueTable 以 Bit 开头，紧跟=或: → BIT_FIELD
        if (isBitFieldFormat) {
            return compuMethodType::BIT_FIELD;
        }

        // 如果 factor 或 offset 不是有效数字，后面的规则无法判断，返回 ERROR
        if (!factorOk || !offsetOk) {
            return compuMethodType::ERROR;
        }

        // 规则 3: valueTable 为空 & factor=1 & offset=0 → IDENTICAL
        if (vtIsEmpty && factorIsOne && offsetIsZero) {
            return compuMethodType::IDENTICAL;
        }

        // 规则 4: valueTable 为空但 (factor!=1 || offset!=0) → LINEAR
        if (vtIsEmpty && (!factorIsOne || !offsetIsZero)) {
            return compuMethodType::LINEAR;
        }

        // 默认情况返回 ERROR
        return compuMethodType::ERROR;
    }

    std::list<int> extractNumbers(const std::string& input, const std::string& prefix) {
        std::list<int> numbers;
        // 构建动态正则表达式，支持自定义前缀（如 "0x" 或 "Bit"）
        // 匹配模式：前缀 + 十六进制/十进制数字 (可选范围 -前缀 + 数字)
        // 注意：Bit 后面通常是十进制，0x 后面是十六进制，这里根据前缀自动选择进制
        const bool isHex = (prefix == "0x" || prefix == "0X");
        std::string numPattern = isHex ? "([0-9A-Fa-f]+)" : "([0-9]+)";
        const std::string patternStr = prefix + numPattern + "(?:" + prefix + numPattern + ")?";
        std::regex regex(patternStr, std::regex_constants::icase);

        auto begin = std::sregex_iterator(input.begin(), input.end(), regex);
        auto end = std::sregex_iterator();

        const int base = isHex ? 16 : 10;
        for (auto it = begin; it != end; ++it) {
            std::smatch match = *it;

            // captured(1) 是第一个数字部分
            const int first = std::stoi(match.str(1), nullptr, base);

            // captured(2) 是范围结束的数字部分（如果有）
            if (match[2].matched) {
                const int second = std::stoi(match.str(2), nullptr, base);
                // 添加从 first 到 second 的所有数字（不去重）
                for (int i = first; i <= second; ++i) {
                    numbers.push_back(i);
                }
            } else {
                numbers.push_back(first);
            }
        }

        // 只排序，不去重
        numbers.sort();
        return numbers;
    }

    void checkDuplicatedNums(const std::list<int>& numbers, std::list<std::string>& errors,
                             const std::string& cellAddress) {
        std::set<int> seen;
        for (int value : numbers) {
            if (seen.contains(value)) {
                errors.emplace_back(cellAddress + ": 发现重复的值：" + std::to_string(value));
            }
            seen.insert(value);
        }
    }

    void checkIncontinuityNums(const std::list<int>& numbers, std::list<std::string>& errors,
                               const std::string& cellAddress) {
        if (numbers.empty()) return;
        int expected = numbers.front();
        for (int value : numbers) {
            if (value != expected) {
                errors.emplace_back(
                    cellAddress + ": 发现不连续的值：" + std::to_string(value) + "，期望值为：" + std::to_string(expected));
            }
            expected++;
        }
    }

    std::list<std::string> extractBitFieldVariable(const std::list<std::string>& bitFieldVars) {
        std::list<std::string> result;
        // 最简单的匹配：分隔符后面跟着非分隔符字符
        static const std::regex regex("[=:：][^=:：]+");
        for (const auto& line : bitFieldVars) {
            auto begin = std::sregex_iterator(line.begin(), line.end(), regex);
            auto end = std::sregex_iterator();
            for (auto it = begin; it != end; ++it) {
                std::string captured = it->str();
                // 去掉开头的分隔符
                if (!captured.empty() && (captured[0] == '=' || captured[0] == ':' || captured[0] == ':')) {
                    captured = captured.substr(1);
                }
                captured = Core::stringTrim(captured);
                if (!captured.empty()) {
                    result.push_back(captured);
                }
            }
        }

        return result;
    }

    void checkBitFieldInside(const std::list<std::string>& bitFieldVars, std::list<std::string>& errors,
                             const std::string& cellAddress) {
        // 检查括号内的位域值定义 (例如：00=no, 01=yes) 是否为二进制
        std::list<int> list;
        for (const auto& varDef : bitFieldVars) {
            std::string trimmed = Core::stringTrim(varDef);
            if (trimmed.empty()) continue;

            // 使用正则提取开头的二进制部分，支持 1~3 位二进制
            static const std::regex binRegex("^([01]{1,3})\\s*[=:：]\\s*(.+)$");
            std::smatch match;

            if (!std::regex_match(trimmed, match, binRegex)) {
                errors.emplace_back(cellAddress + ": 位域值定义格式错误：'" + trimmed + "'（应为：二进制值=变量名，如 00=no）");
            } else {
                try {
                    int value = std::stoi(match.str(1), nullptr, 2); // 使用二进制转换
                    list.push_back(value); // 对于临时变量，使用 push_back 代替 emplace，否则 emplace 可能会构造成 0
                } catch (...) {
                    errors.emplace_back(cellAddress + ": 二进制值转换失败：'" + match.str(1) + "'");
                }
            }
        }
        // 检查位域值是否重复
        list.sort();
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
      tableCol(-1)/*, textEdit(nullptr)*/ {
}

DataTypeConsistencyCheck::DataTypeConsistencyCheck(std::string& filePath)
    : dataTypeNameCol(-1), categoryCol(-1),
      memPosiCol(-1), memNameCol(-1), memTypeRefCol(-1),
      lenTypeCol(-1), minLenCol(-1), maxLenCol(-1),
      baseTypeCol(-1), sigLenCol(-1), factorCol(-1), offsetCol(-1),
      minPhyCol(-1), maxPhyCol(-1), unitCol(-1), tableCol(-1),
      filePath(std::move(filePath))/*, textEdit(textEdit)*/ {
    readDataType();
}

void DataTypeConsistencyCheck::readDataType() {
    errors.clear();
    OpenXLSX::XLDocument doc;
    if (!Xlsx::openXlDocument(doc, filePath)) return;

    auto sheet = doc.workbook().worksheet(5);
    auto sheetName = "Active Worksheet : " + sheet.name();
    LOG(INFO) << sheetName;
    errors.emplace_back(sheetName);

    std::string configPath = Core::getExeDirectory() + "/conf/header_config.ini";
    LOG(INFO) << "config Path is: " << configPath;

    auto settings = Core::parseIniFile(configPath);
    if (settings.empty()) {
        LOG(ERROR) << "Configuration file not found or empty:" << configPath;
        return;
    }

    dataTypeNameCol = Xlsx::getColumnByHeader(sheet, settings["AdtColumnHeaders.DataTypeName"]);
    categoryCol = Xlsx::getColumnByHeader(sheet, settings["AdtColumnHeaders.DataTypeCategory"]);

    memPosiCol = Xlsx::getColumnByHeader(sheet, settings["AdtColumnHeaders.MemberPosition"]);
    memNameCol = Xlsx::getColumnByHeader(sheet, settings["AdtColumnHeaders.MemberName"]);
    memTypeRefCol = Xlsx::getColumnByHeader(sheet, settings["AdtColumnHeaders.MemberDataTypeReference"]);

    lenTypeCol = Xlsx::getColumnByHeader(sheet, settings["AdtColumnHeaders.StringLengthType"]);
    minLenCol = Xlsx::getColumnByHeader(sheet, settings["AdtColumnHeaders.StringLengthMin"]);
    maxLenCol = Xlsx::getColumnByHeader(sheet, settings["AdtColumnHeaders.StringLengthMax"]);

    baseTypeCol = Xlsx::getColumnByHeader(sheet, settings["AdtColumnHeaders.BasicDataType"]);
    sigLenCol = Xlsx::getColumnByHeader(sheet, settings["AdtColumnHeaders.SignalLength"]);
    factorCol = Xlsx::getColumnByHeader(sheet, settings["AdtColumnHeaders.Resolution"]);
    offsetCol = Xlsx::getColumnByHeader(sheet, settings["AdtColumnHeaders.Offset"]);
    minPhyCol = Xlsx::getColumnByHeader(sheet, settings["AdtColumnHeaders.MinPhysicalValue"]);
    maxPhyCol = Xlsx::getColumnByHeader(sheet, settings["AdtColumnHeaders.MaxPhysicalValue"]);
    unitCol = Xlsx::getColumnByHeader(sheet, settings["AdtColumnHeaders.Unit"]);
    tableCol = Xlsx::getColumnByHeader(sheet, settings["AdtColumnHeaders.TableValue"]);

    //检查输入的表格是否有效
    if (dataTypeNameCol == -1 || categoryCol == -1
        || memPosiCol == -1 || memNameCol == -1 || memTypeRefCol == -1
        || lenTypeCol == -1 || minLenCol == -1 || maxLenCol == -1
        || baseTypeCol == -1 || sigLenCol == -1 || factorCol == -1 || offsetCol == -1
        || minPhyCol == -1 || maxPhyCol == -1 || unitCol == -1 || tableCol == -1) {
        const auto msg = "输入文件无效，请检查输入的 Excel 是否为 Application Data Type 表格，且不能修改模板的表头，自定义的 ADT 必须放在第一个 sheet 中";
        LOG(ERROR) << msg;
        errors.emplace_back(msg);
        // textEdit->append(msg);
        return;
    }
    //提示开始检查
    const auto msg = filePath + " 正在执行检查，请等待...";
    LOG(INFO) << msg;

    //获取所有的 data name
    for (int row = 2; row <= sheet.rowCount(); row++) {
        auto name = Xlsx::getCellValue(sheet.cell(row, dataTypeNameCol));
        if (name.empty()) continue;
        dataTypeNames.emplace(name);
    }
    checkDataType(sheet);
}

void DataTypeConsistencyCheck::checkDataType(const OpenXLSX::XLWorksheet& sheet) {
    int row = 2;
    while (row <= sheet.rowCount()) {
        const int endRow = Xlsx::getLastRowNum(const_cast<OpenXLSX::XLWorksheet&>(sheet), row, dataTypeNameCol);

        auto name = Xlsx::getCellValue(sheet.cell(row, dataTypeNameCol));
        auto category = Xlsx::getCellValue(sheet.cell(row, categoryCol));

        //data type name check
        if (!Core::isValidCVariableName(name))
            errors.emplace_back(Core::numToCellAddress(row, dataTypeNameCol)
                + ": " + name + ": 数据类型名称不符合要求（大写字母开头，只能包含字母、数字、下划线，长度不超过 32 个字符）");
        if (category == "Value") {
            valueConsistencyCheck(sheet, row);
        } else if (category == "Struct") {
            recordConsistencyCheck(sheet, row, endRow);
        } else if (category == "Array") {
            arrayConsistencyCheck(sheet, row);
        } else {
        }
        row = endRow + 1;
    }
}

/*=================================================Value check===================================================*/

void DataTypeConsistencyCheck::valueConsistencyCheck(const OpenXLSX::XLWorksheet& sheet, const int row) {
    //无需填写的单元格检查（不适用 Value）
    const auto memPosition = Xlsx::getCellValue(sheet.cell(row, memPosiCol));
    std::string result = checkUnusedCell(memPosition, Core::numToCellAddress(row, memPosiCol));
    if (result.empty()) errors.emplace_back(result);

    const auto memName = Xlsx::getCellValue(sheet.cell(row, memNameCol));
    result = checkUnusedCell(memName, Core::numToCellAddress(row, memNameCol));
    if (result.empty()) errors.emplace_back(result);

    const auto memType = Xlsx::getCellValue(sheet.cell(row, memTypeRefCol));
    result = checkUnusedCell(memType, Core::numToCellAddress(row, memTypeRefCol));
    if (result.empty()) errors.emplace_back(result);

    const auto arrayType = Xlsx::getCellValue(sheet.cell(row, lenTypeCol));
    result = checkUnusedCell(arrayType, Core::numToCellAddress(row, lenTypeCol));
    if (result.empty()) errors.emplace_back(result);

    const auto arrayMinLen = Xlsx::getCellValue(sheet.cell(row, minLenCol));
    result = checkUnusedCell(arrayMinLen, Core::numToCellAddress(row, minLenCol));
    if (result.empty()) errors.emplace_back(result);

    const auto arrayMaxLen = Xlsx::getCellValue(sheet.cell(row, maxLenCol));
    result = checkUnusedCell(arrayMaxLen, Core::numToCellAddress(row, maxLenCol));
    if (result.empty()) errors.emplace_back(result);
    //必须填写的单元格检查
    checkMustDefinedCell(sheet, row);
}

void DataTypeConsistencyCheck::checkMustDefinedCell(const OpenXLSX::XLWorksheet& sheet, const int row) {
    if (const auto baseType = Xlsx::getCellValue(sheet.cell(row, baseTypeCol));
        !BASIC_TYPES.contains(baseType))
        errors.emplace_back(Core::numToCellAddress(row, baseTypeCol) + "基础数据类型不在支持的范围内，"
            "仅支持{bool,uint8,uint16,uint32,uint64,sint8," "sint16,sint32,sint64,float32,float64,\\}");

    bool isSignal;
    const std::string sigLenStr = Xlsx::getCellValue(sheet.cell(row, sigLenCol));
    int sigLen = 0;
    try {
        sigLen = std::stoi(sigLenStr, nullptr, 10);
        isSignal = true;
    } catch (...) {
        isSignal = false;
    }

    if (!isSignal)
        errors.emplace_back(
            Core::numToCellAddress(row, sigLenCol) + " Value 类型的 ADT，信号长度必须填写");

    const auto valueTable = Xlsx::getCellValue(sheet.cell(row, tableCol));
    const auto factor = Xlsx::getCellValue(sheet.cell(row, factorCol));
    const auto offset = Xlsx::getCellValue(sheet.cell(row, offsetCol));

    const auto minPhy = Xlsx::getCellValue(sheet.cell(row, minPhyCol));
    const auto maxPhy = Xlsx::getCellValue(sheet.cell(row, maxPhyCol));
    const auto unit = Xlsx::getCellValue(sheet.cell(row, unitCol));

    const auto tableCellAddr = Core::numToCellAddress(row, tableCol);

    if (const auto methodType = compuMethodType(valueTable, factor, offset);
        methodType == compuMethodType::TEXT_TABLE || methodType == compuMethodType::BIT_FIELD) {
        std::string result = checkUnusedCell(factor, Core::numToCellAddress(row, factorCol));
        if (result.empty())
            errors.emplace_back(Core::numToCellAddress(row, factorCol)
                + ": 枚举或位域，此处建议保持为空，或者填写/或\\");

        result = checkUnusedCell(offset, Core::numToCellAddress(row, offsetCol));
        if (result.empty())
            errors.emplace_back(Core::numToCellAddress(row, offsetCol)
                + ": 枚举或位域，此处建议保持为空，或者填写/或\\");

        result = checkUnusedCell(minPhy, Core::numToCellAddress(row, minPhyCol));
        if (result.empty())
            errors.emplace_back(Core::numToCellAddress(row, minPhyCol)
                + ": 枚举或位域，此处建议保持为空，或者填写/或\\");

        result = checkUnusedCell(maxPhy, Core::numToCellAddress(row, maxPhyCol));
        if (result.empty())
            errors.emplace_back(Core::numToCellAddress(row, maxPhyCol)
                + ": 枚举或位域，此处建议保持为空，或者填写/或\\");

        result = checkUnusedCell(unit, Core::numToCellAddress(row, unitCol));
        if (result.empty())
            errors.emplace_back(Core::numToCellAddress(row, unitCol)
                + ": 枚举或位域，此处建议保持为空，或者填写/或\\");

        //枚举和位域校验
        int maxRaw = -1;
        if (methodType == compuMethodType::TEXT_TABLE) maxRaw = checkValueTable(valueTable, tableCellAddr);

        if (methodType == compuMethodType::BIT_FIELD) maxRaw = checkBitField(valueTable, tableCellAddr);

        if (isSignal && maxRaw != -1) checkSignalLength(maxRaw, sigLen, row);
    } else if (methodType == compuMethodType::IDENTICAL || methodType == compuMethodType::LINEAR) {
        bool isMinNum, isMaxNum, isFactor, isOffset;
        double min = 0, max = 0, f = 1, o = 0;

        try {
            min = std::stod(minPhy);
            isMinNum = true;
        } catch (...) { isMinNum = false; }

        try {
            max = std::stod(maxPhy);
            isMaxNum = true;
        } catch (...) { isMaxNum = false; }

        try {
            f = std::stod(factor);
            isFactor = true;
        } catch (...) { isFactor = false; }

        try {
            o = std::stod(offset);
            isOffset = true;
        } catch (...) { isOffset = false; }

        if (!isMinNum)
            errors.emplace_back(Core::numToCellAddress(row, minPhyCol)
                + "Value 类型如果没有枚举或位域，物理最小值必须填写数字");
        if (!isMaxNum) {
            errors.emplace_back(Core::numToCellAddress(row, maxPhyCol)
                + "Value 类型如果没有枚举或位域，物理最大值必须填写数字");
        }
        if (!isFactor) {
            errors.emplace_back(Core::numToCellAddress(row, factorCol)
                + "Value 类型如果没有枚举或位域，resolution 必须填写数字");
        }
        if (!isOffset) {
            errors.emplace_back(Core::numToCellAddress(row, offsetCol)
                + "Value 类型如果没有枚举或位域，offset 必须填写数字");
        }
        if (isMaxNum && isMinNum && max <= min)
            errors.emplace_back(Core::numToCellAddress(row, maxPhyCol)
                + "物理最大值应大于物理最小值>");
        if (isFactor && isOffset && isMaxNum && isSignal) {
            //phy to raw value (phy = raw*factor + offset)
            const double rawMax = (max - o) / f;
            checkSignalLength(rawMax, sigLen, row);
        }
    } else if (methodType == compuMethodType::NOT_SUPPORTED) {
        errors.emplace_back(tableCellAddr + "不支持的类型，valueTable 中有枚举或位域的定义，但 factor!=1 或 offset!=0");
    }
}

int DataTypeConsistencyCheck::checkValueTable(const std::string& valueTable, const std::string& cellAddress) {
    std::list<int> numbers = extractNumbers(valueTable, "0x");
    //检查枚举值是否有重复
    checkDuplicatedNums(numbers, errors, cellAddress);
    //检查枚举值是否连续，比如 1，2，3，4 不能 1，3，4
    checkIncontinuityNums(numbers, errors, cellAddress);

    // 分隔符正则：等号、英文冒号、中文冒号
    std::list<std::string> list = Core::splitString(valueTable, '\n');
    static const std::regex separatorRegex("[=:：]");
    int lineNumber = 0;
    for (const auto& str : list) {
        lineNumber++;
        std::string trimmedStr = Core::stringTrim(str);

        // 跳过空行
        if (trimmedStr.empty()) continue;

        // 查找分隔符
        std::smatch match;
        if (std::regex_search(trimmedStr, match, separatorRegex)) {
            const size_t separatorPos = match.position();
            std::string enumName = Core::stringTrim(trimmedStr.substr(separatorPos + 1));

            //检查是否以 0x 开头
            if (trimmedStr.size() < 2 || !(trimmedStr[0] == '0' && (trimmedStr[1] == 'x' || trimmedStr[1] == 'X'))) {
                errors.emplace_back(
                    cellAddress + ": 第" + std::to_string(lineNumber) + "行：'" + enumName +
                    "' 没有以 0x 开头（枚举值必须是十六进制，以 0x 开头）");
            }

            // 检查是否符合 C 变量命名规范
            if (!Core::isValidCVariableName(Core::stringFormat(enumName), 16)) {
                std::string errorMsg = cellAddress + ": 第" + std::to_string(lineNumber) + "行：'" + enumName +
                    "' 枚举名称不符合要求（大写字母开头，只能包含字母、数字、下划线，长度不超过 16 个字符";
                if (enumName.size() > 16) {
                    errorMsg += ",实际:" + std::to_string(enumName.size());
                }
                errorMsg += ")";
                errors.emplace_back(errorMsg);
            }
        }
    }
    return numbers.empty() ? -1 : numbers.back();
}

int DataTypeConsistencyCheck::checkBitField(const std::string& valueTable, const std::string& cellAddress) {
    std::list<int> numbers = extractNumbers(valueTable, "Bit");
    //检查位域值是否有重复
    checkDuplicatedNums(numbers, errors, cellAddress);
    //检查位域值是否连续，比如 1，2，3，4 不能 1，3，4
    checkIncontinuityNums(numbers, errors, cellAddress);

    std::list<std::string> list = Core::splitString(valueTable, '\n');
    int lineNumber = 0;
    for (const auto& str : list) {
        std::string trimmedStr = Core::stringTrim(str);

        // 跳过空行
        if (trimmedStr.empty()) continue;

        //检查是否以 Bit 开头
        if (trimmedStr.size() < 3 || !(trimmedStr[0] == 'B' || trimmedStr[0] == 'b')
            || !(trimmedStr[1] == 'I' || trimmedStr[1] == 'i')
            || !(trimmedStr[2] == 'T' || trimmedStr[2] == 't')) {
            errors.emplace_back(
                cellAddress + ": 第" + std::to_string(lineNumber) + "行：'" + trimmedStr +
                "' 没有以 Bit 开头（位域必须以 Bit 开头，如 Bit1:front_right(00=no,01=yes)）");
        }

        //检查是否包含括号
        const size_t index = trimmedStr.find("(");
        if (index == std::string::npos) {
            errors.emplace_back(
                cellAddress + ": 第" + std::to_string(lineNumber) + "行：'" + trimmedStr +
                "' 没有找到左括号（位域定义必须有英文括号，如 Bit1:front_right(00=no,01=yes)）");
        } else {
            std::string variable = trimmedStr.substr(index + 1);
            size_t closeParenPos = variable.find(")");
            if (closeParenPos != std::string::npos) {
                variable = variable.substr(0, closeParenPos);
            }
            if (variable.find(")") != std::string::npos) {
                variable = variable.substr(0, variable.find(")"));
            }

            //检查括号内位域值定义是否为二进制，是否重复，是否连续
            if (variable.find(",") != std::string::npos) {
                // 简单替换逗号后的空格
                size_t pos = 0;
                while ((pos = variable.find(", ", pos)) != std::string::npos) {
                    variable.replace(pos, 2, ",");
                    pos += 1;
                }
            }
            std::list<std::string> bitFieldVars = Core::splitString(variable, ',');
            checkBitFieldInside(bitFieldVars, errors, cellAddress);

            //检查冒号或等号后，括号前的变量是否符合 C 变量命名规范
            static const std::regex varNameRegex(R"([=:：]\s*([^\(]+)\s*\()");
            std::smatch match;
            if (std::regex_search(trimmedStr, match, varNameRegex)) {
                std::string varName = Core::stringTrim(match.str(1));
                if (!varName.empty() && !Core::isValidCVariableName(Core::stringFormat(varName), 16)) {
                    std::string errorMsg = cellAddress + ": 第" + std::to_string(lineNumber) + "行：'" + varName +
                        "' 位域名不符合要求（大写字母开头，只能包含字母、数字、下划线，长度不超过 16 个字符";
                    if (varName.size() > 16) {
                        errorMsg += ",实际:" + std::to_string(varName.size());
                    }
                    errorMsg += ")";
                    errors.emplace_back(errorMsg);
                }
            }

            //检查括号内变量是否符合 C 变量命名规范
            std::list<std::string> variables = extractBitFieldVariable(bitFieldVars);
            for (const auto& var : variables) {
                if (!Core::isValidCVariableName(Core::stringFormat(var), 16)) {
                    std::string errorMsg = cellAddress + ": 第" + std::to_string(lineNumber) + "行：'" + var +
                        "' 括号内定义的名称不符合要求（大写字母开头，只能包含字母、数字、下划线，长度不超过 16 个字符";
                    if (var.size() > 16) {
                        errorMsg += ",实际:" + std::to_string(var.size());
                    }
                    errorMsg += ")";
                    errors.emplace_back(errorMsg);
                }
            }
        }

        lineNumber++;
    }
    return numbers.empty() ? -1 : numbers.back();
}

void DataTypeConsistencyCheck::checkSignalLength(const double maxValue, const int len, const int row) {
    if (pow(2, len) - 1 < maxValue)
        errors.emplace_back(Core::numToCellAddress(row, sigLenCol)
            + ": " + "信号长度太短或无效，最小应该为：" + std::to_string(static_cast<int>(ceil(log2(maxValue + 1)))));
}


/*=================================================Struct check===================================================*/
void DataTypeConsistencyCheck::recordConsistencyCheck(const OpenXLSX::XLWorksheet& sheet, const int startRow,
                                                      const int endRow) {
    std::list<int> numbers;
    for (int row = startRow; row <= endRow; row++) {
        //检查成员名称检查
        const auto memName = Xlsx::getCellValue(sheet.cell(row, memNameCol));
        if (!Core::isValidCVariableName(memName)) {
            errors.emplace_back(Core::numToCellAddress(row, memNameCol) + ": " + memName
                + " 结构体成员名称不符合要求（大写字母开头，只能包含字母、数字、下划线，长度不超过 32 个字符）");
        }
        //检查成员序号
        const auto memPosi = Xlsx::getCellValue(sheet.cell(row, memPosiCol));
        bool ok;
        int posiValue = 0;
        try {
            posiValue = std::stoi(memPosi);
            ok = true;
        } catch (...) {
            ok = false;
        }

        if (!ok || posiValue < 0) {
            errors.emplace_back(Core::numToCellAddress(row, memPosiCol) + ": " + memPosi
                + ": 结构体成员位置必须是正整数");
        } else {
            numbers.push_back(posiValue);
        }

        std::string memTypeRef = Xlsx::getCellValue(sheet.cell(row, memTypeRefCol));
        // 直接在成员所在行定义结构体成员数据类型
        if (checkUnusedCell(memTypeRef, Core::numToCellAddress(row, memTypeRefCol)).empty()) {
            recordElementTypeDirectDefine(sheet, row, memTypeRef);
            //引用数据类型
        } else {
            recordElementTypeRefDefine(sheet, row, memTypeRef);
        }
    }

    //检查结构体成员位置是否有重复，不连续
    std::string msg = Xlsx::getCellValue(sheet.cell(startRow, dataTypeNameCol)) + ": 结构体成员";
    checkDuplicatedNums(numbers, errors, msg);
    checkIncontinuityNums(numbers, errors, msg);
}


void DataTypeConsistencyCheck::recordElementTypeDirectDefine(const OpenXLSX::XLWorksheet& sheet, const int row,
                                                             const std::string& memTypeRef) {
    //无需填写的单元格检查（不适用 Struct）
    std::string result = checkUnusedCell(memTypeRef, Core::numToCellAddress(row, memTypeRefCol));
    if (result.empty()) errors.emplace_back(result);

    const auto arrayType = Xlsx::getCellValue(sheet.cell(row, lenTypeCol));
    result = checkUnusedCell(arrayType, Core::numToCellAddress(row, lenTypeCol));
    if (result.empty()) errors.emplace_back(result);

    const auto arrayMinLen = Xlsx::getCellValue(sheet.cell(row, minLenCol));
    result = checkUnusedCell(arrayMinLen, Core::numToCellAddress(row, minLenCol));
    if (result.empty()) errors.emplace_back(result);

    const auto arrayMaxLen = Xlsx::getCellValue(sheet.cell(row, maxLenCol));
    result = checkUnusedCell(arrayMaxLen, Core::numToCellAddress(row, maxLenCol));
    if (result.empty()) errors.emplace_back(result);

    checkMustDefinedCell(sheet, row);
}

void DataTypeConsistencyCheck::recordElementTypeRefDefine(const OpenXLSX::XLWorksheet& sheet, const int row,
                                                          const std::string& memTypeRef) {
    //检查引用是否有定义
    if (!dataTypeNames.contains(memTypeRef))
        errors.emplace_back(Core::numToCellAddress(row, memTypeRefCol)
            + ": " + memTypeRef + " 结构体成员引用数据类型未在表格中定义");

    //剩余皆是不需要定义的
    const auto arrayType = Xlsx::getCellValue(sheet.cell(row, lenTypeCol));
    std::string result = checkUnusedCell(arrayType, Core::numToCellAddress(row, lenTypeCol));
    if (result.empty()) errors.emplace_back(result);

    const auto arrayMinLen = Xlsx::getCellValue(sheet.cell(row, minLenCol));
    result = checkUnusedCell(arrayMinLen, Core::numToCellAddress(row, minLenCol));
    if (result.empty()) errors.emplace_back(result);

    const auto arrayMaxLen = Xlsx::getCellValue(sheet.cell(row, maxLenCol));
    result = checkUnusedCell(arrayMaxLen, Core::numToCellAddress(row, maxLenCol));
    if (result.empty()) errors.emplace_back(result);

    unusedCellForBasicType(sheet, row);
}

/*=================================================Array check===================================================*/
void DataTypeConsistencyCheck::arrayConsistencyCheck(const OpenXLSX::XLWorksheet& sheet, const int row) {
    //检查数组类型
    if (std::string arrayType = Xlsx::getCellValue(sheet.cell(row, lenTypeCol)); arrayType != "Fix") {
        errors.emplace_back(Core::numToCellAddress(row, lenTypeCol)
            + " 仅支持定长数组，数组类长度型必须为 Fixed，实际为：" + arrayType);
    }

    //数组长度最小值，必须是数字
    const auto minnLenCellAddr = Core::numToCellAddress(row, minLenCol);
    const std::string arrayMinLen = Xlsx::getCellValue(sheet.cell(row, minLenCol));
    bool ok;
    int minLen = 0;
    try {
        minLen = std::stoi(arrayMinLen);
        ok = true;
    } catch (...) {
        ok = false;
    }

    if (checkUnusedCell(arrayMinLen, minnLenCellAddr).empty() || !ok || minLen < 0) {
        errors.emplace_back(minnLenCellAddr
            + " 定长数组类长度最小值必须为正整数，实际为：" + arrayMinLen);
    }
    //数组长度最大值，必须是数字
    const auto maxLenCellAddr = Core::numToCellAddress(row, maxLenCol);
    const std::string arrayMaxLen = Xlsx::getCellValue(sheet.cell(row, maxLenCol));
    ok = false;
    int maxLen = 0;
    try {
        maxLen = std::stoi(arrayMaxLen);
        ok = true;
    } catch (...) {
        ok = false;
    }

    if (checkUnusedCell(arrayMaxLen, maxLenCellAddr).empty() || !ok || maxLen < 0) {
        errors.emplace_back(maxLenCellAddr
            + " 定长数组类长度最大值必须为正整数，实际为：" + arrayMaxLen);
    }
    if (maxLen != minLen)
        errors.emplace_back(maxLenCellAddr
            + " 定长数组类长度最小值应与最大值保持一致");

    //检查引用是否有定义
    std::string memTypeRef = Xlsx::getCellValue(sheet.cell(row, memTypeRefCol));
    // 直接在成员所在行定义结构体成员数据类型
    if (checkUnusedCell(memTypeRef, Core::numToCellAddress(row, memTypeRefCol)).empty()) {
        //直接定义的数据类型
        arrayElementTypeDirectDefine(sheet, row, memTypeRef);
    } else {
        //引用数据类型
        arrayElementTypeRefDefine(sheet, row, memTypeRef);
    }
}

void DataTypeConsistencyCheck::arrayElementTypeDirectDefine(const OpenXLSX::XLWorksheet& sheet, const int row,
                                                            const std::string& memTypeRef) {
    //无需填写的单元格检查（不适用 Struct）
    std::string result = checkUnusedCell(memTypeRef, Core::numToCellAddress(row, memTypeRefCol));
    if (result.empty()) errors.emplace_back(result);

    const auto memName = Xlsx::getCellValue(sheet.cell(row, memNameCol));
    result = checkUnusedCell(memName, Core::numToCellAddress(row, memNameCol));
    if (result.empty()) errors.emplace_back(result);

    const auto memPosi = Xlsx::getCellValue(sheet.cell(row, memPosiCol));
    result = checkUnusedCell(memPosi, Core::numToCellAddress(row, memPosiCol));
    if (result.empty()) errors.emplace_back(result);

    checkMustDefinedCell(sheet, row);
}


void DataTypeConsistencyCheck::arrayElementTypeRefDefine(const OpenXLSX::XLWorksheet& sheet, const int row,
                                                         const std::string& memTypeRef) {
    //检查引用是否有定义
    if (!dataTypeNames.contains(memTypeRef))
        errors.emplace_back(Core::numToCellAddress(row, memTypeRefCol)
            + ": " + memTypeRef + " 结构体成员引用数据类型未在表格中定义");

    //剩余皆是不需要定义的
    const auto memName = Xlsx::getCellValue(sheet.cell(row, memNameCol));
    std::string result = checkUnusedCell(memName, Core::numToCellAddress(row, memNameCol));
    if (result.empty()) errors.emplace_back(result);

    const auto memPosi = Xlsx::getCellValue(sheet.cell(row, memPosiCol));
    result = checkUnusedCell(memPosi, Core::numToCellAddress(row, memPosiCol));
    if (result.empty()) errors.emplace_back(result);

    unusedCellForBasicType(sheet, row);
}

void DataTypeConsistencyCheck::unusedCellForBasicType(const OpenXLSX::XLWorksheet& sheet, const int row) {
    const auto baseType = Xlsx::getCellValue(sheet.cell(row, baseTypeCol));
    std::string result = checkUnusedCell(baseType, Core::numToCellAddress(row, baseTypeCol));
    if (result.empty()) errors.emplace_back(result);

    const auto sigLen = Xlsx::getCellValue(sheet.cell(row, sigLenCol));
    result = checkUnusedCell(sigLen, Core::numToCellAddress(row, sigLenCol));
    if (result.empty()) errors.emplace_back(result);

    const auto valueTable = Xlsx::getCellValue(sheet.cell(row, tableCol));
    result = checkUnusedCell(valueTable, Core::numToCellAddress(row, tableCol));
    if (result.empty()) errors.emplace_back(result);

    const auto factor = Xlsx::getCellValue(sheet.cell(row, factorCol));
    result = checkUnusedCell(factor, Core::numToCellAddress(row, factorCol));
    if (result.empty()) errors.emplace_back(result);

    const auto offset = Xlsx::getCellValue(sheet.cell(row, offsetCol));
    result = checkUnusedCell(offset, Core::numToCellAddress(row, offsetCol));
    if (result.empty()) errors.emplace_back(result);

    const auto minPhy = Xlsx::getCellValue(sheet.cell(row, minPhyCol));
    result = checkUnusedCell(minPhy, Core::numToCellAddress(row, minPhyCol));
    if (result.empty()) errors.emplace_back(result);

    const auto maxPhy = Xlsx::getCellValue(sheet.cell(row, maxPhyCol));
    result = checkUnusedCell(maxPhy, Core::numToCellAddress(row, maxPhyCol));
    if (result.empty()) errors.emplace_back(result);

    const auto unit = Xlsx::getCellValue(sheet.cell(row, unitCol));
    result = checkUnusedCell(unit, Core::numToCellAddress(row, unitCol));
    if (result.empty()) errors.emplace_back(result);
}
