#include <bits/stdc++.h>
using namespace std;

using Board = array<int, 9>;

const int LINES[8][3] = {
    {0,1,2},{3,4,5},{6,7,8},
    {0,3,6},{1,4,7},{2,5,8},
    {0,4,8},{2,4,6}
};

int winner(const Board& b) {
    for(auto& L : LINES) {
        if(b[L[0]] != 0 && b[L[0]] == b[L[1]] && b[L[1]] == b[L[2]])
            return b[L[0]];
    }
    return 0;
}

bool is_full(const Board& b) {
    for(int v : b) if(v == 0) return false;
    return true;
}

int utility(const Board& b, int me) {
    int w = winner(b);
    if(w == me) return 1;
    if(w == 0)  return 0;
    return -1;
}

int minimax(Board& b, int me, int to_move, long long& nodes) {
    nodes++;
    if(winner(b) != 0 || is_full(b)) return utility(b, me);

    int opp = 3 - to_move;
    if(to_move == me) {
        int best = -2;
        for(int i = 0; i < 9; i++) {
            if(b[i] == 0) {
                b[i] = to_move;
                best = max(best, minimax(b, me, opp, nodes));
                b[i] = 0;
            }
        }
        return best;
    } else {
        int best = 2;
        for(int i = 0; i < 9; i++) {
            if(b[i] == 0) {
                b[i] = to_move;
                best = min(best, minimax(b, me, opp, nodes));
                b[i] = 0;
            }
        }
        return best;
    }
}

int alphabeta(Board& b, int me, int to_move, int alpha, int beta, long long& nodes) {
    nodes++;
    if(winner(b) != 0 || is_full(b)) return utility(b, me);

    int opp = 3 - to_move;
    if(to_move == me) {
        int value = -2;
        for(int i = 0; i < 9; i++) {
            if(b[i] == 0) {
                b[i] = to_move;
                value = max(value, alphabeta(b, me, opp, alpha, beta, nodes));
                b[i] = 0;
                alpha = max(alpha, value);
                if(alpha >= beta) break;
            }
        }
        return value;
    } else {
        int value = 2;
        for(int i = 0; i < 9; i++) {
            if(b[i] == 0) {
                b[i] = to_move;
                value = min(value, alphabeta(b, me, opp, alpha, beta, nodes));
                b[i] = 0;
                beta = min(beta, value);
                if(alpha >= beta) break;
            }
        }
        return value;
    }
}

int best_move(Board& b, int player, bool use_ab, int& out_val, long long& nodes) {
    int opp = 3 - player;
    int best_val = -2, best_pos = -1;
    for(int i = 0; i < 9; i++) {
        if(b[i] == 0) {
            b[i] = player;
            int val;
            if(winner(b) == player) val = 1;
            else if(use_ab)         val = alphabeta(b, player, opp, -2, 2, nodes);
            else                    val = minimax(b, player, opp, nodes);
            b[i] = 0;
            if(val > best_val) { best_val = val; best_pos = i; }
        }
    }
    out_val = best_val;
    return best_pos;
}

void render(const Board& b) {
    const char sym[3] = {' ', 'O', 'X'};
    for(int r = 0; r < 3; r++) {
        cout << " " << sym[b[3*r]] << " | " << sym[b[3*r+1]] << " | " << sym[b[3*r+2]] << "\n";
        if(r < 2) cout << "-----------\n";
    }
}

void gen_check(Board& b, int to_move, long long& tested, long long& mismatch, set<pair<array<int,9>,int>>& seen) {
    auto key = make_pair(b, to_move);
    if(seen.count(key)) return;
    seen.insert(key);
    tested++;
    long long n1 = 0, n2 = 0;
    int v1 = minimax(b, to_move, to_move, n1);
    int v2 = alphabeta(b, to_move, to_move, -2, 2, n2);
    if(v1 != v2) mismatch++;
    if(winner(b) != 0 || is_full(b)) return;
    for(int i = 0; i < 9; i++) {
        if(b[i] == 0) {
            b[i] = to_move;
            gen_check(b, 3 - to_move, tested, mismatch, seen);
            b[i] = 0;
        }
    }
}

bool never_loses(Board& b, int agent, int to_move) {
    int w = winner(b);
    if(w != 0) return w != (3 - agent);
    if(is_full(b)) return true;
    if(to_move == agent) {
        int val; long long nodes = 0;
        int pos = best_move(b, agent, true, val, nodes);
        b[pos] = agent;
        bool ok = never_loses(b, agent, 3 - agent);
        b[pos] = 0;
        return ok;
    } else {
        for(int i = 0; i < 9; i++) {
            if(b[i] == 0) {
                b[i] = to_move;
                bool ok = never_loses(b, agent, 3 - to_move);
                b[i] = 0;
                if(!ok) return false;
            }
        }
        return true;
    }
}

int main(int argc, char** argv) {
    if(argc > 1 && string(argv[1]) == "play") {
        Board b{}; b.fill(0);
        int human = 1, ai = 2, to_move = 1;
        render(b); cout << "\n";
        while(winner(b) == 0 && !is_full(b)) {
            if(to_move == human) {
                int pos; cout << "> ";
                if(!(cin >> pos)) break;
                if(pos < 0 || pos > 8 || b[pos] != 0) continue;
                b[pos] = human;
            } else {
                int val; long long nodes = 0;
                int pos = best_move(b, ai, true, val, nodes);
                b[pos] = ai;
                cout << "Agente -> " << pos << "\n";
            }
            render(b); cout << "\n";
            to_move = 3 - to_move;
        }
        return 0;
    }

    Board empty{}; empty.fill(0);

    int val; long long nodes = 0;
    int pos = best_move(empty, 1, true, val, nodes);
    cout << "Jugada inicial optima: " << pos << "\nValor: " << val << "\n\n";

    long long n1 = 0, n2 = 0;
    minimax(empty, 1, 1, n1);
    alphabeta(empty, 1, 1, -2, 2, n2);
    cout << "Nodos Minimax: " << n1 << "\n";
    cout << "Nodos Alfa-Beta: " << n2 << "\n\n";

    long long tested = 0, mismatch = 0;
    set<pair<array<int,9>,int>> seen;
    Board b = empty;
    gen_check(b, 1, tested, mismatch, seen);
    cout << "Estados unicos explorados: " << tested << "\n";
    cout << "Discrepancias Minimax/AB: " << mismatch << "\n\n";

    Board b1 = empty, b2 = empty;
    cout << "J1 Invicto: " << (never_loses(b1, 1, 1) ? "OK" : "FAIL") << "\n";
    cout << "J2 Invicto: " << (never_loses(b2, 2, 1) ? "OK" : "FAIL") << "\n";

    return 0;
}
