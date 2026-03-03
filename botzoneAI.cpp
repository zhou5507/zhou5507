
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <cstring>

using namespace std;

const int GRIDSIZE = 8;
const int EMPTY = 0;
const int BLACK = 1;
const int WHITE = -1;
const int OBSTACLE = 2;
const int INF = 1000000;

const int dx[] = {-1, -1, -1, 0, 0, 1, 1, 1};
const int dy[] = {-1, 0, 1, -1, 1, -1, 0, 1};

int gridInfo[GRIDSIZE][GRIDSIZE];
int currBotColor;
int turnID;
clock_t startTime;
int timeOut = 0;

// 距离场缓存
int distQueen[2][GRIDSIZE][GRIDSIZE];
int distKing[2][GRIDSIZE][GRIDSIZE];
int mobi[GRIDSIZE][GRIDSIZE];

// 精细权重参数（来自成形代码，经过优化调整）
const double K = 0.1;
const double f1[32] = {0.0000, 0.1080, 0.1080, 0.1235, 0.1332, 0.1400,
    0.1468, 0.1565, 0.1720, 0.1949, 0.2217,
    0.2476, 0.2680, 0.2800, 0.2884, 0.3000,
    0.3208, 0.3535, 0.4000, 0.4613, 0.5350,
    0.6181, 0.7075, 0.8000, 1.0000, 1.0000,
    1.0000, 1.0000, 1.0000, 1.0000, 1.0000, 1.0000};
const double f2[32] = {1.0000, 0.3940, 0.3940, 0.3826, 0.3753, 0.3700,
    0.3647, 0.3574, 0.3460, 0.3294, 0.3098,
    0.2903, 0.2740, 0.2631, 0.2559, 0.2500,
    0.2430, 0.2334, 0.2200, 0.2020, 0.1800,
    0.1550, 0.1280, 0.1000, 0.0000, 0.0000,
    0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000};
const double f3[32] = {0.0000, 0.1160, 0.1160, 0.1224, 0.1267, 0.1300,
    0.1333, 0.1376, 0.1440, 0.1531, 0.1640,
    0.1754, 0.1860, 0.1944, 0.1995, 0.2000,
    0.1950, 0.1849, 0.1700, 0.1510, 0.1287,
    0.1038, 0.0773, 0.0500, 0.0000, 0.0000,
    0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000};
const double f4[32] = {0.0000, 0.1160, 0.1160, 0.1224, 0.1267, 0.1300,
    0.1333, 0.1376, 0.1440, 0.1531, 0.1640,
    0.1754, 0.1860, 0.1944, 0.1995, 0.2000,
    0.1950, 0.1849, 0.1700, 0.1510, 0.1287,
    0.1038, 0.0773, 0.0500, 0.0000, 0.0000,
    0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000};
const double f5[32] = {0.0000, 0.2300, 0.2300, 0.2159, 0.2067, 0.2000,
    0.1933, 0.1841, 0.1700, 0.1496, 0.1254,
    0.1010, 0.0800, 0.0652, 0.0557, 0.0500,
    0.0464, 0.0436, 0.0400, 0.0346, 0.0274,
    0.0190, 0.0097, 0.0000, 0.0000, 0.0000,
    0.0000, 0.0000, 0.0000, 0.0000, 0.0000, 0.0000};

// 置换表
const int HASH_SIZE = (1 << 20);
struct HashEntry {
    unsigned long long key;
    int depth;
    double value;
    int flag; // 0: exact, 1: lower bound, 2: upper bound
};
HashEntry hashTable[HASH_SIZE];
unsigned long long zobrist[GRIDSIZE][GRIDSIZE][4];
unsigned long long currentHash;

// 历史启发表
int historyTable[GRIDSIZE][GRIDSIZE][GRIDSIZE][GRIDSIZE];

// Killer moves
struct Move {
    int x0, y0, x1, y1, x2, y2;
    double value;
    int historyScore;
};

Move killerMoves[20][2];

inline bool inMap(int x, int y) {
    return x >= 0 && x < GRIDSIZE && y >= 0 && y < GRIDSIZE;
}

void initZobrist() {
    unsigned long long seed = 12345ULL;
    for (int i = 0; i < GRIDSIZE; i++) {
        for (int j = 0; j < GRIDSIZE; j++) {
            for (int k = 0; k < 4; k++) {
                seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
                zobrist[i][j][k] = seed;
            }
        }
    }
}

inline int getPieceType(int piece) {
    if (piece == 0) return 0;
    if (piece == BLACK) return 1;
    if (piece == WHITE) return 2;
    return 3;
}

void initBoard() {
    memset(gridInfo, 0, sizeof(gridInfo));
    memset(historyTable, 0, sizeof(historyTable));
    memset(hashTable, 0, sizeof(hashTable));
    memset(killerMoves, 0, sizeof(killerMoves));
    
    gridInfo[0][2] = BLACK; gridInfo[2][0] = BLACK;
    gridInfo[5][0] = BLACK; gridInfo[7][2] = BLACK;
    gridInfo[0][5] = WHITE; gridInfo[2][7] = WHITE;
    gridInfo[5][7] = WHITE; gridInfo[7][5] = WHITE;
    
    currentHash = 0;
    for (int i = 0; i < GRIDSIZE; i++) {
        for (int j = 0; j < GRIDSIZE; j++) {
            currentHash ^= zobrist[i][j][getPieceType(gridInfo[i][j])];
        }
    }
}

bool ProcStep(int x0, int y0, int x1, int y1, int x2, int y2, int color) {
    if (!inMap(x0, y0) || !inMap(x1, y1) || !inMap(x2, y2)) return false;
    if (gridInfo[x0][y0] != color || gridInfo[x1][y1] != 0) return false;
    if (gridInfo[x2][y2] != 0 && !(x2 == x0 && y2 == y0)) return false;
    
    currentHash ^= zobrist[x0][y0][getPieceType(color)];
    currentHash ^= zobrist[x0][y0][0];
    currentHash ^= zobrist[x1][y1][0];
    currentHash ^= zobrist[x1][y1][getPieceType(color)];
    int oldType = (x2 == x0 && y2 == y0) ? 0 : getPieceType(gridInfo[x2][y2]);
    currentHash ^= zobrist[x2][y2][oldType];
    currentHash ^= zobrist[x2][y2][3];
    
    gridInfo[x0][y0] = 0;
    gridInfo[x1][y1] = color;
    gridInfo[x2][y2] = OBSTACLE;
    return true;
}

inline void makeMove(const Move& m, int color) {
    currentHash ^= zobrist[m.x0][m.y0][getPieceType(color)];
    currentHash ^= zobrist[m.x0][m.y0][0];
    currentHash ^= zobrist[m.x1][m.y1][0];
    currentHash ^= zobrist[m.x1][m.y1][getPieceType(color)];
    int oldType = (m.x2 == m.x0 && m.y2 == m.y0) ? 0 : 0;
    currentHash ^= zobrist[m.x2][m.y2][oldType];
    currentHash ^= zobrist[m.x2][m.y2][3];
    
    gridInfo[m.x0][m.y0] = 0;
    gridInfo[m.x1][m.y1] = color;
    gridInfo[m.x2][m.y2] = OBSTACLE;
}

inline void undoMove(const Move& m, int color) {
    currentHash ^= zobrist[m.x2][m.y2][3];
    int oldType = (m.x2 == m.x0 && m.y2 == m.y0) ? 0 : 0;
    currentHash ^= zobrist[m.x2][m.y2][oldType];
    currentHash ^= zobrist[m.x1][m.y1][getPieceType(color)];
    currentHash ^= zobrist[m.x1][m.y1][0];
    currentHash ^= zobrist[m.x0][m.y0][0];
    currentHash ^= zobrist[m.x0][m.y0][getPieceType(color)];
    
    gridInfo[m.x2][m.y2] = 0;
    gridInfo[m.x1][m.y1] = 0;
    gridInfo[m.x0][m.y0] = color;
}

// 优化的距离场计算 - BFS
void calcDistanceField(int distField[GRIDSIZE][GRIDSIZE], int pieceType, bool kingMoveOnly) {
    static int qx[128], qy[128], qd[128];
    int head = 0, tail = 0;
    
    for (int i = 0; i < GRIDSIZE; i++)
        for (int j = 0; j < GRIDSIZE; j++)
            distField[i][j] = 100;

    for (int i = 0; i < GRIDSIZE; i++) {
        for (int j = 0; j < GRIDSIZE; j++) {
            if (gridInfo[i][j] == pieceType) {
                distField[i][j] = 0;
                qx[tail] = i; qy[tail] = j; tail++;
            }
        }
    }

    while (head < tail) {
        int x = qx[head], y = qy[head]; head++;
        int currDist = distField[x][y];
        
        for (int k = 0; k < 8; k++) {
            int maxDelta = kingMoveOnly ? 2 : GRIDSIZE;
            for (int delta = 1; delta < maxDelta; delta++) {
                int xx = x + dx[k] * delta;
                int yy = y + dy[k] * delta;
                if (!inMap(xx, yy) || gridInfo[xx][yy] != 0) break;
                if (distField[xx][yy] > currDist + 1) {
                    distField[xx][yy] = currDist + 1;
                    qx[tail] = xx; qy[tail] = yy; tail++;
                }
            }
        }
    }
}

// 计算空格周围的灵活度
void calcMobility() {
    for (int i = 0; i < GRIDSIZE; i++) {
        for (int j = 0; j < GRIDSIZE; j++) {
            if (gridInfo[i][j] != 0) { mobi[i][j] = 0; continue; }
            mobi[i][j] = 0;
            for (int k = 0; k < 8; k++) {
                int xx = i + dx[k], yy = j + dy[k];
                if (inMap(xx, yy) && gridInfo[xx][yy] == 0) mobi[i][j]++;
            }
        }
    }
}

// 棋子灵活度评估
double pieceMobility(int x, int y) {
    double sum = 0;
    for (int k = 0; k < 8; k++) {
        for (int delta = 1; delta < GRIDSIZE; delta++) {
            int xx = x + dx[k] * delta;
            int yy = y + dy[k] * delta;
            if (!inMap(xx, yy) || gridInfo[xx][yy] != 0) break;
            sum += 1.0 * mobi[xx][yy] / delta;
        }
    }
    return sum;
}

int countMoves(int x, int y) {
    int count = 0;
    for (int k = 0; k < 8; k++) {
        for (int delta = 1; delta < GRIDSIZE; delta++) {
            int xx = x + dx[k] * delta;
            int yy = y + dy[k] * delta;
            if (!inMap(xx, yy) || gridInfo[xx][yy] != 0) break;
            count++;
        }
    }
    return count;
}

int totalMobility(int color) {
    int total = 0;
    for (int i = 0; i < GRIDSIZE; i++)
        for (int j = 0; j < GRIDSIZE; j++)
            if (gridInfo[i][j] == color)
                total += countMoves(i, j);
    return total;
}

inline double det(int x, int y) {
    if (x == y) return (x < 100) ? K : 0.0;
    return (x < y) ? 1.0 : -1.0;
}

inline double myPow2Neg(int exp) {
    if (exp <= -20) return 0;
    if (exp >= 0) return (double)(1 << exp);
    return 1.0 / (1 << (-exp));
}

// 核心评估函数 - 整合两版精华
double evaluatePosition() {
    calcDistanceField(distQueen[0], currBotColor, false);
    calcDistanceField(distQueen[1], -currBotColor, false);
    calcDistanceField(distKing[0], currBotColor, true);
    calcDistanceField(distKing[1], -currBotColor, true);
    calcMobility();

    double t[2] = {0, 0}, p[2] = {0, 0}, m = 0;
    int myMoves = 0, oppMoves = 0;
    int emptyCount = 0;

    for (int i = 0; i < GRIDSIZE; i++) {
        for (int j = 0; j < GRIDSIZE; j++) {
            if (gridInfo[i][j] == currBotColor) {
                double pm = pieceMobility(i, j);
                m -= 100.0 / (10 + pm);
                myMoves += countMoves(i, j);
            } else if (gridInfo[i][j] == -currBotColor) {
                double pm = pieceMobility(i, j);
                m += 100.0 / (10 + pm);
                oppMoves += countMoves(i, j);
            }
            
            if (gridInfo[i][j] != 0) continue;
            emptyCount++;

            int dq0 = distQueen[0][i][j], dq1 = distQueen[1][i][j];
            int dk0 = distKing[0][i][j], dk1 = distKing[1][i][j];

            t[0] += det(dq0, dq1);
            t[1] += det(dk0, dk1);
            
            p[0] += myPow2Neg(-dq0) - myPow2Neg(-dq1);
            if (dk0 < 100 && dk1 < 100) {
                double diff = (dk1 - dk0) / 6.0;
                p[1] += max(-1.0, min(1.0, diff));
            }
        }
    }

    if (myMoves == 0) return -100000.0;
    if (oppMoves == 0) return 100000.0;

    p[0] *= 2;
    int idx = min(31, max(0, turnID));
    
    double value = t[0] * f1[idx] + t[1] * f2[idx] + p[0] * f3[idx] + p[1] * f4[idx] + m * f5[idx];
    
    // 终局阶段加强领土权重
    if (emptyCount < 25) {
        value += t[0] * 0.3 * (25 - emptyCount) / 25.0;
    }
    
    // 移动力差异惩罚
    if (turnID < 15) {
        value += (myMoves - oppMoves) * 0.05;
    }
    
    return value;
}

// 快速评估 - 用于排序
double quickEvaluate() {
    int myMob = totalMobility(currBotColor);
    int oppMob = totalMobility(-currBotColor);
    if (myMob == 0) return -100000;
    if (oppMob == 0) return 100000;
    return (myMob - oppMob) * 10.0;
}

// 检查是否为killer move
bool isKillerMove(const Move& m, int depth) {
    for (int i = 0; i < 2; i++) {
        if (killerMoves[depth][i].x0 == m.x0 && killerMoves[depth][i].y0 == m.y0 &&
            killerMoves[depth][i].x1 == m.x1 && killerMoves[depth][i].y1 == m.y1 &&
            killerMoves[depth][i].x2 == m.x2 && killerMoves[depth][i].y2 == m.y2)
            return true;
    }
    return false;
}

void updateKillerMove(const Move& m, int depth) {
    if (depth < 20) {
        killerMoves[depth][1] = killerMoves[depth][0];
        killerMoves[depth][0] = m;
    }
}

vector<Move> generateMoves(int color) {
    vector<Move> moves;
    moves.reserve(2500);
    
    for (int i = 0; i < GRIDSIZE; i++) {
        for (int j = 0; j < GRIDSIZE; j++) {
            if (gridInfo[i][j] != color) continue;
            
            for (int k = 0; k < 8; k++) {
                for (int d1 = 1; d1 < GRIDSIZE; d1++) {
                    int x1 = i + dx[k] * d1;
                    int y1 = j + dy[k] * d1;
                    if (!inMap(x1, y1) || gridInfo[x1][y1] != 0) break;

                    gridInfo[i][j] = 0;
                    gridInfo[x1][y1] = color;

                    for (int l = 0; l < 8; l++) {
                        for (int d2 = 1; d2 < GRIDSIZE; d2++) {
                            int x2 = x1 + dx[l] * d2;
                            int y2 = y1 + dy[l] * d2;
                            if (!inMap(x2, y2)) break;
                            if (gridInfo[x2][y2] != 0 && !(x2 == i && y2 == j)) break;

                            Move m;
                            m.x0 = i; m.y0 = j;
                            m.x1 = x1; m.y1 = y1;
                            m.x2 = x2; m.y2 = y2;
                            m.value = 0;
                            m.historyScore = historyTable[i][j][x1][y1];
                            moves.push_back(m);
                        }
                    }

                    gridInfo[i][j] = color;
                    gridInfo[x1][y1] = 0;
                }
            }
        }
    }
    return moves;
}

Move bestMove;

// Negamax with Alpha-Beta, TT, History Heuristic, Killer Moves
double negamax(int depth, int ply, double alpha, double beta, int color) {
    if ((clock() - startTime) > CLOCKS_PER_SEC * 0.93) {
        timeOut = 1;
        return 0;
    }

    double origAlpha = alpha;

    // 置换表查询
    int hashIdx = currentHash & (HASH_SIZE - 1);
    HashEntry& entry = hashTable[hashIdx];
    if (entry.key == currentHash && entry.depth >= depth) {
        if (entry.flag == 0) return entry.value;
        if (entry.flag == 1) alpha = max(alpha, entry.value);
        else if (entry.flag == 2) beta = min(beta, entry.value);
        if (alpha >= beta) return entry.value;
    }

    if (depth == 0) {
        double val = evaluatePosition();
        return (color == currBotColor) ? val : -val;
    }

    vector<Move> moves = generateMoves(color);
    
    if (moves.empty()) {
        return -99999.0 + ply * 100;
    }

    // 走法排序：Killer > History > Quick Eval
    for (auto &m : moves) {
        makeMove(m, color);
        m.value = quickEvaluate();
        if (color != currBotColor) m.value = -m.value;
        undoMove(m, color);
        
        if (isKillerMove(m, ply)) m.historyScore += 100000;
    }
    
    sort(moves.begin(), moves.end(), [](const Move &a, const Move &b) {
        if (a.historyScore != b.historyScore) return a.historyScore > b.historyScore;
        return a.value > b.value;
    });

    // 动态分支限制 - 随深度减少搜索宽度
    int searchLimit = min((int)moves.size(), max(12, 45 - depth * 5 - ply * 2));
    
    double bestVal = -INF;
    Move bestLocalMove;
    
    for (int i = 0; i < searchLimit; i++) {
        if (timeOut) break;
        
        auto &m = moves[i];
        makeMove(m, color);
        
        double val;
        // Late Move Reduction (LMR)
        if (i >= 4 && depth >= 3 && ply >= 1) {
            val = -negamax(depth - 2, ply + 1, -beta, -alpha, -color);
            if (val > alpha && !timeOut) {
                val = -negamax(depth - 1, ply + 1, -beta, -alpha, -color);
            }
        } else {
            val = -negamax(depth - 1, ply + 1, -beta, -alpha, -color);
        }
        
        undoMove(m, color);

        if (val > bestVal) {
            bestVal = val;
            bestLocalMove = m;
        }
        if (val > alpha) alpha = val;
        if (alpha >= beta) {
            historyTable[m.x0][m.y0][m.x1][m.y1] += depth * depth;
            updateKillerMove(m, ply);
            break;
        }
    }

    // 更新置换表
    if (!timeOut) {
        entry.key = currentHash;
        entry.depth = depth;
        entry.value = bestVal;
        if (bestVal <= origAlpha) entry.flag = 2;
        else if (bestVal >= beta) entry.flag = 1;
        else entry.flag = 0;
    }

    return bestVal;
}

void findBestMove() {
    startTime = clock();
    timeOut = 0;
    
    vector<Move> moves = generateMoves(currBotColor);
    if (moves.empty()) return;

    // 初始完整评估排序
    for (auto &m : moves) {
        makeMove(m, currBotColor);
        m.value = evaluatePosition();
        undoMove(m, currBotColor);
    }
    sort(moves.begin(), moves.end(), [](const Move &a, const Move &b) {
        return a.value > b.value;
    });

    bestMove = moves[0];

    // 迭代加深搜索
    for (int depth = 1; depth <= 25; depth++) {
        if (timeOut || (clock() - startTime) > CLOCKS_PER_SEC * 0.85) break;

        double currentBest = -INF;
        Move currentBestMove = moves[0];
        int searchLimit = min((int)moves.size(), 60 - depth * 2);

        for (int i = 0; i < searchLimit; i++) {
            if (timeOut) break;
            
            auto &m = moves[i];
            makeMove(m, currBotColor);
            
            double val;
            if (i == 0) {
                val = -negamax(depth - 1, 1, -INF, INF, -currBotColor);
            } else {
                // PVS: 窄窗口搜索
                val = -negamax(depth - 1, 1, -currentBest - 0.001, -currentBest, -currBotColor);
                if (val > currentBest && !timeOut) {
                    val = -negamax(depth - 1, 1, -INF, -currentBest, -currBotColor);
                }
            }
            
            undoMove(m, currBotColor);
            m.value = val;

            if (val > currentBest) {
                currentBest = val;
                currentBestMove = m;
            }
        }

        if (!timeOut) {
            bestMove = currentBestMove;
            
            // 重新排序用于下一轮迭代
            sort(moves.begin(), moves.begin() + min((int)moves.size(), searchLimit),
                 [](const Move &a, const Move &b) { return a.value > b.value; });
            
            // 如果必胜或必败，提前结束
            if (currentBest > 90000 || currentBest < -90000) break;
        }
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    
    initZobrist();
    initBoard();

    int x0, y0, x1, y1, x2, y2;
    cin >> turnID;

    currBotColor = WHITE;

    for (int i = 0; i < turnID; i++) {
        cin >> x0 >> y0 >> x1 >> y1 >> x2 >> y2;
        if (x0 == -1)
            currBotColor = BLACK;
        else
            ProcStep(x0, y0, x1, y1, x2, y2, -currBotColor);

        if (i < turnID - 1) {
            cin >> x0 >> y0 >> x1 >> y1 >> x2 >> y2;
            if (x0 >= 0)
                ProcStep(x0, y0, x1, y1, x2, y2, currBotColor);
        }
    }

    findBestMove();

    cout << bestMove.x0 << " " << bestMove.y0 << " "
         << bestMove.x1 << " " << bestMove.y1 << " "
         << bestMove.x2 << " " << bestMove.y2 << endl;

    return 0;
}