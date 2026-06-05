#include <fstream>
#include <string>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <iomanip>  // 用于JSON格式化输出
// 注意：需要将nlohmann/json.hpp放在项目目录下，或配置头文件路径
#include "json.hpp"  

using json = nlohmann::json;

// 持久化存储类（LocalStorage）
class LocalStorage {
public:
    // 构造函数：指定存储文件路径，初始化时加载已有数据
    explicit LocalStorage(const std::string& file_path = "local_storage.json") 
        : file_path_(file_path) {
        loadFromFile();  // 启动时加载本地数据
    }

    // 析构函数：确保数据写入文件
    ~LocalStorage() {
        saveToFile();
    }

    // 1. 存储键值对（支持string/int/double/bool等基础类型）
    template <typename T>
    void set(const std::string& key, const T& value) {
        data_[key] = value;  // JSON对象直接赋值
        saveToFile();        // 实时写入文件，保证持久化
    }

    // 2. 获取指定键的值（找不到返回默认值）
    template <typename T>
    T get(const std::string& key, const T& default_value = T{}) const {
        // C++14兼容写法（替换C++17的contains）
        if (data_.find(key) != data_.end()) {  
            return data_[key].get<T>();
        }
        return default_value;
    }

    // 3. 删除指定键
    void remove(const std::string& key) {
        data_.erase(key);
        saveToFile();
    }

    // 4. 清空所有数据
    void clear() {
        data_.clear();
        saveToFile();
    }

    // 5. 检查键是否存在
    bool contains(const std::string& key) const {
        return data_.find(key) != data_.end();  // C++14兼容写法
    }

    // 6. 加载内存块数据（新增：适配nodeDataLib）
    void loadNodeData(std::vector<std::string>& nodeDataLib) {
        nodeDataLib.clear();
        if (contains("node_data")) {
            json node_json = get<json>("node_data");
            // 将JSON数组转换为vector<string>
            for (auto& item : node_json) {
                nodeDataLib.push_back(item.get<std::string>());
            }
        }
    }

    // 7. 保存内存块数据（新增：适配nodeDataLib）
    void saveNodeData(const std::vector<std::string>& nodeDataLib) {
        json node_json = json::array();
        for (const auto& data : nodeDataLib) {
            node_json.push_back(data);
        }
        set("node_data", node_json);
    }

private:
    // 从文件加载数据
    void loadFromFile() {
        std::ifstream file(file_path_);
        if (file.is_open()) {
            try {
                file >> data_;  // JSON反序列化
            } catch (const std::exception& e) {
                std::cerr << "加载LocalStorage失败：" << e.what() << std::endl;
                data_ = json::object();  // 加载失败则初始化空对象
            }
            file.close();
        } else {
            data_ = json::object();
            saveToFile();
        }
    }

    // 将数据写入文件
    void saveToFile() {
        std::ofstream file(file_path_);
        if (file.is_open()) {
            try {
                // 格式化输出JSON，便于阅读
                file << std::setw(4) << data_;
            } catch (const std::exception& e) {
                std::cerr << "save LocalStorage node data fail(run log): " << e.what() << std::endl;
            }
            file.close();
        } else {
            throw std::runtime_error("Cannot open file: " + file_path_);
        }
    }

    json data_;               // 内存中存储的键值对
    std::string file_path_;   // 持久化文件路径
};

// 全局存储实例（程序生命周期内有效）
LocalStorage storage;
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

// 核心指令解析函数（融入LocalStorage）
std::string parse(const std::string& cmd) {
    std::string res;
    std::string command = cmd;
    // 去除首尾空格（避免空指令干扰）
    command.erase(0, command.find_first_not_of(" \t"));
    command.erase(command.find_last_not_of(" \t") + 1);

    // 1. 退出指令（退出前保存内存块数据）
    if (command == "exit") {
        storage.saveNodeData(nodeDataLib);  // 保存内存块到文件
        res = "System shutdown... (data saved to local_storage.json)";
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

    // 3. 内存块分配指令（create）- 新增：持久化
    if (command == "create") {
        nodeDataLib.emplace_back(""); // C++14高效构造空字符串
        storage.saveNodeData(nodeDataLib);  // 实时保存
        res = "Memory block allocated. Total blocks: " + std::to_string(nodeDataLib.size());
        return res;
    }

    // 4. 内存写入指令（recover）- 修复核心：正确提取索引+任意格式数据 + 持久化
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
            storage.saveNodeData(nodeDataLib);  // 实时保存
            res = "Success: Memory block " + std::to_string(idx) + " updated to '" + data + "' (saved to storage)";
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

    // ====================== 新增：LocalStorage 专属指令 ======================
    // 7. storage set 指令：设置键值对（如 storage set name "malabodge os"）
    if (startsWith(command, "storage set ")) {
        size_t firstSpace = command.find(' ', 12);  // 跳过 "storage set "
        if (firstSpace == std::string::npos) {
            res = "Error: Usage: storage set [key] [value]";
            return res;
        }
        std::string key = command.substr(12, firstSpace - 12);
        std::string value = command.substr(firstSpace + 1);
        storage.set(key, value);
        res = "Success: Storage key '" + key + "' set to '" + value + "'";
        return res;
    }

    // 8. storage get 指令：获取键值对（如 storage get name）
    if (startsWith(command, "storage get ")) {
        std::string key = command.substr(12);  // 跳过 "storage get "
        if (key.empty()) {
            res = "Error: Usage: storage get [key]";
            return res;
        }
        if (storage.contains(key)) {
            std::string value = storage.get<std::string>(key);
            res = "Success: Storage key '" + key + "' = '" + value + "'";
        } else {
            res = "Error: Storage key '" + key + "' not found";
        }
        return res;
    }

    // 9. storage remove 指令：删除键（如 storage remove name）
    if (startsWith(command, "storage remove ")) {
        std::string key = command.substr(16);  // 跳过 "storage remove "
        if (key.empty()) {
            res = "Error: Usage: storage remove [key]";
            return res;
        }
        if (storage.contains(key)) {
            storage.remove(key);
            res = "Success: Storage key '" + key + "' removed";
        } else {
            res = "Error: Storage key '" + key + "' not found";
        }
        return res;
    }

    // 10. storage clear 指令：清空所有存储
    if (command == "storage clear") {
        storage.clear();
        res = "Success: All storage data cleared";
        return res;
    }

    // 未知指令
    res = "Runed Failure: Unknown command (support: create/recover/check/cs/list/storage/exit)";
    return res;
}

int main() {
    // 程序启动时加载历史内存块数据
    storage.loadNodeData(nodeDataLib);
	
	std::cout << "CopyRight gitHub @songxuefeng1" << std::endl;
    std::cout << "Malabodge OS C2PESP v4 (with LocalStorage)\n";
    std::cout << "Commands:\n";
    std::cout << "  create                  - Allocate new memory block\n";
    std::cout << "  recover [idx] [data]    - Write data to memory block (support space in data)\n";
    std::cout << "  check [idx] [desc]      - Read data from memory block (support space in desc)\n";
    std::cout << "  cs                      - Clear screen\n";
    std::cout << "  list                    - Show all memory blocks\n";
    std::cout << "  storage set [key] [val] - Set key-value in LocalStorage\n";
    std::cout << "  storage get [key]       - Get value from LocalStorage\n";
    std::cout << "  storage remove [key]    - Remove key from LocalStorage\n";
    std::cout << "  storage clear           - Clear all LocalStorage data\n";
    std::cout << "  exit                    - Shutdown system (save data)\n";
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