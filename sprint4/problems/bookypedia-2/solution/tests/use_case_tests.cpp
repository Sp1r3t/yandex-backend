#include <catch2/catch_test_macros.hpp>

#include "../src/app/use_cases_impl.h"
#include "../src/domain/author.h"
#include "../src/domain/book.h"

namespace {

struct MockAuthorRepository : domain::AuthorRepository {
    std::vector<domain::Author> saved_authors;

    void Save(const domain::Author& author) override {
        saved_authors.emplace_back(author);
    }

    void Update(const domain::AuthorId&, const std::string&) override {}

    void Delete(const domain::AuthorId&) override {}

    std::vector<domain::Author> GetAll() const override {
        return saved_authors;
    }

    std::optional<domain::Author> GetByName(const std::string&) const override {
        return std::nullopt;
    }
};

struct MockBookRepository : domain::BookRepository {
    std::vector<domain::Book> saved_books;

    void Save(const domain::Book& book, const std::vector<std::string>&) override {
        saved_books.emplace_back(book);
    }

    void Delete(const domain::BookId&) override {}

    void Update(const domain::Book&, const std::vector<std::string>&) override {}

    std::vector<domain::Book> GetAll() const override {
        return saved_books;
    }

    std::vector<domain::Book> GetByAuthorId(
        const domain::AuthorId& author_id) const override {
        std::vector<domain::Book> result;
        for (const auto& book : saved_books) {
            if (book.GetAuthorId() == author_id) {
                result.push_back(book);
            }
        }
        return result;
    }

    std::vector<domain::Book> GetByTitle(const std::string&) const override {
        return {};
    }

    std::optional<domain::Book> GetById(const domain::BookId&) const override {
        return std::nullopt;
    }

    std::vector<std::string> GetTags(const domain::BookId&) const override {
        return {};
    }
};

struct Fixture {
    MockAuthorRepository authors;
    MockBookRepository books;
};

}  // namespace

SCENARIO_METHOD(Fixture, "Book Adding") {
    GIVEN("Use cases") {
        app::UseCasesImpl use_cases{authors, books};

        WHEN("Adding an author") {
            const auto author_name = "Joanne Rowling";
            use_cases.AddAuthor(author_name);

            THEN("author with the specified name is saved to repository") {
                REQUIRE(authors.saved_authors.size() == 1);
                CHECK(authors.saved_authors.at(0).GetName() == author_name);
                CHECK(authors.saved_authors.at(0).GetId() != domain::AuthorId{});
            }
        }

        WHEN("Adding a book") {
            const auto author_name = "Joanne Rowling";
            use_cases.AddAuthor(author_name);

            REQUIRE(authors.saved_authors.size() == 1);
            const auto author = authors.saved_authors.at(0);

            const auto title = "Harry Potter and the Philosopher's Stone";
            constexpr int pub_year = 1997;
            use_cases.AddBook(pub_year, title, author.GetId().ToString(), {});

            THEN("book is saved to repository") {
                REQUIRE(books.saved_books.size() == 1);
                CHECK(books.saved_books.at(0).GetTitle() == title);
                CHECK(books.saved_books.at(0).GetPublicationYear() == pub_year);
                CHECK(books.saved_books.at(0).GetAuthorId() == author.GetId());
                CHECK(books.saved_books.at(0).GetId() != domain::BookId{});
            }
        }
    }
}
