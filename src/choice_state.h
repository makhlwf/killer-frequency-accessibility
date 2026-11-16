#pragma once
#include <string>
#include <vector>

struct ChoiceState {
    std::string question;
    std::vector<std::string> answers, ids;
    int selected = 0;
    unsigned generation = 0;
    bool active = false;
    void clear() {
        active = false;
        question.clear();
        answers.clear();
        ids.clear();
        selected = 0;
        ++generation;
    }
    bool set(std::string q, std::vector<std::string> a, std::vector<std::string> i) {
        clear();
        if (a.empty() || a.size() != i.size())
            return false;
        question = std::move(q);
        answers = std::move(a);
        ids = std::move(i);
        active = true;
        return true;
    }
    bool move(int direction) {
        if (!active || answers.empty())
            return false;
        selected = (selected + direction + static_cast<int>(answers.size())) %
                   static_cast<int>(answers.size());
        return true;
    }
    std::string current() const {
        if (!active || selected < 0 || selected >= static_cast<int>(answers.size()))
            return "No active answers.";
        return "Answer " + std::to_string(selected + 1) + " of " + std::to_string(answers.size()) +
               ". " + answers[selected];
    }
    std::string all() const {
        if (!active)
            return "No active question.";
        std::string out = question.empty() ? "Choose a response." : question;
        for (size_t i = 0; i < answers.size(); ++i)
            out += " Answer " + std::to_string(i + 1) + " of " + std::to_string(answers.size()) +
                   ". " + answers[i];
        return out;
    }
};
