#include "ApiClient.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <memory>
#include <utility>

namespace {
constexpr long ConnectTimeoutMs = 1000;
constexpr long RequestTimeoutMs = 3000;
constexpr std::size_t MaxResponseBytes = 64 * 1024;

// Initialize once before starting any workers; clean up after all clients exit.
struct CurlRuntime {
    CURLcode status = curl_global_init(CURL_GLOBAL_DEFAULT);
    ~CurlRuntime()
    {
        if (status == CURLE_OK) curl_global_cleanup();
    }
};

CurlRuntime& GetCurlRuntime()
{
    static CurlRuntime runtime;
    return runtime;
}

std::size_t CollectResponse(char* data, std::size_t size, std::size_t count, void* destination) noexcept
{
    auto& response = *static_cast<std::string*>(destination);
    if (size != 0 && count > (MaxResponseBytes - response.size()) / size) return 0;
    const std::size_t bytes = size * count;
    try {
        response.append(data, bytes);
        return bytes;
    } catch (...) {
        return 0;
    }
}

std::size_t DiscardResponse(char*, std::size_t size, std::size_t count, void*) noexcept
{
    return size * count;
}
}

ApiClient::ApiClient(std::string baseUrl) : baseUrl(std::move(baseUrl))
{
    while (!this->baseUrl.empty() && this->baseUrl.back() == '/') this->baseUrl.pop_back();
    if (GetCurlRuntime().status != CURLE_OK) {
        std::cerr << "API: could not initialize HTTP; continuing without a session\n";
        stopping = true;
        return;
    }
    // A synchronous DNS resolver can outlive curl's timeout in a worker thread.
    // Stay offline on such builds rather than risking an unbounded shutdown.
    if (!(curl_version_info(CURLVERSION_NOW)->features & CURL_VERSION_ASYNCHDNS)) {
        std::cerr << "API: HTTP requires asynchronous DNS; continuing without a session\n";
        stopping = true;
        return;
    }
    worker = std::thread(&ApiClient::ProcessRequests, this);
}

ApiClient::~ApiClient()
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        stopping = true;
        // Do not wait for a backlog on exit. Only an in-flight request finishes,
        // with its normal timeout; canceled requests resolve to an empty ID.
        for (auto& request : requests) {
            if (request.type == RequestType::CreateSession) request.result.set_value("");
        }
        requests.clear();
    }
    ready.notify_one();
    if (worker.joinable()) worker.join();
}

std::future<std::string> ApiClient::QueueCreateSession(std::string playerName)
{
    Request request;
    request.playerName = std::move(playerName);
    request.session = std::make_shared<SessionContext>();
    auto result = request.result.get_future();
    {
        std::lock_guard<std::mutex> lock(mutex);
        // Always replace the context, including when creation cannot be queued,
        // so a new game's events can never attach to the previous session.
        currentSession = request.session;
        if (stopping || requests.size() >= MaxQueuedRequests) {
            std::cerr << "API: session request skipped; client stopped or queue full\n";
            request.result.set_value("");
            return result;
        }
        requests.push_back(std::move(request));
    }
    ready.notify_one();
    return result;
}

void ApiClient::QueueEvent(std::string eventType)
{
    Request request;
    request.type = RequestType::Event;
    request.eventType = std::move(eventType);
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (stopping || !currentSession || requests.size() >= MaxQueuedRequests) {
            std::cerr << "API: event skipped; client stopped, no session, or queue full\n";
            return;
        }
        request.session = currentSession;
        requests.push_back(std::move(request));
    }
    ready.notify_one();
}

void ApiClient::ProcessRequests()
{
    while (true) {
        Request request;
        {
            std::unique_lock<std::mutex> lock(mutex);
            ready.wait(lock, [this] { return stopping || !requests.empty(); });
            if (stopping) return;
            request = std::move(requests.front());
            requests.pop_front();
        }
        if (request.type == RequestType::CreateSession) {
            request.session->id = CreateSession(request.playerName);
            request.result.set_value(request.session->id);
        } else if (!request.session->id.empty()) {
            SendEvent(request.session->id, request.eventType);
        }
    }
}

std::string ApiClient::CreateSession(const std::string& playerName)
{
    try {
        const std::string body = nlohmann::json{{"player_name", playerName}}.dump();
        std::string response;
        if (!PostJson("/session", body, &response)) return "";

        // Only session creation reads a response body, to obtain its ID.
        const auto json = nlohmann::json::parse(response, nullptr, false);
        if (json.is_object()) {
            const auto id = json.find("session_id");
            if (id != json.end() && id->is_string()) {
                const std::string value = id->get<std::string>();
                if (value.find_first_not_of(" \t\r\n") != std::string::npos) return value;
            }
        }
        std::cerr << "API: session response has no valid session_id\n";
    } catch (const std::exception&) {
        std::cerr << "API: could not serialize request or read session response\n";
    }
    return "";
}

void ApiClient::SendEvent(const std::string& sessionId, const std::string& eventType)
{
    try {
        const std::string body = nlohmann::json{
            {"session_id", sessionId}, {"event_type", eventType}
        }.dump();
        PostJson("/event", body, nullptr);
    } catch (const std::exception&) {
        std::cerr << "API: could not serialize event request\n";
    }
}

bool ApiClient::PostJson(const char* path, const std::string& body, std::string* response)
{
    const std::string url = baseUrl + path;
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> handle(curl_easy_init(), curl_easy_cleanup);
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers(
        curl_slist_append(nullptr, "Content-Type: application/json"), curl_slist_free_all
    );
    if (!handle || !headers) {
        std::cerr << "API: could not allocate HTTP request\n";
        return false;
    }

    CURL* curl = handle.get();
    if (curl_easy_setopt(curl, CURLOPT_URL, url.c_str()) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get()) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_POST, 1L) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str()) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size())) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, ConnectTimeoutMs) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, RequestTimeoutMs) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, response ? CollectResponse : DiscardResponse) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response) != CURLE_OK) {
        std::cerr << "API: could not configure HTTP request\n";
        return false;
    }

    // One attempt only; requests are never retried automatically.
    const CURLcode result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        std::cerr << "API: " << path << " request failed: " << curl_easy_strerror(result) << '\n';
        return false;
    }
    // Event bodies are discarded and their HTTP status is not inspected.
    if (!response) return true;
    long status = 0;
    if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status) != CURLE_OK ||
        status < 200 || status >= 300) {
        std::cerr << "API: session request returned HTTP " << status << '\n';
        return false;
    }
    return true;
}
