#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>

// 模拟内存块存储（核心数据区）
std::vector<std::string> nodeDataLib;

// 原生字符串前缀判断函数（C++14 兼容，无constexpr）
bool startsWith(const std::string& data, const std::string& startData) {
    return data.compare(0, startData.size(), startData) == 0;
}

// 字符串转整数（鲁棒性处理：仅纯数字返回有效值）
int strToInt(const std::string& s) {
    int num = -1;
    std::stringstream ss(s);
    // 严格判断：全数字且无剩余字符才返回有效值
    if (!(ss >> num) || !ss.eof()) {
        return -1;
    }
    return num;
}

// 核心指令解析函数（修复recover参数解析逻辑）
std::string parse(const std::string& cmd) {
    std::string res;
    std::string command = cmd;
    // 去除首尾空格（避免空指令干扰）
    command.erase(0, command.find_first_not_of(" \t"));
    command.erase(command.find_last_not_of(" \t") + 1);

    // 1. 退出指令
    if (command == "exit") {
        res = "System shutdown...";
        return res;
    }

    // 2. 清屏指令（跨平台，直接调用system，符合C++14原生支持）
    if (command == "cs") {
        #ifdef _WIN32
            system("cls");
        #else
            system("clear");
        #endif
        res = "Screen cleared";
        return res;
    }

    // 3. 内存块分配指令（create）
    if (command == "create") {
        nodeDataLib.emplace_back(""); // C++14高效构造空字符串
        res = "Memory block allocated. Total blocks: " + std::to_string(nodeDataLib.size());
        return res;
    }

    // 4. 内存写入指令（recover）- 修复核心：正确提取索引+任意格式数据
    if (startsWith(command, "recover ")) {
        // 第一步：找到第一个空格（分割recover和参数）
        size_t firstSpace = command.find(' ');
        if (firstSpace == std::string::npos) {
            res = "Error: Usage: recover [index] [data]";
            return res;
        }
        // 第二步：找到第二个空格（分割索引和数据）
        size_t secondSpace = command.find(' ', firstSpace + 1);
        if (secondSpace == std::string::npos) {
            res = "Error: No data to write (usage: recover [index] [data])";
            return res;
        }
        // 提取索引（第一个空格到第二个空格之间的内容）
        std::string idxStr = command.substr(firstSpace + 1, secondSpace - (firstSpace + 1));
        int idx = strToInt(idxStr);
        if (idx == -1) {
            res = "Error: Index must be a number (e.g., recover 0 test data)";
            return res;
        }
        // 提取数据（第二个空格之后的所有内容，支持含空格）
        std::string data = command.substr(secondSpace + 1);
        // 写入内存块
        if (idx >= 0 && idx < nodeDataLib.size()) {
            nodeDataLib[idx] = data;
            res = "Success: Memory block " + std::to_string(idx) + " updated to '" + data + "'";
        } else {
            res = "Error: Index " + std::to_string(idx) + " out of range (total blocks: " + std::to_string(nodeDataLib.size()) + ")";
        }
        return res;
    }

    // 5. 内存读取指令（check）- 同步适配含空格的描述
    if (startsWith(command, "check ")) {
        size_t firstSpace = command.find(' ');
        if (firstSpace == std::string::npos) {
            res = "Error: Usage: check [index] [description]";
            return res;
        }
        size_t secondSpace = command.find(' ', firstSpace + 1);
        // 提取索引
        std::string idxStr = command.substr(firstSpace + 1, secondSpace - (firstSpace + 1));
        int idx = strToInt(idxStr);
        if (idx == -1) {
            res = "Error: Index must be a number (e.g., check 0 test desc)";
            return res;
        }
        // 提取描述（支持含空格）
        std::string desc = (secondSpace != std::string::npos) ? command.substr(secondSpace + 1) : "data";
        // 读取内存块
        if (idx >= 0 && idx < nodeDataLib.size()) {
            res = "The " + desc + " in memory block " + std::to_string(idx) + " is '" + nodeDataLib[idx] + "'";
        } else {
            res = "Error: Memory block " + std::to_string(idx) + " not exist (total blocks: " + std::to_string(nodeDataLib.size()) + ")";
        }
        return res;
    }

    // 6. 内存块列表指令（list）
    if (command == "list") {
        res = "All memory blocks (" + std::to_string(nodeDataLib.size()) + "):\n";
        for (size_t i = 0; i < nodeDataLib.size(); ++i) {
            res += "[" + std::to_string(i) + "] : '" + nodeDataLib[i] + "'\n";
        }
        return res;
    }

    // 未知指令
    res = "Runed Failure: Unknown command (support: create/recover/check/cs/list/exit)";
    return res;
}

int main() {
	std::cout << "CopyRight gitHub @songxuefeng1" << std::endl;
    std::cout << "Malabodge OS C2PESP v3\n";
    std::cout << "Commands:\n";
    std::cout << "  create          - Allocate new memory block\n";
    std::cout << "  recover [idx] [data] - Write data to memory block (support space in data)\n";
    std::cout << "  check [idx] [desc]   - Read data from memory block (support space in desc)\n";
    std::cout << "  cs              - Clear screen\n";
    std::cout << "  list            - Show all memory blocks\n";
    std::cout << "  exit            - Shutdown system\n";
    std::cout << "===============================================\n";

    std::string cmd;
    while (true) {
        std::cout << "\nCOMMAND PORT > ";
        std::getline(std::cin, cmd);
        std::string result = parse(cmd);
        std::cout << result << std::endl;
        if (cmd == "exit") break;
    }
    return 0;
}