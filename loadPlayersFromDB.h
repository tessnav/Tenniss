//
// Created by 004ha on 07.05.2026.
//

#include <vector>
#include <sqlite3.h>
#include "Player.h"
#include <iostream>

std::pair<std::vector<Player>, std::vector<Player>> loadPlayersFromDB() {
    std::vector<Player> teamA;
    std::vector<Player> teamB;

    sqlite3* db;
    if (sqlite3_open("turnaj.db", &db) != SQLITE_OK) {
        std::cerr << "Chyba při otevírání DB" << std::endl;
        return {teamA, teamB};
    }

    const char* sql = "SELECT player_id, name, elo, matches_won, games_won, games_lost, team_id FROM players;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            // Načtení dat z aktuálního řádku
            int id = sqlite3_column_int(stmt, 0);
            std::string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

            // Ošetření volitelného ELO (NULL v DB)
            std::optional<int> elo;
            if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
                elo = sqlite3_column_int(stmt, 2);
            }

            int m_won = sqlite3_column_int(stmt, 3);
            int g_won = sqlite3_column_int(stmt, 4);
            int g_lost = sqlite3_column_int(stmt, 5);
            int t_id = sqlite3_column_int(stmt, 6);

            // Vytvoření objektu Player
            Player p(id, name, elo, m_won, g_won, g_lost, t_id);

            // Rozřazení do týmů (předpokládáme team_id 1 pro A a 2 pro B)
            if (t_id == 1) {
                teamA.push_back(p);
            } else {
                teamB.push_back(p);
            }
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return {teamA, teamB};
}