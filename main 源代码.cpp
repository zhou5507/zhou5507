#include "D:\\mingw64\\include\\graphics.h" // EGE图形库，用于创建图形界面、绘制图形、处理鼠标键盘事件
#include <string>                           // C++标准字符串库，提供string类用于字符串操作
#include <stack>                            // 栈容器库，用于保存游戏历史状态实现悔棋功能
#include <algorithm>                        // 算法库，提供sort等排序算法用于AI走法排序
#include <iostream>                         // 输入输出流库，用于控制台信息输出
#include <fstream>                          // 文件流库，用于游戏存档的读取和保存
#include <vector>                           // 动态数组容器库，用于存储棋盘、走法列表等
#include <random>                           // 随机数库，用于随机数生成
#include <utility>                          // 工具库，提供pair等数据结构用于存储坐标对
#include <windows.h>                        // Windows API库，用于控制台操作、消息框等系统功能
#include <cmath>                            // 数学函数库，提供abs、pow、min、max等数学运算
#include <cstring>                          // C字符串处理库，提供memset、strcpy等函数
#include <ctime>                            // 时间库，用于AI计时控制和随机种子初始化
#include <unordered_map>                    // 哈希表容器库，提供O(1)查找的键值对存储

using namespace std;
#pragma execution_character_set("utf-8")//确保代码中的中文字符串以 UTF-8 编码编译到程序中，避免因编码不一致导致的中文乱码问题。

// 全局图像缓冲区
IMAGE *c_buffer = nullptr;
IMAGE *img_menu1 = nullptr;
IMAGE *img_menu2 = nullptr;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 棋盘和窗口常量
const int AMAZONS_SIZE = 8;
const int WINDOW_WIDTH = 1000;
const int WINDOW_HEIGHT = 800;
const int CELL_SIZE = 70;
const int HALF_CELL = CELL_SIZE / 2;
const int BOARD_X_START = 150;
const int BOARD_Y_START = 150;
const int FORCE_CENTER_FIX = -30;

// 棋子类型常量
const int EMPTY = 0;
const int QUEEN_BLACK = 1;
const int QUEEN_WHITE = 2;
const int ARROW = 3;

const string SAVE_FILE = "amazons_data.dat";

// AI移动方向
const int dx[] = {-1, -1, -1, 0, 0, 1, 1, 1};
const int dy[] = {-1, 0, 1, -1, 1, -1, 0, 1};

// AI评估参数
double K = 0.15;
double f1[32] = {0.0000, 0.1200, 0.1200, 0.1400, 0.1500, 0.1600,
                 0.1700, 0.1800, 0.2000, 0.2200, 0.2500,
                 0.2800, 0.3000, 0.3200, 0.3400, 0.3600,
                 0.3800, 0.4000, 0.4500, 0.5000, 0.5800,
                 0.6500, 0.7200, 0.8000, 1.0000, 1.0000,
                 1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000};

// AI走法结构体
struct AIMove
{
    int x0, y0, x1, y1, x2, y2;
    double value;
};

// 清空控制台
void ClearConsole()
{
    COORD coordScreen = {0, 0};
    DWORD cCharsWritten;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD dwConSize;
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi))
        return;
    dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
    FillConsoleOutputCharacter(hConsole, (TCHAR)' ', dwConSize, coordScreen, &cCharsWritten);
    GetConsoleScreenBufferInfo(hConsole, &csbi);
    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten);
    SetConsoleCursorPosition(hConsole, coordScreen);
}

// 页面枚举
enum struct Page
{
    Menu1,
    Menu2,
    Menu_Exit,
    Board,
    Pause,
    Menu_Win,
};

// 游戏状态枚举
enum struct GameState
{
    STATE_SELECT_QUEEN,
    STATE_MOVE_QUEEN,
    STATE_SHOOT_ARROW
};

// 全局变量
vector<vector<int>> Brd(AMAZONS_SIZE + 2, vector<int>(AMAZONS_SIZE + 2, 0));
int Mode{0}, rnd{1}, result{0};
Page currentPage = Page::Menu1;
GameState currentGameState = GameState::STATE_SELECT_QUEEN;
int selected_i = -1, selected_j = -1;
int winner = EMPTY;

// 游戏状态快照结构体
struct GameStateSnapshot
{
    vector<vector<int>> board;
    int round;
    int mode;
};
stack<GameStateSnapshot> history;

// 边界检查宏
#define inBounds(x, y) (x >= 1 && x <= AMAZONS_SIZE && y >= 1 && y <= AMAZONS_SIZE)
#define IN_BUTTON(msg, bx, by, w, h) \
    ((msg).x >= (bx) && (msg).x <= (bx) + (w) && (msg).y >= (by) && (msg).y <= (by) + (h))

// 函数声明
void putrnd(int r);
void putplayer(int r);
void initAmazonsBoard();
void saveState();
string getSaveFilePath();

string getSaveFilePath()
{
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    string fullPath(path);
    size_t pos = fullPath.find_last_of("\\/");
    if (pos != string::npos)
    {
        return fullPath.substr(0, pos + 1) + "amazons_data.dat";
    }
    return "amazons_data.dat";
}
// 保存游戏
void saveGame()
{
    // 如果游戏已结束，不保存（避免保存无效状态）
    if (winner != EMPTY)
    {
        ClearConsole();
        cout << "游戏已结束，无需保存。" << endl;
        return;
    }
    
    string saveFilePath = getSaveFilePath();
    ofstream ofs(saveFilePath, ios::binary);
    if (!ofs)
    {
        MessageBox(getHWnd(), "保存失败！无法创建存档文件。", "错误", MB_OK | MB_ICONERROR);
        return;
    }
    ofs.write((char *)&Mode, sizeof(Mode));
    ofs.write((char *)&rnd, sizeof(rnd));
    for (int i = 0; i < AMAZONS_SIZE + 2; i++)
    {
        for (int j = 0; j < AMAZONS_SIZE + 2; j++)
        {
            ofs.write((char *)&Brd[i][j], sizeof(int));
        }
    }
    ofs.close();
    ClearConsole();
    cout << "进度已保存" << endl;
    cout << "存档路径: " << saveFilePath << endl;
    cout << "当前回合: " << rnd << endl;
}

// 加载游戏
bool loadGame()
{
    string saveFilePath = getSaveFilePath();
    ifstream ifs(saveFilePath, ios::binary);
    if (!ifs)
        return false;
    
    // 检查文件大小是否正确
    ifs.seekg(0, ios::end);
    streamsize fileSize = ifs.tellg();
    streamsize expectedSize = sizeof(Mode) + sizeof(rnd) + 
                              (AMAZONS_SIZE + 2) * (AMAZONS_SIZE + 2) * sizeof(int);
    if (fileSize != expectedSize)
    {
        ifs.close();
        return false;  // 文件损坏
    }
    ifs.seekg(0, ios::beg);
    
    ifs.read((char *)&Mode, sizeof(Mode));
    ifs.read((char *)&rnd, sizeof(rnd));
    for (int i = 0; i < AMAZONS_SIZE + 2; i++)
    {
        for (int j = 0; j < AMAZONS_SIZE + 2; j++)
        {
            ifs.read((char *)&Brd[i][j], sizeof(int));
        }
    }
    ifs.close();
    winner = EMPTY;
    currentGameState = GameState::STATE_SELECT_QUEEN;
    selected_i = -1;
    selected_j = -1;
    while (!history.empty())
        history.pop();
    ClearConsole();
    cout << "存档已读取" << endl;
    cout << "存档路径: " << saveFilePath << endl;
    cout << "模式: " << (Mode == 1 ? "单人" : "双人") << " | 回合: " << rnd << endl;
    return true;
}

// 字符串转换辅助结构
struct String : public std::string
{
    using std::string::string;
    operator LPCSTR()
    {
        return c_str();
    }
};

// 宽字符转ANSI
String operator""_ansi(const wchar_t *src, std::size_t len)
{
    if (len == 0)
        return {};
    int bufferSize =
        WideCharToMultiByte(CP_ACP, 0, src, len, nullptr, 0, nullptr, nullptr);
    String result(bufferSize, 0);
    WideCharToMultiByte(CP_ACP, 0, src, len, (LPSTR)result.data(), bufferSize, nullptr,
                        nullptr);
    return result;
}

// 绘制按钮
void Button(const wchar_t *title, int x, int y, int w, int h, int FONT_SIZE)
{
    setfont(FONT_SIZE, 0, L"锐字潮牌驰光黑-闪 中粗"_ansi);
    int tw = textwidth(title);
    int th = textheight(title);
    int yOffset = (h - th) / 2;
    int wid = textwidth(">>");
    int xOffset = (w - tw - wid) / 2;
    struct
    {
        int x, y;
    } mouse{};
    setbkmode(TRANSPARENT);
    setcolor(EGERGB(139, 69, 19));
    mousepos(&mouse.x, &mouse.y);
    if (IN_BUTTON(mouse, x, y, w, h))
    {
        setcolor(EGERGB(255, 0, 0));
        outtextxy(x + xOffset, y + yOffset, ">>");
    }
    setcolor(EGERGB(139, 69, 19));
    outtextxy(x + xOffset + wid, y + yOffset, title);
    setcolor(EGERGB(139, 69, 19));
    setlinewidth(2);
    rectangle(x, y, x + w, y + h);
}

// 绘制居中按钮
void ButtonCentered(const wchar_t *title, int x, int y, int w, int h, int FONT_SIZE)
{
    setfont(FONT_SIZE, 0, L"隶书"_ansi);
    int tw = textwidth(title);
    int th = textheight(title);
    int xOffset = (w - tw) / 2;
    int yOffset = (h - th) / 2;
    setbkmode(TRANSPARENT);
    struct
    {
        int x, y;
    } mouse{};
    mousepos(&mouse.x, &mouse.y);
    if (IN_BUTTON(mouse, x, y, w, h))
    {
        setcolor(EGERGB(255, 0, 0));
    }
    else
    {
        setcolor(EGERGB(139, 69, 19));
    }
    outtextxy(x + xOffset, y + yOffset, title);
    setcolor(EGERGB(139, 69, 19));
    setlinewidth(2);
    rectangle(x, y, x + w, y + h);
}

// 显示回合数
void putrnd(int r)
{
    int base_y = 30;
    const int X_START = 720;
    setfont(55, 0, L"锐字潮牌驰光黑-闪 中粗"_ansi);
    setcolor(EGERGB(139, 69, 19));
    outtextxy(X_START, base_y, L"第");
    int wid_di = textwidth(L"第");
    setfont(65, 0, L"锐字潮牌驰光黑-闪 中粗"_ansi);
    setcolor(EGERGB(139, 58, 58));
    string r_str = std::to_string(r);
    int x_start_num = X_START + wid_di + 10;
    for (size_t i = 0; i < r_str.length(); ++i)
    {
        outtextxy(x_start_num + i * 27, base_y - 7.5, r_str.substr(i, 1).c_str());
    }
    int x_start_round = x_start_num + r_str.length() * 27 + 5;
    setfont(55, 0, L"锐字潮牌驰光黑-闪 中粗"_ansi);
    setcolor(EGERGB(139, 69, 19));
    outtextxy(x_start_round, base_y, L"回合");
}

// 显示当前玩家
void putplayer(int r)
{
    int base_y = 150;
    setfont(30, 0, L"锐字潮牌驰光黑-闪 中粗"_ansi);
    setcolor(EGERGB(139, 69, 19));
    outtextxy(780, base_y, L"当前落子：");
    int x_current = 780 + textwidth(L"当前落子：");
    setfont(30, 0, L"锐字潮牌驰光黑-闪 中粗"_ansi);
    const wchar_t *player_text;
    if (r % 2 == 1)
    {
        player_text = L"白方 (玩家)";
    }
    else
    {
        player_text = (Mode == 1) ? L"黑方 (AI)" : L"黑方 (玩家)";
    }
    outtextxy(x_current, base_y, player_text);
}

// 绘制主菜单
void drawMenu1()
{
    cleardevice();
    setfillcolor(EGERGB(255, 255, 255));
    bar(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    int areaX = 40, areaY = 100, areaW = 380, areaH = 650;
    setbkmode(TRANSPARENT);

    // 英文名言
    setfont(24, 0, "Lucida Calligraphy");
    setcolor(EGERGB(70, 70, 70));
    outtextxy(areaX + 10, areaY, "The beauty of a move");
    outtextxy(areaX + 10, areaY + 35, "lies not in its appearance,");
    outtextxy(areaX + 10, areaY + 70, "but in the thought behind it.");

    // 中文名言
    setfont(32, 0, L"华文行楷"_ansi);
    setcolor(EGERGB(0, 0, 0));
    outtextxy(areaX + 10, areaY + 150, L"落子之美，不在其表，");
    outtextxy(areaX + 10, areaY + 200, L"而在其后的深谋远虑。");

    // 左侧图片
    if (img_menu1 != nullptr)
    {
        int imgY = areaY + 300;
        int imgW = areaW - 20;
        int imgH = 200;
        setcolor(EGERGB(235, 235, 235));
        rectangle(areaX + 9, imgY - 1, areaX + 11 + imgW, imgY + imgH + 1);
        putimage(areaX + 10, imgY, imgW, imgH,
                 img_menu1, 0, 0, getwidth(img_menu1), getheight(img_menu1));
    }

    // 右侧标题
    color_t titleColor = EGERGB(44, 62, 80);
    setcolor(titleColor);
    setfont(110, 0, "Algerian");
    outtextxy(500, 100, "AMAZONS");
    setcolor(EGERGB(80, 100, 120));
    setfont(45, 0, L"华文行楷"_ansi);
    outtextxy(580, 210, L"—— 亚马逊棋");

    // 按钮
    Button(L"新游戏", 620, 350, 210, 65, 30);
    Button(L"原有存档", 620, 450, 210, 65, 30);
    Button(L"结束游戏", 620, 550, 210, 65, 30);
}

// 绘制模式选择菜单
void drawMenu2()
{
    cleardevice();
    setfillcolor(EGERGB(255, 255, 255));
    bar(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    int leftX = 80;
    int leftY = 60;
    int imgSide = 380;

    // 左侧图片
    if (img_menu2 != nullptr)
    {
        putimage(leftX, leftY, imgSide, imgSide, img_menu2, 0, 0, getwidth(img_menu2), getheight(img_menu2));
    }

    // 游戏简介
    int textX = leftX + 15;
    int textY = leftY + imgSide + 20;
    setcolor(EGERGB(0, 0, 0));
    setfont(26, 0, L"华文行楷"_ansi);
    outtextxy(textX, textY, L"Amazons简介:");
    setfont(20, 0, L"华文行楷"_ansi);
    int lineH = 28;
    int contentY = textY + 40;
    outtextxy(textX, contentY, L"双方各持四子，每回合先将一子按国际皇后走法");
    outtextxy(textX, contentY + lineH, L"（沿横、竖、斜移动任意格）移至空位，再从该子");
    outtextxy(textX, contentY + lineH * 2, L"新落点出发，按同样走法向任一可见空位放置障碍。");
    outtextxy(textX, contentY + lineH * 3, L"移动与放置障碍均不可跨越或停留在已有棋子或");
    outtextxy(textX, contentY + lineH * 4, L"障碍上。随着空间不断被封锁，最后导致无棋可");
    outtextxy(textX, contentY + lineH * 5, L"走的一方即为负方。");

    // 右侧标题和按钮
    setcolor(EGERGB(44, 62, 80));
    setfont(110, 0, "Algerian");
    outtextxy(600, 100, "MODE");
    setcolor(EGERGB(80, 100, 120));
    setfont(35, 0, L"华文行楷"_ansi);
    outtextxy(640, 210, L"选择游戏模式");
    Button(L"双人游戏", 650, 350, 230, 65, 30);
    Button(L"单人游戏", 650, 460, 230, 65, 30);
    Button(L"返回主菜单", 650, 570, 230, 65, 30);
}

// 检查移动是否合法
bool canMove(int r1, int c1, int r2, int c2, int piece_type = EMPTY)
{
    if (!inBounds(r1, c1) || !inBounds(r2, c2) || (r1 == r2 && c1 == c2))
    {
        return false;
    }
    int dr = r2 - r1;
    int dc = c2 - c1;
    if (abs(dr) != abs(dc) && dr != 0 && dc != 0)
    {
        return false;
    }
    int step_r = (dr == 0) ? 0 : (dr > 0 ? 1 : -1);
    int step_c = (dc == 0) ? 0 : (dc > 0 ? 1 : -1);
    int curr_r = r1 + step_r;
    int curr_c = c1 + step_c;
    while (curr_r != r2 || curr_c != c2)
    {
        if (Brd[curr_r][curr_c] != EMPTY && Brd[curr_r][curr_c] != piece_type)
        {
            return false;
        }
        curr_r += step_r;
        curr_c += step_c;
    }
    if (Brd[r2][c2] != EMPTY && Brd[r2][c2] != piece_type)
    {
        return false;
    }
    return true;
}

// 获取可移动位置
vector<pair<int, int>> getValidMoves(int r1, int c1)
{
    vector<pair<int, int>> moves;
    for (int k = 0; k < 8; k++)
    {
        for (int delta = 1; delta <= AMAZONS_SIZE; delta++)
        {
            int r2 = r1 + dx[k] * delta;
            int c2 = c1 + dy[k] * delta;
            if (!inBounds(r2, c2))
                break;
            if (Brd[r2][c2] != EMPTY)
                break;
            moves.push_back({r2, c2});
        }
    }
    return moves;
}

// 检查是否有合法走法
bool hasLegalMove(int pieceType)
{
    for (int r1 = 1; r1 <= AMAZONS_SIZE; ++r1)
    {
        for (int c1 = 1; c1 <= AMAZONS_SIZE; ++c1)
        {
            if (Brd[r1][c1] != pieceType)
                continue;
            for (int r2 = 1; r2 <= AMAZONS_SIZE; ++r2)
            {
                for (int c2 = 1; c2 <= AMAZONS_SIZE; ++c2)
                {
                    if (r1 == r2 && c1 == c2)
                        continue;
                    if (Brd[r2][c2] != EMPTY)
                        continue;
                    if (canMove(r1, c1, r2, c2))
                    {
                        Brd[r1][c1] = EMPTY;
                        Brd[r2][c2] = pieceType;
                        for (int r3 = 1; r3 <= AMAZONS_SIZE; ++r3)
                        {
                            for (int c3 = 1; c3 <= AMAZONS_SIZE; ++c3)
                            {
                                if (canMove(r2, c2, r3, c3, (r3 == r1 && c3 == c1) ? pieceType : EMPTY))
                                {
                                    Brd[r1][c1] = pieceType;
                                    Brd[r2][c2] = EMPTY;
                                    return true;
                                }
                            }
                        }
                        Brd[r1][c1] = pieceType;
                        Brd[r2][c2] = EMPTY;
                    }
                }
            }
        }
    }
    return false;
}

// 检查胜负条件
void checkWinCondition()
{
    int currentPlayerPiece = (rnd % 2 == 1) ? QUEEN_WHITE : QUEEN_BLACK;
    int opponentPiece = (rnd % 2 == 1) ? QUEEN_BLACK : QUEEN_WHITE;
    if (!hasLegalMove(currentPlayerPiece))
    {
        winner = opponentPiece;
        currentGameState = GameState::STATE_SELECT_QUEEN;
        cout << "游戏结束 " << ((winner == QUEEN_WHITE) ? "白方" : "黑方") << "获胜！" << endl;
    }
}

// 计算距离场
void calcDistanceField(int distField[AMAZONS_SIZE][AMAZONS_SIZE], int pieceType, bool kingMoveOnly)
{
    for (int i = 0; i < AMAZONS_SIZE; i++)
        for (int j = 0; j < AMAZONS_SIZE; j++)
            distField[i][j] = 100;
    vector<pair<int, int>> queue;
    for (int i = 1; i <= AMAZONS_SIZE; i++)
    {
        for (int j = 1; j <= AMAZONS_SIZE; j++)
        {
            if (Brd[i][j] == pieceType)
            {
                distField[i - 1][j - 1] = 0;
                queue.push_back({i - 1, j - 1});
            }
        }
    }
    int pace = 0;
    while (!queue.empty())
    {
        pace++;
        vector<pair<int, int>> nextQueue;
        for (auto &pos : queue)
        {
            int x = pos.first, y = pos.second;
            for (int k = 0; k < 8; k++)
            {
                int maxDelta = kingMoveOnly ? 2 : AMAZONS_SIZE;
                for (int delta = 1; delta < maxDelta; delta++)
                {
                    int xx = x + dx[k] * delta;
                    int yy = y + dy[k] * delta;
                    if (xx < 0 || xx >= AMAZONS_SIZE || yy < 0 || yy >= AMAZONS_SIZE)
                        break;
                    if (Brd[xx + 1][yy + 1] != EMPTY)
                        break;
                    if (distField[xx][yy] != 100)
                        continue;
                    distField[xx][yy] = pace;
                    nextQueue.push_back({xx, yy});
                }
            }
        }
        queue = nextQueue;
    }
}

// 计算灵活度
double calcMobility(int x, int y)
{
    double sum = 0;
    int moveCount = 0;
    for (int k = 0; k < 8; k++)
    {
        for (int delta = 1; delta < AMAZONS_SIZE; delta++)
        {
            int xx = x + dx[k] * delta;
            int yy = y + dy[k] * delta;
            if (!inBounds(xx, yy) || Brd[xx][yy] != EMPTY)
                break;
            moveCount++;
            int mobi = 0;
            for (int d = 0; d < 8; d++)
            {
                int nx = xx + dx[d];
                int ny = yy + dy[d];
                if (inBounds(nx, ny) && Brd[nx][ny] == EMPTY)
                    mobi++;
            }
            sum += (double)mobi / (delta * delta);
        }
    }
    return sum + moveCount * 2;
}

// 计算领土控制
double calcTerritoryControl(int pieceType)
{
    int distField[AMAZONS_SIZE][AMAZONS_SIZE];
    int opponentDistField[AMAZONS_SIZE][AMAZONS_SIZE];
    calcDistanceField(distField, pieceType, false);
    calcDistanceField(opponentDistField, pieceType == QUEEN_BLACK ? QUEEN_WHITE : QUEEN_BLACK, false);
    double territory = 0;
    for (int i = 0; i < AMAZONS_SIZE; i++)
    {
        for (int j = 0; j < AMAZONS_SIZE; j++)
        {
            if (Brd[i + 1][j + 1] != EMPTY)
                continue;
            if (distField[i][j] < opponentDistField[i][j])
            {
                territory += 1.0 / (1 + distField[i][j]);
            }
            else if (distField[i][j] > opponentDistField[i][j])
            {
                territory -= 1.0 / (1 + opponentDistField[i][j]);
            }
        }
    }
    return territory;
}

// AI评估函数
double evaluatePosition()
{
    int distQueen[2][AMAZONS_SIZE][AMAZONS_SIZE];
    int distKing[2][AMAZONS_SIZE][AMAZONS_SIZE];
    calcDistanceField(distQueen[0], QUEEN_BLACK, false);
    calcDistanceField(distQueen[1], QUEEN_WHITE, false);
    calcDistanceField(distKing[0], QUEEN_BLACK, true);
    calcDistanceField(distKing[1], QUEEN_WHITE, true);
    double tQueen = 0, tKing = 0, pQueen = 0, pKing = 0, mobility = 0;
    double territory = calcTerritoryControl(QUEEN_WHITE) - calcTerritoryControl(QUEEN_BLACK);
    for (int i = 0; i < AMAZONS_SIZE; i++)
    {
        for (int j = 0; j < AMAZONS_SIZE; j++)
        {
            if (Brd[i + 1][j + 1] == QUEEN_BLACK)
                mobility -= 100.0 / (5 + calcMobility(i + 1, j + 1));
            else if (Brd[i + 1][j + 1] == QUEEN_WHITE)
                mobility += 100.0 / (5 + calcMobility(i + 1, j + 1));
            if (Brd[i + 1][j + 1] != EMPTY)
                continue;
            int diff_q = distQueen[1][i][j] - distQueen[0][i][j];
            int diff_k = distKing[1][i][j] - distKing[0][i][j];
            if (diff_q < 0)
                tQueen += 1.2;
            else if (diff_q > 0)
                tQueen -= 1.2;
            else if (distQueen[1][i][j] < 100)
                tQueen += K;
            if (diff_k < 0)
                tKing += 1.0;
            else if (diff_k > 0)
                tKing -= 1.0;
            else if (distKing[1][i][j] < 100)
                tKing += K;
            pQueen += pow(2.0, -distQueen[1][i][j]) - pow(2.0, -distQueen[0][i][j]);
            pKing += min(1.5, max(-1.5, (double)(distKing[0][i][j] - distKing[1][i][j]) / 5.0));
        }
    }
    pQueen *= 2.5;
    int turnIdx = min(rnd - 1, 31);
    double score = tQueen * f1[turnIdx] * 1.5 +
                   tKing * 0.4 +
                   pQueen * 0.3 +
                   pKing * 0.25 +
                   mobility * 0.15 +
                   territory * 0.5;
    return score;
}

// 生成所有合法走法
vector<AIMove> generateMoves(int pieceType)
{
    vector<AIMove> moves;
    for (int i = 1; i <= AMAZONS_SIZE; i++)
    {
        for (int j = 1; j <= AMAZONS_SIZE; j++)
        {
            if (Brd[i][j] != pieceType)
                continue;
            for (int k = 0; k < 8; k++)
            {
                for (int delta1 = 1; delta1 < AMAZONS_SIZE; delta1++)
                {
                    int ii = i + dx[k] * delta1;
                    int jj = j + dy[k] * delta1;
                    if (!inBounds(ii, jj) || Brd[ii][jj] != EMPTY)
                        break;
                    Brd[i][j] = EMPTY;
                    Brd[ii][jj] = pieceType;
                    for (int l = 0; l < 8; l++)
                    {
                        for (int delta2 = 1; delta2 < AMAZONS_SIZE; delta2++)
                        {
                            int iii = ii + dx[l] * delta2;
                            int jjj = jj + dy[l] * delta2;
                            if (!inBounds(iii, jjj))
                                break;
                            if (Brd[iii][jjj] != EMPTY && !(i == iii && j == jjj))
                                break;
                            AIMove move;
                            move.x0 = i;
                            move.y0 = j;
                            move.x1 = ii;
                            move.y1 = jj;
                            move.x2 = iii;
                            move.y2 = jjj;
                            move.value = 0;
                            moves.push_back(move);
                        }
                    }
                    Brd[i][j] = pieceType;
                    Brd[ii][jj] = EMPTY;
                }
            }
        }
    }
    return moves;
}

// Alpha-Beta剪枝搜索
double alphaBeta(int depth, double alpha, double beta, bool maximizing, clock_t startTime, int &nodesSearched)
{
    nodesSearched++;
    if ((double)(clock() - startTime) / CLOCKS_PER_SEC > 1.8 || depth == 0)
    {
        return evaluatePosition();
    }
    int pieceType = maximizing ? QUEEN_WHITE : QUEEN_BLACK;
    vector<AIMove> moves = generateMoves(pieceType);
    if (moves.empty())
    {
        return maximizing ? -50000.0 : 50000.0;
    }
    // 预排序
    for (auto &move : moves)
    {
        int v0 = Brd[move.x0][move.y0], v1 = Brd[move.x1][move.y1], v2 = Brd[move.x2][move.y2];
        Brd[move.x0][move.y0] = EMPTY;
        Brd[move.x1][move.y1] = pieceType;
        Brd[move.x2][move.y2] = ARROW;
        move.value = evaluatePosition();
        Brd[move.x0][move.y0] = v0;
        Brd[move.x1][move.y1] = v1;
        Brd[move.x2][move.y2] = v2;
    }
    if (maximizing)
        sort(moves.begin(), moves.end(), [](const AIMove &a, const AIMove &b)
             { return a.value > b.value; });
    else
        sort(moves.begin(), moves.end(), [](const AIMove &a, const AIMove &b)
             { return a.value < b.value; });
    int searchLimit = min((int)moves.size(), max(10, 30 - depth * 5));
    if (maximizing)
    {
        double maxEval = -100000;
        for (int i = 0; i < searchLimit; i++)
        {
            auto &move = moves[i];
            int v0 = Brd[move.x0][move.y0], v1 = Brd[move.x1][move.y1], v2 = Brd[move.x2][move.y2];
            Brd[move.x0][move.y0] = EMPTY;
            Brd[move.x1][move.y1] = QUEEN_WHITE;
            Brd[move.x2][move.y2] = ARROW;
            double eval = alphaBeta(depth - 1, alpha, beta, false, startTime, nodesSearched);
            Brd[move.x0][move.y0] = v0;
            Brd[move.x1][move.y1] = v1;
            Brd[move.x2][move.y2] = v2;
            maxEval = max(maxEval, eval);
            alpha = max(alpha, eval);
            if (beta <= alpha)
                break;
        }
        return maxEval;
    }
    else
    {
        double minEval = 100000;
        for (int i = 0; i < searchLimit; i++)
        {
            auto &move = moves[i];
            int v0 = Brd[move.x0][move.y0], v1 = Brd[move.x1][move.y1], v2 = Brd[move.x2][move.y2];
            Brd[move.x0][move.y0] = EMPTY;
            Brd[move.x1][move.y1] = QUEEN_BLACK;
            Brd[move.x2][move.y2] = ARROW;
            double eval = alphaBeta(depth - 1, alpha, beta, true, startTime, nodesSearched);
            Brd[move.x0][move.y0] = v0;
            Brd[move.x1][move.y1] = v1;
            Brd[move.x2][move.y2] = v2;
            minEval = min(minEval, eval);
            beta = min(beta, eval);
            if (beta <= alpha)
                break;
        }
        return minEval;
    }
}

// 保存状态
void saveState()
{
    GameStateSnapshot snap;
    snap.board = Brd;
    snap.round = rnd;
    snap.mode = Mode;
    history.push(snap);
}

// 悔棋
void undoMove()
{
    if (history.empty())
    {
        cout << "无法悔棋，已是初始状态。" << endl;
        return;
    }
    int undoCount = (Mode == 1) ? 2 : 1;
    for (int i = 0; i < undoCount; i++)
    {
        if (history.empty())
            break;
        history.pop();
    }
    if (history.empty())
    {
        currentPage = Page::Menu1;
        initAmazonsBoard();
        return;
    }
    GameStateSnapshot prevSnap = history.top();
    Brd = prevSnap.board;
    rnd = prevSnap.round;
    Mode = prevSnap.mode;
    winner = EMPTY;
    selected_i = -1;
    selected_j = -1;
    currentGameState = GameState::STATE_SELECT_QUEEN;
    ClearConsole();
    if (Mode == 1)
        cout << "成功悔棋回退2回合，回到第 " << rnd << " 回合。" << endl;
    else
        cout << "成功悔棋，回到第 " << rnd << " 回合。" << endl;
}

// 退出确认对话框
bool showExitConfirmDialog()
{
    int result = MessageBoxW(getHWnd(), L"你确定要退出游戏吗？", L"退出确认", MB_YESNO | MB_ICONQUESTION);
    return (result == IDYES);
}

// 绘制棋盘
void drawAmazonsBoard()
{
    setfillcolor(EGERGB(255, 255, 255));
    bar(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    int board_pixel_size = AMAZONS_SIZE * CELL_SIZE;
    int board_x_end = BOARD_X_START + board_pixel_size;
    int board_y_end = BOARD_Y_START + board_pixel_size;
    setfillcolor(EGERGB(220, 220, 220));
    bar(BOARD_X_START, BOARD_Y_START, board_x_end, board_y_end);

    // 水印WX
    const wchar_t *watermark = L"WX";
    const int FONT_SIZE_WM_HEIGHT = 300;
    LOGFONT f;
    memset(&f, 0, sizeof(LOGFONT));
    f.lfHeight = FONT_SIZE_WM_HEIGHT;
    f.lfWeight = FW_HEAVY;
    f.lfCharSet = GB2312_CHARSET;
    strcpy(f.lfFaceName, "Arial");
    f.lfOrientation = 0;
    setfont(&f);
    setcolor(EGERGBA(255, 255, 255, 40));
    setbkmode(TRANSPARENT);
    outtextxy(BOARD_X_START + 50, BOARD_Y_START + 50, watermark);

    // 水印Aurora
    const wchar_t *watermark2 = L"Aurora";
    const int FONT_SIZE_AURORA = FONT_SIZE_WM_HEIGHT / 4;
    LOGFONT f2;
    memset(&f2, 0, sizeof(LOGFONT));
    f2.lfHeight = FONT_SIZE_AURORA;
    f2.lfWeight = FW_NORMAL;
    f2.lfCharSet = DEFAULT_CHARSET;
    strcpy(f2.lfFaceName, "Lucida Calligraphy");
    f2.lfOrientation = 0;
    setfont(&f2);
    setcolor(EGERGBA(255, 255, 255, 40));
    setbkmode(TRANSPARENT);
    outtextxy(BOARD_X_START + 150, BOARD_Y_START + 320, watermark2);

    // 棋盘边框
    setcolor(EGERGB(0, 0, 0));
    setlinewidth(3);
    rectangle(BOARD_X_START - 2, BOARD_Y_START - 2, board_x_end + 2, board_y_end + 2);
    setcolor(EGERGB(100, 100, 100));
    setlinewidth(1);
    rectangle(BOARD_X_START, BOARD_Y_START, board_x_end, board_y_end);

    // 棋盘网格线
    setcolor(EGERGB(0, 0, 0));
    setlinewidth(1);
    for (int i = 0; i <= AMAZONS_SIZE; ++i)
    {
        int pos = BOARD_X_START + i * CELL_SIZE;
        line(pos, BOARD_Y_START, pos, board_y_end);
        line(BOARD_X_START, pos, board_x_end, pos);
    }

    // 坐标标注
    setcolor(EGERGB(0, 0, 0));
    setfont(20, 0, L"宋体"_ansi);
    setbkmode(TRANSPARENT);
    for (int i = 0; i < AMAZONS_SIZE; ++i)
    {
        int x_center = BOARD_X_START + i * CELL_SIZE + HALF_CELL;
        int y_center = BOARD_Y_START + i * CELL_SIZE + HALF_CELL;
        outtextxy(x_center - 10, BOARD_Y_START - 30, (wchar_t)(L'A' + i));
        outtextxy(BOARD_X_START - 30, y_center - 10, std::to_wstring(i + 1).c_str());
    }

    // 底部标题
    const char *title_text_ansi = "Amazon";
    LOGFONT title_f;
    memset(&title_f, 0, sizeof(LOGFONT));
    title_f.lfHeight = 70;
    title_f.lfCharSet = DEFAULT_CHARSET;
    title_f.lfWeight = FW_BOLD;
    strcpy(title_f.lfFaceName, "Kunstler Script");
    setfont(&title_f);
    setcolor(EGERGB(0, 0, 0));
    int title_x = BOARD_X_START + (board_pixel_size - textwidth(title_text_ansi)) / 2;
    outtextxy(title_x, board_y_end + 20, title_text_ansi);
}

// 绘制棋子
void drawPieces()
{
    ege_enable_aa(true);
    const int PIECE_RADIUS = CELL_SIZE / 2 - 5;
    const int BORDER_RADIUS = PIECE_RADIUS + 2;
    const color_t BORDER_COLOR = EGERGBA(150, 150, 150, 255);

    // 绘制可移动位置高亮
    if (currentGameState == GameState::STATE_MOVE_QUEEN && selected_i != -1 && selected_j != -1)
    {
        vector<pair<int, int>> validMoves = getValidMoves(selected_i, selected_j);
        setcolor(EGERGB(144, 238, 144));
        setlinewidth(3);
        for (auto &pos : validMoves)
        {
            int r = pos.first;
            int c = pos.second;
            rectangle(BOARD_X_START + (c - 1) * CELL_SIZE + 3,
                      BOARD_Y_START + (r - 1) * CELL_SIZE + 3,
                      BOARD_X_START + c * CELL_SIZE - 3,
                      BOARD_Y_START + r * CELL_SIZE - 3);
        }
    }

    // 绘制可放置障碍位置高亮
    if (currentGameState == GameState::STATE_SHOOT_ARROW && selected_i != -1 && selected_j != -1)
    {
        vector<pair<int, int>> validMoves = getValidMoves(selected_i, selected_j);
        setcolor(EGERGB(100, 149, 237));
        setlinewidth(3);
        for (auto &pos : validMoves)
        {
            int r = pos.first;
            int c = pos.second;
            rectangle(BOARD_X_START + (c - 1) * CELL_SIZE + 3,
                      BOARD_Y_START + (r - 1) * CELL_SIZE + 3,
                      BOARD_X_START + c * CELL_SIZE - 3,
                      BOARD_Y_START + r * CELL_SIZE - 3);
        }
    }

    // 绘制棋子
    for (int i = 1; i <= AMAZONS_SIZE; ++i)
    {
        for (int j = 1; j <= AMAZONS_SIZE; ++j)
        {
            int flag = Brd[i][j];
            int center_x = BOARD_X_START + (j - 1) * CELL_SIZE + HALF_CELL + FORCE_CENTER_FIX;
            int center_y = BOARD_Y_START + (i - 1) * CELL_SIZE + HALF_CELL + FORCE_CENTER_FIX;
            color_t inner_piece_color;
            if (flag == QUEEN_BLACK)
                inner_piece_color = EGERGBA(0, 0, 0, 255);
            else if (flag == QUEEN_WHITE)
                inner_piece_color = EGERGBA(255, 255, 255, 255);
            else if (flag == ARROW)
                inner_piece_color = EGERGBA(0, 0, 255, 255);
            if (flag != EMPTY)
            {
                setfillcolor(BORDER_COLOR);
                ege_fillellipse(center_x, center_y, BORDER_RADIUS * 2, BORDER_RADIUS * 2);
                setfillcolor(inner_piece_color);
                ege_fillellipse(center_x, center_y, PIECE_RADIUS * 2, PIECE_RADIUS * 2);
            }
            // 绘制选中棋子的黄色边框
            if (currentGameState != GameState::STATE_SELECT_QUEEN && i == selected_i && j == selected_j)
            {
                setcolor(EGERGB(255, 255, 0));
                setlinewidth(4);
                rectangle(BOARD_X_START + (j - 1) * CELL_SIZE,
                          BOARD_Y_START + (i - 1) * CELL_SIZE,
                          BOARD_X_START + j * CELL_SIZE,
                          BOARD_Y_START + i * CELL_SIZE);
            }
        }
    }
}

// 绘制游戏页面
void drawBoardPage()
{
    cleardevice();
    drawAmazonsBoard();
    drawPieces();
    putrnd(rnd);
    putplayer(rnd);
    const int BTN_FONT_SIZE = 22;
    if (Mode == 1 && rnd % 2 == 0 && winner == EMPTY)
    {
        setfont(30, 0, L"锐字潮牌驰光黑-闪 中粗"_ansi);
        setcolor(EGERGB(0, 0, 255));
        outtextxy(760, 250, L"AI 正在深度思考...");
        ButtonCentered(L"返回主菜单", 800, 420, 150, 50, BTN_FONT_SIZE);
        ButtonCentered(L"退出游戏", 800, 520, 150, 50, BTN_FONT_SIZE);
    }
    else if (winner == EMPTY)
    {
        ButtonCentered(L"悔棋", 800, 220, 150, 50, BTN_FONT_SIZE);
        ButtonCentered(L"保存存档", 800, 320, 150, 50, BTN_FONT_SIZE);
        ButtonCentered(L"返回主菜单", 800, 420, 150, 50, BTN_FONT_SIZE);
        ButtonCentered(L"退出游戏", 800, 520, 150, 50, BTN_FONT_SIZE);
    }
    else
    {
        setfont(40, 0, L"楷体"_ansi);
        setcolor(EGERGB(255, 0, 0));
        outtextxy(800, 250, (winner == QUEEN_WHITE) ? L"白方胜利！" : L"黑方胜利！");
        ButtonCentered(L"返回主菜单", 800, 420, 150, 50, BTN_FONT_SIZE);
        ButtonCentered(L"退出游戏", 800, 520, 150, 50, BTN_FONT_SIZE);
    }
}

// AI走棋
void aiMakeMove()
{
    vector<AIMove> moves = generateMoves(QUEEN_BLACK);
    if (moves.empty())
    {
        winner = QUEEN_WHITE;
        checkWinCondition();
        return;
    }
    clock_t startTime = clock();
    // 预评估排序
    for (auto &move : moves)
    {
        int v0 = Brd[move.x0][move.y0], v1 = Brd[move.x1][move.y1], v2 = Brd[move.x2][move.y2];
        Brd[move.x0][move.y0] = EMPTY;
        Brd[move.x1][move.y1] = QUEEN_BLACK;
        Brd[move.x2][move.y2] = ARROW;
        move.value = evaluatePosition();
        Brd[move.x0][move.y0] = v0;
        Brd[move.x1][move.y1] = v1;
        Brd[move.x2][move.y2] = v2;
    }
    sort(moves.begin(), moves.end(), [](const AIMove &a, const AIMove &b)
         { return a.value < b.value; });
    AIMove bestMove = moves[0];
    double bestVal = 100000;
    int ns = 0;
    for (int d = 1; d <= 4; d++)
    {
        if ((double)(clock() - startTime) / CLOCKS_PER_SEC > 1.5)
            break;
        for (int i = 0; i < min((int)moves.size(), 30); i++)
        {
            auto &m = moves[i];
            int v0 = Brd[m.x0][m.y0], v1 = Brd[m.x1][m.y1], v2 = Brd[m.x2][m.y2];
            Brd[m.x0][m.y0] = EMPTY;
            Brd[m.x1][m.y1] = QUEEN_BLACK;
            Brd[m.x2][m.y2] = ARROW;
            double val = alphaBeta(d - 1, -100000, 100000, true, startTime, ns);
            Brd[m.x0][m.y0] = v0;
            Brd[m.x1][m.y1] = v1;
            Brd[m.x2][m.y2] = v2;
            if (val < bestVal)
            {
                bestVal = val;
                bestMove = m;
            }
        }
    }
    saveState();
    // 黑棋移动
    Brd[bestMove.x0][bestMove.y0] = EMPTY;
    Brd[bestMove.x1][bestMove.y1] = QUEEN_BLACK;
    // 刷新屏幕显示移动
    if (c_buffer)
    {
        settarget(c_buffer);
        drawBoardPage();
        settarget(NULL);
        putimage(0, 0, c_buffer);
    }
    Sleep(900);
    // 放置障碍
    Brd[bestMove.x2][bestMove.y2] = ARROW;
    rnd++;
    currentGameState = GameState::STATE_SELECT_QUEEN;
    checkWinCondition();
}

// 处理鼠标输入
void process_mouse_input(mouse_msg msg)
{
    if (msg.is_left() && msg.is_down())
    {
        if (currentPage == Page::Menu1)
        {
            if (IN_BUTTON(msg, 620, 350, 210, 65))
            {
                string saveFilePath = getSaveFilePath();
                remove(saveFilePath.c_str());
                currentPage = Page::Menu2;
            }
            else if (IN_BUTTON(msg, 620, 450, 210, 65))
            {
                if (loadGame())
                    currentPage = Page::Board;
                else
                    MessageBox(getHWnd(), "没有找到有效的存档文件！\n可能存档不存在或已损坏。", "提示", MB_OK | MB_ICONWARNING);
            }
            else if (IN_BUTTON(msg, 620, 550, 210, 65))
            {
                if (showExitConfirmDialog())
                {
                    exit(0);
                }
            }
            return;
        }
        if (currentPage == Page::Menu2)
        {
            if (IN_BUTTON(msg, 650, 350, 230, 65))
            {
                Mode = 0;
                initAmazonsBoard();
                currentPage = Page::Board;
            }
            else if (IN_BUTTON(msg, 650, 460, 230, 65))
            {
                Mode = 1;
                initAmazonsBoard();
                currentPage = Page::Board;
            }
            else if (IN_BUTTON(msg, 650, 570, 230, 65))
            {
                currentPage = Page::Menu1;
            }
            return;
        }
        if (currentPage == Page::Board)
        {
            // 退出游戏按钮 - 添加自动保存
            if (IN_BUTTON(msg, 800, 520, 150, 50))
            {
                if (showExitConfirmDialog())
                {
                    if (winner == EMPTY)
                    {
                        saveGame();
                    }
                    exit(0);
                }
                return;
            }
            // 返回主菜单按钮 - 只在游戏未结束时保存
            if (IN_BUTTON(msg, 800, 420, 150, 50))
            {
                if (winner == EMPTY)
                {
                    saveGame();
                }
                currentPage = Page::Menu1;
                return;
            }
            if (winner == EMPTY)
            {
                // 悔棋按钮
                if (!(Mode == 1 && rnd % 2 == 0) && IN_BUTTON(msg, 800, 220, 150, 50))
                {
                    undoMove();
                    return;
                }
                // 保存存档按钮
                if (!(Mode == 1 && rnd % 2 == 0) && IN_BUTTON(msg, 800, 320, 150, 50))
                {
                    saveGame();
                    return;  // 只保存，不返回主菜单
                }
            }
            else
            {
                return;
            }
            // AI回合时不响应玩家点击
            if (Mode == 1 && rnd % 2 == 0)
                return;
            int click_x = msg.x;
            int click_y = msg.y;
            int board_pixel_size = AMAZONS_SIZE * CELL_SIZE;
            if (click_x >= BOARD_X_START && click_x <= BOARD_X_START + board_pixel_size &&
                click_y >= BOARD_Y_START && click_y <= BOARD_Y_START + board_pixel_size)
            {
                int i = (click_y - BOARD_Y_START) / CELL_SIZE + 1;
                int j = (click_x - BOARD_X_START) / CELL_SIZE + 1;
                int currentPlayerPiece = (rnd % 2 == 1) ? QUEEN_WHITE : QUEEN_BLACK;
                switch (currentGameState)
                {
                case GameState::STATE_SELECT_QUEEN:
                    if (Brd[i][j] == currentPlayerPiece)
                    {
                        selected_i = i;
                        selected_j = j;
                        currentGameState = GameState::STATE_MOVE_QUEEN;
                    }
                    break;
                case GameState::STATE_MOVE_QUEEN:
                    if (Brd[i][j] == EMPTY)
                    {
                        if (canMove(selected_i, selected_j, i, j))
                        {
                            saveState();
                            Brd[i][j] = currentPlayerPiece;
                            Brd[selected_i][selected_j] = EMPTY;
                            selected_i = i;
                            selected_j = j;
                            currentGameState = GameState::STATE_SHOOT_ARROW;
                        }
                        else if (Brd[i][j] == currentPlayerPiece)
                        {
                            selected_i = i;
                            selected_j = j;
                        }
                    }
                    else if (Brd[i][j] == currentPlayerPiece)
                    {
                        selected_i = i;
                        selected_j = j;
                    }
                    break;
                case GameState::STATE_SHOOT_ARROW:
                    if (Brd[i][j] == EMPTY || (i == selected_i && j == selected_j))
                    {
                        if (canMove(selected_i, selected_j, i, j, currentPlayerPiece))
                        {
                            Brd[i][j] = ARROW;
                            if (c_buffer != nullptr)
                            {
                                settarget(c_buffer);
                                drawBoardPage();
                                settarget(NULL);
                                putimage(0, 0, c_buffer);
                            }
                            rnd++;
                            currentGameState = GameState::STATE_SELECT_QUEEN;
                            ClearConsole();
                            cout << "棋盘状态更新" << endl;
                            checkWinCondition();
                            if (Mode == 1 && rnd % 2 == 0 && winner == EMPTY)
                            {
                                if (c_buffer != nullptr)
                                {
                                    settarget(c_buffer);
                                    drawBoardPage();
                                    settarget(NULL);
                                    putimage(0, 0, c_buffer);
                                }
                            }
                        }
                    }
                    break;
                }
            }
        }
    }
}

// 初始化棋盘
void initAmazonsBoard()
{
    for (int i = 1; i <= AMAZONS_SIZE; ++i)
        for (int j = 1; j <= AMAZONS_SIZE; ++j)
            Brd[i][j] = EMPTY;
    Brd[1][3] = Brd[1][6] = Brd[3][1] = Brd[6][1] = QUEEN_WHITE;
    Brd[3][8] = Brd[6][8] = Brd[8][3] = Brd[8][6] = QUEEN_BLACK;
    rnd = 1;
    winner = EMPTY;
    currentGameState = GameState::STATE_SELECT_QUEEN;
    selected_i = -1;
    selected_j = -1;
    while (!history.empty())
        history.pop();
    saveState();
}

// 主函数
int main()
{
    srand(time(NULL));
    initgraph(WINDOW_WIDTH, WINDOW_HEIGHT);
    img_menu1 = newimage();
    img_menu2 = newimage();
    getimage(img_menu1, "C:\\Users\\zhou5507\\Desktop\\Amazons_AI_Project\\chess.jpg");
    getimage(img_menu2, "C:\\Users\\zhou5507\\Desktop\\Amazons_AI_Project\\amazon.jpg");
    setcaption("亚马逊棋 增强AI (GUI)");
    c_buffer = newimage(WINDOW_WIDTH, WINDOW_HEIGHT);
    for (; is_run(); delay_fps(60))
    {
        mouse_msg msg = {0};
        while (mousemsg())
        {
            msg = getmouse();
            process_mouse_input(msg);
        }
        settarget(c_buffer);
        switch (currentPage)
        {
        case Page::Menu1:
            drawMenu1();
            break;
        case Page::Menu2:
            drawMenu2();
            break;
        case Page::Board:
            drawBoardPage();
            if (Mode == 1 && rnd % 2 == 0 && winner == EMPTY)
                aiMakeMove();
            break;
        case Page::Menu_Exit:
            exit(0);
            break;
        default:
            break;
        }
        settarget(NULL);
        putimage(0, 0, c_buffer);
        if (kbhit())
        {
            key_msg key = getkey();
            if (key.msg == MCI_CLOSE)
                break;
        }
    }
    delimage(c_buffer);
    closegraph();
    return 0;
}