//
// Created by 004ha on 06.05.2026.
//

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
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Player, player_id, name, elo, matches_won, games_won, games_lost, team_id)
};

#endif //TENNISS_PLAYER_H