#include <algorithm>
#include <cassert>
#include <climits>
#include <fstream>
#include <iostream>
#include <list>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <map>

#include "raylib.h"
#include "raymath.h"
#if defined(PLATFORM_WEB)
#include "emscripten.h"
#endif

#define WIDTH 800
#define HEIGHT 600
#define GRID 32
#define INDICATOR_THICKNESS 10
#define SLOWTICK 24
#define FASTTICK 12

#define WALL '#'
#define SNAKE 'S'
#define PATHWALL '$'
#define PATH 's'
#define EMPTY '.'
#define APPLE 'A'
#define ENEMY 'E'

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

    int hash() {
        return 10000 * y + x;
    }
};

struct compass {
    int clockwise = -1;
    V2 cardinal[4] = {V2(0, -1), V2(1, 0), V2(0, 1), V2(-1, 0)};

    void reverse() {
        clockwise *= -1;
    }

    V2 get(V2& current, int offset, int clockwiseParam = 0) {
        int index = -1;
        for (int i = 0; i < 4; i++) {
            if (cardinal[i] == current) {
                index = i;
            }
        }
        if (clockwiseParam == 0) {
            index = ((index + offset * clockwise) + 8) % 4;
        }
        else {
            index = ((index + offset * clockwiseParam) + 8) % 4;
        }
        return cardinal[index];
    }

    int cardinalToDegrees(V2& current) {
        if (current == cardinal[0]) {
            return 270;
        } else if (current == cardinal[1]) {
            return 0;
        } else if (current == cardinal[2]) {
            return 90;
        } else {
            return 180;
        }
    }
};

struct segment {
    V2 pos; //Absolute world position
    V2 forward; //Direction from previous segment to this segment
    V2 down;    //Direction from this segment to wall it's against
    int clockwise;

    segment() {}
    segment(V2 newPos, V2 newForward, V2 newDown, int newClockwise) :
        pos(newPos), forward(newForward), down(newDown), clockwise(newClockwise) {}
};

struct critter {
    compass c;
    vector<string> moveMap;
    list<segment> segments;

    critter() {}

    critter(V2 pos, vector<string>& map) {
        segment newHead;
        for (int i = 0; i < 4; i++) {
            V2 adj = pos + c.cardinal[i];
            if (map[adj.y][adj.x] == WALL) {
                newHead = segment(pos, c.get(c.cardinal[i], 1), c.cardinal[i], c.clockwise);
                makeMoveMap(adj, map);
            }
        }
        segments.push_front(newHead);
    }

    bool ok(V2 v) {
        return v.y >= 0 && v.y < moveMap.size() && v.x >= 0 && v.x < moveMap[v.y].size();
    }

    // Generate which tiles the snake can traverse (adjacent to wall)
    void makeMoveMap(V2 start, vector<string>& map) {
        moveMap = map;
        list<V2> Q;
        Q.push_back(start);
        moveMap[start.y][start.x] = PATHWALL;
        while (!Q.empty()) {
            V2 next = *Q.begin();
            Q.pop_front();
            for (int i = 0; i < 4; i++) {
                V2 adj = next + c.cardinal[i];
                if (ok(adj)) {
                    if (moveMap[adj.y][adj.x] == WALL) {
                        moveMap[adj.y][adj.x] = PATHWALL;
                        Q.push_back(adj);
                    }
                    else if (moveMap[adj.y][adj.x] != PATHWALL) {
                        moveMap[adj.y][adj.x] = PATH;
                    }
                }
            }
        }
        for (int row = 0; row < moveMap.size(); row++) {
            for (int col = 0; col < moveMap[row].size(); col++) {
                V2 pos(col, row);
                if (moveMap[pos.y][pos.x] == APPLE || moveMap[pos.y][pos.x] == EMPTY || moveMap[pos.y][pos.x] == SNAKE || moveMap[pos.y][pos.x] == ENEMY) {
                    for (V2 diag : {V2(1, 1), V2(1, -1), V2(-1, 1), V2(-1, -1)}) {
                        V2 adj = pos + diag;
                        if (ok(adj) && moveMap[adj.y][adj.x] == PATHWALL) {
                            moveMap[pos.y][pos.x] = PATH;
                        }
                    }
                }
            }
        }
    }

    segment getNextSegment() {
        int score = -1;
        segment next;
        for (int i = -1; i < 3; i++) {
            // currentForward = entrance direction of previous tile
            // nextForward = exit direction of previous tile = entrance direction of new tile
            V2 currentForward = segments.begin()->forward;
            V2 nextForward = c.get(currentForward, i);
            V2 nextDown = c.get(nextForward, -1);
            V2 nextPos = segments.begin()->pos + nextForward;
            V2 nextWall = nextPos + nextDown;
            if (moveMap[nextPos.y][nextPos.x] == PATH) {
                // +8 points for not going around the path the wrong way
                int newScore = moveMap[nextWall.y][nextWall.x] == EMPTY ? 0 : 8;
                // +4 points for not going backwards
                newScore += i != 2 ? 4 : 0;
                // +2 points for not overlapping previous snake
                newScore += 2;
                for (segment& otherS : segments) {
                    if (otherS.pos == nextPos) {
                        newScore -= 2;
                        break;
                    }
                }
                // +1 points for adhering to wall
                newScore += moveMap[nextWall.y][nextWall.x] == PATHWALL ? 1 : 0;
                if (newScore > score) {
                    next = segment(nextPos, nextForward, nextDown, c.clockwise);
                    score = newScore;
                }
            }
        }
        return next;
    }

    virtual void render(bool debug) {}
};


struct snake : public critter {
    list<segment> moveQueue;
    unordered_map<string, Texture2D> textures;
    int snakeSize = 1;

    void initTextures() {
        for (const string& name : {"head_0", "head_1", "body", "tail", "tail_inside_corner", "tail_outside_corner", "tail_u_turn", "body_inside_corner", "body_outside_corner", "body_u_turn"}) {
            stringstream filename;
            filename << "assets/" << name << ".png";
            Image img = LoadImage(filename.str().c_str());
            ImageResize(&img, GRID, GRID);
            Texture2D tex = LoadTextureFromImage(img);
            textures[name] = tex;
            UnloadImage(img);
        }
    }

    snake() {}

    snake(V2 head, vector<string>& map) : critter(head, map) {
        initTextures();
    }

    void doTick(vector<string>& map) {
        //Snake movement: Wall following
        if (!moveQueue.empty()) {
            //Move queue is filled when we start crossing a gap
            segments.push_front(*moveQueue.begin());
            moveQueue.pop_front();
        }
        else {  //Wall following
            segment next = getNextSegment();
            segments.push_front(next);
        }
        V2 head = segments.begin()->pos;
        if (map[head.y][head.x] == APPLE) {
            snakeSize++;
        }
        map[head.y][head.x] = SNAKE;
        while (segments.size() > snakeSize) {
            V2 tail = segments.rbegin()->pos;
            map[tail.y][tail.x] = EMPTY;
            segments.pop_back();
        }
    }

    void handleInput(vector<string>& map) {
        V2 head = segments.begin()->pos;
        if (IsKeyPressed(KEY_SPACE) && moveQueue.empty()) {
            //Crossing to opposite wall
            V2 up = c.get(segments.begin()->down, 2);
            bool canCross = false;
            for (int i = 1; i < snakeSize + 1; i++) {
                V2 swapWall = head + up * i;
                for (segment& s : segments) {
                    if (s.pos == swapWall) {
                        break;
                    }
                }
                if (map[swapWall.y][swapWall.x] == WALL) {
                    canCross = true;
                    makeMoveMap(swapWall, map);
                    //Following opposite wall now
                    c.reverse();
                    break;
                }
                else {
                    segment next(swapWall, up, up, c.clockwise);
                    moveQueue.push_back(next);
                }
            }
            if (!canCross) {
                moveQueue.clear();
            }
        }
    }

    int countSidesTouching(segment& s) {
        int sidesTouching = 0;
        // make sure 
        bool forward = false;
        int i = 0;
        for (const V2& adj : {s.down, s.forward, {-s.down.x, -s.down.y}}) {
            if ((adj.x == 0) && (adj.y == 0)) continue;

            V2 logicalPos = s.pos + adj;
            if (moveMap[logicalPos.y][logicalPos.x] == PATHWALL) {
                if (i == 1) { forward = true; }
                // don't count opposite sides if forward was not counted
                if (i == 2 && !forward) { break; }
                sidesTouching++;
            }
            i++;
        }
        return sidesTouching;
    }

    void render(bool debug) {
        if (debug) {
            for (int row = 0; row < moveMap.size(); row++) {
                for (int col = 0; col < moveMap[row].size(); col++) {
                    if (moveMap[row][col] == PATHWALL) {
                        DrawRectangle(GRID * col, GRID * row, GRID, GRID, BLUE);
                    }
                    else if (moveMap[row][col] == PATH) {
                        DrawRectangle(GRID * col, GRID * row, GRID, GRID, Fade(GREEN, 0.5));
                    }
                }
            }
        }
        int off = 0;
        for (auto segIter = segments.rbegin(); segIter != segments.rend(); segIter++) {
            //draw sluggo
            segment& s = *segIter;
            auto segIter2 = segIter;
            segIter2++;
            segment& s2 = *segIter2;
            Texture2D* tex = nullptr;
            int sidesTouching = countSidesTouching(s);
            if (&s == &(segments.front())) {
                tex = snakeSize > 1 ? &textures["head_1"] : &textures["head_0"];
            } else if (&s == &(segments.back())) {
                if (s2.forward == c.get(s.forward, -1, s.clockwise)) {
                    tex = &textures["tail_outside_corner"];
                }
                else if (s2.forward == c.get(s.forward, 0, s.clockwise)) {
                    tex = &textures["tail"];
                }
                else if (s2.forward == c.get(s.forward, 1, s.clockwise)) {
                    tex = &textures["tail_inside_corner"];
                }
                else if (s2.forward == c.get(s.forward, 2, s.clockwise)) {
                    tex = &textures["tail_u_turn"];
                }
                else {
                    cerr << "Nonexistant angle\n";
                }
/*                // check which tail segment we should be using
                if (sidesTouching == 0) {
                    tex = &textures["tail_outside_corner"];
                } else if (sidesTouching == 2) {
                    tex = &textures["tail_inside_corner"];
                } else if (sidesTouching == 3) {
                    tex = &textures["tail_u_turn"];
                } else {
                    tex = &textures["tail"];
                }*/
            } else {
                // check which body segment we should be using
                if (s2.forward == c.get(s.forward, -1, s.clockwise)) {
                    tex = &textures["body_outside_corner"];
                }
                else if (s2.forward == c.get(s.forward, 0, s.clockwise)) {
                    tex = &textures["body"];
                }
                else if (s2.forward == c.get(s.forward, 1, s.clockwise)) {
                    tex = &textures["body_inside_corner"];
                }
                else if (s2.forward == c.get(s.forward, 2, s.clockwise)) {
                    tex = &textures["body_u_turn"];
                }
                else {
                    cerr << "Nonexistant tex" << s2.forward.x << " " << s2.forward.y << "\n";
                }
/*
                if (sidesTouching == 0) {
                    tex = &textures["body_outside_corner"];
                } else if (sidesTouching == 2) {
                    tex = &textures["body_inside_corner"];
                } else if (sidesTouching == 3) {
                    tex = &textures["body_u_turn"];
                } else {
                    tex = &textures["body"];
                }*/
            }
            Vector2 center = {s.pos.x * GRID + tex->width/2, s.pos.y * GRID + tex->width/2};
            int rotation = c.cardinalToDegrees(s.forward);
            Rectangle sourceRec = {0, 0, tex->width, tex->height * -1 * s.clockwise};
            Rectangle destRec = {center.x, center.y, tex->width, tex->height};

            DrawTexturePro(*tex, sourceRec, destRec, { tex->width/2, tex->height/2 }, rotation, WHITE);

            // draw debug overlay
            if (debug) {
                Vector2 down = Vector2Add(center, Vector2Scale((Vector2){s.down.x, s.down.y}, GRID));
                DrawLineV(center, down, GREEN);
                Vector2 forward = Vector2Add(center, Vector2Scale((Vector2){s.forward.x, s.forward.y}, GRID));
                DrawLineV(center, forward, RED);
                DrawText(TextFormat("segment %i: sides touching: %i", off, sidesTouching), 10, 20*(off++), 20, RED);
                // draw slime
                for (V2 adj : {s.down, s.forward}) {
                    if ((adj.x == 0) && (adj.y == 0)) continue;
                    int w = adj.x != 0 ? INDICATOR_THICKNESS : GRID;
                    int h = adj.y != 0 ? INDICATOR_THICKNESS : GRID;

                    V2 logicalPos = s.pos + adj;
                    // draw the indicator on the inside of the wall we are on
                    int x = logicalPos.x * GRID + (adj.x < 0 ? GRID - INDICATOR_THICKNESS : 0);
                    int y = logicalPos.y * GRID + (adj.y < 0 ? GRID - INDICATOR_THICKNESS : 0);
                    if (moveMap[logicalPos.y][logicalPos.x] == PATHWALL) {
                        DrawRectangle(x, y , w, h, YELLOW);
                    }
                }
            }

        }
    }

    V2 head() {
        return segments.begin()->pos;
    }

};

struct spider : public critter {

    Texture2D tex;
    void initTextures() {
        tex = LoadTexture("assets/exam.png");
    }

    spider(V2 pos, vector<string>& map) : critter(pos, map) {
        initTextures();
        segments.push_front(getNextSegment());
    }

    void doTick(vector<string>& map) {
        //Spider has 2 segments (to prevent passing through length-1 snake.)
        // Check if either of those segments touching snake.
        V2 head = segments.begin()->pos;
        V2 tail = segments.rbegin()->pos;
        if (map[head.y][head.x] == SNAKE || map[tail.y][tail.x] == SNAKE) {
            cout << "You got caught by a spider!\n";
            exit(0);
        }
        // Use DFS to search for snake markings
        list<V2> Q;
        Q.push_back(segments.begin()->pos);
        unordered_map<int, V2> parents;
        while (!Q.empty()) {
            V2 next = *Q.begin();
            Q.pop_front();
            for (int i = 0; i < 4; i++) {
                V2 adj = next + c.cardinal[i];
                if (ok(adj) && moveMap[adj.y][adj.x] == PATH && parents.count(adj.hash()) == 0) {
                    parents[adj.hash()] = next;
                    Q.push_back(adj);
                    if (map[adj.y][adj.x] == SNAKE) {
                        while (parents[next.hash()] != head && next != head) {
                            next = parents[next.hash()];
                        }
                        segments.begin()->forward = next - head;
                        segment next = getNextSegment();
                        segments.push_front(next);
                        segments.pop_back();
                    }
                }
            }
        }
    }

    void render(bool debug) {
        V2 head = segments.begin()->pos;
        /* DrawCircle((head.x + 0.5) * GRID, (head.y + 0.5) * GRID, 0.5 * GRID, PURPLE); */
        DrawTexture(tex, (head.x+0.5)*GRID-tex.width/2, (head.y+0.5)*GRID-tex.height/2,  WHITE);
        if (debug) {
            for (int row = 0; row < moveMap.size(); row++) {
                for (int col = 0; col < moveMap[row].size(); col++) {
                    if (moveMap[row][col] == PATH) {
                        DrawRectangle(GRID * col, GRID * row, GRID, GRID, Fade(PURPLE, 0.5));
                    }
                }
            }
        }
    }
};

struct mainData {

    vector<string> map;
    list<spider> spiders;
    int mapWidth = 0;
    int tickCount = 0;
    bool pause = false;
    RenderTexture2D canvas;
    Vector2 camera = {0, 0};
    bool moveCameraX, moveCameraY;
    snake s;
    Texture2D dirt;
    Texture2D dirtHorizontal;
    Texture2D yerb;
    int totalApples = 0;

    char& at(V2 v) {
        return map[v.y][v.x];
    }

    // update rate for game logic is 60fps/tickRate()
    int tickRate() {
        if (pause) { return INT_MAX; }
        else if (IsKeyDown(KEY_LEFT_SHIFT)) { return FASTTICK; }
        else { return SLOWTICK; }
    }

    void readLevel(string levelName) {
        ifstream level(levelName);
        if (!level) {
            cerr << "Couldn't open " << levelName << endl;
            exit(EXIT_FAILURE);
        }
        string line;
        V2 newSnakeHead;
        list<V2> newSpiders;
        while (getline(level, line)) {
            mapWidth = max((int)line.size(), mapWidth);
            for (int i = 0; i < line.size(); i++) {
                if (line[i] == SNAKE) {
                    newSnakeHead = V2(i, map.size());
                }
                else if (line[i] == ENEMY) {
                    newSpiders.push_back(V2(i, map.size()));
                }
                else if (line[i] == APPLE) {
                    totalApples++;
                }
            }
            map.push_back(line);
        }
        level.close();
        canvas = LoadRenderTexture(mapWidth * GRID, map.size() * GRID);
        moveCameraX = mapWidth * GRID > WIDTH;
        moveCameraY = map.size() * GRID > HEIGHT;
        s = snake(newSnakeHead, map);
        for (V2& pos : newSpiders) {
            spiders.push_back(spider(pos, map));
        }
        dirt = LoadTexture("assets/dirt.png");
        dirtHorizontal = LoadTexture("assets/dirt_horizontal.png");
        yerb = LoadTexture("assets/yerb.png");
    }

    void render(bool debug) {
        BeginTextureMode(canvas);
        ClearBackground(BLACK);
        // draw map
        Rectangle background = {96, 64, 32, 32};
        for (int row = 0; row < map.size(); row++) {
            for (int col = 0; col < map[row].size(); col++) {
                Vector2 dest = {col * GRID, row * GRID};
                DrawTextureRec(dirt, background, dest, WHITE);
                Rectangle source;
                Texture2D* tex = &dirt;
                V2 pos(col, row);
                if (at(pos) == WALL) {
                    int openAdjCount = 0;
                    V2 sourceTile(1, 1);
                    for (V2 adj : {V2(-1, 0), V2(1, 0), V2(0, -1), V2(0, 1)}) {
                        if (s.ok(pos + adj) && (at(pos + adj) == EMPTY || at(pos + adj) == SNAKE || at(pos + adj) == APPLE || at(pos + adj) == ENEMY)) {
                            sourceTile = sourceTile + adj;
                            openAdjCount++;
                        }
                    }
                    if (openAdjCount == 4) {
                        source = {128, 0, 32, 32};
                    }
                    else if (openAdjCount == 3) {
                        if (map[row + 1][col] == WALL) {
                            source = {128, 0, 32, 32};
                        }
                        else if (map[row - 1][col] == WALL) {
                            source = {128, 64, 32, 32};
                        }
                        else if (map[row][col + 1] == WALL) {
                            source = {0, 0, 32, 32};
                            tex = &dirtHorizontal;
                        }
                        else if (map[row][col - 1] == WALL) {
                            source = {64, 0, 32, 32};
                            tex = &dirtHorizontal;
                        }
                    }
                    else if (openAdjCount == 2 && map[row][col + 1] == WALL && map[row][col - 1] == WALL) {
                        source = {32, 0, 32, 32};
                        tex = &dirtHorizontal;
                    }
                    else if (openAdjCount == 2 && map[row + 1][col] == WALL && map[row - 1][col] == WALL) {
                        source = {128, 32, 32, 32};
                    }
                    else {
                        source = (Rectangle){32 * sourceTile.x, 32 * sourceTile.y, 32, 32};
                    }
                    DrawTextureRec(*tex, source, dest, WHITE);
                }
                else if (at(pos) == APPLE) {
                    DrawTexture(yerb, (col+0.5)*GRID-yerb.width/2, (row+0.5)*GRID-yerb.height/2,  WHITE);
                }
            }
        }
        // draw snake
        s.render(debug);
        for (spider& enemy : spiders) {
            enemy.render(debug);
        }
        if (s.snakeSize == totalApples + 1) {
            DrawRectangle(0, 0, WIDTH, HEIGHT, (Color){0, 0, 0, 100});
            DrawText("You got all the yerbs.\nYou won!", GRID, GRID, 1.3 * GRID, WHITE);
        }
        EndTextureMode();
    }

    void mainLoop() {
        BeginDrawing();
        //DO THE FOLLOWING AT TICK RATE
        if (tickCount % tickRate() == 0) {
            if (s.snakeSize != totalApples + 1) {
                for (spider& enemy : spiders) {
                    enemy.doTick(map);
                }
                s.doTick(map);
            }
            render(false);
        }
        //DO THE FOLLOWING AT 60FPS
        s.handleInput(map);
        // debug: pause the game if we press backspace
        if (IsKeyPressed(KEY_BACKSPACE)) {
            // toggle pause
            pause = !pause;
            render(true);
        }
        //Update camera position to keep snake head near center of screen
        Vector2 targetCamera = camera;
        if (moveCameraX) {
            targetCamera.x = min(mapWidth * GRID - WIDTH, max(0, int((s.head().x + 0.5) * GRID - WIDTH / 2)));
        }
        if (moveCameraY) {
            targetCamera.y = min((int)map.size() * GRID - HEIGHT, max(0, int((s.head().y + 0.5) * GRID - HEIGHT / 2)));
        }
        Vector2 cameraMove = Vector2Subtract(targetCamera, camera);
        camera = Vector2Add(camera, Vector2Scale(cameraMove, 0.02));
        ClearBackground(BLACK);
        Texture* t = &canvas.texture;
        Rectangle source = {0, 0, (float)t->width, -1 * (float)t->height};
        Rectangle dest = {0, 0, t->width, t->height};
        float rotation = 0;
        DrawTexturePro(canvas.texture, source, dest, camera, rotation, WHITE);

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

    InitWindow(WIDTH, HEIGHT, "snacman");
    everything.readLevel(argv[1]);
#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(doEverything, 6, 1);
#else
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        everything.mainLoop();
    }
#endif
}
