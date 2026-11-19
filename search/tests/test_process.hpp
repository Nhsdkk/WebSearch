#ifndef SEARCH_TESTS_TEST_PROCESS_HPP_
#define SEARCH_TESTS_TEST_PROCESS_HPP_

#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

extern char** environ;

namespace search_test {

class TestProcess {
   public:
    explicit TestProcess(std::vector<std::string> command, const std::filesystem::path& log) {
        std::vector<char*> args;
        for (auto& argument : command) {
            args.push_back(argument.data());
        }
        args.push_back(nullptr);

        posix_spawn_file_actions_t actions;
        Check(posix_spawn_file_actions_init(&actions), "posix_spawn_file_actions_init");
        int error =
            posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (error == 0) {
            error = posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);
        }
        if (error == 0) {
            error = posix_spawn(&pid_, args.front(), &actions, nullptr, args.data(), environ);
        }
        posix_spawn_file_actions_destroy(&actions);
        Check(error, "Starting test process");
    }

    TestProcess(const TestProcess&) = delete;
    TestProcess& operator=(const TestProcess&) = delete;

    ~TestProcess() {
        if (pid_ > 0) {
            kill(pid_, SIGTERM);
            if (!WaitFor(std::chrono::seconds(2))) {
                kill(pid_, SIGKILL);
                while (waitpid(pid_, nullptr, 0) == -1 && errno == EINTR) {
                }
            }
        }
    }

    std::optional<int> WaitFor(std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        do {
            int status = 0;
            const auto result = waitpid(pid_, &status, WNOHANG);
            if (result == pid_) {
                pid_ = -1;
                return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            }
            if (result == -1 && errno != EINTR) {
                pid_ = -1;
                return -1;
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        } while (true);
        return std::nullopt;
    }

   private:
    static void Check(int error, const char* operation) {
        if (error != 0) {
            throw std::system_error(error, std::generic_category(), operation);
        }
    }

    pid_t pid_ = -1;
};

}  // namespace search_test

#endif  // SEARCH_TESTS_TEST_PROCESS_HPP_
