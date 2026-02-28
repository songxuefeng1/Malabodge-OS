#ifndef MALABODGE_LIVE_PLUGS_H
#define MALABODGE_LIVE_PLUGS_H

#include <vector>
#include <random>
#include <mutex>
#include <conio.h>
#include <windows.h>
#include <iostream>

namespace MB_LIVE {
    std::vector<std::vector<char>> game_space;
    const int init_game_space = 20;
    const int playerInitX = 0, playerInitY = 0;
    int playerX = playerInitX, playerY = playerInitY;

    // 函数实现（不要 inline）
    inline int random_int(int min, int max) {
        static std::mt19937 engine(std::random_device{}());
        static std::mutex mtx;
        std::lock_guard<std::mutex> lock(mtx);
        std::uniform_int_distribution<int> dist(min, max);
        return dist(engine);
    }

    inline void init() {
        char resc[3] = {' ', 'O', 'I'};
        game_space.clear();
        for (int i = 0; i < init_game_space; i++) {
            game_space.push_back(std::vector<char>());
            for (int j = 0; j < init_game_space; j++) {
                if (i == playerInitX && j == playerInitY) {
                    game_space[i].push_back(resc[random_int(0, 2)]);
                    playerX = i;
                    playerY = j;
                } else {
                    game_space[i].push_back('#');
                }
            }
        }
    }

    inline void update(int key) {
        #define IS_UP (key == 72 || key == 'W' || key == 'w')
        #define IS_DOWN (key == 80 || key == 'S' || key == 's')
        #define IS_LEFT (key == 75 || key == 'A' || key == 'a')
        #define IS_RIGHT (key == 77 || key == 'D' || key == 'd')
        
        int newX = playerX;
        int newY = playerY;
        
        if (IS_UP) newX--;
        else if (IS_DOWN) newX++;
        else if (IS_LEFT) newY--;
        else if (IS_RIGHT) newY++;
        else return;
        
        if (newX < 0 || newX >= init_game_space || newY < 0 || newY >= init_game_space)
            return;
        
        if (game_space[newX][newY] == 'O' || game_space[newX][newY] == 'I')
            return;
        
        game_space[playerX][playerY] = '#';
        game_space[newX][newY] = ' ';
        playerX = newX;
        playerY = newY;
        
        system("cls");
        for (auto& row : game_space) {
            for (char c : row) std::cout << c;
            std::cout << '\n';
        }
        
        #undef IS_UP
        #undef IS_DOWN
        #undef IS_LEFT
        #undef IS_RIGHT
    }

    inline void game1() {
        int key;
        init();
        while (true) {
            if (_kbhit()) {
                key = _getch();
                if (key == 0 || key == 224) {
                    update(_getch());
                } else if (key == 27) {
                    std::cout << "\nMB-OS Plugins successfully closed\n";
                    return;
                } else {
                    update(key);
                }
            }
            Sleep(100);
        }
    }
}

#endif