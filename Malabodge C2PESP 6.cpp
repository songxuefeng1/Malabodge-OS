//author: @songxuefeng1

#include <fstream>
#include <string>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <iomanip>
#include "json.hpp"
#include "SpeakerController.hpp"
#include "Malabodge_Live_Plugs.hpp"

using json = nlohmann::json;

class LocalStorage {
public:
    explicit LocalStorage(const std::string& file_path = "local_storage.json") 
        : file_path_(file_path) {
        loadFromFile();
    }

    ~LocalStorage() {
        saveToFile();
    }

    template <typename T>
    void set(const std::string& key, const T& value) {
        data_[key] = value;
        saveToFile();
    }

    template <typename T>
    T get(const std::string& key, const T& default_value = T{}) const {
        if (data_.find(key) != data_.end()) {  
            return data_[key].get<T>();
        }
        return default_value;
    }

    void remove(const std::string& key) {
        data_.erase(key);
        saveToFile();
    }

    void clear() {
        data_.clear();
        saveToFile();
    }

    bool contains(const std::string& key) const {
        return data_.find(key) != data_.end();
    }

    void loadNodeData(std::vector<std::string>& nodeDataLib) {
        nodeDataLib.clear();
        if (contains("node_data")) {
            json node_json = get<json>("node_data");
            for (auto& item : node_json) {
                nodeDataLib.push_back(item.get<std::string>());
            }
        }
    }

    void saveNodeData(const std::vector<std::string>& nodeDataLib) {
        json node_json = json::array();
        for (const auto& data : nodeDataLib) {
            node_json.push_back(data);
        }
        set("node_data", node_json);
    }

private:
    void loadFromFile() {
        std::ifstream file(file_path_);
        if (file.is_open()) {
            try {
                file >> data_;
            } catch (const std::exception& e) {
                std::cerr << "加载LocalStorage失败：" << e.what() << std::endl;
                data_ = json::object();
            }
            file.close();
        } else {
            data_ = json::object();
            saveToFile();
        }
    }

    void saveToFile() {
        std::ofstream file(file_path_);
        if (file.is_open()) {
            try {
                file << std::setw(4) << data_;
            } catch (const std::exception& e) {
                std::cerr << "save LocalStorage node data fail(run log): " << e.what() << std::endl;
            }
            file.close();
        } else {
            throw std::runtime_error("Cannot open file: " + file_path_);
        }
    }

    json data_;
    std::string file_path_;
};

LocalStorage storage;
std::vector<std::string> nodeDataLib;

bool startsWith(const std::string& data, const std::string& startData) {
    return data.compare(0, startData.size(), startData) == 0;
}

// 修复：正确将整数转0-1幅度（原逻辑返回小数部分完全错误）
double intFitDouble(const int& num) {
    double d_num = static_cast<double>(num);
    // 手动实现 clamp 逻辑，兼容 C++14
    if (d_num < 0.0) {
        d_num = 0.0;
    } else if (d_num > 100.0) {
        d_num = 100.0;
    }
    return d_num / 100.0;
}

std::string getTime() {
    try {
        std::string monTags[12] = {"Jan.", "Feb.", "Mar.", "Apr.", "May.", "Jun.", "Jul.", "Aug.", "Sep.", "Oct.", "Nov.", "Dec."};
        auto now = std::chrono::system_clock::now();
        time_t now_time_t = std::chrono::system_clock::to_time_t(now);
        tm local_tm = *std::localtime(&now_time_t);
        std::ostringstream oss;
        
        int year = local_tm.tm_year + 1900, month = local_tm.tm_mon + 1, day = local_tm.tm_mday, hour = local_tm.tm_hour, minute = local_tm.tm_min;
        oss << "year: " << year << std::endl << "month: " << monTags[month-1] << std::endl << "day: " << day << std::endl << hour << ":" << minute << std::endl;
        return oss.str();
    } catch (const std::exception& e) {
        std::ostringstream log;
        log << "Time get failure, runtime log: " << e.what();
        return log.str();
    }
}

int strToInt(const std::string& s) {
    int num = -1;
    std::stringstream ss(s);
    if (!(ss >> num) || !ss.eof()) {
        return -1;
    }
    return num;
}

bool isDigitChar(char c) {
    return c >= '0' && c <= '9';
}

// 修复：解析声音编码，返回频率/幅度/时长三个数组（原逻辑只返回频率）
void parse_voice_code(const std::string& data, std::vector<int>& frequency, std::vector<int>& range, std::vector<int>& delay) {
    frequency.clear();
    range.clear();
    delay.clear();
    bool isData = false;
    int DL = 0; // 0=频率, 1=幅度, 2=时长（固定顺序，不随意切换）
    std::string tempnum = "";

    try {
        for (size_t i = 0; i < data.length(); i++) {
            if (data[i] != ' ' && !isData) {
                if (isDigitChar(data[i])) {
                    tempnum += data[i];
                    isData = true;
                } else {
                    frequency.clear();
                    range.clear();
                    delay.clear();
                    return;
                }
            } else if (data[i] == ' ' && isData) {
                // 遇到空格，先存当前数字，再切换DL
                int num = strToInt(tempnum);
                if (num != -1) {
                    if (DL == 0) frequency.push_back(num);
                    else if (DL == 1) range.push_back(num);
                    else if (DL == 2) delay.push_back(num);
                }
                tempnum = "";
                isData = false;
                DL = (DL + 1) % 3; // 只有遇到空格时才切换DL
            }
        }
        // 处理最后一个数字：不再切换DL，直接按当前DL存入（核心修复！）
        if (!tempnum.empty()) {
            int num = strToInt(tempnum);
            if (num != -1) {
                if (DL == 0) frequency.push_back(num);
                else if (DL == 1) range.push_back(num);
                else if (DL == 2) delay.push_back(num);
            }
        }
    } catch(const std::exception& e) {
        std::cerr << e.what() << std::endl;
        frequency.clear();
        range.clear();
        delay.clear();
    }
}

// 修复：声音播放逻辑（原逻辑因空数组跳过播放）
void play_sound(const std::vector<int>& freq, const std::vector<int>& rng, const std::vector<int>& dey) {
    try {
        SpeakerController speaker;
        size_t maxSize = std::min({freq.size(), rng.size(), dey.size()});
        if (maxSize == 0) {
            std::cerr << "错误：声音参数格式错误（需：频率 幅度 时长）" << std::endl;
            return;
        }
        for (size_t i = 0; i < maxSize; i++) {
            speaker.PlaySound(static_cast<double>(freq.at(i)), intFitDouble(rng.at(i)), static_cast<DWORD>(dey.at(i)));
            Sleep(dey.at(i) + 100); // 避免声音重叠
        }
        speaker.Cleanup();
        std::cout << "Media Player Sucsessed";
    } catch(const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}

std::string parse(const std::string& cmd) {
    std::string res;
    std::string command = cmd;
    command.erase(0, command.find_first_not_of(" \t"));
    if (!command.empty()) {
        command.erase(command.find_last_not_of(" \t") + 1);
    }

    if (command == "exit") {
        storage.saveNodeData(nodeDataLib);
        res = "System shutdown... (data saved to local_storage.json)";
        return res;
    }
    if (command == "cs") {
        #ifdef _WIN32
            system("cls");
        #else
            system("clear");
        #endif
        res = "Screen cleared";
        return res;
    }
    
    if (command == "time") {
        res = getTime();
        return res;
    }

    if (command == "create") {
        nodeDataLib.emplace_back("");
        storage.saveNodeData(nodeDataLib);
        res = "Memory block allocated. Total blocks: " + std::to_string(nodeDataLib.size());
        return res;
    }
    
    // 修复1：匹配 LivePlugins（首字母大写）+ livePlugins，兼容你的命令输入
    if (command == "LivePlugins" || command == "livePlugins") {
        try {
            res = "MB-OS Live Plugins Loaded sucsess";
            MB_LIVE::game1(); // 保留你原有的游戏调用逻辑
            return res;
        } catch(const std::exception& e) {
            res = "runtime-error had been caugthed: " + std::string(e.what());
            return res;
        }
    }

    if (startsWith(command, "recover ")) {
        size_t firstSpace = command.find(' ');
        if (firstSpace == std::string::npos) {
            res = "Error: Usage: recover [index] [data]";
            return res;
        }
        size_t secondSpace = command.find(' ', firstSpace + 1);
        if (secondSpace == std::string::npos) {
            res = "Error: No data to write (usage: recover [index] [data])";
            return res;
        }
        std::string idxStr = command.substr(firstSpace + 1, secondSpace - (firstSpace + 1));
        int idx = strToInt(idxStr);
        if (idx == -1) {
            res = "Error: Index must be a number (e.g., recover 0 test data)";
            return res;
        }
        std::string data = command.substr(secondSpace + 1);
        if (idx >= 0 && static_cast<size_t>(idx) < nodeDataLib.size()) {
            nodeDataLib[idx] = data;
            storage.saveNodeData(nodeDataLib);
            res = "Success: Memory block " + std::to_string(idx) + " updated to '" + data + "' (saved to storage)";
        } else {
            res = "Error: Index " + std::to_string(idx) + " out of range (total blocks: " + std::to_string(nodeDataLib.size()) + ")";
        }
        return res;
    }

    if (startsWith(command, "check ")) {
        size_t firstSpace = command.find(' ');
        if (firstSpace == std::string::npos) {
            res = "Error: Usage: check [index] [description]";
            return res;
        }
        size_t secondSpace = command.find(' ', firstSpace + 1);
        std::string idxStr = command.substr(firstSpace + 1, secondSpace != std::string::npos ? secondSpace - (firstSpace + 1) : command.size() - firstSpace - 1);
        int idx = strToInt(idxStr);
        if (idx == -1) {
            res = "Error: Index must be a number (e.g., check 0 test desc)";
            return res;
        }
        std::string desc = (secondSpace != std::string::npos) ? command.substr(secondSpace + 1) : "data";
        if (idx >= 0 && static_cast<size_t>(idx) < nodeDataLib.size()) {
            res = "sound displayed sucsessful";
        } else {
            res = "Error: Memory block " + std::to_string(idx) + " not exist (total blocks: " + std::to_string(nodeDataLib.size()) + ")";
        }
        return res;
    }

    if (command == "list") {
        res = "All memory blocks (" + std::to_string(nodeDataLib.size()) + "):\n";
        for (size_t i = 0; i < nodeDataLib.size(); ++i) {
            res += "[" + std::to_string(i) + "] : '" + nodeDataLib[i] + "'\n";
        }
        return res;
    }

    if (startsWith(command, "storage set ")) {
        size_t firstSpace = command.find(' ', 12);
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

    if (startsWith(command, "storage get ")) {
        std::string key = command.substr(12);
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

    if (startsWith(command, "storage remove ")) {
        std::string key = command.substr(16);
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
    
    // 修复2：display命令调用修复后的parse_voice_code，传入三个数组
    if (startsWith(command, "display ")) {
        size_t firstSpace = command.find(' ');
        if (firstSpace == std::string::npos) {
            res = "Error: Usage: play [index] [description]";
            return res;
        }
        size_t secondSpace = command.find(' ', firstSpace + 1);
        std::string idxStr = command.substr(firstSpace + 1, secondSpace != std::string::npos ? secondSpace - (firstSpace + 1) : command.size() - firstSpace - 1);
        int idx = strToInt(idxStr);
        if (idx == -1) {
            res = "Error: Index must be a number (e.g., play 0 test desc)";
            return res;
        }
        std::string desc = (secondSpace != std::string::npos) ? command.substr(secondSpace + 1) : "data";
        if (idx >= 0 && static_cast<size_t>(idx) < nodeDataLib.size()) {
            res = "The " + desc + " in memory block " + std::to_string(idx) + " is '" + nodeDataLib[idx] + "'";
            // 修复：传入三个数组解析
            std::vector<int> freq, rng, dey;
            parse_voice_code(nodeDataLib[idx], freq, rng, dey);
            play_sound(freq, rng, dey);
        } else {
            res = "Error: Memory block " + std::to_string(idx) + " not exist (total blocks: " + std::to_string(nodeDataLib.size()) + ")";
        }
        return res;
    }

    if (command == "storage clear") {
        storage.clear();
        res = "Success: All storage data cleared";
        return res;
    }

    res = "Runed Failure: Unknown command";
    return res;
}

int main() {
    storage.loadNodeData(nodeDataLib);

    std::cout << "Malabodge OS C2PESP v6 (with LocalStorage)\n";
    std::cout << "Commands:\n";
    std::cout << "  LivePlugins             - Activate the MB-OS Live plugins (so that you can play game)\n";
    std::cout << "  display [idx]           - display the index data with a format with a sound\n";
    std::cout << "  create                  - Allocate new memory block\n";
    std::cout << "  recover [idx] [data]    - Write data to memory block (support space in data)\n";
    std::cout << "  check [idx] [desc]      - Read data from memory block (support space in data)\n";
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