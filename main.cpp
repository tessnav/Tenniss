#include <iostream>

#include "crow.h"
#include <sqlite3.h>
#include <nlohmann/json_fwd.hpp>
#include <algorithm>
#include <vector>
#include "player.h"
#include "loadPlayersFromDB.h"


#define WIN32_LEAN_AND_MEAN
#ifdef _WIN32
    #include <windows.h>
#endif

using namespace std;

int main()
{
    // Řekni Crowu, že šablony jsou o úroveň výš
    crow::mustache::set_base("templates");
    
    crow::SimpleApp app;

    // when landing on page
    CROW_ROUTE(app, "/") //neni funkce ale makro... ve skutecnosti to returni objekt kteremu pak pridavam do konstruktoru tu lambda fci.. tzv. method chaining
    ([]() {
        crow::mustache::context ctx;
       return crow::mustache::load("index.html").render(ctx);
    });

    CROW_ROUTE(app, "/matches")
    ([]() {
        crow::mustache::context ctx;
       return crow::mustache::load("matches.html").render(ctx);
    });

    CROW_ROUTE(app, "/players")
    ([]() {
    crow::mustache::context ctx;
    return crow::mustache::load("players.html").render(ctx);
    });

    CROW_ROUTE(app, "/admin")
    ([](){
        crow::mustache::context ctx;
        return crow::mustache::load("admin.html").render(ctx);
    });

    CROW_ROUTE(app, "/api/newround").methods(crow::HTTPMethod::Post)([](const crow::request& req) {

            // 1. Zpracování JSONu z frontendu
            auto body = nlohmann::json::parse(req.body, nullptr, false);
            if (body.is_discarded()) {
                return crow::response(400, "Neplatny JSON format");
            }

            // 2. Připojení k databázi
            sqlite3* db;
            if (sqlite3_open("turnaj.db", &db) != SQLITE_OK) {
                return crow::response(500, "Nelze otevrit databazi");
            }

            // 3. Start bezpečné transakce
            sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

            // --- A) Vložení záznamu do tabulky MATCHES ---
            const char* insert_match_sql =
                "INSERT INTO matches (p1_a_id, p2_a_id, p1_b_id, p2_b_id, score_a, score_b) "
                "VALUES (?, ?, ?, ?, ?, ?);";

            sqlite3_stmt* stmt;
            sqlite3_prepare_v2(db, insert_match_sql, -1, &stmt, nullptr);

            // Nabindování hodnot z JSONu do SQL dotazu (prevence proti SQL injection)
            sqlite3_bind_int(stmt, 1, body["p1_a_id"]);
            sqlite3_bind_int(stmt, 2, body["p2_a_id"]);
            sqlite3_bind_int(stmt, 3, body["p1_b_id"]);
            sqlite3_bind_int(stmt, 4, body["p2_b_id"]);
            sqlite3_bind_int(stmt, 5, body["score_a"]);
            sqlite3_bind_int(stmt, 6, body["score_b"]);

            sqlite3_step(stmt);
            sqlite3_finalize(stmt); // Vždy musíme uvolnit paměť statementu!

            // --- B) Výpočet bodů za výhru ---
            int score_a = body["score_a"];
            int score_b = body["score_b"];

            // Nová logika: 2 body za výhru, 1 za remízu, 0 za prohru
            int points_a = 0;
            int points_b = 0;

            if (score_a > score_b) {
                points_a = 2;
            } else if (score_b > score_a) {
                points_b = 2;
            } else {
                // Remíza
                points_a = 1;
                points_b = 1;
            }

            // --- C) Pomocná lambda pro update hráče ---
            // Tohle udrží kód krásně čitelný
            auto update_player = [&](int player_id, int match_points, int games_won, int games_lost) {
                const char* update_sql =
                    "UPDATE players SET "
                    "matches_won = matches_won + ?, "
                    "games_won = games_won + ?, "
                    "games_lost = games_lost + ? "
                    "WHERE player_id = ?;";

                sqlite3_stmt* update_stmt;
                sqlite3_prepare_v2(db, update_sql, -1, &update_stmt, nullptr);
                sqlite3_bind_int(update_stmt, 1, match_points);
                sqlite3_bind_int(update_stmt, 2, games_won);
                sqlite3_bind_int(update_stmt, 3, games_lost);
                sqlite3_bind_int(update_stmt, 4, player_id);

                sqlite3_step(update_stmt);
                sqlite3_finalize(update_stmt);
            };

            // --- D) Update všech 4 hráčů ---
            update_player(body["p1_a_id"], points_a, score_a, score_b);
            update_player(body["p2_a_id"], points_a, score_a, score_b);
            update_player(body["p1_b_id"], points_b, score_b, score_a);
            update_player(body["p2_b_id"], points_b, score_b, score_a);

            // 4. Potvrzení změn a zavření DB
            sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
            sqlite3_close(db);

            std::cout << "Zapas zapsan do DB!" << std::endl;


            // --- 2. Zde vytáhneš updatované hráče z DB do vektorů ---
            auto [teamA, teamB] = loadPlayersFromDB();

            // ŘAZENÍ: Použijeme tvého Švýcara
            auto swiss_comparator = [](const Player& a, const Player& b) {
                if (a.get_matches_won() != b.get_matches_won()) return a.get_matches_won() > b.get_matches_won();
                if (a.get_games_won() != b.get_games_won()) return a.get_games_won() > b.get_games_won();
                if (a.get_games_lost() != b.get_games_lost()) return a.get_games_lost() < b.get_games_lost();
                return a.get_diff() > b.get_diff();
            };

            std::sort(teamA.begin(), teamA.end(), swiss_comparator);
            std::sort(teamB.begin(), teamB.end(), swiss_comparator);

            // GENEROVÁNÍ: Vytvoříme JSON pro nové kolo
            nlohmann::json new_round = nlohmann::json::array();
            for (size_t i = 0; i < teamA.size() && i < teamB.size(); i += 2) {
                new_round.push_back({
                    {"court", (i / 2) + 1},
                    {"teamA", {teamA[i].get_name(), teamA[i+1].get_name()}},
                    {"teamB", {teamB[i].get_name(), teamB[i+1].get_name()}}
                });
            }

            // Tady ten JSON 'new_round' můžeš třeba někam uložit nebo poslat zpět
            return crow::response(200, new_round.dump());
        });

    app.loglevel(crow::LogLevel::Info);
    app.port(18080).multithreaded().run();
}


