#include <stdio.h>
#include <string.h>
#include "dark_chess_client.h"
#include <windows.h>
#include <time.h>

// --- Helper Functions for JSON parsing ---
void get_piece_at(const char* json, int index, char* out_piece) {
    const char* board_start = strstr(json, "\"board\": [[");
    if (!board_start) {
        strcpy(out_piece, "Unknown");
        return;
    }
    const char* p = board_start + 11;
    for (int i = 0; i <= index; i++) {
        p = strchr(p, '\"');
        if (!p) break;
        p++;
        const char* end = strchr(p, '\"');
        if (!end) break;
        if (i == index) {
            int len = end - p;
            if (len > 31) len = 31;
            strncpy(out_piece, p, len);
            out_piece[len] = '\0';
            return;
        }
        p = end + 1;
    }
    strcpy(out_piece, "Unknown");
}

void get_role_color(const char* json, const char* role, char* out_color) {
    char search_key[20];
    sprintf(search_key, "\"%s\": \"", role);
    const char* p = strstr(json, search_key);
    if (p) {
        p += strlen(search_key);
        const char* end = strchr(p, '\"');
        if (end) {
            int len = end - p;
            strncpy(out_color, p, len);
            out_color[len] = '\0';
            return;
        }
    }
    strcpy(out_color, "None");
}

// --- Game Logic Helpers ---
int get_piece_rank(const char* piece) {
    if (strstr(piece, "King")) return 7;
    if (strstr(piece, "Guard")) return 6;
    if (strstr(piece, "Elephant")) return 5;
    if (strstr(piece, "Car")) return 4;
    if (strstr(piece, "Horse")) return 3;
    if (strstr(piece, "Cannon")) return 2;
    if (strstr(piece, "Soldier")) return 1;
    return 0; // Null or Covered
}

int can_capture(const char* attacker, const char* victim) {
    if (strcmp(victim, "Null") == 0) return 1; // Always legal to move to empty space
    if (strcmp(victim, "Covered") == 0) return 0; // Cannot capture covered pieces

    int a_rank = get_piece_rank(attacker);
    int v_rank = get_piece_rank(victim);

    // Rule: Soldier (1) can capture King (7) or another Soldier (1)
    if (a_rank == 1) {
        return (v_rank == 7 || v_rank == 1);
    }
    
    // Rule: King (7) can capture everything EXCEPT Soldier (1)
    if (a_rank == 7) {
        return (v_rank != 1);
    }

    // Rule: Cannon (2) captures by jumping (not handled in adjacent logic)
    // Most rules state Cannons cannot capture adjacently at all.
    if (a_rank == 2) return 0; 

    // Standard Rule: Rank must be greater than or equal to victim
    return a_rank >= v_rank;
}

void make_move(const char* json, const char* my_role_ab) {
    char my_color[10], piece[32], target[32], action[64];
    get_role_color(json, my_role_ab, my_color);
    char opp_color[10];
    strcpy(opp_color, strcmp(my_color, "Red") == 0 ? "Black" : "Red");

    // 1. PRIORITY 1: ATTACK! (Check all my pieces for legal captures)
    for (int i = 0; i < 32; i++) {
        get_piece_at(json, i, piece);
        if (strstr(piece, my_color) && strcmp(piece, "Covered") != 0) {
            int r = i / 8, c = i % 8;
            int dirs[4][2] = {{0,1}, {0,-1}, {1,0}, {-1,0}};

            for (int d = 0; d < 4; d++) {
                int tr = r + dirs[d][0], tc = c + dirs[d][1];
                if (tr >= 0 && tr < 4 && tc >= 0 && tc < 8) {
                    int target_idx = tr * 8 + tc;
                    get_piece_at(json, target_idx, target);

                    // Check if target is an enemy AND rank is okay
                    if (strstr(target, opp_color) && can_capture(piece, target)) {
                        sprintf(action, "%d %d %d %d\n", r, c, tr, tc);
                        printf("AI Action: CAPTURE %s with %s\n", target, piece);
                        Sleep(2000);
                        send_action(action);
                        return;
                    }
                }
            }
        }
    }

    // 2. PRIORITY 2: REVEAL! (If no captures, flip a random piece)
    int covered[32], count = 0;
    for (int i = 0; i < 32; i++) {
        get_piece_at(json, i, piece);
        if (strcmp(piece, "Covered") == 0) covered[count++] = i;
    }
    if (count > 0) {
        int idx = covered[rand() % count];
        sprintf(action, "%d %d\n", idx / 8, idx % 8);
        printf("AI Action: FLIPPING index %d\n", idx);
        Sleep(2000);
        send_action(action);
        return;
    }

    // 3. PRIORITY 3: MOVE! (If nothing to flip, move to a random empty space)
    for (int i = 0; i < 32; i++) {
        get_piece_at(json, i, piece);
        if (strstr(piece, my_color) && strcmp(piece, "Covered") != 0) {
            int r = i / 8, c = i % 8;
            int dirs[4][2] = {{0,1}, {0,-1}, {1,0}, {-1,0}};
            for (int d = 0; d < 4; d++) {
                int tr = r + dirs[d][0], tc = c + dirs[d][1];
                if (tr >= 0 && tr < 4 && tc >= 0 && tc < 8) {
                    get_piece_at(json, tr * 8 + tc, target);
                    if (strcmp(target, "Null") == 0) {
                        sprintf(action, "%d %d %d %d\n", r, c, tr, tc);
                        printf("AI Action: MOVING %s to empty space\n", piece);
                        Sleep(2000);
                        send_action(action);
                        return;
                    }
                }
            }
        }
    }
}

int main() {
    srand(time(NULL));
    char board_data[4000];
    int last_total_moves = -1;

    if (init_connection() != 0) return 1;
    auto_join_room();

    char my_role_ab[2] = "";
    if (strcmp(_assigned_role, "first") == 0) strcpy(my_role_ab, "A");
    else if (strcmp(_assigned_role, "second") == 0) strcpy(my_role_ab, "B");
    else strcpy(my_role_ab, _assigned_role);

    printf("Waiting for server updates...\n");

    while (1) {
        receive_update(board_data, 4000);
        if (strlen(board_data) == 0) break;

        if (strstr(board_data, "UPDATE")) {
            int current_total_moves = -1;
            char* moves_p = strstr(board_data, "\"total_moves\":");
            if (moves_p) sscanf(moves_p + 14, "%d", &current_total_moves);

            const char* turn_role_p = strstr(board_data, "\"current_turn_role\": \"");
            if (turn_role_p) {
                turn_role_p += 22;
                char current_turn_role[2] = { turn_role_p[0], '\0' };
                
                if (strcmp(current_turn_role, my_role_ab) == 0 && current_total_moves != last_total_moves) {
                    printf("\n--- My Turn! (Move %d) ---\n", current_total_moves);
                    make_move(board_data, my_role_ab);
                    last_total_moves = current_total_moves;
                }
            }
        }
    }
    close_connection();
    return 0;
}