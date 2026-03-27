#pragma once

#include <array>
#include <iostream>
#include <random>
#include <set>
#include <utility>
#include <vector>

class SeabattleField {
public:
    enum class State {
        UNKNOWN,
        EMPTY,
        KILLED,
        SHIP
    };

    enum class ShotResult {
        MISS = 0,
        HIT = 1,
        KILL = 2
    };

    static const size_t field_size = 8;

    SeabattleField(State default_elem = State::UNKNOWN) {
        field_.fill(default_elem);
    }

    template <class T>
    static SeabattleField GetRandomField(T& random_engine) {
        SeabattleField result;
        while (!TryGetRandomField(random_engine, result)) {
        }
        return result;
    }

    ShotResult Shoot(size_t x, size_t y) {
        if (Get(x, y) != State::SHIP) {
            if (Get(x, y) == State::UNKNOWN) {
                Get(x, y) = State::EMPTY;
            }
            return ShotResult::MISS;
        }

        Get(x, y) = State::KILLED;
        --weight_;

        return IsKilled(static_cast<int>(x), static_cast<int>(y)) ? ShotResult::KILL : ShotResult::HIT;
    }

    void MarkMiss(size_t x, size_t y) {
        if (Get(x, y) == State::UNKNOWN) {
            Get(x, y) = State::EMPTY;
        }
    }

    void MarkHit(size_t x, size_t y) {
        if (Get(x, y) == State::UNKNOWN) {
            --weight_;
            Get(x, y) = State::KILLED;
        }
    }

    void MarkKill(size_t x, size_t y) {
        if (Get(x, y) == State::UNKNOWN) {
            MarkHit(x, y);
        }
        MarkKillInDirection(static_cast<int>(x), static_cast<int>(y), 1, 0);
        MarkKillInDirection(static_cast<int>(x), static_cast<int>(y), -1, 0);
        MarkKillInDirection(static_cast<int>(x), static_cast<int>(y), 0, 1);
        MarkKillInDirection(static_cast<int>(x), static_cast<int>(y), 0, -1);
    }

    State operator()(size_t x, size_t y) const {
        return Get(x, y);
    }

    bool IsKilled(int x, int y) const {
        return IsKilledInDirection(x, y, 1, 0) &&
               IsKilledInDirection(x, y, -1, 0) &&
               IsKilledInDirection(x, y, 0, 1) &&
               IsKilledInDirection(x, y, 0, -1);
    }

    static void PrintDigitLine(std::ostream& out) {
        out << "  1 2 3 4 5 6 7 8  ";
    }

    void PrintLine(std::ostream& out, size_t y) const {
        std::array<char, field_size * 2 - 1> line{};
        for (size_t x = 0; x < field_size; ++x) {
            line[x * 2] = Repr((*this)(x, y));
            if (x + 1 < field_size) {
                line[x * 2 + 1] = ' ';
            }
        }

        char line_char = static_cast<char>('A' + y);

        out.put(line_char);
        out.put(' ');
        out.write(line.data(), static_cast<std::streamsize>(line.size()));
        out.put(' ');
        out.put(line_char);
    }

    bool IsLoser() const {
        return weight_ == 0;
    }

private:
    template <class T>
    static bool TryGetRandomField(T& random_engine, SeabattleField& result) {
        result = SeabattleField(State::EMPTY);

        std::set<std::pair<size_t, size_t>> available_elements;
        std::vector<int> ship_sizes = {4, 3, 3, 2, 2, 2, 1, 1, 1, 1};

        for (size_t y = 0; y < field_size; ++y) {
            for (size_t x = 0; x < field_size; ++x) {
                available_elements.insert(std::make_pair(x, y));
            }
        }

        const int max_attempts = 100;

        for (size_t ship_index = 0; ship_index < ship_sizes.size(); ++ship_index) {
            int length = ship_sizes[ship_index];
            int attempt = 0;
            bool placed = false;

            while (attempt < max_attempts && !placed && !available_elements.empty()) {
                ++attempt;

                size_t pos_index = static_cast<size_t>(random_engine() % available_elements.size());
                size_t direction = static_cast<size_t>(random_engine() % 4);

                auto it = available_elements.begin();
                std::advance(it, static_cast<int>(pos_index));

                size_t x = it->first;
                size_t y = it->second;

                int dx = 0;
                int dy = 0;

                if (direction == 0) dy = 1;
                if (direction == 1) dx = 1;
                if (direction == 2) dy = -1;
                if (direction == 3) dx = -1;

                bool ok = true;
                for (int i = 0; i < length; ++i) {
                    int cx = static_cast<int>(x) + dx * i;
                    int cy = static_cast<int>(y) + dy * i;

                    if (cx < 0 || cx >= static_cast<int>(field_size) ||
                        cy < 0 || cy >= static_cast<int>(field_size)) {
                        ok = false;
                        break;
                    }

                    if (available_elements.count(std::make_pair(static_cast<size_t>(cx), static_cast<size_t>(cy))) == 0) {
                        ok = false;
                        break;
                    }
                }

                if (!ok) {
                    continue;
                }

                for (int i = 0; i < length; ++i) {
                    int cx = static_cast<int>(x) + dx * i;
                    int cy = static_cast<int>(y) + dy * i;

                    result.Get(static_cast<size_t>(cx), static_cast<size_t>(cy)) = State::SHIP;

                    for (int oy = -1; oy <= 1; ++oy) {
                        for (int ox = -1; ox <= 1; ++ox) {
                            int nx = cx + ox;
                            int ny = cy + oy;
                            if (nx >= 0 && nx < static_cast<int>(field_size) &&
                                ny >= 0 && ny < static_cast<int>(field_size)) {
                                available_elements.erase(
                                    std::make_pair(static_cast<size_t>(nx), static_cast<size_t>(ny))
                                );
                            }
                        }
                    }
                }

                placed = true;
            }

            if (!placed) {
                return false;
            }
        }

        return true;
    }

    bool IsKilledInDirection(int x, int y, int dx, int dy) const {
        while (x >= 0 && x < static_cast<int>(field_size) &&
               y >= 0 && y < static_cast<int>(field_size)) {
            State cell = Get(static_cast<size_t>(x), static_cast<size_t>(y));

            if (cell == State::EMPTY) {
                return true;
            }
            if (cell != State::KILLED) {
                return false;
            }

            x += dx;
            y += dy;
        }

        return true;
    }

    void MarkKillInDirection(int x, int y, int dx, int dy) {
        while (x >= 0 && x < static_cast<int>(field_size) &&
               y >= 0 && y < static_cast<int>(field_size)) {
            int x1 = x + dy;
            int y1 = y + dx;
            int x2 = x - dy;
            int y2 = y - dx;

            if (x1 >= 0 && x1 < static_cast<int>(field_size) &&
                y1 >= 0 && y1 < static_cast<int>(field_size)) {
                if (Get(static_cast<size_t>(x1), static_cast<size_t>(y1)) == State::UNKNOWN) {
                    Get(static_cast<size_t>(x1), static_cast<size_t>(y1)) = State::EMPTY;
                }
            }

            if (x2 >= 0 && x2 < static_cast<int>(field_size) &&
                y2 >= 0 && y2 < static_cast<int>(field_size)) {
                if (Get(static_cast<size_t>(x2), static_cast<size_t>(y2)) == State::UNKNOWN) {
                    Get(static_cast<size_t>(x2), static_cast<size_t>(y2)) = State::EMPTY;
                }
            }

            if (Get(static_cast<size_t>(x), static_cast<size_t>(y)) == State::UNKNOWN) {
                Get(static_cast<size_t>(x), static_cast<size_t>(y)) = State::EMPTY;
            }

            if (Get(static_cast<size_t>(x), static_cast<size_t>(y)) != State::KILLED) {
                return;
            }

            x += dx;
            y += dy;
        }
    }

    State& Get(size_t x, size_t y) {
        return field_[x + y * field_size];
    }

    State Get(size_t x, size_t y) const {
        return field_[x + y * field_size];
    }

    static char Repr(State state) {
        switch (state) {
            case State::UNKNOWN: return '?';
            case State::EMPTY: return '.';
            case State::SHIP: return 'o';
            case State::KILLED: return 'x';
        }
        return '\0';
    }

private:
    std::array<State, field_size * field_size> field_{};
    int weight_ = 1 * 4 + 2 * 3 + 3 * 2 + 4 * 1;
};
