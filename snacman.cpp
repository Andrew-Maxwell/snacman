#include <algorithm>
#include <climits>
#include <fstream>
#include <iostream>
#include <list>
#include <raylib.h>
#include <vector>
#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

#define WIDTH 800
#define HEIGHT 600
#define GRID 40
#define INDICATOR_THICKNESS 10
// update rate for game logic is 60fps/TICK_RATE
static int TICK_RATE = 12;

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

    V2 operator*(int scalar) {
        return V2(x * scalar, y * scalar);
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
    V2 forward;
    V2 down;

    segment(V2 newPos, V2 newForward, V2 newDown) : pos(newPos), forward(newForward), down(newDown) {}
};

struct mainData {

    vector<string> map;
    list<segment> snake;
    list<segment> moveQueue;
    list<spider> spiders;
    int snakeSize = 1;
    int tickCount = 0;
    compass c;
    bool pause = false;

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
                snake.push_front(segment(V2(snakePos, map.size()), V2(0, 0), V2(0, 0)));
            }
            map.push_back(line);
        }
        level.close();
    }

    void mainLoop() {
        BeginDrawing();
        ClearBackground(BLACK);
        // draw map
        for (int row = 0; row < map.size(); row++) {
            for (int col = 0; col < map[row].size(); col++) {
                if (map[row][col] == '#') {
                    DrawRectangle(GRID * col, GRID * row, GRID, GRID, GRAY);
                }
                if (map[row][col] == 'A') {
                    DrawCircle((col + 0.5) * GRID, (row + 0.5) * GRID, GRID / 2, RED);
                }
            }
        }
        // draw snake
        for (segment & s : snake) {
            DrawCircle((s.pos.x + 0.5) * GRID, (s.pos.y + 0.5) * GRID, GRID / 2, YELLOW);
            // draw indicator of which side we are on
            if ((s.down.x == 0) && (s.down.y == 0)) continue;
            int w = s.down.x != 0 ? INDICATOR_THICKNESS : GRID;
            int h = s.down.y != 0 ? INDICATOR_THICKNESS : GRID;

            V2 logicalPos = s.pos + s.down;
            // draw the indicator on the inside of the wall we are on
            int x = logicalPos.x * GRID + (s.down.x < 0 ? GRID - INDICATOR_THICKNESS : 0);
            int y = logicalPos.y * GRID + (s.down.y < 0 ? GRID - INDICATOR_THICKNESS : 0);
            // only draw on tiles that would be floor
            if (map[logicalPos.y][logicalPos.x] == '#') {
                DrawRectangle(x, y , w, h, BLUE);
            }
        }
        if (tickCount % TICK_RATE == 0) {
            if (!moveQueue.empty()) {
                snake.push_front(*moveQueue.begin());
                moveQueue.pop_front();
            }
            else {
                for (int i = -1; i < 3; i++) {
                    V2 nextForward = c.get(snake.begin()->forward, i);
                    V2 nextPos = snake.begin()->pos + nextForward;
                    V2 nextDown = c.get(nextForward, -1);
                    if (map[nextPos.y][nextPos.x] != '#') {
                        segment next(nextPos, nextForward, nextDown);
                        snake.push_front(next);
                        break;
                    }
                }
            }
            V2 head = snake.begin()->pos;
            // found apple, add a new segment
            if (map[head.y][head.x] == 'A') {
                snakeSize++;
                // remove apple from map
                map[head.y][head.x] = '.';
            }
            // Remove any extra segments (Every tick unless found apple)
            while (snake.size() > snakeSize) {
                snake.pop_back();
            }
        }
        if (IsKeyPressed(KEY_SPACE)) {
            V2 pos = snake.begin()->pos;
            V2 up = c.get(snake.begin()->down, 2);
            //V2 swapWall = snake.begin()->pos + c.get(snake.begin()->down, 2);
/*            if (map[swapWall.y][swapWall.x] == '#') {
                c.reverse();
                color++;
            }
            else {  //Crossing gaps using snake body*/
                bool canCross = false;
                for (int i = 1; i < snakeSize + 1; i++) {
                    V2 swapWall = pos + up * i;
                    if (map[swapWall.y][swapWall.x] == '#') {
                        canCross = true;
                        c.reverse();
                        break;
                    }
                    else {
                        moveQueue.push_back(segment(swapWall, V2(), V2()));
                    }
                }
                if (!canCross) {
                    moveQueue.clear();
                }
//            }
        }

        // debug: pause the game if we press backspace
        if (IsKeyPressed(KEY_BACKSPACE)) {
            // toggle pause
            pause = !pause;
            if (pause) {
                TICK_RATE = INT_MAX;
            } else {
                TICK_RATE = 10;
            }
        }

        EndDrawing();
        tickCount++;
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
    emscripten_set_main_loop(doEverything, 6, 1);
#else
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        everything.mainLoop();
    }
#endif
}
