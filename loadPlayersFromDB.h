#include <vector>
#include <sqlite3.h>
#include "player.h"
#include <iostream>

std::vector<Player> loadPlayersFromDB() {
    std::vector<Player> allPlayers;
    sqlite3* db;
    sqlite3_open("turnaj.db", &db);

    const char* sql = "SELECT player_id, name, elo, matches_won, games_won, games_lost FROM players;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            std::string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            std::optional<int> elo;
            if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) elo = sqlite3_column_int(stmt, 2);
            
            double m_won = sqlite3_column_double(stmt, 3);
            int g_won = sqlite3_column_int(stmt, 4);
            int g_lost = sqlite3_column_int(stmt, 5);

            allPlayers.emplace_back(id, name, elo, m_won, g_won, g_lost);
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return allPlayers;
}