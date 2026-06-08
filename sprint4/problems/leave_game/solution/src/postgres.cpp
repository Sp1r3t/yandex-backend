#include "postgres.h"

namespace postgres {

namespace {

void InitSchema(pqxx::connection& conn) {
    pqxx::work tx{conn};
    tx.exec(R"(
        CREATE EXTENSION IF NOT EXISTS "pgcrypto";
        CREATE TABLE IF NOT EXISTS retired_players (
            id      UUID        PRIMARY KEY DEFAULT gen_random_uuid(),
            name    VARCHAR     NOT NULL,
            score   INTEGER     NOT NULL,
            play_time_ms BIGINT NOT NULL
        );
        CREATE INDEX IF NOT EXISTS retired_players_score_time_name_idx
            ON retired_players (score DESC, play_time_ms ASC, name ASC);
    )");
    tx.commit();
}

}  // namespace

Database::Database(const std::string& db_url, size_t pool_size)
    : pool_(pool_size, [&db_url] {
          return std::make_shared<pqxx::connection>(db_url);
      }) {
    auto conn = pool_.GetConnection();
    InitSchema(*conn);
}

void Database::SaveRetiredPlayer(const std::string& name, int score, int64_t play_time_ms) {
    auto conn = pool_.GetConnection();
    pqxx::work tx{*conn};
    tx.exec_params(
        "INSERT INTO retired_players (name, score, play_time_ms) VALUES ($1, $2, $3)",
        name, score, play_time_ms);
    tx.commit();
}

std::vector<RetiredPlayer> Database::GetRetiredPlayers(int start, int max_items) {
    auto conn = pool_.GetConnection();
    pqxx::read_transaction tx{*conn};
    const auto rows = tx.exec_params(
        "SELECT name, score, play_time_ms FROM retired_players "
        "ORDER BY score DESC, play_time_ms ASC, name ASC "
        "LIMIT $1 OFFSET $2",
        max_items, start);

    std::vector<RetiredPlayer> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        RetiredPlayer p;
        p.name = row[0].as<std::string>();
        p.score = row[1].as<int>();
        p.play_time = static_cast<double>(row[2].as<int64_t>()) / 1000.0;
        result.push_back(std::move(p));
    }
    return result;
}

}  // namespace postgres
