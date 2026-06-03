#pragma once
#include <pqxx/pqxx>

#include "../domain/author.h"
#include "../domain/book.h"

namespace postgres {

class AuthorRepositoryImpl : public domain::AuthorRepository {
public:
    explicit AuthorRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {}

    void Save(const domain::Author& author) override;
    void Update(const domain::AuthorId& id, const std::string& new_name) override;
    void Delete(const domain::AuthorId& id) override;
    std::vector<domain::Author> GetAll() const override;
    std::optional<domain::Author> GetByName(const std::string& name) const override;

private:
    pqxx::connection& connection_;
};

class BookRepositoryImpl : public domain::BookRepository {
public:
    explicit BookRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {}

    void Save(const domain::Book& book, const std::vector<std::string>& tags) override;
    void Delete(const domain::BookId& id) override;
    void Update(const domain::Book& book, const std::vector<std::string>& tags) override;
    std::vector<domain::Book> GetAll() const override;
    std::vector<domain::Book> GetByAuthorId(const domain::AuthorId& author_id) const override;
    std::vector<domain::Book> GetByTitle(const std::string& title) const override;
    std::optional<domain::Book> GetById(const domain::BookId& id) const override;
    std::vector<std::string> GetTags(const domain::BookId& id) const override;

private:
    pqxx::connection& connection_;
};

class Database {
public:
    explicit Database(pqxx::connection connection);
    AuthorRepositoryImpl& GetAuthors() & { return authors_; }
    BookRepositoryImpl& GetBooks() & { return books_; }

private:
    pqxx::connection connection_;
    AuthorRepositoryImpl authors_{connection_};
    BookRepositoryImpl books_{connection_};
};

}  // namespace postgres
