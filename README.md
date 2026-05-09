## Tennis Tournament Manager

A lightweight C++ web application designed to manage doubles tennis tournaments. Built with the Crow web framework and SQLite3, it features an automated matchmaking system based on a balanced Swiss-system variant.

## Features

- **Unified Player Management**: Register players into a single pool. No predefined teams required; pairings are generated dynamically.
- **Automated Matchmaking**:
    - **Balanced Pairing**: Uses a `1+4 vs 2+3` logic (the strongest player pairs with the 4th strongest against the 2nd and 3rd).
    - **History Tracking**: The generator automatically checks match history to ensure players don't play with the same partner repeatedly.
    - **Manual Overrides**: Admin can manually create specific matchups via the dashboard.
- **Real-time Leaderboard**:
    - Scoring: 1.0 point for a win, 0.5 points for a draw.
    - Tie-breaking: Uses game difference (games won - games lost) and total games won.
- **Interactive Admin Dashboard**:
    - Status indicators (Checkmarks/Circles) to track which match results have been recorded.
    - Safety prompts to prevent generating new rounds before current results are saved.
    - Ability to delete matches and automatically roll back player statistics.
- **Public View**: Dedicated pages for current match schedules and the live leaderboard.

## 🛠 Tech Stack

- **Backend**: C++
- **Web Framework**: [Crow](https://crowcpp.org/)
- **Database**: SQLite3
- **Templating**: Mustache
- **JSON**: [nlohmann/json](https://github.com/nlohmann/json)
- **Frontend**: HTML5, CSS
