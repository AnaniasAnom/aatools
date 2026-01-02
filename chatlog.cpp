#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <chrono>
#include <format>
#include <regex>
#include <filesystem>
#include <cassert>
#include <unistd.h>


using std::string;
using std::filesystem::path;

const string datadir = "chatlogs";
const string appname = "chatlog";
const string editor = "/usr/bin/mousepad";

int status = 0;

void warn(const string& s) {
  status = 1;
  std::cerr << s << "\n";
}

static string date_string(std::chrono::time_point<std::chrono::system_clock> tp) {
  return std::format("{:%Y%m%d}", tp);
}

std::regex days_regex{"^-[0-9]{1,5}$"};
std::regex mmdd_regex{"^[0-9]{4}$"};
std::regex ymd_regex{"^20[0-9]{6}$"};

class cache {
public:
  cache();
  string get_subject();
  string set_subject(string name);
private:
  path file_path;
  char buffer[40];
};

// global recently-used cache
cache saved;

static path base_dir() {
  auto home = std::getenv("HOME");
  if (home)
    return path{home};
  else return path{"."};
}

cache::cache() {
  auto home = std::getenv("HOME");
  if (home) {
    path dir(home);
    dir /= ".cache";
    if (std::filesystem::is_directory(dir)) {
      file_path = dir / appname;
      return;
    }
  }
  auto dotfile = string{"."} + appname;
  file_path = path{dotfile};
}

string cache::get_subject() {
  std::ifstream file(file_path);
  if (file.good()) {
    file.read(buffer, sizeof(buffer));
    if (file.gcount() == sizeof(buffer)) {
      string all{buffer, sizeof(buffer)};
      all.erase(all.find_last_not_of(" ") + 1);
      return all;
    } else std::cerr << "read " << file.gcount() << "\n";
  }
  return string{"NONE"};
}

string cache::set_subject(string name) {
  if (name.size() > sizeof(buffer)) {
    warn("name is too long");
  }
  std::ofstream file(file_path);
  if (!file.good()) {
    warn("cannot open cache file");
    return name;
  }
  file << name;
  int padding = sizeof(buffer) - name.size();
  if (padding > 0) {
    std::fill(buffer, buffer + sizeof(buffer), ' ');
    file.write(buffer, padding);
  }
  return name;
}

static path full_path(string subject, string datestr) {
  path p{base_dir() / datadir};
  return p / subject / datestr;
}

class match_name {
public:
  match_name(const string& prefix) {
    path chat_path{base_dir() / datadir};
    for (std::filesystem::directory_iterator dir{chat_path};
         dir != std::filesystem::directory_iterator{};
         ++dir) {
      auto fn = dir->path().filename().string();
      if (fn == prefix) {
        found = fn;
        break;
      } else {
        if (fn.starts_with(prefix)) {
          possibles.push_back(fn);
        }
      }
    }
    if (possibles.size() == 1)
      found = possibles.front();
  }
  bool matched() const { return !found.empty(); }
  string match() const { return found; }
  bool any() const { return (!possibles.empty() && (possibles.size() < 10)); }
  size_t count() const { return possibles.size(); }
  string get(size_t n) const {
    assert(n < possibles.size());
    if (n < possibles.size()) return possibles[n];
    return "ERROR";
  }
  std::vector<string>::const_iterator begin() const { return possibles.begin(); }
  std::vector<string>::const_iterator end() const { return possibles.end(); }
private:
  string found;
  std::vector<string> possibles;
};


class option_argument {
public:
  option_argument(std::span<string>& args);
  bool matches(const string& s) const { return s == value; };
  operator bool() const { return !value.empty(); }
private:
  string value;
};

option_argument::option_argument(std::span<string>& args) {
  if (!args.empty()) {
    const auto& a = args.front();
    if ((a == "create") ||
        (a == "ls") ||
        (a == "-complete") ||
        (a == "latest")) {
      value = a;
      args = args.subspan(1);
    }
  }
}

class date_argument {
public:
  date_argument(std::span<string>& args);
  operator string() const { return value; }
  bool was_set() const { return !defaulted; }
private:
  string value;
  bool defaulted;
};

date_argument::date_argument(std::span<string>& args) : defaulted{false} {
  auto today = std::chrono::time_point_cast<std::chrono::days>(std::chrono::system_clock::now());
  
  if (!args.empty()) {
    const string& a = args.front();
    if (a == "yesterday") {
      auto dt = today - std::chrono::days(1);
      value = date_string(dt);
      args = args.subspan(1); return;
    }
    if (std::regex_match(a, days_regex)) {
      auto offset = std::stoi(a);
      value = date_string(today + std::chrono::days(offset));
      args = args.subspan(1); return;
    }
    if (std::regex_match(a, mmdd_regex)) {
      std::chrono::year_month_day dt{today};
      value = std::format("{:04d}{}", static_cast<int>(dt.year()), a);
      args = args.subspan(1); return;
    }
    if (std::regex_match(a, ymd_regex)) {
      value = a;
      args = args.subspan(1); return;
    }
  }
  defaulted = true;
  value = date_string(today);
}

std::ostream& operator << (std::ostream& str, const date_argument& x) { return str << static_cast<string>(x); }

int open_file(string subject, string datestr) {
  auto p = full_path(subject, datestr);
  if (std::filesystem::is_regular_file(p)) {
    std::cout << "opening " << p.string() << "\n";
  }
  else if (!std::filesystem::exists(p)) {
    std::cout << "creating " << p.string() << "\n";
  }
  else {
    warn("File is not a regular file\n");
    return 1;
  }
  if (fork() == 0)
    execl(editor.c_str(), appname.c_str(), p.string().c_str(), nullptr);
  return 0;
}

std::tuple<std::vector<string>, std::vector<string>> read_dates(string subject) {
  path data_path{base_dir() / datadir / subject};
  assert(std::filesystem::is_directory(data_path));
  std::vector<string> dates;
  std::vector<string> others;

  for (std::filesystem::directory_iterator dir{data_path};
         dir != std::filesystem::directory_iterator{};
       ++dir) {
    auto fn = dir->path().filename().string();
    if (std::regex_match(fn, ymd_regex)) {
      dates.push_back(fn);
    }
    else if (!fn.starts_with(".")) {
      others.push_back(fn);
    }
  }
  std::ranges::sort(dates);
  return std::make_tuple<>(dates, others);
}

void list_dates(string subject) {
  auto [ dates, others ] = read_dates(subject);
  for (const auto& d : dates) std::cout << d << "\n";
  for (const auto& d : others) std::cout << d << "\n";
}

string latest_date(string subject) {
  auto [ dates, _ ] = read_dates(subject);
  if (dates.empty()) return "";
  else return dates.back();
}

int output_shell_completions(std::span<string> parsing) {
  // do shell completion for a partial subject
  if (parsing.size() > 1) {
    auto subject = parsing[1];
    match_name matched(subject);
    for (auto g : matched) {
      std::cout << g << "\n";
    }
    return 0;
  } else return 1;
}

int create_subject(std::span<string> parsing) {
  if (parsing.empty()) {
    warn("Need an explicit name to create\n");
    return 1;
  }
  auto name = parsing.front();
  if ((name.size() > 40) ||
      (name.find(' ') != string::npos) ||
      name.empty()) {
    warn("Invalid name\n");
    return 1;
  }
  path p{base_dir() / datadir / name};
  if (std::filesystem::exists(p)) {
    warn("Already exists");
    return 1;
  } else {
    int status = 0;
    if (!std::filesystem::create_directory(p)) {
      warn("Failed");
      status = 1;
    } else {
      auto today = std::chrono::time_point_cast<std::chrono::days>(std::chrono::system_clock::now());
      status = open_file(name, date_string(today));
    }
    saved.set_subject(name);

    return status;
  }
}

int main(int argc, char* argv[]) {
  std::vector<string> arguments;
  std::transform(argv+1, argv+argc, std::back_inserter(arguments), [](const char* cp){ return string(cp); });
  std::span<string> parsing(arguments);

  bool opt_list{false};
  bool opt_latest{false};
  for (; option_argument option{parsing} ;) {
    if (option.matches("create")) {
      return create_subject(parsing);
    }

    if (option.matches("-complete")) {
      return output_shell_completions(parsing);
    }

    if (option.matches("ls")) {
      // list dates for the given subject
      opt_list = true;
    }

    if (option.matches("latest")) opt_latest = true;
  }

  date_argument for_date{parsing};

  if (parsing.empty()) {
    if (opt_list) {
      // write all the subjects out
      match_name all_subjects(string{});
      for (const auto& s : all_subjects)
        std::cout << s << "\n";
    }
    else return open_file(saved.get_subject(), for_date);
  } else {
    auto subject = parsing.front();
    parsing = parsing.subspan(1);

    match_name matched(subject);
    if (matched.matched()) {
      saved.set_subject(matched.match());
      if (opt_list) {
        list_dates(matched.match());
        if (!for_date.was_set()) return 0;
      }
      else if (opt_latest) {
        auto latest = latest_date(matched.match());
        if (latest.empty()) return 1;
        return open_file(matched.match(), latest);
      }
      return open_file(matched.match(), for_date);
    }
    else if (matched.any()) {
      // write out the possible matches, and prompt for a choice
      size_t label = 1;
      for (auto g : matched) {
        std::cout << label++ << " : " << g << "\n";
      }

      while(true) {
        string input;
        std::cin >> input;
        if (!input.empty()) {
          size_t num = std::stoi(input);
          if ((num >= 1) && (num <= matched.count())) {
            auto subject = matched.get(num - 1);
            saved.set_subject(subject);
            return open_file(subject, for_date);
          }
          break;
        }
      }
      warn("No input selected\n");
    }
    else warn("No matching subject\n");
  }

  return status;
}
