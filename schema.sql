PRAGMA foreign_keys = ON;

CREATE TABLE players (
    player_id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT UNIQUE NOT NULL,
    elo INTEGER, 
    matches_won REAL DEFAULT 0.0,
    games_won INTEGER DEFAULT 0,
    games_lost INTEGER DEFAULT 0
);

CREATE TABLE matches (
    match_id INTEGER PRIMARY KEY AUTOINCREMENT,
    p1_a_id INTEGER NOT NULL,
    p2_a_id INTEGER NOT NULL,
    p1_b_id INTEGER NOT NULL,
    p2_b_id INTEGER NOT NULL,
    score_a INTEGER NOT NULL,
    score_b INTEGER NOT NULL,
    FOREIGN KEY (p1_a_id) REFERENCES players(player_id),
    FOREIGN KEY (p2_a_id) REFERENCES players(player_id),
    FOREIGN KEY (p1_b_id) REFERENCES players(player_id),
    FOREIGN KEY (p2_b_id) REFERENCES players(player_id)
);