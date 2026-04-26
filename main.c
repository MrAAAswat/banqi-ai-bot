#include <stdio.h>
#include <string.h>
#include "dark_chess_client.h"

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

int can_capture(int my_rank, int opp_rank) {
    if (opp_rank == 0) return 0; // Cannot capture Covered or Null this way
    // Soldier beats King
    if (my_rank == 1 && opp_rank == 7) return 1;
    // King cannot beat Soldier
    if (my_rank == 7 && opp_rank == 1) return 0;
    // Cannons capture anything (handled separately by jump logic, but logically valid)
    if (my_rank == 2) return 1; 
    // Normal rule
    return my_rank >= opp_rank;
}

void make_move(const char* json, const char* my_role_ab) {
    char piece[32], my_color[10], opp_color[10];
    get_role_color(json, my_role_ab, my_color);
    strcpy(opp_color, strcmp(my_color, "Red") == 0 ? "Black" : "Red");

    printf("I am playing as %s.\n", strcmp(my_color, "None") == 0 ? "Unknown yet" : my_color);

    // 1. Scan the 4x8 board (32 squares) for "Covered" pieces
    int covered_indices[32];
    int covered_count = 0;

    for (int i = 0; i < 32; i++) {
        get_piece_at(json, i, piece);
        if (strcmp(piece, "Covered") == 0) {
            covered_indices[covered_count] = i;
            covered_count++;
        }
    }

    char action[50];

    // 2. If there are covered pieces, pick a random one to flip!
    if (covered_count > 0) {
        int random_choice = rand() % covered_count;
        int target_index = covered_indices[random_choice];
        
        // Convert the 1D index (0-31) to 2D Row/Col (0-3, 0-7)
        int r = target_index / 8;
        int c = target_index % 8;
        
        // Format exactly as the documentation strictly requires: "r c\n"
        sprintf(action, "%d %d\n", r, c);
        
        printf("AI Decision: Flipping piece at Index %d (Row %d, Col %d)\n", target_index, r, c);
        
        // Pause for 2 seconds to keep the server happy
        Sleep(2000); 
        
        send_action(action);
    } else {
        // Eventually, the board will be fully flipped.
        printf("No pieces left to flip! (We need to add movement logic here)\n");
        
    }
}

int main() {
    char board_data[4000];
    int last_total_moves = -1;

    if (init_connection() != 0) return 1;
    auto_join_room();

    // Determine our role string for JSON parsing
    char my_role_ab[2] = "";
    if (strcmp(_assigned_role, "first") == 0) strcpy(my_role_ab, "A");
    else if (strcmp(_assigned_role, "second") == 0) strcpy(my_role_ab, "B");
    else strcpy(my_role_ab, _assigned_role); // Fallback if server directly assigns "A" or "B"

    printf("Waiting for server updates...\n");

    while (1) {
        receive_update(board_data, 4000);
        printf("%s", board_data);
        if (strlen(board_data) == 0) {
            printf("Server closed the connection.\n");
            break;
        }

        if (strstr(board_data, "UPDATE")) {
            int current_total_moves = -1;
            char* moves_p = strstr(board_data, "\"total_moves\":");
            if (moves_p) {
                sscanf(moves_p + 14, "%d", &current_total_moves);
            }

            const char* turn_role_p = strstr(board_data, "\"current_turn_role\": \"");
            if (turn_role_p) {
                turn_role_p += 22;
                char current_turn_role[2] = { turn_role_p[0], '\0' };
                
                // Act only if it is our turn AND the server has registered a new move state
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