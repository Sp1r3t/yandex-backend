#include "view.h"

#include <boost/algorithm/string/trim.hpp>
#include <iostream>
#include <stdexcept>

#include "../app/use_cases.h"
#include "../menu/menu.h"

using namespace std::literals;
namespace ph = std::placeholders;

namespace ui {

template <typename T>
void PrintVector(std::ostream& out, const std::vector<T>& vec) {
    int i = 1;
    for (const auto& value : vec) {
        out << i++ << " " << value << "\n";
    }
}

View::View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output)
    : menu_{menu}
    , use_cases_{use_cases}
    , input_{input}
    , output_{output} {
    menu_.AddAction(
        "AddAuthor"s, "name"s, "Adds author"s,
        std::bind(&View::AddAuthor, this, ph::_1));
    menu_.AddAction(
        "AddBook"s, "<pub year> <title>"s, "Adds book"s,
        std::bind(&View::AddBook, this, ph::_1));
    menu_.AddAction(
        "ShowAuthors"s, {}, "Show authors"s,
        std::bind(&View::ShowAuthors, this));
    menu_.AddAction(
        "ShowBooks"s, {}, "Show books"s,
        std::bind(&View::ShowBooks, this));
    menu_.AddAction(
        "ShowAuthorBooks"s, {}, "Show author books"s,
        std::bind(&View::ShowAuthorBooks, this));
}

bool View::AddAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        use_cases_.AddAuthor(std::move(name));
    } catch (const std::exception&) {
        output_ << "Failed to add author"sv << std::endl;
    }
    return true;
}

bool View::AddBook(std::istream& cmd_input) const {
    try {
        int year = 0;
        cmd_input >> year;
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        if (auto author = SelectAuthor()) {
            use_cases_.AddBook(year, title, author->id);
        }
    } catch (const std::exception&) {
        output_ << "Failed to add book"sv << std::endl;
    }
    return true;
}

bool View::ShowAuthors() const {
    const auto authors = GetAuthors();
    for (int i = 0; i < static_cast<int>(authors.size()); ++i) {
        output_ << (i + 1) << " " << authors[i].name << "\n";
    }
    return true;
}

bool View::ShowBooks() const {
    const auto books = use_cases_.GetAllBooks();
    for (int i = 0; i < static_cast<int>(books.size()); ++i) {
        output_ << (i + 1) << " " << books[i].title
                << ", " << books[i].publication_year << "\n";
    }
    return true;
}

bool View::ShowAuthorBooks() const {
    try {
        if (auto author = SelectAuthor()) {
            const auto books = use_cases_.GetAuthorBooks(author->id);
            for (int i = 0; i < static_cast<int>(books.size()); ++i) {
                output_ << (i + 1) << " " << books[i].title
                        << ", " << books[i].publication_year << "\n";
            }
        }
    } catch (const std::exception& e) {
        output_ << e.what() << std::endl;
    }
    return true;
}

std::optional<app::AuthorInfo> View::SelectAuthor() const {
    output_ << "Select author:" << std::endl;
    const auto authors = GetAuthors();
    for (int i = 0; i < static_cast<int>(authors.size()); ++i) {
        output_ << (i + 1) << " " << authors[i].name << "\n";
    }
    output_ << "Enter author # or empty line to cancel" << std::endl;

    std::string str;
    if (!std::getline(input_, str) || str.empty()) {
        return std::nullopt;
    }

    int idx;
    try {
        idx = std::stoi(str);
    } catch (const std::exception&) {
        throw std::runtime_error("Invalid author num");
    }

    --idx;
    if (idx < 0 || idx >= static_cast<int>(authors.size())) {
        throw std::runtime_error("Invalid author num");
    }

    return authors[idx];
}

std::vector<app::AuthorInfo> View::GetAuthors() const {
    return use_cases_.GetAllAuthors();
}

}  // namespace ui
