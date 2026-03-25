//
// Created by gaopeng on 2026/3/15.
//

#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H
#include <string>
#include <regex>
#include <unordered_set>
#include "xlsxdocument.h"
#include <qcoreapplication.h>
// #include <QFileInfo>
// #include <QSettings>
#include <QDir>

class common_utils {
    public:
    common_utils() = default;
    ~common_utils() = default;

private:
    // C语言关键字集合（C11标准）
    const std::unordered_set<std::string> C_KEYWORDS = {
        "auto", "break", "case", "char", "const", "continue", "default", "do",
        "double", "else", "enum", "extern", "float", "for", "goto", "if",
        "inline", "int", "long", "register", "restrict", "return", "short",
        "signed", "sizeof", "static", "struct", "switch", "typedef", "union",
        "unsigned", "void", "volatile", "while", "_Alignas", "_Alignof",
        "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary", "_Noreturn",
        "_Static_assert", "_Thread_local"
    };

public:
    /**
     * @brief 格式化字符串，去除所有空白字符
     *
     * 该函数会移除输入字符串中的所有空白字符，包括空格、制表符、回车和换行符。
     * 如果输入为空字符串，则返回空字符串。
     *
     * @param str 输入的字符串
     * @return std::string 格式化后的字符串
     */
    std::string stringFormat(const std::string &str);

    /**
     * @brief 获取或设置最近打开的目录路径
     *
     * 当传入newPath参数时，保存该路径作为最近打开的目录；
     * 当不传参数时，读取并返回之前保存的最近打开目录。
     * 路径信息保存在程序目录下的conf/lastdir.txt文件中。
     *
     * @param newPath 新的路径，如果非空则保存；如果为空则执行读取操作
     * @return QString 最近打开的目录路径
     */
    QString getLastOpenDir(const QString &newPath = QString());

    /**
     * @brief 将行列号转换为Excel单元格地址格式
     *
     * 将给定的行号和列号转换为Excel风格的单元格地址（如A1, B2等）。
     * 列号使用字母表示（A-Z, AA-ZZ等），行号使用数字表示。
     *
     * @param rowNum 行号
     * @param colNum 列号
     * @return std::string Excel单元格地址格式的字符串
     */
    std::string cellNumberToLetter(int rowNum,int colNum);

    /**
     * @brief 检查给定的字符串是否为有效的C语言变量名
     *
     * 该函数检查字符串是否符合C语言变量名的命名规范：
     * 1. 不能是C语言关键字
     * 2. 必须以大写字母开头
     * 3. 后续字符只能包含字母、数字或下划线
     * 4. 长度不能超过指定的最大长度
     *
     * @param name 要检查的变量名字符串
     * @param length 允许的最大长度，默认值为32
     * @return bool 如果是有效的C变量名返回true，否则返回false
     */
    bool isValidCVariableName(const std::string& name, int length = 32);

    /**
     * @brief 检查字符串是否为有效的数字
     *
     * 该函数判断输入字符串是否表示一个有效的数字，
     * 支持整数、小数、负数和科学计数法表示的数字。
     *
     * @param str 输入的字符串
     * @return bool 如果是有效数字返回true，否则返回false
     */
    bool isNumber(const std::string& str);

    /**
     * @brief 检查指定单元格是否位于合并区域内
     *
     * 该函数检查给定工作表中的指定单元格(row, col)是否属于某个合并区域。
     *
     * @param worksheet 指向工作表的指针
     * @param targetRow 目标行号
     * @param targetCol 目标列号
     * @return bool 如果单元格在合并区域内返回true，否则返回false
     */
    bool isCellMerged(const QXlsx::Worksheet *worksheet,int targetRow, int targetCol);

    /**
     * @brief 获取目标单元格所在连续相同值区域的最后一行
     *
     * 该函数获取目标单元格所在区域的最后行号，考虑两种情况：
     * 1. 如果单元格在合并区域内，返回合并区域的最后一行
     * 2. 如果单元格有值，返回向下连续相同值区域的最后一行
     *
     * @param worksheet 指向工作表的指针
     * @param targetRow 目标行号
     * @param targetCol 目标列号
     * @return int 最后一行的行号
     */
    int getLastRowNum(const QXlsx::Worksheet *worksheet,int targetRow, int targetCol);

    /**
     * @brief 根据列标题名称获取列号（第一行）
     *
     * 在指定工作表的第一行中查找与给定标题名称匹配的单元格，
     * 并返回其列号。如果没有找到匹配项，返回0。
     *
     * @param sheet 指向工作表的指针
     * @param headerName 要查找的列标题名称
     * @return int 匹配列的列号，未找到返回-1
     */
    int getColumnByHeader(const QXlsx::Worksheet *sheet, const std::string &headerName);

    /**
     * @brief 根据列标题名称获取列号（指定行）
     *
     * 在指定工作表的指定行中查找与给定标题名称匹配的单元格，
     * 并返回其列号。如果没有找到匹配项，返回0。
     *
     * @param sheet 指向工作表的指针
     * @param headerName 要查找的列标题名称
     * @param rowNum 指定查找的行号
     * @return int 匹配列的列号，未找到返回-1
     */
    int getColumnByHeader(const QXlsx::Worksheet *sheet, const std::string &headerName, int rowNum);

    /**
     * @brief 读取单元格的值并进行格式化
     *
     * 该函数读取指定单元格的值，并根据值的类型进行适当的格式化处理：
     * - 日期时间：格式化为"yyyy-MM-dd"格式
     * - 浮点数：根据精度要求进行格式化
     * - 整数：直接转换为字符串
     * - 字符串：直接返回
     *
     * @param cell 指向单元格的智能指针
     * @param precision 数值精度，-1表示自动判断，其他值表示小数位数
     * @return QString 格式化后的字符串值
     */
    QString readCellValue(const std::shared_ptr<QXlsx::Cell>& cell, int precision=-1);
};


#endif //COMMON_UTILS_H
