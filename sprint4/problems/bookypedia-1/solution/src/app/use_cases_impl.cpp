#include "use_cases_impl.h"

#include <stdexcept>

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {
using namespace domain;

void UseCasesImpl::AddAuthor(const std::string& name) {
    if (name.empty()) {
        throw std::invalid_argument("Author name cannot be empty");
    }
    authors_.Save({AuthorId::New(), name});
}

void UseCasesImpl::AddBook(int publication_year, const std::string& title,
                           const std::string& author_id) {
    books_.Save({BookId::New(), AuthorId::FromString(author_id), title, publication_year});
}

std::vector<AuthorInfo> UseCasesImpl::GetAllAuthors() const {
    std::vector<AuthorInfo> result;
    for (const auto& author : authors_.GetAll()) {
        result.push_back({author.GetId().ToString(), author.GetName()});
    }
    return result;
}

std::vector<BookInfo> UseCasesImpl::GetAllBooks() const {
    std::vector<BookInfo> result;
    for (const auto& book : books_.GetAll()) {
        result.push_back({book.GetTitle(), book.GetPublicationYear()});
    }
    return result;
}

std::vector<BookInfo> UseCasesImpl::GetAuthorBooks(const std::string& author_id) const {
    std::vector<BookInfo> result;
    for (const auto& book : books_.GetByAuthorId(AuthorId::FromString(author_id))) {
        result.push_back({book.GetTitle(), book.GetPublicationYear()});
    }
    return result;
}

}  // namespace app
