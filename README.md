# hulaloop ⭕️
![Build Status](https://github.com/shnupta/hulaloop/actions/workflows/build.yml/badge.svg)
[![codecov](https://codecov.io/gh/shnupta/hulaloop/graph/badge.svg?token=8QP83GK99O)](https://codecov.io/gh/shnupta/hulaloop)

A header-only C++ event loop.

## API
How do you use this thing?

### Signals and Slots
Somewhat inspired by Qt, a `hula::signal<T, Tag>` represents a function that can fire, by calling any connected `hula::slot<T, Tag>`.

If you wish to notify some part of your application of a given event, with some type `T`, create a `hula::signal<T>` and allow other parts of your application to connect to that signal. Later fire the signal and the registered slots will be fired.

The second template argument to `hula::signal` allows for 'strong-typing' the signal, such that you must explicitly pass a slot for the intended signal, and not pass some generic function to any signal.

**Properties:**
It is safe to connect or disconnect to/from a signal whilst it is firing.

#### Example
```c++
class email_fetcher
{
public:
    using new_email_signal = hula::signal<email_id>;
    
    closer listen_to_new_emails(new_email_signal::slot s)
    {
        return _new_email_signal.connect(s);
    }

private:
    hula::signal<email_id> _new_email_signal;
};
```

To disconnect, simply close the closer.

### Closer
A `hula::closer` is a function which can be called _only once_.

Once closed, it will not trigger again.

It can be used as a safety mechanism in asynchronous code where objects might be destroyed at some unknown time whilst having registered callbacks which might be executed in the future.

If an object holds a `hula::closer` and is destroyed, the closer will be closed and the callback subscription can be cancelled.

### Loop
The core of the library is the `hula::loop`.

It is a simple event loop which handles polling file descriptors and handling signals.

To run an application, create a `hula::loop`, register any file descriptors, callbacks, or signal handlers and start the loop with `hula::loop::run()`.

You can stop the loop at any time with `hula::loop::stop()`.

File descriptors are first polled and slots for handling different poll events are called. To register a file descriptor call `hula::loop::add_fd(int, fd_slots)` with your callbacks for handling read, write, and error events.

After this any registered callbacks are fired.

#### Example
```c++
class tcp_connection
{
public:
    explicit tcp_connection()
    {
        // ...
        _fd_slots = hula::fd_slots{
            .readable = [this] (int fd) { fd_readable(fd); },
            .writable = [this] (int fd) { fd_writable(fd); },
            .error = [this] (int fd) { fd_error(fd); }
        }
    }

    void connect(hula::loop* loop, const std::string& endpoint)
    {
        // setup socket
        loop->add_fd(_fd, _fd_slots, hula::fd_events::write);
    }

private:
    int _fd = -1;
    hula::fd_slots _fd_slots;
};

```

To register a signal handler you can connect to the signal `loop::connect_to_unix_signal(hula::unix::sig, hula::unix::signal::slot)`.

Signals are queued and called at the end of each event loop cycle.

### Callbacks
There are two main interfaces to the `hula::loop`. `post` and `schedule`.

`schedule` is the stricter of the two interfaces. It returns a `hula::closer` which is a function that can be called _once_. 

If you prefer to manually remove the registered callback you can do so with `hula::loop::cancel_callback(id)`.
