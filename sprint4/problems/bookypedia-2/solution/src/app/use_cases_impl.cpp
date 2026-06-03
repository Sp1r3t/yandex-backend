#include "use_cases_impl.h"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>

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
                           const std::string& author_id,
                           const std::vector<std::string>& tags) {
    auto book_id = BookId::New();
    books_.Save({book_id, AuthorId::FromString(author_id), title, publication_year}, tags);
}

void UseCasesImpl::DeleteAuthor(const std::string& author_id) {
    authors_.Delete(AuthorId::FromString(author_id));
}

void UseCasesImpl::EditAuthor(const std::string& author_id, const std::string& new_name) {
    if (new_name.empty()) {
        throw std::invalid_argument("Author name cannot be empty");
    }
    authors_.Update(AuthorId::FromString(author_id), new_name);
}

void UseCasesImpl::DeleteBook(const std::string& book_id) {
    books_.Delete(BookId::FromString(book_id));
}

void UseCasesImpl::EditBook(const std::string& book_id, const std::string& new_title,
                             int new_year, const std::vector<std::string>& new_tags) {
    books_.Update({BookId::FromString(book_id), AuthorId{}, new_title, new_year}, new_tags);
}

std::vector<AuthorInfo> UseCasesImpl::GetAllAuthors() const {
    std::vector<AuthorInfo> result;
    for (const auto& author : authors_.GetAll()) {
        result.push_back({author.GetId().ToString(), author.GetName()});
    }
    return result;
}

std::optional<AuthorInfo> UseCasesImpl::GetAuthorByName(const std::string& name) const {
    auto author = authors_.GetByName(name);
    if (!author) {
        return std::nullopt;
    }
    return AuthorInfo{author->GetId().ToString(), author->GetName()};
}

std::vector<BookInfo> UseCasesImpl::GetAllBooks() const {
    auto all_books = books_.GetAll();
    auto all_authors = authors_.GetAll();

    std::unordered_map<std::string, std::string> author_map;
    for (const auto& a : all_authors) {
        author_map[a.GetId().ToString()] = a.GetName();
    }

    std::vector<BookInfo> result;
    result.reserve(all_books.size());
    for (const auto& book : all_books) {
        result.push_back({
            book.GetId().ToString(),
            book.GetTitle(),
            author_map[book.GetAuthorId().ToString()],
            book.GetPublicationYear()
        });
    }

    std::sort(result.begin(), result.end(), [](const BookInfo& a, const BookInfo& b) {
        if (a.title != b.title) return a.title < b.title;
        if (a.author_name != b.author_name) return a.author_name < b.author_name;
        return a.publication_year < b.publication_year;
    });

    return result;
}

std::vector<BookInfo> UseCasesImpl::GetBooksByTitle(const std::string& title) const {
    auto matching = books_.GetByTitle(title);
    auto all_authors = authors_.GetAll();

    std::unordered_map<std::string, std::string> author_map;
    for (const auto& a : all_authors) {
        author_map[a.GetId().ToString()] = a.GetName();
    }

    std::vector<BookInfo> result;
    result.reserve(matching.size());
    for (const auto& book : matching) {
        result.push_back({
            book.GetId().ToString(),
            book.GetTitle(),
            author_map[book.GetAuthorId().ToString()],
            book.GetPublicationYear()
        });
    }

    std::sort(result.begin(), result.end(), [](const BookInfo& a, const BookInfo& b) {
        if (a.author_name != b.author_name) return a.author_name < b.author_name;
        return a.publication_year < b.publication_year;
    });

    return result;
}

std::optional<BookDetails> UseCasesImpl::GetBookDetails(const std::string& book_id) const {
    auto book_opt = books_.GetById(BookId::FromString(book_id));
    if (!book_opt) {
        return std::nullopt;
    }

    const auto& book = *book_opt;
    auto all_authors = authors_.GetAll();

    std::string author_name;
    for (const auto& a : all_authors) {
        if (a.GetId() == book.GetAuthorId()) {
            author_name = a.GetName();
            break;
        }
    }

    auto tags = books_.GetTags(book.GetId());

    return BookDetails{
        book.GetId().ToString(),
        book.GetTitle(),
        author_name,
        book.GetPublicationYear(),
        std::move(tags)
    };
}

std::vector<BookInfo> UseCasesImpl::GetAuthorBooks(const std::string& author_id) const {
    std::vector<BookInfo> result;
    for (const auto& book : books_.GetByAuthorId(AuthorId::FromString(author_id))) {
        result.push_back({book.GetId().ToString(), book.GetTitle(), {}, book.GetPublicationYear()});
    }
    return result;
}

}  // namespace app
