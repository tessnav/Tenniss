//
// Created by 004ha on 06.05.2026.
//
#define _WIN32_WINNT 0x0A00 // Přidej toto jako první řádek!
#ifndef TENNISS_PLAYER_H
#define TENNISS_PLAYER_H

#include <string>
#include <optional>
#include <nlohmann/json.hpp>

class Player {
private:
    int player_id;
    std::string name;
    std::optional<int> elo;
    int matches_won = 0;
    int games_won = 0;
    int games_lost = 0;
    int team_id;

public:
    // Defaultní konstruktor (nutný pro nlohmann_json, pokud bys přidával i vlastní)
    Player() = default;
    Player(int id, std::string n, std::optional<int> e, int w, int gw, int gl, int t_id)
        : player_id(id), name(n), elo(e), matches_won(w), games_won(gw), games_lost(gl), team_id(t_id) {}
    // Gettery pro tvůj řadící algoritmus (aby si mohl číst data, i když jsou private)
    int get_matches_won() const { return matches_won; }
    int get_games_won() const { return games_won; }
    int get_games_lost() const { return games_lost; }
    std::string get_name() const { return name; }
    int get_id() const { return player_id; }

    // Analytická pomocná metoda
    int get_diff() const {
        return games_won - games_lost;
    }

    // Volitelný setter: Kdybys chtěl po dohrání zápasu updatovat statistiky bez přímého zásahu do proměnných
    void add_match_result(bool won, int g_won, int g_lost) {
        if (won) matches_won++;
        games_won += g_won;
        games_lost += g_lost;
    }

    // INTRUSIVE makro: Musí být uvnitř třídy (ideálně na konci v public nebo private sekci)
    // Stará se o obousměrný překlad: JSON <-> C++ objekt
    // --- Manuální C++ JSON Serializace ---

    // Převod z C++ do JSONu (odesílání na frontend)
    friend void to_json(nlohmann::json& j, const Player& p) {
        j = nlohmann::json{
                {"player_id", p.player_id},
                {"name", p.name},
                {"matches_won", p.matches_won},
                {"games_won", p.games_won},
                {"games_lost", p.games_lost},
                {"team_id", p.team_id}
        };
        // Ošetření prázdného ELO
        if (p.elo.has_value()) {
            j["elo"] = p.elo.value();
        } else {
            j["elo"] = nullptr;
        }
    }

    // Převod z JSONu do C++ (přijímání dat)
    friend void from_json(const nlohmann::json& j, Player& p) {
        j.at("player_id").get_to(p.player_id);
        j.at("name").get_to(p.name);
        j.at("matches_won").get_to(p.matches_won);
        j.at("games_won").get_to(p.games_won);
        j.at("games_lost").get_to(p.games_lost);
        j.at("team_id").get_to(p.team_id);

        if (j.contains("elo") && !j["elo"].is_null()) {
            p.elo = j["elo"].get<int>();
        } else {
            p.elo = std::nullopt;
        }
    }
};

#endif //TENNISS_PLAYER_H