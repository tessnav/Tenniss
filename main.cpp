#include <iostream>
#include <fstream>
#include <sstream>
#include <sqlite3.h>
#include <algorithm>
#include <nlohmann/json_fwd.hpp>
#include <vector>
#include <set>

#include "crow.h"
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

    crow::mustache::set_base("templates");
    crow::SimpleApp app;

    // when landing on page
    CROW_ROUTE(app, "/") //returns an object ... method chaining
    ([]() {
        crow::mustache::context ctx;
       return crow::mustache::load("index.html").render(ctx);
    });

    // --- ADMIN PANEL ---
    CROW_ROUTE(app, "/admin")
    ([](){
        crow::mustache::context ctx;
        auto allPlayers = loadPlayersFromDB();

        // 1. load matches from DB
        sqlite3* db;
        sqlite3_open("turnaj.db", &db);

        // get all 4 players names for each match
        const char* sql = 
            "SELECT m.match_id, p1a.name, p2a.name, p1b.name, p2b.name, m.score_a, m.score_b "
            "FROM matches m "
            "JOIN players p1a ON m.p1_a_id = p1a.player_id "
            "JOIN players p2a ON m.p2_a_id = p2a.player_id "
            "JOIN players p1b ON m.p1_b_id = p1b.player_id "
            "JOIN players p2b ON m.p2_b_id = p2b.player_id "
            "ORDER BY m.match_id DESC;"; // latest matches on top

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
                int sa = sqlite3_column_int(stmt, 5);
                int sb = sqlite3_column_int(stmt, 6);
                m["s_a"] = sa;
                m["s_b"] = sb;
                
                // show tick if match results were entered
                m["is_played"] = (sa + sb > 0); 
                
                matches_list.push_back(std::move(m));
            }
        }
        sqlite3_finalize(stmt);
        sqlite3_close(db);

        // map players for dropdowns
        crow::json::wvalue::list players_list;
        for (const auto& p : allPlayers) {
            players_list.push_back(crow::json::wvalue({{"name", p.get_name()}, {"id", p.get_id()}}));
        }

        ctx["all_players"] = std::move(players_list);
        ctx["active_matches"] = std::move(matches_list);

        return crow::mustache::load("admin.html").render(ctx);
    });

    // --- MATCHES LIST ---
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
                // map data to names
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
        auto all_players = loadPlayersFromDB();

        // sort by: 1. points(matches won) 2. game points difference 3. game points won
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


    // --- API FOR ADDING PLAYERS ---
    CROW_ROUTE(app, "/api/add_player").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req){
        auto params = req.get_body_params();
        const char* name_ptr = params.get("name");

        if (!name_ptr) return crow::response(400, "No name!");

        std::string name = name_ptr;
        sqlite3* db;
        sqlite3_open("turnaj.db", &db);

        const char* sql = "INSERT INTO players (name) VALUES (?);";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::cerr << "Error while adding player: " << sqlite3_errmsg(db) << std::endl;
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);

        crow::response res;
        res.set_header("Location", "/admin");
        res.code = 303;
        return res;
    });

    CROW_ROUTE(app, "/api/newround").methods(crow::HTTPMethod::Post)
    ([]() {
        auto players = loadPlayersFromDB();
        if (players.size() < 4) return crow::response(400, "Not enough players.");

        sqlite3* db;
        sqlite3_open("turnaj.db", &db);
        
        // 1. LOAD PREVIOUS TEAMS
        std::set<std::pair<int, int>> history; 
        const char* hist_sql = "SELECT p1_a_id, p2_a_id, p1_b_id, p2_b_id FROM matches;";
        sqlite3_stmt* h_stmt;
        if (sqlite3_prepare_v2(db, hist_sql, -1, &h_stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(h_stmt) == SQLITE_ROW) {
                int a1 = sqlite3_column_int(h_stmt, 0), a2 = sqlite3_column_int(h_stmt, 1);
                int b1 = sqlite3_column_int(h_stmt, 2), b2 = sqlite3_column_int(h_stmt, 3);
                history.insert({std::min(a1, a2), std::max(a1, a2)});
                history.insert({std::min(b1, b2), std::max(b1, b2)});
            }
        }
        sqlite3_finalize(h_stmt);

        // 2. SORT PLAYERS BY SUCCESS
        std::sort(players.begin(), players.end(), [](const Player& a, const Player& b) {
            if (a.get_matches_won() != b.get_matches_won()) return a.get_matches_won() > b.get_matches_won();
            return a.get_diff() > b.get_diff();
        });

        
        const char* ins_sql = "INSERT INTO matches (p1_a_id, p2_a_id, p1_b_id, p2_b_id, score_a, score_b) VALUES (?, ?, ?, ?, 0, 0);";

        for (size_t i = 0; i + 3 < players.size(); i += 4) {
            int p1 = players[i].get_id(), p2 = players[i+1].get_id(), p3 = players[i+2].get_id(), p4 = players[i+3].get_id();
        
            struct Combo { int t1a, t1b, t2a, t2b; };
            std::vector<Combo> variants = {
                {p1, p4, p2, p3}, // Preferred: 1+4 vs 2+3 (more balanced)
                {p1, p3, p2, p4}, // Otherwise try: 1+3 vs 2+4
                {p1, p2, p3, p4}  // Else: 1+2 vs 3+4
            };

            Combo chosen = variants[0];
            for (const auto& v : variants) {
                auto pairA = std::make_pair(std::min(v.t1a, v.t1b), std::max(v.t1a, v.t1b));
                auto pairB = std::make_pair(std::min(v.t2a, v.t2b), std::max(v.t2a, v.t2b));
                
                // Pokud tato varianta nebyla v historii, vybereme ji
                if (history.find(pairA) == history.end() && history.find(pairB) == history.end()) {
                    chosen = v;
                    break;
                }
            }

            sqlite3_stmt* stmt;
            sqlite3_prepare_v2(db, ins_sql, -1, &stmt, nullptr);
            sqlite3_bind_int(stmt, 1, chosen.t1a);
            sqlite3_bind_int(stmt, 2, chosen.t1b);
            sqlite3_bind_int(stmt, 3, chosen.t2a);
            sqlite3_bind_int(stmt, 4, chosen.t2b);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

        sqlite3_close(db);
        return crow::response(200, "Matches generated with match history checks.");
    });

    CROW_ROUTE(app, "/api/add_match").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req){
        auto params = req.get_body_params();

        const char* s1a = params.get("p1_a");
        const char* s2a = params.get("p2_a");
        const char* s1b = params.get("p1_b");
        const char* s2b = params.get("p2_b");

        // Check admin set all 4/4 players
        if (!s1a || !s2a || !s1b || !s2b || 
            std::string(s1a).empty() || std::string(s2a).empty() || 
            std::string(s1b).empty() || std::string(s2b).empty()) {
            return crow::response(400, "Error: You must choose all 4 players!");
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
            return crow::response(400, "Invalid player IDs.");
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

        // 1. Get old data from database
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

        // 2. Calculate score difference
        int diff_a = new_s_a - old_s_a;
        int diff_b = new_s_b - old_s_b;

        // 1 point for win, 0.5 for a tied match
        auto get_pts = [](int a, int b) { 
            if (a + b == 0) return 0.0; // No result yet
            if (a > b) return 1.0; 
            if (a == b) return 0.5; 
            return 0.0; 
        };

        double diff_pts_a = get_pts(new_s_a, new_s_b) - get_pts(old_s_a, old_s_b);
        double diff_pts_b = get_pts(new_s_b, new_s_a) - get_pts(old_s_b, old_s_a);

        // 3. Update all 4 players
        auto update_p = [&](int id, double p_diff, int gw_diff, int gl_diff) {
            const char* up_sql = "UPDATE players SET matches_won = matches_won + ?, games_won = games_won + ?, games_lost = games_lost + ? WHERE player_id = ?;";
            sqlite3_stmt* up_stmt;
            sqlite3_prepare_v2(db, up_sql, -1, &up_stmt, nullptr);
            sqlite3_bind_double(up_stmt, 1, p_diff);
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

        // 4.Update matches table
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

    // --- API FOR MATCH DELETION ---
    CROW_ROUTE(app, "/api/delete_match").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req){
        auto params = req.get_body_params();
        const char* match_id_ptr = params.get("match_id");

        if (!match_id_ptr) return crow::response(400, "Chybi ID zapasu!");
        int match_id = std::stoi(match_id_ptr);

        sqlite3* db;
        sqlite3_open("turnaj.db", &db);

        // 1. Get score and the players of the match so that points can be subtracted
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

        // subtract points
        auto get_pts = [](int a, int b) { 
            if (a + b == 0) return 0.0;
            if (a > b) return 1.0;
            if (a == b) return 0.5;
            return 0.0;
        };
        double pts_a = get_pts(s_a, s_b);
        double pts_b = get_pts(s_b, s_a);

        // 3. Lambda
        auto subtract_p = [&](int id, double p, int gw, int gl) {
            const char* up_sql = "UPDATE players SET matches_won = matches_won - ?, games_won = games_won - ?, games_lost = games_lost - ? WHERE player_id = ?;";
            sqlite3_stmt* up_stmt;
            sqlite3_prepare_v2(db, up_sql, -1, &up_stmt, nullptr);
            sqlite3_bind_double(up_stmt, 1, p);
            sqlite3_bind_int(up_stmt, 2, gw);
            sqlite3_bind_int(up_stmt, 3, gl);
            sqlite3_bind_int(up_stmt, 4, id);
            sqlite3_step(up_stmt);
            sqlite3_finalize(up_stmt);
        };

        subtract_p(p1a, pts_a, s_a, s_b);
        subtract_p(p2a, pts_a, s_a, s_b);
        subtract_p(p1b, pts_b, s_b, s_a);
        subtract_p(p2b, pts_b, s_b, s_a);

        // 4. Delete the match
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
    app.port(18060).multithreaded().run();
}

void initDatabase() {
    sqlite3* db;
    // Open or create new .db
    if (sqlite3_open("turnaj.db", &db) == SQLITE_OK) {
        
        sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

        // 2. load schema.sql
        std::ifstream file("schema.sql");
        if (!file.is_open()) {
            std::cerr << "Error: schema.sql not found!" << std::endl;
            sqlite3_close(db);
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string sql = buffer.str();

        char* errMsg = nullptr;
        if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::cerr << "Error while initializing tables: " << errMsg << std::endl;
            sqlite3_free(errMsg);
        } else {
            std::cout << "Database successfully initialized." << std::endl;
        }
        sqlite3_close(db);
    }
}
