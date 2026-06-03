#pragma once
#include <optional>
#include <string>
#include <vector>

namespace app {

struct AuthorInfo {
    std::string id;
    std::string name;
};

struct BookInfo {
    std::string id;
    std::string title;
    std::string author_name;
    int publication_year;
};

struct BookDetails {
    std::string id;
    std::string title;
    std::string author_name;
    int publication_year;
    std::vector<std::string> tags;
};

class UseCases {
public:
    virtual void AddAuthor(const std::string& name) = 0;
    virtual void AddBook(int publication_year, const std::string& title,
                         const std::string& author_id,
                         const std::vector<std::string>& tags) = 0;
    virtual void DeleteAuthor(const std::string& author_id) = 0;
    virtual void EditAuthor(const std::string& author_id, const std::string& new_name) = 0;
    virtual void DeleteBook(const std::string& book_id) = 0;
    virtual void EditBook(const std::string& book_id, const std::string& new_title,
                          int new_year, const std::vector<std::string>& new_tags) = 0;

    virtual std::vector<AuthorInfo> GetAllAuthors() const = 0;
    virtual std::optional<AuthorInfo> GetAuthorByName(const std::string& name) const = 0;
    virtual std::vector<BookInfo> GetAllBooks() const = 0;
    virtual std::vector<BookInfo> GetBooksByTitle(const std::string& title) const = 0;
    virtual std::optional<BookDetails> GetBookDetails(const std::string& book_id) const = 0;
    virtual std::vector<BookInfo> GetAuthorBooks(const std::string& author_id) const = 0;

protected:
    ~UseCases() = default;
};

}  // namespace app
