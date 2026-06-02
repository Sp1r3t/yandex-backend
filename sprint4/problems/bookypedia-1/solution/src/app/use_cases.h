#pragma once
#include <string>
#include <vector>

namespace app {

struct AuthorInfo {
    std::string id;
    std::string name;
};

struct BookInfo {
    std::string title;
    int publication_year;
};

class UseCases {
public:
    virtual void AddAuthor(const std::string& name) = 0;
    virtual void AddBook(int publication_year, const std::string& title,
                         const std::string& author_id) = 0;

    virtual std::vector<AuthorInfo> GetAllAuthors() const = 0;
    virtual std::vector<BookInfo> GetAllBooks() const = 0;
    virtual std::vector<BookInfo> GetAuthorBooks(const std::string& author_id) const = 0;

protected:
    ~UseCases() = default;
};

}  // namespace app
