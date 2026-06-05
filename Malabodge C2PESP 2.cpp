#include<iostream>
#include<string>
#include<vector>
#include<sstream> // 用于字符串转数字
using namespace std;

// 判断字符串是否以指定前缀开头
// 返回值：1=是，0=否，2=长度不足
int startsWith(const string data, const string startData) {
    if (data.length() < startData.length()) {
        return 2;
    } else {
        return (data.substr(0, startData.length()) == startData) ? 1 : 0;
    }
}

// 按分隔符分割字符串，返回第maxsplit次分割后的子串
// 例如：split("recover 1 abc", ' ', 1) 返回 "1 abc"
string split(const string data ,char spChar, int maxsplit) {
    int cnt = 0;
    for (int i = 0; i < data.length(); i++) {
        if (data[i] == spChar) {
            cnt += 1;
            // 找到第maxsplit次分隔符时，返回后续子串
            if (cnt == maxsplit) {
                return data.substr(i+1); // i+1 跳过分隔符
            }
        }
    }
    // 未找到指定次数的分隔符时返回空字符串
    return "";
}

// 将字符串转换为整数（处理split返回的数字字符串）
int strToInt(const string& s) {
    int num;
    stringstream ss(s);
    ss >> num;
    return num;
}

// 解析命令（nodeDataLib需传引用，否则无法修改原数组）
void parse(const string command, vector<string>& nodeDataLib) {
    string res;
    if (startsWith(command, "recover ") == 1) {
        string nodeStr = split(command, ' ', 1);
        string dataStr = split(command, ' ', 2);
        int nodeIdx = strToInt(nodeStr);
        if (nodeIdx >= 0 && nodeIdx < nodeDataLib.size()) {
            nodeDataLib[nodeIdx] = dataStr;
            res = "Recover success";
        } else {
            res = "Invalid node index";
        }
    } else if (startsWith(command, "check ") == 1) {
        string nodeStr = split(command, ' ', 1);
        int nodeIdx = strToInt(nodeStr);
        if (nodeIdx >= 0 && nodeIdx < nodeDataLib.size()) {
            res = "the " + split(command, ' ', 2) + " data node is `" + nodeDataLib.at(nodeIdx) + "`";
        } else {
            res = "Invalid node index";
        }
    } else if (startsWith(command, "create ") == 1) {
        nodeDataLib.push_back("");
        res = "Create success";
	} else if (command == "cs") {
		system("cls");
	} else if (startsWith(command, "execute ")) {
		string nodeStr = split(command, ' ', 1);
        int nodeIdx = strToInt(nodeStr);
        if (nodeIdx >= 0 && nodeIdx < nodeDataLib.size()) {
            parse(nodeDataLib.at(nodeIdx), nodeDataLib);
            res = "parsed";
        } else {
            res = "Invalid node index";
    	}
    } else {
        if (command != "exit") {
            res = "Runed Failure";
        }
    }
    if (!res.empty()) {
        cout << res << endl;
    }
}

int main() {
    vector<string> nodeDataLib(100, "");
    string cmd;
    cout << "CopyRight gitHub @songxuefeng1" << endl;
    cout << "Malabodge System DSCWC++" << endl;
    cout << "Enter command (exit to quit, cs to clear screen):" << endl;
    while (true) {
        cout << "COMMAND ENTER> ";
        getline(cin, cmd);
        if (cmd == "exit") {
            break;
        }
        parse(cmd, nodeDataLib);
    }
    return 0;
}