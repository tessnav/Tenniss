#include <iostream>
#include <fstream>
#include <sstream>

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

void initDatabase();

int main()
{
    initDatabase();

    // Řekni Crowu, že šablony jsou o úroveň výš
    crow::mustache::set_base("templates");
    
    crow::SimpleApp app;

    // when landing on page
    CROW_ROUTE(app, "/") //neni funkce ale makro... ve skutecnosti to returni objekt kteremu pak pridavam do konstruktoru tu lambda fci.. tzv. method chaining
    ([]() {
        crow::mustache::context ctx;
       return crow::mustache::load("index.html").render(ctx);
    });

    // --- ROUTA PRO ZOBRAZENÍ ADMIN PANELU ---
    CROW_ROUTE(app, "/admin")
    ([](){
        crow::mustache::context ctx;
        auto [teamA, teamB] = loadPlayersFromDB();

        // 1. Načtení zápasů z DB
        sqlite3* db;
        sqlite3_open("turnaj.db", &db);

        // SQL dotaz, který vytáhne jména všech 4 hráčů pro každý zápas
        const char* sql = 
            "SELECT m.match_id, p1a.name, p2a.name, p1b.name, p2b.name, m.score_a, m.score_b "
            "FROM matches m "
            "JOIN players p1a ON m.p1_a_id = p1a.player_id "
            "JOIN players p2a ON m.p2_a_id = p2a.player_id "
            "JOIN players p1b ON m.p1_b_id = p1b.player_id "
            "JOIN players p2b ON m.p2_b_id = p2b.player_id "
            "ORDER BY m.match_id DESC;"; // Nejnovější zápasy nahoře

        sqlite3_stmt* stmt;
        crow::json::wvalue::list matches_list;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                crow::json::wvalue m;
                m["id"] = sqlite3_column_int(stmt, 0);
                m["p1a"] = (const char*)sqlite3_column_text(stmt, 1);
                m["p2a"] = (const char*)sqlite3_column_text(stmt, 2);
                m["p1b"] = (const char*)sqlite3_column_text(stmt, 3);
                m["p2b"] = (const char*)sqlite3_column_text(stmt, 4);
                m["s_a"] = sqlite3_column_int(stmt, 5);
                m["s_b"] = sqlite3_column_int(stmt, 6);
                matches_list.push_back(std::move(m));
            }
        }
        sqlite3_finalize(stmt);
        sqlite3_close(db);

        // 2. Mapování hráčů pro dropdowny (to už máš)
        auto map_players = [](std::vector<Player>& players) {
            crow::json::wvalue::list l;
            for (const auto& p : players) {
                l.push_back(crow::json::wvalue({{"name", p.get_name()}, {"id", p.get_id()}}));
            }
            return l;
        };

        ctx["playersA"] = map_players(teamA);
        ctx["playersB"] = map_players(teamB);
        ctx["active_matches"] = std::move(matches_list); // Pošleme zápasy do šablony

        return crow::mustache::load("admin.html").render(ctx);
    });

    CROW_ROUTE(app, "/matches")
    ([]() {
        crow::mustache::context ctx;

        sqlite3* db;
        sqlite3_open("turnaj.db", &db);

        const char* sql = 
            "SELECT p1a.name, p2a.name, p1b.name, p2b.name, m.score_a, m.score_b "
            "FROM matches m "
            "JOIN players p1a ON m.p1_a_id = p1a.player_id "
            "JOIN players p2a ON m.p2_a_id = p2a.player_id "
            "JOIN players p1b ON m.p1_b_id = p1b.player_id "
            "JOIN players p2b ON m.p2_b_id = p2b.player_id "
            "ORDER BY m.match_id DESC;";

        sqlite3_stmt* stmt;
        crow::json::wvalue::list matches_list;

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                crow::json::wvalue m;
                // Mapujeme data přímo na jména z šablony
                m["p1_a"] = (const char*)sqlite3_column_text(stmt, 0);
                m["p2_a"] = (const char*)sqlite3_column_text(stmt, 1);
                m["p1_b"] = (const char*)sqlite3_column_text(stmt, 2);
                m["p2_b"] = (const char*)sqlite3_column_text(stmt, 3);
                m["score_a"] = sqlite3_column_int(stmt, 4);
                m["score_b"] = sqlite3_column_int(stmt, 5);
                matches_list.push_back(std::move(m));
            }
        }
        sqlite3_finalize(stmt);
        sqlite3_close(db);

        ctx["matches"] = std::move(matches_list);
        return crow::mustache::load("matches.html").render(ctx);
    });

    CROW_ROUTE(app, "/players")
    ([]() {
        crow::mustache::context ctx;
        auto [teamA, teamB] = loadPlayersFromDB();

        // Spojíme všechny hráče do jednoho seznamu pro žebříček
        std::vector<Player> all_players = teamA;
        all_players.insert(all_players.end(), teamB.begin(), teamB.end());

        // Seřazení: 1. Body (výhry), 2. Rozdíl gamů, 3. Více vyhraných gamů
        std::sort(all_players.begin(), all_players.end(), [](const Player& a, const Player& b) {
            if (a.get_matches_won() != b.get_matches_won()) return a.get_matches_won() > b.get_matches_won();
            if (a.get_diff() != b.get_diff()) return a.get_diff() > b.get_diff();
            return a.get_games_won() > b.get_games_won();
        });

        crow::json::wvalue::list players_list;
        for (const auto& p : all_players) {
            players_list.push_back({
                {"name", p.get_name()},
                {"wins", p.get_matches_won()},
                {"points_plus", p.get_games_won()},
                {"points_minus", p.get_games_lost()},
                {"score_diff", p.get_diff()}
            });
        }

        ctx["players"] = std::move(players_list);
        return crow::mustache::load("players.html").render(ctx);
    });

    // CROW_ROUTE(app, "/admin")
    // ([](){
    //     crow::mustache::context ctx;
    //     return crow::mustache::load("admin.html").render(ctx);
    // });

    // --- API PRO PŘIDÁNÍ HRÁČE ---
    CROW_ROUTE(app, "/api/add_player").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req){
        // Použijeme get_body_params pro zpracování POST formuláře
        auto params = req.get_body_params();
    
        const char* name_ptr = params.get("name");
        const char* team_ptr = params.get("team_id");

        // Ochrana proti null pointeru (řeší tvůj crash)
        if (!name_ptr || !team_ptr) {
            return crow::response(400, "Chybi jmeno nebo ID tymu!");
        }

        std::string name = name_ptr;
        int team_id = std::stoi(team_ptr);

        sqlite3* db;
        if (sqlite3_open("turnaj.db", &db) != SQLITE_OK) {
            return crow::response(500, "Nelze otevrit databazi");
        }

        const char* sql = "INSERT INTO players (name, team_id) VALUES (?, ?);";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, team_id);
    
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        sqlite3_close(db);

        // Přesměrování zpět na admin stránku
        crow::response res;
        res.set_header("Location", "/admin");
        res.code = 303; // Redirect status code
        return res;
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

    CROW_ROUTE(app, "/api/add_match").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req){
        auto params = req.get_body_params();

        // Načtení hodnot jako text
        const char* s1a = params.get("p1_a");
        const char* s2a = params.get("p2_a");
        const char* s1b = params.get("p1_b");
        const char* s2b = params.get("p2_b");

        // Kontrola, zda admin skutečně vybral všechna 4 pole
        if (!s1a || !s2a || !s1b || !s2b || 
            std::string(s1a).empty() || std::string(s2a).empty() || 
            std::string(s1b).empty() || std::string(s2b).empty()) {
            return crow::response(400, "Chyba: Musite vybrat vsechny 4 hrace!");
        }

        try {
            int p1a = stoi(s1a);
            int p2a = stoi(s2a);
            int p1b = stoi(s1b);
            int p2b = stoi(s2b);

            sqlite3* db;
            sqlite3_open("turnaj.db", &db);
            const char* sql = "INSERT INTO matches (p1_a_id, p2_a_id, p1_b_id, p2_b_id, score_a, score_b) VALUES (?, ?, ?, ?, 0, 0);";
            sqlite3_stmt* stmt;
            sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
            sqlite3_bind_int(stmt, 1, p1a);
            sqlite3_bind_int(stmt, 2, p2a);
            sqlite3_bind_int(stmt, 3, p1b);
            sqlite3_bind_int(stmt, 4, p2b);

            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            sqlite3_close(db);
        } catch (...) {
            return crow::response(400, "Neplatna ID hracu.");
        }

        crow::response res;
        res.set_header("Location", "/admin");
        res.code = 303;
        return res;
    });

    CROW_ROUTE(app, "/api/update_score").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req){
        auto params = req.get_body_params();

        int match_id = stoi(params.get("match_id"));
        int new_s_a = stoi(params.get("score_a"));
        int new_s_b = stoi(params.get("score_b"));

        sqlite3* db;
        sqlite3_open("turnaj.db", &db);

        // 1. Zjistíme, co v DB bylo doteď (staré skóre a ID hráčů)
        int old_s_a = 0, old_s_b = 0;
        int p1a, p2a, p1b, p2b;
        const char* select_sql = "SELECT p1_a_id, p2_a_id, p1_b_id, p2_b_id, score_a, score_b FROM matches WHERE match_id = ?;";
        sqlite3_stmt* sel_stmt;
        sqlite3_prepare_v2(db, select_sql, -1, &sel_stmt, nullptr);
        sqlite3_bind_int(sel_stmt, 1, match_id);

        if (sqlite3_step(sel_stmt) == SQLITE_ROW) {
            p1a = sqlite3_column_int(sel_stmt, 0);
            p2a = sqlite3_column_int(sel_stmt, 1);
            p1b = sqlite3_column_int(sel_stmt, 2);
            p2b = sqlite3_column_int(sel_stmt, 3);
            old_s_a = sqlite3_column_int(sel_stmt, 4);
            old_s_b = sqlite3_column_int(sel_stmt, 5);
        }
        sqlite3_finalize(sel_stmt);

        // 2. Spočítáme rozdíl (o kolik se skóre změnilo)
        int diff_a = new_s_a - old_s_a;
        int diff_b = new_s_b - old_s_b;

        // Upravená pomocná logika pro body (1.0 za výhru, 0.5 za remízu)
        auto get_pts = [](int a, int b) { 
            if (a + b == 0) return 0.0; // Zápas se ještě nehrál
            if (a > b) return 1.0; 
            if (a == b) return 0.5; 
            return 0.0; 
        };

        double diff_pts_a = get_pts(new_s_a, new_s_b) - get_pts(old_s_a, old_s_b);
        double diff_pts_b = get_pts(new_s_b, new_s_a) - get_pts(old_s_b, old_s_a);

        // 3. Update všech 4 hráčů v tabulce players (přičítáme/odčítáme rozdíl)
        auto update_p = [&](int id, double p_diff, int gw_diff, int gl_diff) {
            const char* up_sql = "UPDATE players SET matches_won = matches_won + ?, games_won = games_won + ?, games_lost = games_lost + ? WHERE player_id = ?;";
            sqlite3_stmt* up_stmt;
            sqlite3_prepare_v2(db, up_sql, -1, &up_stmt, nullptr);
            sqlite3_bind_double(up_stmt, 1, p_diff); // Změna na bind_double
            sqlite3_bind_int(up_stmt, 2, gw_diff);
            sqlite3_bind_int(up_stmt, 3, gl_diff);
            sqlite3_bind_int(up_stmt, 4, id);
            sqlite3_step(up_stmt);
            sqlite3_finalize(up_stmt);
        };

        update_p(p1a, diff_pts_a, diff_a, diff_b);
        update_p(p2a, diff_pts_a, diff_a, diff_b);
        update_p(p1b, diff_pts_b, diff_b, diff_a);
        update_p(p2b, diff_pts_b, diff_b, diff_a);

        // 4. Update samotného zápasu v tabulce matches
        const char* m_up = "UPDATE matches SET score_a = ?, score_b = ? WHERE match_id = ?;";
        sqlite3_stmt* m_stmt;
        sqlite3_prepare_v2(db, m_up, -1, &m_stmt, nullptr);
        sqlite3_bind_int(m_stmt, 1, new_s_a);
        sqlite3_bind_int(m_stmt, 2, new_s_b);
        sqlite3_bind_int(m_stmt, 3, match_id);
        sqlite3_step(m_stmt);
        sqlite3_finalize(m_stmt);

        sqlite3_close(db);

        crow::response res;
        res.set_header("Location", "/admin");
        res.code = 303;
        return res;
    });

    // --- API PRO SMAZÁNÍ ZÁPASU ---
    CROW_ROUTE(app, "/api/delete_match").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req){
        auto params = req.get_body_params();
        const char* match_id_ptr = params.get("match_id");

        if (!match_id_ptr) return crow::response(400, "Chybi ID zapasu!");
        int match_id = std::stoi(match_id_ptr);

        sqlite3* db;
        sqlite3_open("turnaj.db", &db);

        // 1. Musíme zjistit skóre a hráče smazaného zápasu, abychom jim odečetli body
        int s_a = 0, s_b = 0;
        int p1a, p2a, p1b, p2b;
        const char* select_sql = "SELECT p1_a_id, p2_a_id, p1_b_id, p2_b_id, score_a, score_b FROM matches WHERE match_id = ?;";
        sqlite3_stmt* sel_stmt;
        sqlite3_prepare_v2(db, select_sql, -1, &sel_stmt, nullptr);
        sqlite3_bind_int(sel_stmt, 1, match_id);

        if (sqlite3_step(sel_stmt) == SQLITE_ROW) {
            p1a = sqlite3_column_int(sel_stmt, 0);
            p2a = sqlite3_column_int(sel_stmt, 1);
            p1b = sqlite3_column_int(sel_stmt, 2);
            p2b = sqlite3_column_int(sel_stmt, 3);
            s_a = sqlite3_column_int(sel_stmt, 4);
            s_b = sqlite3_column_int(sel_stmt, 5);
        }
        sqlite3_finalize(sel_stmt);

        // Upravená logika pro odečtení bodů
        auto get_pts = [](int a, int b) { 
            if (a + b == 0) return 0.0;
            if (a > b) return 1.0;
            if (a == b) return 0.5;
            return 0.0;
        };
        double pts_a = get_pts(s_a, s_b);
        double pts_b = get_pts(s_b, s_a);

        // 3. Pomocná lambda pro odečtení (všimni si znaménka MINUS v SQL)
        auto subtract_p = [&](int id, double p, int gw, int gl) {
            const char* up_sql = "UPDATE players SET matches_won = matches_won - ?, games_won = games_won - ?, games_lost = games_lost - ? WHERE player_id = ?;";
            sqlite3_stmt* up_stmt;
            sqlite3_prepare_v2(db, up_sql, -1, &up_stmt, nullptr);
            sqlite3_bind_double(up_stmt, 1, p); // Změna na bind_double
            sqlite3_bind_int(up_stmt, 2, gw);
            sqlite3_bind_int(up_stmt, 3, gl);
            sqlite3_bind_int(up_stmt, 4, id);
            sqlite3_step(up_stmt);
            sqlite3_finalize(up_stmt);
        };

        // Odečteme statistiky všem 4 hráčům
        subtract_p(p1a, pts_a, s_a, s_b);
        subtract_p(p2a, pts_a, s_a, s_b);
        subtract_p(p1b, pts_b, s_b, s_a);
        subtract_p(p2b, pts_b, s_b, s_a);

        // 4. Nakonec smažeme samotný zápas
        const char* del_sql = "DELETE FROM matches WHERE match_id = ?;";
        sqlite3_stmt* del_stmt;
        sqlite3_prepare_v2(db, del_sql, -1, &del_stmt, nullptr);
        sqlite3_bind_int(del_stmt, 1, match_id);
        sqlite3_step(del_stmt);
        sqlite3_finalize(del_stmt);

        sqlite3_close(db);

        crow::response res;
        res.set_header("Location", "/admin");
        res.code = 303;
        return res;
    });

    app.loglevel(crow::LogLevel::Info);
    app.port(18080).multithreaded().run();
}

void initDatabase() {
    sqlite3* db;
    // Otevře (nebo vytvoří) soubor
    if (sqlite3_open("turnaj.db", &db) == SQLITE_OK) {
        
        // 1. Zapneme cizí klíče
        sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

        // 2. Načteme schema.sql ze souboru
        std::ifstream file("../schema.sql"); // Cesta k tvému schématu
        if (!file.is_open()) {
            std::cerr << "Chyba: Nelze najít schema.sql!" << std::endl;
            sqlite3_close(db);
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string sql = buffer.str();

        // 3. Spustíme celé schéma najednou
        char* errMsg = nullptr;
        if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::cerr << "Chyba při inicializaci tabulek: " << errMsg << std::endl;
            sqlite3_free(errMsg);
        } else {
            std::cout << "Databáze byla úspěšně inicializována." << std::endl;
        }

        // 4. Volitelně: Vložíme základní týmy, pokud tam nejsou
        // Používáme INSERT OR IGNORE, aby to neházelo chybu, když už tam jsou
        const char* initTeams = "INSERT OR IGNORE INTO teams (team_id, name) VALUES (1, 'Tým A'), (2, 'Tým B');";
        sqlite3_exec(db, initTeams, nullptr, nullptr, nullptr);

        sqlite3_close(db);
    }
}
