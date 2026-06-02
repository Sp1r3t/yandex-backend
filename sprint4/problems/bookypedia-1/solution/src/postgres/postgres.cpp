#include "postgres.h"

#include <pqxx/zview.hxx>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

// ---- AuthorRepositoryImpl ----

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(INSERT INTO authors (id, name) VALUES ($1, $2) ON CONFLICT (id) DO UPDATE SET name=$2;)"_zv,
        author.GetId().ToString(), author.GetName());
    work.commit();
}

std::vector<domain::Author> AuthorRepositoryImpl::GetAll() const {
    pqxx::read_transaction tx{connection_};
    const auto rows = tx.exec("SELECT id, name FROM authors ORDER BY name;"_zv);

    std::vector<domain::Author> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        result.emplace_back(
            domain::AuthorId::FromString(row["id"].c_str()),
            row["name"].c_str());
    }
    return result;
}

// ---- BookRepositoryImpl ----

void BookRepositoryImpl::Save(const domain::Book& book) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4);)",
        book.GetId().ToString(),
        book.GetAuthorId().ToString(),
        book.GetTitle(),
        book.GetPublicationYear());
    work.commit();
}

std::vector<domain::Book> BookRepositoryImpl::GetAll() const {
    pqxx::read_transaction tx{connection_};
    const auto rows = tx.exec(
        "SELECT id, author_id, title, publication_year FROM books ORDER BY title;"_zv);

    std::vector<domain::Book> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        result.emplace_back(
            domain::BookId::FromString(row["id"].c_str()),
            domain::AuthorId::FromString(row["author_id"].c_str()),
            row["title"].c_str(),
            row["publication_year"].as<int>());
    }
    return result;
}

std::vector<domain::Book> BookRepositoryImpl::GetByAuthorId(
    const domain::AuthorId& author_id) const {
    pqxx::read_transaction tx{connection_};
    const auto rows = tx.exec_params(
        R"(SELECT id, author_id, title, publication_year
           FROM books
           WHERE author_id = $1
           ORDER BY publication_year, title;)",
        author_id.ToString());

    std::vector<domain::Book> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        result.emplace_back(
            domain::BookId::FromString(row["id"].c_str()),
            domain::AuthorId::FromString(row["author_id"].c_str()),
            row["title"].c_str(),
            row["publication_year"].as<int>());
    }
    return result;
}

// ---- Database ----

Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)} {
    pqxx::work work{connection_};
    work.exec(R"(
CREATE TABLE IF NOT EXISTS authors (
    id   UUID CONSTRAINT author_id_constraint PRIMARY KEY,
    name varchar(100) UNIQUE NOT NULL
);
)"_zv);
    work.exec(R"(
CREATE TABLE IF NOT EXISTS books (
    id               UUID CONSTRAINT book_id_constraint PRIMARY KEY,
    author_id        UUID NOT NULL,
    title            varchar(100) NOT NULL,
    publication_year integer
);
)"_zv);
    work.commit();
}

}  // namespace postgres
