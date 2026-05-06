#include <iostream>

#include "crow.h"
#include <sqlite3.h>
#include <nlohmann/json_fwd.hpp>
#include <algorithm>
#include <vector>
#include "player.h"


#define WIN32_LEAN_AND_MEAN
#ifdef _WIN32
    #include <windows.h>
#endif

using namespace std;

int main()
{
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

    CROW_ROUTE(app, "/api/results").methods(crow::HTTPMethod::Post)([](const crow::request& req) {

            // --- 1. Zde zpracuješ request a zapíšeš výsledky do SQLite ---

            // --- 2. Zde vytáhneš updatované hráče z DB do vektorů ---
            std::vector<Player> teamA = /* ... */;
            std::vector<Player> teamB = /* ... */;

            // --- 3. TADY definuješ komparátor a provedeš řazení ---
            auto swiss_comparator = [](const Player& a, const Player& b) {
                if (a.get_matches_won() != b.get_matches_won()) return a.get_matches_won() > b.get_matches_won();
                if (a.get_games_won() != b.get_games_won()) return a.get_games_won() > b.get_games_won();
                if (a.get_games_lost() != b.get_games_lost()) return a.get_games_lost() < b.get_games_lost();
                return a.get_diff() > b.get_diff();
            };

            std::sort(teamA.begin(), teamA.end(), swiss_comparator);
            std::sort(teamB.begin(), teamB.end(), swiss_comparator);

            // --- 4. Zde z poskládaných vektorů vygeneruješ nový JSON rozpis ---

            return crow::response(200, "OK");
        });

    app.loglevel(crow::LogLevel::Info);
    app.port(18080).multithreaded().run();
}
