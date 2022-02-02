#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <list>
#include <optional>
#include <condition_variable>
#include <algorithm>
#include <string>
#include <time.h>

// Thread-safe std::cout
void log(const std::string &&str)
{
    static std::mutex _m;
    std::lock_guard<std::mutex> lock(_m);
    std::cout << str << std::endl;
}

struct Message
{
    const std::string phone_number;
    const std::string login;

    Message(const std::string &&_phone_number, const std::string &&_login) : phone_number(_phone_number), login(_login){};

    bool IsValid()
    {
        return !phone_number.empty() || !login.empty();
    }
};

/**
 * @brief Thread-safe shared container, implemented as queue  
 * 
 * @tparam MessageType 
 */
template <class MessageType>
class Container
{
private:
    std::queue<MessageType> _container;
    std::mutex _mutex;
    std::condition_variable _cv;

public:
    void push(MessageType &&message)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _container.push(message);
        _cv.notify_one();
    }

    /**
     * @brief Blocks the thread until element is received from the container
     * 
     * @return MessageType
     */
    MessageType pop()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [this]()
                 { return !_container.empty(); });
        MessageType result = std::move(_container.front());
        _container.pop();
        lock.unlock();
        return std::move(result);
    }

    bool empty()
    {
        return _container.empty();
    }
};

class Generator
{
    Container<Message> &_container;
    std::thread _thread;
    std::atomic<bool> _terminate_flag;

public:
    Generator(Container<Message> &container) : _container(container)
    {
        _terminate_flag = false;
        _thread = std::thread([](Container<Message> &container, std::atomic<bool> &terminate)
            {
                srand(time(0));
                std::string number, login;
                while (!terminate)
                {
                    static int i = 0;
                    number = "+7-915-XXX-XX-0" + std::to_string(i % 7);
                    login = std::string("login_") + char(97 + (rand() % 10));
                    ++i;
                    Message msg(std::move(number), std::move(login));
                    log("[Debug] [Generator]: Adding (" + msg.phone_number + ", " + msg.login + ")");
                    container.push(std::move(msg));
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                }
            },
            std::ref(_container), std::ref(_terminate_flag));
    };

    ~Generator()
    {
        _terminate_flag = true;
        _thread.join();
    }
};

class Searcher
{
    Container<Message> &_container;
    std::thread _thread;
    std::atomic<bool> _terminate_flag;

    using timestamp = std::chrono::steady_clock::time_point;
    std::list<std::pair<timestamp, Message>> _buffer; // buffer queue
    const unsigned int _delay = 5;

    void remove_expired()
    {
        auto time = std::chrono::steady_clock::now();
        auto first_expired = std::find_if(_buffer.begin(), _buffer.end(),
            [this, &time](const std::pair<Searcher::timestamp, Message> &elem)
            {
                auto diff = std::chrono::duration_cast<std::chrono::seconds>(time - elem.first);
                return diff.count() >= _delay;
            });
        // All elements after first_expired are expired too
        if (first_expired != _buffer.end())
        {
            // Debug log
            std::string expired_elements;
            for (auto it = first_expired; it != _buffer.end(); ++it)
            {
                expired_elements += "\n\t(" + it->second.phone_number + ", " + it->second.login + ")";
            }
            log("[Debug] [Searcher]: Expired elements with delay " + std::to_string(_delay) + "s:" + expired_elements);
            // \Debug log

            _buffer.erase(first_expired, _buffer.end());
        }
    }

    std::list<std::pair<Searcher::timestamp, Message>>::iterator search(const Message &msg)
    {
        // Validate container
        remove_expired();

        // Find candidate
        auto best_candidate = _buffer.end();
        unsigned int max_score = 0;
        for (auto it = _buffer.rbegin(); it != _buffer.rend(); ++it)
        {
            unsigned int score = (msg.login == it->second.login) + (msg.phone_number == it->second.phone_number);
            if (score > max_score)
            {
                max_score = true;
                best_candidate = --it.base(); // reverse_iterator to iterator
            }
        }
        return best_candidate;
    }

public:
    Searcher(Container<Message> &container) : _container(container)
    {
        _terminate_flag = false;
        _thread = std::thread([this]()
            {
                while (!_terminate_flag)
                {
                    if (!_container.empty())
                    {
                        auto msg = _container.pop(); // blocks thread until message receiving
                        auto time = std::chrono::steady_clock::now();
                        auto found_it = search(msg);
                        if (found_it != _buffer.end())
                        {
                            // Debug log
                            std::string elements;
                            for (auto it = _buffer.begin(); it != _buffer.end(); ++it)
                            {
                                elements += "\n\t(" + it->second.phone_number + ", " + it->second.login + ")";
                            }
                            if (!elements.empty())
                                log("[Debug] [Searcher]: Internal storage:" + elements);
                            // \Debug log

                            unsigned int score = (msg.login == found_it->second.login) + (msg.phone_number == found_it->second.phone_number);
                            log("[Searcher]: Found (with score: " + std::to_string(score) + ")\n" +
                                "\tFrom shared storage: (" + msg.phone_number + ", " + msg.login + ")\n" +
                                "\tFrom internal storage: (" + found_it->second.phone_number + ", " + found_it->second.login + ")\n");
                            _buffer.erase(found_it);
                        }
                        else
                        {
                            _buffer.emplace_front(time, msg); // newer items infront
                        }
                    }
                }
            });
    };

    ~Searcher()
    {
        _terminate_flag = true;
        _thread.join();
    }
};

int main()
{
    Container<Message> shared_container;

    Generator generator_thread(shared_container);
    Searcher searcher_thread(shared_container);

    std::this_thread::sleep_for(std::chrono::seconds(50));

    return 0;
}