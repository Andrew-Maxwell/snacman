#include <iostream>
#include <fstream>
#include <raylib.h>
#include <list>
#include <vector>
#include <algorithm>
#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

#define WIDTH 800
#define HEIGHT 600
#define GRID 40

using namespace std;

struct V2 {
    int x, y;

    V2(int newX, int newY) : x(newX), y(newY) {}
    V2() {x = y = 0;}

    bool operator==(const V2& other) {
        return x == other.x && y == other.y;
    }

    bool operator!=(const V2& other) {
        return x != other.x || y != other.y;
    }

    V2 operator+(const V2& other) {
        return V2(x + other.x, y + other.y);
    }

    V2 operator-(const V2& other) {
        return V2(x - other.x, y - other.y);
    }
};

struct spider {

};

struct compass {
    int clockwise = -1;
    V2 cardinal[4] = {V2(0, -1), V2(1, 0), V2(0, 1), V2(-1, 0)};

    void reverse() {
        clockwise *= -1;
    }

    V2 get(V2 current, int offset) {
        int index = -1;
        for (int i = 0; i < 4; i++) {
            if (cardinal[i] == current) {
                index = i;
            }
        }
        index = ((index + offset * clockwise) + 4) % 4;
        return cardinal[index];
    }
};

struct segment {
    V2 pos;
    V2 dir;

    segment(V2 newPos, V2 newDir) : pos(newPos), dir(newDir) {}
};

struct mainData {

    vector<string> map;
    list<segment> snake;
    list<spider> spiders;
    int snakeSize = 1;
    compass c;

    void readLevel(string levelName) {
        ifstream level(levelName);
        if (!level) {
            cerr << "Couldn't open " << levelName << endl;
            exit(EXIT_FAILURE);
        }
        string line;
        while (getline(level, line)) {
            int snakePos = line.find('S');
            if (snakePos != string::npos) {
                snake.push_front(segment(V2(snakePos, map.size()), V2(0, 0)));
            }
            map.push_back(line);
        }
        level.close();
    }

    void mainLoop() {
        BeginDrawing();
        ClearBackground(BLACK);
        for (int row = 0; row < map.size(); row++) {
            for (int col = 0; col < map[row].size(); col++) {
                if (map[row][col] == '#') {
                    DrawRectangle(GRID * col, GRID * row, GRID, GRID, GRAY);
                }
            }
        }
        for (segment & s : snake) {
            DrawCircle((s.pos.x + 0.5) * GRID, (s.pos.y + 0.5) * GRID, GRID / 2, YELLOW);
        }
        segment next = *(snake.begin());
        for (int i = 0; i < 4; i++) {
            V2 nextDir = c.get(next.dir, i);
            V2 nextPos = next.pos + nextDir;
            if (map[nextPos.y][nextPos.x] != '#') {
                next.pos = nextPos;
                next.dir = c.get(nextDir, -1);
                snake.push_front(next);
                break;
            }
        }
        while (snake.size() > snakeSize) {
            snake.pop_back();
        }
        EndDrawing();
    }

};

mainData everything;
void doEverything() {
    everything.mainLoop();
}

int main(int argc, char** argv) {

    if (argc != 2) {
        cerr << "Usage: " << argv[0] << "<level file>\n";
        exit(EXIT_FAILURE);
    }
    everything.readLevel(argv[1]);

    InitWindow(WIDTH, HEIGHT, "snacman");
    
#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(doEverything, 60, 1);
#else
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        everything.mainLoop();
    }
#endif
}
