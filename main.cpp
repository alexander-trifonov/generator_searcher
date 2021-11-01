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

void log(const std::string && str)
{
    static std::mutex _m;
    std::lock_guard<std::mutex> lock(_m);
    std::cout << str << std::endl;
}

struct Message
{
    const std::string phone_number;
    const std::string login;

    Message(const std::string && _phone_number, const std::string && _login): phone_number(_phone_number), login(_login) {};

    bool IsValid()
    {
        return !phone_number.empty() || !login.empty();
    }
};

template<class MessageType>
class Container
{
private:
    std::queue<MessageType> _container;
    std::mutex _mutex;
    std::condition_variable _cv;
public:
    void push(MessageType && message)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _container.push(message);
        _cv.notify_one();
    }

    // Non-blocking call
    std::optional<MessageType> try_pop()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_container.empty())
            return {};
        MessageType result = _container.front();
        _container.pop();
        return result;
    }

    // Blocking call - blocks the thread, until receiving an element from container
    MessageType pop()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [this](){return !_container.empty();});
        MessageType result = _container.front();
        _container.pop();
        lock.unlock();
        return result;
    }

    bool empty()
    {
        return  _container.empty();
    }
};

class Generator
{
    Container<Message> & _container;
    std::thread _thread;
    bool _terminate_flag = false;
    
public:
    Generator(Container<Message> & container): _container(container)
    {
        _thread = std::thread([](Container<Message> & container, bool & terminate){
            int i = 0;
            while (!terminate)
            {
                std::string login = "Mobiou";
                login += (i%3 == 0 ? "s" : "");
                Message msg("+7-915-" + std::to_string(++i), std::move(login));
                log("[Generator]: Adding (" + msg.phone_number + ", " + msg.login + ")");
                container.push(std::move(msg));
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            } 
        }, std::ref(_container), std::ref(_terminate_flag));
    };

    ~Generator()
    {
        _terminate_flag = true;
        _thread.join();
    }
};

class Searcher
{
    Container<Message> & _container;
    std::thread _thread;
    bool _terminate_flag = false;

    using timestamp = std::chrono::steady_clock::time_point;
    std::list<std::pair<timestamp, Message>> _buffer; //std::pair<Message, timestamp>> _buffer;
    const unsigned int _delay = 5;

public:
    Searcher(Container<Message> & container): _container(container)
    {
        _thread = std::thread([this](){
            while (!_terminate_flag)
            {
                if (!_container.empty())
                {
                    auto msg = _container.pop(); // blocks thread until message receiving
                    auto time = std::chrono::steady_clock::now();
                    // Delete expired messages
                    auto first_expired = std::find_if(_buffer.begin(), _buffer.end(), 
                        [this, &time](const std::pair<Searcher::timestamp, Message> & elem){
                            auto diff = std::chrono::duration_cast<std::chrono::seconds>(time - elem.first);
                            return diff.count() >= _delay;
                        });
                    // If found expired element -> all elements after are expired too
                    if (first_expired != _buffer.end())
                    {
                        std::string expired_elements;
                        for (auto it = first_expired; it != _buffer.end(); ++it)
                        {
                            expired_elements += "\n(" + it->second.phone_number + ", " + it->second.login + ")";
                        }
                        log("[Searcher]: Expired elements:" + expired_elements);
                        _buffer.erase(first_expired, _buffer.end());                       
                    }

                    auto best_candidate = _buffer.end();
                    unsigned int max_score = 0;
                    for (auto it = _buffer.begin(); it != _buffer.end(); ++it)
                    {
                        unsigned int score = (msg.login == it->second.login) + (msg.phone_number == it->second.phone_number);
                        if (score > max_score)
                        {
                            max_score = true;
                            best_candidate = it;
                        }   
                    }

                    if (best_candidate != _buffer.end())
                    {
                        log("[Searcher]: Found (" + best_candidate->second.phone_number + ", " + best_candidate->second.login + ")");
                        _buffer.erase(best_candidate);
                    }
                    else
                    {
                        //log("[Searcher]: Not found (" + msg.phone_number + ", " + msg.login + ")");
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

    std::this_thread::sleep_for(std::chrono::seconds(10));

    return 0;
}