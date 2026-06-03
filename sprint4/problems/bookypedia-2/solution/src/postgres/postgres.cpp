#include "postgres.h"

#include <pqxx/pqxx>
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

void AuthorRepositoryImpl::Update(const domain::AuthorId& id, const std::string& new_name) {
    pqxx::work work{connection_};
    auto result = work.exec_params(
        R"(UPDATE authors SET name=$2 WHERE id=$1;)"_zv,
        id.ToString(), new_name);
    if (result.affected_rows() == 0) {
        throw std::runtime_error("Author not found");
    }
    work.commit();
}

void AuthorRepositoryImpl::Delete(const domain::AuthorId& id) {
    pqxx::work work{connection_};
    // Manually cascade: book_tags -> books -> author
    // (works regardless of whether ON DELETE CASCADE is set in the schema)
    work.exec_params(
        R"(DELETE FROM book_tags WHERE book_id IN (SELECT id FROM books WHERE author_id=$1);)",
        id.ToString());
    work.exec_params(
        R"(DELETE FROM books WHERE author_id=$1;)",
        id.ToString());
    auto result = work.exec_params(
        R"(DELETE FROM authors WHERE id=$1;)"_zv,
        id.ToString());
    if (result.affected_rows() == 0) {
        throw std::runtime_error("Author not found");
    }
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

std::optional<domain::Author> AuthorRepositoryImpl::GetByName(const std::string& name) const {
    pqxx::read_transaction tx{connection_};
    const auto rows = tx.exec_params(
        R"(SELECT id, name FROM authors WHERE name=$1;)"_zv,
        name);
    if (rows.empty()) {
        return std::nullopt;
    }
    return domain::Author{
        domain::AuthorId::FromString(rows[0]["id"].c_str()),
        rows[0]["name"].c_str()};
}

// ---- BookRepositoryImpl ----

void BookRepositoryImpl::Save(const domain::Book& book, const std::vector<std::string>& tags) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4);)",
        book.GetId().ToString(),
        book.GetAuthorId().ToString(),
        book.GetTitle(),
        book.GetPublicationYear());
    for (const auto& tag : tags) {
        work.exec_params(
            R"(INSERT INTO book_tags (book_id, tag) VALUES ($1, $2);)",
            book.GetId().ToString(), tag);
    }
    work.commit();
}

void BookRepositoryImpl::Delete(const domain::BookId& id) {
    pqxx::work work{connection_};
    // Delete associated tags first (to avoid FK violation if CASCADE is not configured)
    work.exec_params(
        R"(DELETE FROM book_tags WHERE book_id=$1;)",
        id.ToString());
    auto result = work.exec_params(
        R"(DELETE FROM books WHERE id=$1;)"_zv,
        id.ToString());
    if (result.affected_rows() == 0) {
        throw std::runtime_error("Book not found");
    }
    work.commit();
}

void BookRepositoryImpl::Update(const domain::Book& book, const std::vector<std::string>& tags) {
    pqxx::work work{connection_};
    auto result = work.exec_params(
        R"(UPDATE books SET title=$2, publication_year=$3 WHERE id=$1;)"_zv,
        book.GetId().ToString(),
        book.GetTitle(),
        book.GetPublicationYear());
    if (result.affected_rows() == 0) {
        throw std::runtime_error("Book not found");
    }
    work.exec_params(
        R"(DELETE FROM book_tags WHERE book_id=$1;)"_zv,
        book.GetId().ToString());
    for (const auto& tag : tags) {
        work.exec_params(
            R"(INSERT INTO book_tags (book_id, tag) VALUES ($1, $2);)",
            book.GetId().ToString(), tag);
    }
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

std::vector<domain::Book> BookRepositoryImpl::GetByTitle(const std::string& title) const {
    pqxx::read_transaction tx{connection_};
    const auto rows = tx.exec_params(
        R"(SELECT id, author_id, title, publication_year
           FROM books
           WHERE title = $1
           ORDER BY title;)",
        title);

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

std::optional<domain::Book> BookRepositoryImpl::GetById(const domain::BookId& id) const {
    pqxx::read_transaction tx{connection_};
    const auto rows = tx.exec_params(
        R"(SELECT id, author_id, title, publication_year FROM books WHERE id=$1;)"_zv,
        id.ToString());
    if (rows.empty()) {
        return std::nullopt;
    }
    return domain::Book{
        domain::BookId::FromString(rows[0]["id"].c_str()),
        domain::AuthorId::FromString(rows[0]["author_id"].c_str()),
        rows[0]["title"].c_str(),
        rows[0]["publication_year"].as<int>()};
}

std::vector<std::string> BookRepositoryImpl::GetTags(const domain::BookId& id) const {
    pqxx::read_transaction tx{connection_};
    const auto rows = tx.exec_params(
        R"(SELECT tag FROM book_tags WHERE book_id=$1 ORDER BY tag;)"_zv,
        id.ToString());
    std::vector<std::string> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        result.emplace_back(row["tag"].c_str());
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
    author_id        UUID NOT NULL REFERENCES authors(id) ON DELETE CASCADE,
    title            varchar(100) NOT NULL,
    publication_year integer
);
)"_zv);
    work.exec(R"(
CREATE TABLE IF NOT EXISTS book_tags (
    book_id UUID NOT NULL REFERENCES books(id) ON DELETE CASCADE,
    tag     varchar(30) NOT NULL
);
)"_zv);
    work.commit();
}

}  // namespace postgres
