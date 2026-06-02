#pragma once
#include "../domain/author_fwd.h"
#include "use_cases.h"

namespace domain {
class BookRepository;
}  // namespace domain

namespace app {

class UseCasesImpl : public UseCases {
public:
    explicit UseCasesImpl(domain::AuthorRepository& authors,
                          domain::BookRepository& books)
        : authors_{authors}
        , books_{books} {
    }

    void AddAuthor(const std::string& name) override;
    void AddBook(int publication_year, const std::string& title,
                 const std::string& author_id) override;

    std::vector<AuthorInfo> GetAllAuthors() const override;
    std::vector<BookInfo> GetAllBooks() const override;
    std::vector<BookInfo> GetAuthorBooks(const std::string& author_id) const override;

private:
    domain::AuthorRepository& authors_;
    domain::BookRepository& books_;
};

}  // namespace app
