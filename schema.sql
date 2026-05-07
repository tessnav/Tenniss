-- Zapnutí cizích klíčů (v SQLite se musí zapínat ručně)
PRAGMA foreign_keys = ON;

-- Tabulka týmů
CREATE TABLE teams (
                       team_id INTEGER PRIMARY KEY AUTOINCREMENT,
                       name TEXT UNIQUE NOT NULL,
                       score INTEGER DEFAULT 0
);

-- Tabulka hráčů
CREATE TABLE players (
                         player_id INTEGER PRIMARY KEY AUTOINCREMENT,
                         name TEXT UNIQUE NOT NULL,
                         elo INTEGER, -- Může být NULL
                         matches_won REAL DEFAULT 0.0,
                         games_won INTEGER DEFAULT 0,
                         games_lost INTEGER DEFAULT 0,
                         team_id INTEGER NOT NULL,
                         FOREIGN KEY (team_id) REFERENCES teams(team_id)
);

-- Tabulka pro zápasy (čtyřhry)
CREATE TABLE matches (
                         match_id INTEGER PRIMARY KEY AUTOINCREMENT,
    -- Hráči týmu A
                         p1_a_id INTEGER NOT NULL,
                         p2_a_id INTEGER NOT NULL,
    -- Hráči týmu B
                         p1_b_id INTEGER NOT NULL,
                         p2_b_id INTEGER NOT NULL,
    -- Výsledné skóre
                         score_a INTEGER NOT NULL,
                         score_b INTEGER NOT NULL,

                         FOREIGN KEY (p1_a_id) REFERENCES players(player_id),
                         FOREIGN KEY (p2_a_id) REFERENCES players(player_id),
                         FOREIGN KEY (p1_b_id) REFERENCES players(player_id),
                         FOREIGN KEY (p2_b_id) REFERENCES players(player_id)

    -- Toto zajistí, že dvojice v týmu A bude v tabulce jen jednou
                       -- UNIQUE(p1_a_id, p2_a_id),
    -- Toto zajistí, že dvojice v týmu B bude v tabulce jen jednou
                       -- UNIQUE(p1_b_id, p2_b_id),
);