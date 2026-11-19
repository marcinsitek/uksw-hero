#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

class ApiClient {
public:
    explicit ApiClient(std::string baseUrl = "http://127.0.0.1:8080");
    ~ApiClient();

    ApiClient(const ApiClient&) = delete;
    ApiClient& operator=(const ApiClient&) = delete;

    // Never waits for HTTP. The future resolves to the server ID, or an empty
    // string on failure, shutdown, or a full queue. Requests execute in order.
    std::future<std::string> QueueCreateSession(std::string playerName);
    // Uses the latest queued session, even while its creation is in progress.
    // Events are best effort: no result, response processing, or retries.
    void QueueEvent(std::string eventType);

private:
    struct SessionContext {
        // Read and written only by the worker, after session creation finishes.
        std::string id;
    };

    enum class RequestType { CreateSession, Event };
    struct Request {
        RequestType type = RequestType::CreateSession;
        std::shared_ptr<SessionContext> session;
        std::string playerName;
        std::string eventType;
        std::promise<std::string> result;
    };

    static constexpr std::size_t MaxQueuedRequests = 8;

    std::string baseUrl;
    std::mutex mutex;
    std::condition_variable ready;
    std::deque<Request> requests;
    std::shared_ptr<SessionContext> currentSession;
    bool stopping = false;
    std::thread worker;

    void ProcessRequests();
    // Only the worker calls this blocking function; failures return "".
    std::string CreateSession(const std::string& playerName);
    void SendEvent(const std::string& sessionId, const std::string& eventType);
    bool PostJson(const char* path, const std::string& body, std::string* response);
};
