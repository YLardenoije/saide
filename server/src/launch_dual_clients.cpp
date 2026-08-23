#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <iostream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <process.h>
#else
#include <arpa/inet.h>
#include <csignal>
#include <netinet/in.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
#endif

namespace {

#ifdef _WIN32
using ProcessHandle = intptr_t;

ProcessHandle spawn_process(const std::string& executable, const char* argument = nullptr) {
    if (argument == nullptr) {
        return _spawnlp(_P_NOWAIT, executable.c_str(), executable.c_str(), nullptr);
    }
    return _spawnlp(
        _P_NOWAIT, executable.c_str(), executable.c_str(), "--path", argument, nullptr);
}

int wait_for_process(ProcessHandle process) {
    int status{};
    return _cwait(&status, process, 0) == -1 ? -1 : status;
}

void stop_process(ProcessHandle process) {
    TerminateProcess(reinterpret_cast<HANDLE>(process), 0);
    WaitForSingleObject(reinterpret_cast<HANDLE>(process), 5000);
    CloseHandle(reinterpret_cast<HANDLE>(process));
}
#else
using ProcessHandle = pid_t;

ProcessHandle spawn_process(const std::string& executable, const char* argument = nullptr) {
    char* no_arguments[] = {const_cast<char*>(executable.c_str()), nullptr};
    char* client_arguments[] = {
        const_cast<char*>(executable.c_str()), const_cast<char*>("--path"),
        const_cast<char*>(argument), nullptr};
    pid_t process_id{};
    const int spawn_result = posix_spawnp(
        &process_id, executable.c_str(), nullptr, nullptr,
        argument == nullptr ? no_arguments : client_arguments, environ);
    if (spawn_result != 0) {
        return -1;
    }
    return process_id;
}

int wait_for_process(ProcessHandle process) {
    int status{};
    if (waitpid(process, &status, 0) == -1) {
        return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

void stop_process(ProcessHandle process) {
    kill(process, SIGTERM);
    waitpid(process, nullptr, 0);
}
#endif

int launch_client(const std::string& godot_executable) {
    const ProcessHandle process = spawn_process(godot_executable, SAIDE_GODOT_PROJECT_PATH);
    return process == -1 ? -1 : wait_for_process(process);
}

bool server_is_ready() {
#ifdef _WIN32
    WSADATA socket_data{};
    if (WSAStartup(MAKEWORD(2, 2), &socket_data) != 0) {
        return false;
    }
    const SOCKET socket_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    const int socket_handle = socket(AF_INET, SOCK_STREAM, 0);
#endif
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(43594);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const bool connected =
        connect(socket_handle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
#ifdef _WIN32
    closesocket(socket_handle);
    WSACleanup();
#else
    close(socket_handle);
#endif
    return connected;
}

bool wait_for_server() {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
        if (server_is_ready()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}

}  // namespace

int main() {
    const std::string godot_executable = SAIDE_GODOT_EXE;
    const std::string server_executable = SAIDE_SERVER_EXE;

    if (godot_executable.empty()) {
        std::cerr << "SAIDE_GODOT_EXE is empty. Reconfigure CMake with "
                     "-DSAIDE_GODOT_EXE=/path/to/godot.\n";
        return 2;
    }

    if (godot_executable.find_first_of("/\\") != std::string::npos &&
        !std::filesystem::exists(godot_executable)) {
        std::cerr << "Godot executable not found: " << godot_executable
                  << ". Reconfigure CMake with -DSAIDE_GODOT_EXE=/path/to/godot.\n";
        return 2;
    }

    const ProcessHandle server_process = spawn_process(server_executable);
    if (server_process == -1) {
        std::cerr << "Could not start server: " << server_executable << '\n';
        return 2;
    }
    if (!wait_for_server()) {
        std::cerr << "Server did not begin listening on port 43594 within 10 seconds.\n";
        stop_process(server_process);
        return 2;
    }

    auto first_client = std::async(std::launch::async, launch_client, godot_executable);
    auto second_client = std::async(std::launch::async, launch_client, godot_executable);

    const int first_result = first_client.get();
    const int second_result = second_client.get();
    stop_process(server_process);
    if (first_result != 0 || second_result != 0) {
        std::cerr << "A Godot client exited unsuccessfully.\n";
        return first_result != 0 ? first_result : second_result;
    }

    return 0;
}
