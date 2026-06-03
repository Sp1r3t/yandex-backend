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
                 const std::string& author_id,
                 const std::vector<std::string>& tags) override;
    void DeleteAuthor(const std::string& author_id) override;
    void EditAuthor(const std::string& author_id, const std::string& new_name) override;
    void DeleteBook(const std::string& book_id) override;
    void EditBook(const std::string& book_id, const std::string& new_title,
                  int new_year, const std::vector<std::string>& new_tags) override;

    std::vector<AuthorInfo> GetAllAuthors() const override;
    std::optional<AuthorInfo> GetAuthorByName(const std::string& name) const override;
    std::vector<BookInfo> GetAllBooks() const override;
    std::vector<BookInfo> GetBooksByTitle(const std::string& title) const override;
    std::optional<BookDetails> GetBookDetails(const std::string& book_id) const override;
    std::vector<BookInfo> GetAuthorBooks(const std::string& author_id) const override;

private:
    domain::AuthorRepository& authors_;
    domain::BookRepository& books_;
};

}  // namespace app
