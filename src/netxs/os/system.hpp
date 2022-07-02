// Copyright (c) NetXS Group.
// Licensed under the MIT license.

#ifndef NETXS_SYSTEM_HPP
#define NETXS_SYSTEM_HPP

#ifndef VTM_USE_CLASSICAL_WIN32_INPUT
#define VTM_USE_CLASSICAL_WIN32_INPUT // Turns on classical console win32 input mode.
#endif

#if (defined(__unix__) || defined(__APPLE__)) && !defined(__linux__)
    #define __BSD__
#endif

#if defined(__clang__) || defined(__APPLE__)
    #pragma clang diagnostic ignored "-Wunused-variable"
    #pragma clang diagnostic ignored "-Wunused-function"
#endif

#include "file_system.hpp"
#include "../text/logger.hpp"
#include "../console/input.hpp"
#include "../abstract/ptr.hpp"
#include "../console/ansi.hpp"
#include "../ui/layout.hpp"

#include <type_traits>
#include <iostream>         // std::cout

#if defined(_WIN32)

    #ifndef NOMINMAX
        #define NOMINMAX
    #endif

    #pragma warning(disable:4996) // disable std::getenv warnimg
    #pragma comment(lib, "Advapi32.lib")  // GetUserName()

    #include <Windows.h>
    #include <Psapi.h>      // GetModuleFileNameEx
    #include <Wtsapi32.h>   // WTSEnumerateSessions, get_logged_usres

    #include <Shlwapi.h>
    #include <algorithm>
    #include <Wtsapi32.h>
    #include <shobjidl.h>   // IShellLink
    #include <shlguid.h>    // IID_IShellLink
    #include <Shlobj.h>     // create_shortcut: SHGetFolderLocation / (SHGetFolderPathW - vist and later) CLSID_ShellLink
    #include <Psapi.h>      // GetModuleFileNameEx

    #include <DsGetDC.h>    // DsGetDcName
    #include <LMCons.h>     // DsGetDcName
    #include <Lmapibuf.h>   // DsGetDcName

    #include <Sddl.h>       //security_descriptor

#else

    #include <errno.h>      // ::errno
    #include <spawn.h>      // ::exec
    #include <unistd.h>     // ::gethostname(), ::getpid(), ::read()
    #include <sys/param.h>  //
    #include <sys/types.h>  // ::getaddrinfo
    #include <sys/socket.h> // ::shutdown() ::socket(2)
    #include <netdb.h>      //

    #include <stdio.h>
    #include <unistd.h>     // ::read(), PIPE_BUF
    #include <sys/un.h>
    #include <stdlib.h>

    #include <csignal>      // ::signal()
    #include <termios.h>    // console raw mode
    #include <sys/ioctl.h>  // ::ioctl
    #include <sys/wait.h>   // ::waitpid
    #include <syslog.h>     // syslog, daemonize

    #include <sys/types.h>
    #include <sys/stat.h>
    #include <fcntl.h>      // ::splice()

    #if defined(__linux__)
        #include <sys/vt.h> // ::console_ioctl()
        #ifdef __ANDROID__
            #include <linux/kd.h>   // ::console_ioctl()
        #else
            #include <sys/kd.h>     // ::console_ioctl()
        #endif
        #include <linux/keyboard.h> // ::keyb_ioctl()
    #endif

    extern char **environ;

#endif

namespace netxs::os
{
    namespace ipc
    {
        class iobase;
    }

    using list = std::vector<text>;
    using xipc = std::shared_ptr<ipc::iobase>;
    using namespace std::chrono_literals;
    using namespace netxs::ui::atoms;

    enum role { client, server };

    static constexpr si32 STDIN_BUF = 1024;
    static bool is_daemon = faux;

    #if defined(_WIN32)

        using fd_t = HANDLE;
        using conmode = DWORD[2];
        static const fd_t INVALID_FD = INVALID_HANDLE_VALUE;
        static const fd_t STDIN_FD  = ::GetStdHandle(STD_INPUT_HANDLE);
        static const fd_t STDOUT_FD = ::GetStdHandle(STD_OUTPUT_HANDLE);
        static const fd_t STDERR_FD = ::GetStdHandle(STD_ERROR_HANDLE);
        static const si32 PIPE_BUF = 65536;
        static const auto WR_PIPE_PATH = "\\\\.\\pipe\\w_";
        static const auto RD_PIPE_PATH = "\\\\.\\pipe\\r_";

        template<class T1, class T2 = si32>
        auto kbstate(T1 ctrls, T2 scancode = {})
        {
            bool ralt   = ctrls & RIGHT_ALT_PRESSED;
            bool lalt   = ctrls & LEFT_ALT_PRESSED;
            bool rctrl  = ctrls & RIGHT_CTRL_PRESSED;
            bool lctrl  = ctrls & LEFT_CTRL_PRESSED;
            bool lshift = ctrls & SHIFT_PRESSED && scancode == 0x2a;
            bool rshift = ctrls & SHIFT_PRESSED && scancode == 0x36;
            bool nums   = ctrls & NUMLOCK_ON;
            bool scrl   = ctrls & SCROLLLOCK_ON;
            bool caps   = ctrls & CAPSLOCK_ON;
            auto state  = si32{};
            if (lshift) state |= input::hids::LShift;
            if (rshift) state |= input::hids::RShift;
            if (lalt  ) state |= input::hids::LAlt;
            if (ralt  ) state |= input::hids::RAlt;
            if (lctrl ) state |= input::hids::LCtrl;
            if (rctrl ) state |= input::hids::RCtrl;
            if (nums  ) state |= input::hids::NumLock;
            if (caps  ) state |= input::hids::CapsLock;
            if (scrl  ) state |= input::hids::ScrlLock;
            return state;
        }

        //static constexpr char* security_descriptor_string =
        //	//"D:P(A;NP;GA;;;SY)(A;NP;GA;;;BA)(A;NP;GA;;;WD)";
        //	"O:AND:P(A;NP;GA;;;SY)(A;NP;GA;;;BA)(A;NP;GA;;;CO)";
        //	//"D:P(A;NP;GA;;;SY)(A;NP;GA;;;BA)(A;NP;GA;;;AU)";// Authenticated users
        //	//"D:P(A;NP;GA;;;SY)(A;NP;GA;;;BA)(A;NP;GA;;;CO)"; // CREATOR OWNER
        //	//"D:P(A;OICI;GA;;;SY)(A;OICI;GA;;;BA)(A;OICI;GA;;;CO)";
        //	//  "D:"  DACL
        //	//  "P"   SDDL_PROTECTED        The SE_DACL_PROTECTED flag is set.
        //	//  "A"   SDDL_ACCESS_ALLOWED
        //	// ace_flags:
        //	//  "OI"  SDDL_OBJECT_INHERIT
        //	//  "CI"  SDDL_CONTAINER_INHERIT
        //	//  "NP"  SDDL_NO_PROPAGATE
        //	// rights:
        //	//  "GA"  SDDL_GENERIC_ALL
        //	// account_sid: see https://docs.microsoft.com/en-us/windows/win32/secauthz/sid-strings
        //	//  "SY"  SDDL_LOCAL_SYSTEM
        //	//  "BA"  SDDL_BUILTIN_ADMINISTRATORS
        //	//  "CO"  SDDL_CREATOR_OWNER
        //	//  "WD"  SDDL_EVERYONE

    #else

        using fd_t = int;
        using conmode = ::termios;
        static constexpr fd_t INVALID_FD = -1;
        static constexpr fd_t STDIN_FD  = STDIN_FILENO;
        static constexpr fd_t STDOUT_FD = STDOUT_FILENO;
        static constexpr fd_t STDERR_FD = STDERR_FILENO;

    #endif

    class args
    {
        int    argc;
        char** argv;
        int    iter;

    public:
        args(int argc, char** argv)
            : argc{ argc }, argv{ argv }, iter{ 1 }
        { }

        // args: Returns true if not end.
        operator bool () const { return iter < argc; }
        // args: Return the next argument starting with '-' or '/' (skip others).
        auto next()
        {
            if (iter < argc)
            {
                if (*(argv[iter]) == '-' || *(argv[iter]) == '/')
                {
                    return *(argv[iter++] + 1);
                }
                ++iter;
            }
            return '\0';
        }
        // args: Return the rest of the command line arguments.
        auto tail()
        {
            auto crop = text{};
            while (iter < argc)
            {
                crop += view{ argv[iter++] };
                crop.push_back(' ');
            }
            if (!crop.empty()) crop.pop_back(); // Pop last space.
            return crop;
        }
    };

    class nothing
    {
    public:
        template<class T>
        operator T () { return T{}; }
    };

    void close(fd_t& h)
    {
        if (h != INVALID_FD)
        {
            #if defined(_WIN32)
                ::CloseHandle(h);
            #else
                ::close(h);
            #endif
            h = INVALID_FD;
        }
    }
    auto error()
    {
        #if defined(_WIN32)
            return ::GetLastError();
        #else
            return errno;
        #endif
    }
    template<class ...Args>
    auto fail(Args&&... msg)
    {
        log("  os: ", msg..., " (", os::error(), ") ");
        return nothing{};
    };
    template<class T>
    auto ok(T error_condition, text msg = {})
    {
        if (
            #if defined(_WIN32)
                error_condition == 0
            #else
                error_condition == (T)-1
            #endif
        )
        {
            os::fail(msg);
            return faux;
        }
        else return true;
    }

    class file
    {
        fd_t r; // file: Read descriptor.
        fd_t w; // file: Send descriptor.

    public:
        auto& get_r()       { return r; }
        auto& get_w()       { return w; }
        auto& get_r() const { return r; }
        auto& get_w() const { return w; }
        void  set_r(fd_t f) { r = f;    }
        void  set_w(fd_t f) { w = f;    }
        operator bool ()    { return r != INVALID_FD
                                  && w != INVALID_FD; }
        void close()
        {
            if (w == r)
            {
                os::close(r);
                w = r;
            }
            else
            {
                os::close(r);
                os::close(w);
            }
        }
        void shutdown() // Reset writing end of the pipe to interrupt reading call.
        {
            if (w == r)
            {
                os::close(w);
                r = w;
            }
            else
            {
                os::close(w);
            }
        }
        friend auto& operator << (std::ostream& s, file const& handle)
        {
            return s << handle.r << "," << handle.w;
        }
        auto& operator = (file&& f)
        {
            r = f.r;
            w = f.w;
            f.r = INVALID_FD;
            f.w = INVALID_FD;
            return *this;
        }
        file(file const&) = delete;
        file(file&& f)
            : r{ f.r },
              w{ f.w }
        {
            f.r = INVALID_FD;
            f.w = INVALID_FD;
        }
        file(fd_t r = INVALID_FD, fd_t w = INVALID_FD)
            : r{ r },
              w{ w }
        { }
        ~file()
        {
            close();
        }
    };

    class dstd
    {
        fd_t r; // file: Read descriptor.
        fd_t w; // file: Send descriptor.
        fd_t l; // file: Logs descriptor.

    public:
        auto& get_r()       { return r; }
        auto& get_w()       { return w; }
        auto& get_l()       { return l; }
        auto& get_r() const { return r; }
        auto& get_w() const { return w; }
        auto& get_l() const { return l; }
        void  set_r(fd_t f) { r = f;    }
        void  set_w(fd_t f) { w = f;    }
        void  set_l(fd_t f) { l = f;    }
        operator bool ()    { return r != INVALID_FD
                                  && w != INVALID_FD
                                  && l != INVALID_FD; }
        void close()
        {
            os::close(r);
            os::close(w);
            os::close(l);
        }
        void shutdown() // Reset writing end of the pipe to interrupt reading call.
        {
            os::close(w);
            os::close(l);
        }
        void shutsend() // Reset writing end of the pipe to interrupt reading call.
        {
            os::close(w);
        }
        friend auto& operator << (std::ostream& s, dstd const& handle)
        {
            return s << handle.r << "," << handle.w << "," << handle.l;
        }
        auto& operator = (dstd&& f)
        {
            r = f.r;
            w = f.w;
            l = f.l;
            f.r = INVALID_FD;
            f.w = INVALID_FD;
            f.l = INVALID_FD;
            return *this;
        }
        dstd(dstd const&) = delete;
        dstd(dstd&& f)
            : r{ f.r },
              w{ f.w },
              l{ f.l }
        {
            f.r = INVALID_FD;
            f.w = INVALID_FD;
            f.l = INVALID_FD;
        }
        dstd(fd_t r = INVALID_FD, fd_t w = INVALID_FD, fd_t l = INVALID_FD)
            : r{ r },
              w{ w },
              l{ l }
        { }
        ~dstd()
        {
            close();
        }
    };

    class fifo
    {
        bool                    alive;
        text                    store;
        std::mutex              mutex;
        std::condition_variable synch;

    public:
        fifo()
            : alive{ true }
        { }

        bool send(view data)
        {
            auto guard = std::lock_guard{ mutex };
            store += data;
            synch.notify_one();
            return true;
        }
        bool read(text& data)
        {
            auto guard = std::unique_lock{ mutex };
            if (store.size()
             || ((void)synch.wait(guard, [&]{ return store.size() || !alive; }), alive))
            {
                std::swap(data, store);
                store.clear();
                return true;
            }
            return faux;
        }
        bool read(char& c)
        {
            auto guard = std::unique_lock{ mutex };
            if (store.size()
             || ((void)synch.wait(guard, [&]{ return store.size() || !alive; }), alive))
            {
                c = store.front();
                store = store.substr(1);
                return true;
            }
            return faux;
        }
        void stop()
        {
            auto guard = std::lock_guard{ mutex };
            alive = faux;
            synch.notify_one();
        }
    };

    template<class SIZE_T>
    auto recv(fd_t fd, char* buff, SIZE_T size)
    {
        #if defined(_WIN32)

            auto count = DWORD{};
            auto fSuccess = ::ReadFile( fd,       // pipe handle
                                        buff,     // buffer to receive reply
                                (DWORD) size,     // size of buffer
                                       &count,    // number of bytes read
                                        nullptr); // not overlapped
            if (!fSuccess) count = 0;

        #else

            auto count = ::read(fd, buff, size);

        #endif

        return count > 0 ? qiew{ buff, count }
                         : qiew{}; // An empty result is always an error condition.
    }
    template<bool IS_TTY = true, class SIZE_T>
    auto send(fd_t fd, char const* buff, SIZE_T size)
    {
        while (size)
        {
            #if defined(_WIN32)

                auto count = DWORD{};
                auto fSuccess = ::WriteFile( fd,       // pipe handle
                                             buff,     // message
                                     (DWORD) size,     // message length
                                            &count,    // bytes written
                                             nullptr); // not overlapped

            #else
                // Mac OS X does not support the flag MSG_NOSIGNAL
                // See GH#182, https://lists.apple.com/archives/macnetworkprog/2002/Dec/msg00091.html
                #if defined(__APPLE__)
                    #define NO_SIGSEND SO_NOSIGPIPE
                #else
                    #define NO_SIGSEND MSG_NOSIGNAL
                #endif

                auto count = IS_TTY ? ::write(fd, buff, size)              // recursive connection causes sigpipe on destroy when using write(2) despite using ::signal(SIGPIPE, SIG_IGN)
                                    : ::send (fd, buff, size, NO_SIGSEND); // ::send() does not work with ::open_pty() and ::pipe() (Errno 88 - it is not a socket)

                #undef NO_SIGSEND
                //  send(2) does not work with file descriptors, only sockets.
                // write(2) works with fds as well as sockets.

            #endif

            if (count != size)
            {
                if (count > 0)
                {
                    log("send: partial writing: socket=", fd,
                        " total=", size, ", written=", count);
                    buff += count;
                    size -= count;
                }
                else
                {
                    os::fail("send: error write to socket=", fd, " count=", count, " size=", size, " IS_TTY=", IS_TTY ?"true":"faux");
                    return faux;
                }
            }
            else return true;
        }
        return faux;
    }
    template<bool IS_TTY = true, class T, class SIZE_T>
    auto send(fd_t fd, T* buff, SIZE_T size)
    {
        return os::send<IS_TTY, SIZE_T>(fd, (char const*)buff, size);
    }
    template<class ...Args>
    auto recv(file& handle, Args&&... args)
    {
        return recv(handle.get_r(), std::forward<Args>(args)...);
    }
    template<class ...Args>
    auto send(file& handle, Args&&... args)
    {
        return send(handle.get_w(), std::forward<Args>(args)...);
    }

    namespace
    {
        #if defined(_WIN32)

            template<class A, std::size_t... I>
            constexpr auto _repack(fd_t h, A const& a, std::index_sequence<I...>)
            {
                return std::array{ a[I]..., h };
            }
            template<std::size_t N, class P, class IDX = std::make_index_sequence<N>, class ...Args>
            constexpr auto _combine(std::array<fd_t, N> const& a, fd_t h, P&& proc, Args&&... args)
            {   // Clang 11.0.1 don't allow sizeof...(args) as bool
                if constexpr (!!sizeof...(args)) return _combine(_repack(h, a, IDX{}), std::forward<Args>(args)...);
                else                             return _repack(h, a, IDX{});
            }
            template<class P, class ...Args>
            constexpr auto _fd_set(fd_t handle, P&& proc, Args&&... args)
            {
                if constexpr (!!sizeof...(args)) return _combine(std::array{ handle }, std::forward<Args>(args)...);
                else                             return std::array{ handle };
            }
            template<class R, class P, class ...Args>
            constexpr auto _handle(R i, fd_t handle, P&& proc, Args&&... args)
            {
                if (i == 0)
                {
                    proc();
                    return true;
                }
                else
                {
                    if constexpr (!!sizeof...(args)) return _handle(--i, std::forward<Args>(args)...);
                    else                             return faux;
                }
            }

        #else
            
            template<class P, class ...Args>
            auto _fd_set(fd_set& socks, fd_t handle, P&& proc, Args&&... args)
            {
                FD_SET(handle, &socks);
                if constexpr (!!sizeof...(args))
                {
                    return std::max(handle, _fd_set(socks, std::forward<Args>(args)...));
                }
                else
                {
                    return handle;
                }
            }
            template<class P, class ...Args>
            auto _select(fd_set& socks, fd_t handle, P&& proc, Args&&... args)
            {
                if (FD_ISSET(handle, &socks))
                {
                    proc();
                }
                else
                {
                    if constexpr (!!sizeof...(args)) _select(socks, std::forward<Args>(args)...);
                }
            }

        #endif
    }

    template<bool NONBLOCKED = faux, class ...Args>
    void select(Args&&... args)
    {
        #if defined(_WIN32)

            static constexpr auto timeout = NONBLOCKED ? 0 /*milliseconds*/ : INFINITE;
            auto socks = _fd_set(std::forward<Args>(args)...);

            // Note: ::WaitForMultipleObjects() does not work with pipes (DirectVT).
            auto yield = ::WaitForMultipleObjects((DWORD)socks.size(), socks.data(), FALSE, timeout);
            yield -= WAIT_OBJECT_0;
            _handle(yield, std::forward<Args>(args)...);

        #else

            auto timeval = ::timeval{ .tv_sec = 0, .tv_usec = 0 };
            auto timeout = NONBLOCKED ? &timeval/*returns immediately*/ : nullptr;
            auto socks = fd_set{};
            FD_ZERO(&socks);

            auto nfds = 1 + _fd_set(socks, std::forward<Args>(args)...);
            if (::select(nfds, &socks, 0, 0, timeout) > 0)
            {
                _select(socks, std::forward<Args>(args)...);
            }

        #endif
    }

    class legacy
    {
    public:
        static constexpr auto clean  = 0;
        static constexpr auto mouse  = 1 << 0;
        static constexpr auto vga16  = 1 << 1;
        static constexpr auto vga256 = 1 << 2;
        static constexpr auto direct = 1 << 3;
        static auto& get_winsz()
        {
            static auto winsz = twod{};
            return winsz;
        }
        static auto& get_state()
        {
            static auto state = faux;
            return state;
        }
        static auto& get_start()
        {
            static auto start = text{};
            return start;
        }
        static auto& get_ready()
        {
            static auto ready = faux;
            return ready;
        }
        template<class T>
        static auto str(T mode)
        {
            auto result = text{};
            if (mode)
            {
                if (mode & mouse ) result += "mouse ";
                if (mode & vga16 ) result += "vga16 ";
                if (mode & vga256) result += "vga256 ";
                if (mode & direct) result += "direct ";
                if (result.size()) result.pop_back();
            }
            else result = "fresh";
            return result;
        }
        static void send_dmd(fd_t m_pipe_w, twod const& winsz)
        {
            auto buffer = ansi::dtvt::marker{ winsz };
            os::send<true>(m_pipe_w, buffer.data, buffer.size);
        }
        static auto peek_dmd(fd_t stdin_fd)
        {
            auto& ready = get_ready();
            auto& winsz = get_winsz();
            auto& state = get_state();
            auto& start = get_start();
            if (ready) return state;
            ready = true;

            #if defined(_WIN32)
                // ::WaitForMultipleObjects() does not work with pipes (DirectVT).
                auto buffer = ansi::dtvt::marker{};
                auto length = DWORD{ 0 };
                if (::PeekNamedPipe(stdin_fd,       // hNamedPipe
                                    &buffer,        // lpBuffer
                                    sizeof(buffer), // nBufferSize
                                    &length,        // lpBytesRead
                                    NULL,           // lpTotalBytesAvail,
                                    NULL))          // lpBytesLeftThisMessage
                {
                    if (length)
                    {
                        state = buffer.size == length
                             && buffer.get_sz(winsz);
                        if (state)
                        {
                            os::recv(stdin_fd, buffer.data, buffer.size);
                        }
                    }
                }
            #else
                os::select<true>(stdin_fd, [&]()
                {
                    auto buffer = ansi::dtvt::marker{};
                    auto header = os::recv(stdin_fd, buffer.data, buffer.size);
                    auto length = header.length();
                    if (length)
                    {
                        state = buffer.size == length
                             && buffer.get_sz(winsz);
                        if (!state)
                        {
                            start = header; //todo use it when the reading thread starts
                        }
                    }
                });
            #endif
            return state;
        }
    };

    void exit(int code)
    {
        #if defined(_WIN32)
            ::ExitProcess(code);
        #else
            if (is_daemon) ::closelog();
            ::exit(code);
        #endif
    }
    template<class ...Args>
    void exit(int code, Args&&... args)
    {
        log(args...);
        os::exit(code);
    }
    auto get_env(text&& var)
    {
        auto val = std::getenv(var.c_str());
        return val ? text{ val }
                   : text{};
    }
    text get_shell()
    {
        #if defined(_WIN32)
            return "cmd";
        #else
            auto shell = os::get_env("SHELL");
            if (shell.empty()
             || shell.ends_with("vtm"))
            {
                shell = "bash"; //todo request it from user if empty; or make it configurable
                log("  os: using '", shell, "' as a fallback login shell");
            }
            return shell;
        #endif
    }
    text homepath()
    {
        #if defined(_WIN32)
            return os::get_env("HOMEDRIVE") + os::get_env("HOMEPATH");
        #else
            return os::get_env("HOME");
        #endif
    }
    //system: Get list of envvars using wildcard.
    auto get_envars(text&& var)
    {
        auto crop = std::vector<text>{};
        auto list = environ;
        while (*list)
        {
            auto v = view{ *list++ };
            if (v.starts_with(var))
            {
                crop.emplace_back(v);
            }
        }
        std::sort(crop.begin(), crop.end());
        return crop;
    }
    auto local_mode()
    {
        auto conmode = -1;
        #if defined (__linux__)
            
            if (ok(::ioctl(STDOUT_FD, KDGETMODE, &conmode), "ioctl(STDOUT_FD, KDGETMODE) failed"))
            {
                switch (conmode)
                {
                    case KD_TEXT:     break;
                    case KD_GRAPHICS: break;
                    default:          break;
                }
            }

        #endif

        return conmode != -1;
    }
    auto vt_mode()
    {
        auto vga16colors = { // https://github.com//termstandard/colors
            "ansi",
            "linux",
            "xterm-color",
            "dvtm", //todo track: https://github.com/martanne/dvtm/issues/10
            "fbcon",
        };
        auto vga256colors = {
            "rxvt-unicode-256color",
        };

        si32 mode = legacy::clean;

        if (os::legacy::peek_dmd(STDIN_FD))
        {
            log("  os: DirectVT");
            mode |= legacy::direct;
        }

        auto term = os::get_env("TERM");
        if (term.size())
        {
            log("  os: terminal type \"", term, "\"");
            if (term.ends_with("16color") || term.ends_with("16colour"))
            {
                mode |= legacy::vga16;
            }
            else
            {
                for (auto& type : vga16colors)
                {
                    if (term == type)
                    {
                        mode |= legacy::vga16;
                        break;
                    }
                }
                if (!mode)
                {
                    for (auto& type : vga256colors)
                    {
                        if (term == type)
                        {
                            mode |= legacy::vga256;
                            break;
                        }
                    }
                }
            }
            if (os::get_env("TERM_PROGRAM") == "Apple_Terminal")
            {
                log("  os: macOS Apple_Terminal detected");
                if (!(mode & legacy::vga16)) mode |= legacy::vga256;
            }

            if (os::local_mode()) mode |= legacy::mouse;
            log("  os: color mode: ", mode & legacy::vga16  ? "16-color"
                                    : mode & legacy::vga256 ? "256-color"
                                                            : "true-color");
            log("  os: mouse mode: ", mode & legacy::mouse ? "console" : "VT-style");
        }
        return mode;
    }
    auto vgafont_update(si32 mode)
    {
        #if defined (__linux__)

            if (mode & legacy::mouse)
            {
                auto chars = std::vector<unsigned char>(512 * 32 * 4);
                auto fdata = console_font_op{ .op        = KD_FONT_OP_GET,
                                              .flags     = 0,
                                              .width     = 32,
                                              .height    = 32,
                                              .charcount = 512,
                                              .data      = chars.data() };
                if (!ok(::ioctl(STDOUT_FD, KDFONTOP, &fdata), "first KDFONTOP + KD_FONT_OP_GET failed")) return;

                auto slice_bytes = (fdata.width + 7) / 8;
                auto block_bytes = (slice_bytes * fdata.height + 31) / 32 * 32;
                auto tophalf_idx = 10;
                auto lowhalf_idx = 254;
                auto tophalf_ptr = fdata.data + block_bytes * tophalf_idx;
                auto lowhalf_ptr = fdata.data + block_bytes * lowhalf_idx;
                for (auto row = 0; row < fdata.height; row++)
                {
                    auto is_top = row < fdata.height / 2;
                   *tophalf_ptr = is_top ? 0xFF : 0x00;
                   *lowhalf_ptr = is_top ? 0x00 : 0xFF;
                    tophalf_ptr+= slice_bytes;
                    lowhalf_ptr+= slice_bytes;
                }
                fdata.op = KD_FONT_OP_SET;
                if (!ok(::ioctl(STDOUT_FD, KDFONTOP, &fdata), "second KDFONTOP + KD_FONT_OP_SET failed")) return;

                auto max_sz = std::numeric_limits<unsigned short>::max();
                auto spairs = std::vector<unipair>(max_sz);
                auto dpairs = std::vector<unipair>(max_sz);
                auto srcmap = unimapdesc{ max_sz, spairs.data() };
                auto dstmap = unimapdesc{ max_sz, dpairs.data() };
                auto dstptr = dstmap.entries;
                auto srcptr = srcmap.entries;
                if (!ok(::ioctl(STDOUT_FD, GIO_UNIMAP, &srcmap), "ioctl(STDOUT_FD, GIO_UNIMAP) failed")) return;
                auto srcend = srcmap.entries + srcmap.entry_ct;
                while (srcptr != srcend) // Drop 10, 211, 254 and 0x2580▀ + 0x2584▄.
                {
                    auto& smap = *srcptr++;
                    if (smap.fontpos != 10
                     && smap.fontpos != 211
                     && smap.fontpos != 254
                     && smap.unicode != 0x2580
                     && smap.unicode != 0x2584) *dstptr++ = smap;
                }
                dstmap.entry_ct = dstptr - dstmap.entries;
                unipair new_recs[] = { { 0x2580,  10 },
                                       { 0x2219, 211 },
                                       { 0x2022, 211 },
                                       { 0x25CF, 211 },
                                       { 0x25A0, 254 },
                                       { 0x25AE, 254 },
                                       { 0x2584, 254 } };
                if (dstmap.entry_ct < max_sz - std::size(new_recs)) // Add new records.
                {
                    for (auto& p : new_recs) *dstptr++ = p;
                    dstmap.entry_ct += std::size(new_recs);
                    if (!ok(::ioctl(STDOUT_FD, PIO_UNIMAP, &dstmap), "ioctl(STDOUT_FD, PIO_UNIMAP) failed")) return;
                }
                else log("  os: vgafont_update failed - UNIMAP is full");
            }

        #endif
    }
    auto vtgafont_revert()
    {
        //todo implement
    }
    auto current_module_file()
    {
        auto result = text{};

        #if defined(_WIN32)

            auto handle = ::GetCurrentProcess();
            auto buffer = std::vector<char>(MAX_PATH);

            while (buffer.size() <= 32768)
            {
                auto length = ::GetModuleFileNameEx(handle, NULL,
                    buffer.data(), static_cast<DWORD>(buffer.size()));

                if (!length) break;

                if (buffer.size() > length + 1)
                {
                    result = text(buffer.data(), length);
                    break;
                }

                buffer.resize(buffer.size() << 1);
            }

        #else

            char* resolved = ::realpath("/proc/self/exe", NULL);
            if (resolved)
            {
                result = text(resolved);
                ::free(resolved);
            }

        #endif

        return result;
    }
    auto split_cmdline(view cmdline)
    {
        auto args = std::vector<text>{};
        auto mark = '\0';
        auto temp = text{};
        temp.reserve(cmdline.length());

        auto push = [&]()
        {
            args.push_back(temp);
            temp.clear();
        };

        for (auto c : cmdline)
        {
            if (mark)
            {
                if (c != mark)
                {
                    temp.push_back(c);
                }
                else
                {
                    mark = '\0';
                    push();
                }
            }
            else
            {
                if (c == ' ')
                {
                    if (temp.length()) push();
                }
                else
                {
                    if (c == '\'' || c == '"') mark = c;
                    else                       temp.push_back(c);
                }
            }
        }
        if (temp.length()) push();

        return args;
    }
    auto exec(text binary, text params = {}, int window_state = 0)
    {
        #if defined(_WIN32)

            auto ShExecInfo = ::SHELLEXECUTEINFO{};
            ShExecInfo.cbSize = sizeof(::SHELLEXECUTEINFO);
            ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
            ShExecInfo.hwnd = NULL;
            ShExecInfo.lpVerb = NULL;
            ShExecInfo.lpFile = binary.c_str();
            ShExecInfo.lpParameters = params.c_str();
            ShExecInfo.lpDirectory = NULL;
            ShExecInfo.nShow = window_state;
            ShExecInfo.hInstApp = NULL;
            ::ShellExecuteEx(&ShExecInfo);
            return true;

        #else

            auto p_id = ::fork();
            if (p_id == 0) // Child branch.
            {
                log("exec: executing '", binary, " ", params, "'");
                char* argv[] = { binary.data(), params.data(), nullptr };

                ::execvp(argv[0], argv);
                os::exit(1, "exec: error ", errno);
            }

            if (p_id > 0) // Parent branch.
            {
                int stat;
                ::waitpid(p_id, &stat, 0); // Wait for the child to avoid zombies.

                if (WIFEXITED(stat) && (WEXITSTATUS(stat) == 0))
                {
                    return true; // Child forked and exited successfully.
                }
            }

            log("exec: failed to spawn '", binary, " ' with args '", params, "'");
            return faux;

        #endif
    }
    void start_log(text srv_name)
    {
        is_daemon = true;

        #if defined(_WIN32)

            //todo implement

        #else

            ::openlog(srv_name.c_str(), LOG_NOWAIT | LOG_PID, LOG_USER);

        #endif
    }
    void stdlog(view data)
    {
        std::cerr << data;
        //os::send<true>(STDERR_FD, data.data(), data.size());
    }
    void syslog(view data)
    {
        if (os::is_daemon)
        {

        #if defined(_WIN32)

            //todo implement            

        #else

            auto copy = text{ data };
            ::syslog(LOG_NOTICE, "%s", copy.c_str());

        #endif

        }
        else std::cout << data << std::flush;
    }
    auto daemonize(text srv_name)
    {
        #if defined(_WIN32)

            return true;

        #else

            auto pid = ::fork();
            if (pid < 0)
            {
                os::exit(1, "daemon: fork error");
            }

            if (pid == 0)
            { // CHILD
                ::setsid(); // Make this process the session leader of a new session.

                pid = ::fork();
                if (pid < 0)
                {
                    os::exit(1, "daemon: fork error");
                }

                if (pid == 0)
                { // GRANDCHILD
                    ::umask(0);
                    os::start_log(srv_name);

                    ::close(STDIN_FD);  // A daemon cannot use the terminal,
                    ::close(STDOUT_FD); // so close standard file descriptors
                    ::close(STDERR_FD); // for security reasons.
                    return true;
                }

                os::exit(0); // SUCCESS (This child is reaped below with waitpid()).
            }

            // Reap the child, leaving the grandchild to be inherited by init.
            int stat;
            ::waitpid(pid, &stat, 0);
            if (WIFEXITED(stat) && (WEXITSTATUS(stat) == 0))
            {
                os::exit(0); // Child forked and exited successfully.
            }
            return faux;

        #endif
    }
    auto host_name()
    {
        auto hostname = text{};

        #if defined(_WIN32)

            auto dwSize = DWORD{ 0 };
            ::GetComputerNameEx(::ComputerNamePhysicalDnsFullyQualified, NULL, &dwSize);

            if (dwSize)
            {
                auto buffer = std::vector<char>(dwSize);
                if (::GetComputerNameEx(::ComputerNamePhysicalDnsFullyQualified, buffer.data(), &dwSize))
                {
                    if (dwSize && buffer[dwSize - 1] == 0) dwSize--;
                    hostname = text(buffer.data(), dwSize);
                }
            }

        #else

            // APPLE: AI_CANONNAME undeclared
            //std::vector<char> buffer(MAXHOSTNAMELEN);
            //if (0 == gethostname(buffer.data(), buffer.size()))
            //{
            //	struct addrinfo hints, * info;
            //
            //	::memset(&hints, 0, sizeof hints);
            //	hints.ai_family = AF_UNSPEC;
            //	hints.ai_socktype = SOCK_STREAM;
            //	hints.ai_flags = AI_CANONNAME | AI_CANONIDN;
            //
            //	if (0 == getaddrinfo(buffer.data(), "http", &hints, &info))
            //	{
            //		hostname = std::string(info->ai_canonname);
            //		//for (auto p = info; p != NULL; p = p->ai_next)
            //		//{
            //		//	hostname = std::string(p->ai_canonname);
            //		//}
            //		freeaddrinfo(info);
            //	}
            //}

        #endif
        return hostname;
    }
    auto is_mutex_exists(text&& mutex_name)
    {
        auto result = faux;

        #if defined(_WIN32)

            auto mutex = ::CreateMutex(0, 0, mutex_name.c_str());
            result = ::GetLastError() == ERROR_ALREADY_EXISTS;
            ::CloseHandle(mutex);
            return result;

        #else

            //todo linux implementation
            return true;

        #endif
    }
    auto process_id()
    {
        auto result = ui32{};

        #if defined(_WIN32)
            result = ::GetCurrentProcessId();
        #else
            result = ::getpid();
        #endif

        return result;
    }
    static auto logged_in_users(view domain_delimiter = "\\", view record_delimiter = "\0") //  static required by ::WTSQuerySessionInformation
    {
        auto active_users_array = text{};

        #if defined(_WIN32)

            auto SessionInfo_pointer = PWTS_SESSION_INFO{};
            auto count = DWORD{};
            if (::WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &SessionInfo_pointer, &count))
            {
                for (DWORD i = 0; i < count; i++)
                {
                    auto si = SessionInfo_pointer[i];
                    auto buffer_pointer = LPTSTR{};
                    auto buffer_length = DWORD{};

                    ::WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, si.SessionId, WTSUserName, &buffer_pointer, &buffer_length);
                    auto user = text(utf::to_view(buffer_pointer, buffer_length));
                    ::WTSFreeMemory(buffer_pointer);

                    if (user.length())
                    {
                        ::WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, si.SessionId, WTSDomainName, &buffer_pointer, &buffer_length);
                        auto domain = text(utf::to_view(buffer_pointer, buffer_length / sizeof(wchar_t)));
                        ::WTSFreeMemory(buffer_pointer);

                        active_users_array += domain;
                        active_users_array += domain_delimiter;
                        active_users_array += user;
                        active_users_array += domain_delimiter;
                        active_users_array += "local";
                        active_users_array += record_delimiter;
                    }
                }
                ::WTSFreeMemory(SessionInfo_pointer);
                if (active_users_array.size())
                {
                    active_users_array = utf::remove(active_users_array, record_delimiter);
                }
            }

        #else

            static constexpr auto NAME_WIDTH = 8;

            // APPLE: utmp is deprecated

            // if (FILE* ufp = ::fopen(_PATH_UTMP, "r"))
            // {
            // 	struct utmp usr;
            //
            // 	while (::fread((char*)&usr, sizeof(usr), 1, ufp) == 1)
            // 	{
            // 		if (*usr.ut_user && *usr.ut_host && *usr.ut_line && *usr.ut_line != '~')
            // 		{
            // 			active_users_array += usr.ut_host;
            // 			active_users_array += domain_delimiter;
            // 			active_users_array += usr.ut_user;
            // 			active_users_array += domain_delimiter;
            // 			active_users_array += usr.ut_line;
            // 			active_users_array += record_delimiter;
            // 		}
            // 	}
            // 	::fclose(ufp);
            // }

        #endif
        return active_users_array;
    }
    auto user()
    {
        #if defined(_WIN32)

            static constexpr auto INFO_BUFFER_SIZE = 32767UL;
            char infoBuf[INFO_BUFFER_SIZE];
            auto bufCharCount = DWORD{ INFO_BUFFER_SIZE };

            if (::GetUserNameA(infoBuf, &bufCharCount))
            {
                if (bufCharCount && infoBuf[bufCharCount - 1] == 0)
                {
                    bufCharCount--;
                }
                return text(infoBuf, bufCharCount);
            }
            else
            {
                os::fail("GetUserName error");
                return text{};
            }

        #else

            uid_t id;
            id = ::geteuid();
            return id;

        #endif
    }

    #if defined(_WIN32)

    /* cl.exe issue
    class security_descriptor
    {
        SECURITY_ATTRIBUTES descriptor;

    public:
        text security_string;

        operator SECURITY_ATTRIBUTES* () throw()
        {
            return &descriptor;
        }

        security_descriptor(text SSD)
            : security_string{ SSD }
        {
            ZeroMemory(&descriptor, sizeof(descriptor));
            descriptor.nLength = sizeof(descriptor);

            // four main components of a security descriptor https://docs.microsoft.com/en-us/windows/win32/secauthz/security-descriptor-string-format
            //       "O:" - owner
            //       "G:" - primary group
            //       "D:" - DACL discretionary access control list https://docs.microsoft.com/en-us/windows/desktop/SecGloss/d-gly
            //       "S:" - SACL system access control list https://docs.microsoft.com/en-us/windows/desktop/SecGloss/s-gly
            //
            // the object's owner:
            //   O:owner_sid
            //
            // the object's primary group:
            //   G:group_sid
            //
            // Security descriptor control flags that apply to the DACL:
            //   D:dacl_flags(string_ace1)(string_ace2)... (string_acen)
            //
            // Security descriptor control flags that apply to the SACL
            //   S:sacl_flags(string_ace1)(string_ace2)... (string_acen)
            //
            //   dacl_flags/sacl_flags:
            //   "P"                 SDDL_PROTECTED        The SE_DACL_PROTECTED flag is set.
            //   "AR"                SDDL_AUTO_INHERIT_REQ The SE_DACL_AUTO_INHERIT_REQ flag is set.
            //   "AI"                SDDL_AUTO_INHERITED   The SE_DACL_AUTO_INHERITED flag is set.
            //   "NO_ACCESS_CONTROL" SSDL_NULL_ACL         The ACL is null.
            //
            //   string_ace - The fields of the ACE are in the following
            //                order and are separated by semicolons (;)
            //   for syntax see https://docs.microsoft.com/en-us/windows/win32/secauthz/ace-strings
            //   ace_type;ace_flags;rights;object_guid;inherit_object_guid;account_sid;(resource_attribute)
            // ace_type:
            //  "A" SDDL_ACCESS_ALLOWED
            // ace_flags:
            //  "OI"	SDDL_OBJECT_INHERIT
            //  "CI"	SDDL_CONTAINER_INHERIT
            //  "NP"	SDDL_NO_PROPAGATE
            // rights:
            //  "GA"	SDDL_GENERIC_ALL
            // account_sid: see https://docs.microsoft.com/en-us/windows/win32/secauthz/sid-strings
            //  "SY"	SDDL_LOCAL_SYSTEM
            //  "BA"	SDDL_BUILTIN_ADMINISTRATORS
            //  "CO"	SDDL_CREATOR_OWNER
            if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(
                //"D:P(A;OICI;GA;;;SY)(A;OICI;GA;;;BA)(A;OICI;GA;;;CO)",
                SSD.c_str(),
                SDDL_REVISION_1,
                &descriptor.lpSecurityDescriptor,
                NULL))
            {
                log("ConvertStringSecurityDescriptorToSecurityDescriptor error:",
                    GetLastError());
            }
        }

        ~security_descriptor()
        {
            LocalFree(descriptor.lpSecurityDescriptor);
        }
    };

    static security_descriptor global_access{ "D:P(A;OICI;GA;;;SY)(A;OICI;GA;;;BA)(A;OICI;GA;;;CO)" };
    */

    auto take_partition(text&& utf8_file_name)
    {
        auto result = text{};
        auto volume = std::vector<char>(std::max<size_t>(MAX_PATH, utf8_file_name.size() + 1));
        if (::GetVolumePathName(utf8_file_name.c_str(), volume.data(), (DWORD)volume.size()) != 0)
        {
            auto partition = std::vector<char>(50 + 1);
            if (0 != ::GetVolumeNameForVolumeMountPoint( volume.data(),
                                                      partition.data(),
                                               (DWORD)partition.size()))
            {
                result = text(partition.data());
            }
            else
            {
                //error_handler();
            }
        }
        else
        {
            //error_handler();
        }
        return result;
    }
    auto take_temp(text&& utf8_file_name)
    {
        auto tmp_dir = text{};
        auto file_guid = take_partition(std::move(utf8_file_name));
        auto i = uint8_t{ 0 };

        while (i < 100 && netxs::os::test_path(tmp_dir = file_guid + "\\$temp_" + utf::adjust(std::to_string(i++), 3, "0", true)))
        { }

        if (i == 100) tmp_dir.clear();

        return tmp_dir;
    }
    static auto trusted_domain_name() // static required by ::DsEnumerateDomainTrusts
    {
        auto info = PDS_DOMAIN_TRUSTS{};
        auto domain_name = text{};
        auto DomainCount = ULONG{};

        bool result = ::DsEnumerateDomainTrusts(nullptr, DS_DOMAIN_PRIMARY, &info, &DomainCount);
        if (result == ERROR_SUCCESS)
        {
            domain_name = text(info->DnsDomainName);
            ::NetApiBufferFree(info);
        }
        return domain_name;
    }
    static auto trusted_domain_guid() // static required by ::DsEnumerateDomainTrusts
    {
        auto info = PDS_DOMAIN_TRUSTS{};
        auto domain_guid = text{};
        auto domain_count = ULONG{};

        bool result = ::DsEnumerateDomainTrusts(nullptr, DS_DOMAIN_PRIMARY, &info, &domain_count);
        if (result == ERROR_SUCCESS && domain_count > 0)
        {
            auto& guid = info->DomainGuid;
            domain_guid = utf::to_hex(guid.Data1)
                  + '-' + utf::to_hex(guid.Data2)
                  + '-' + utf::to_hex(guid.Data3)
                  + '-' + utf::to_hex(std::string(guid.Data4, guid.Data4 + 2))
                  + '-' + utf::to_hex(std::string(guid.Data4 + 2, guid.Data4 + sizeof(guid.Data4)));

            ::NetApiBufferFree(info);
        }
        return domain_guid;
    }
    auto create_shortcut(text&& path_to_object, text&& path_to_link)
    {
        auto result = HRESULT{};
        IShellLink* psl;
        ::CoInitialize(0);
        result = ::CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl);

        if (result == S_OK)
        {
            IPersistFile* ppf;
            auto path_to_link_w = utf::to_utf(path_to_link);

            psl->SetPath(path_to_object.c_str());
            result = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);
            if (SUCCEEDED(result))
            {
                result = ppf->Save(path_to_link_w.c_str(), TRUE);
                ppf->Release();
                ::CoUninitialize();
                return true;
            }
            else
            {
                //todo
                //shell_error_handler(result);
            }
            psl->Release();
        }
        else
        {
            //todo
            //shell_error_handler(result);
        }
        ::CoUninitialize();

        return faux;
    }
    auto expand(text&& directory)
    {
        auto result = text{};
        if (auto len = ::ExpandEnvironmentStrings(directory.c_str(), NULL, 0))
        {
            auto buffer = std::vector<char>(len);
            if (::ExpandEnvironmentStrings(directory.c_str(), buffer.data(), (DWORD)buffer.size()))
            {
                result = text(buffer.data());
            }
        }
        return result;
    }
    auto set_registry_key(text&& key_path, text&& parameter_name, text&& value)
    {
        auto hKey = HKEY{};
        auto status = ::RegCreateKeyEx( HKEY_LOCAL_MACHINE,
                                        key_path.c_str(),
                                        0,
                                        0,
                                        REG_OPTION_NON_VOLATILE,
                                        KEY_ALL_ACCESS,
                                        NULL,
                                        &hKey,
                                        0);

        if (status == ERROR_SUCCESS && hKey != NULL)
        {
            status = ::RegSetValueEx( hKey,
                                      parameter_name.empty() ? nullptr : parameter_name.c_str(),
                                      0,
                                      REG_SZ,
                                      (BYTE*)value.c_str(),
                                      ((DWORD)value.size() + 1) * sizeof(wchar_t)
            );

            ::RegCloseKey(hKey);
        }
        else
        {
            //todo
            //error_handler();
        }

        return (status == ERROR_SUCCESS);
    }
    auto get_registry_string_value(text&& key_path, text&& parameter_name)
    {
        auto result = text{};
        auto hKey = HKEY{};
        auto value_type = DWORD{};
        auto data_length = DWORD{};
        auto status = ::RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                                      key_path.c_str(),
                                      0,
                                      KEY_ALL_ACCESS,
                                      &hKey);

        if (status == ERROR_SUCCESS && hKey != NULL)
        {
            auto param = parameter_name.empty() ? nullptr : parameter_name.c_str();

            status = ::RegQueryValueEx( hKey,
                                        param,
                                        NULL,
                                        &value_type,
                                        NULL,
                                        &data_length);

            if (status == ERROR_SUCCESS && value_type == REG_SZ)
            {
                auto data = std::vector<BYTE>(data_length);
                status = ::RegQueryValueEx( hKey,
                                            param,
                                            NULL,
                                            &value_type,
                                            data.data(),
                                            &data_length);

                if (status == ERROR_SUCCESS)
                {
                    result = text(utf::to_view(reinterpret_cast<char*>(data.data()), data.size()));
                }
            }
        }
        if (status != ERROR_SUCCESS)
        {
            //todo
            //error_handler();
        }

        return result;
    }
    auto get_registry_subkeys(text&& key_path)
    {
        auto result = list{};
        auto hKey = HKEY{};
        auto status = ::RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                                      key_path.c_str(),
                                      0,
                                      KEY_ALL_ACCESS,
                                      &hKey);

        if (status == ERROR_SUCCESS && hKey != NULL)
        {
            auto lpcbMaxSubKeyLen = DWORD{};
            if (ERROR_SUCCESS == ::RegQueryInfoKey(hKey, NULL, NULL, NULL, NULL, &lpcbMaxSubKeyLen, NULL, NULL, NULL, NULL, NULL, NULL))
            {
                auto size = lpcbMaxSubKeyLen;
                auto index = DWORD{ 0 };
                auto szName = std::vector<char>(size);

                while (ERROR_SUCCESS == ::RegEnumKeyEx( hKey,
                                                        index++,
                                                        szName.data(),
                                                        &lpcbMaxSubKeyLen,
                                                        NULL,
                                                        NULL,
                                                        NULL,
                                                        NULL))
                {
                    result.push_back(text(utf::to_view(szName.data(), std::min<size_t>(lpcbMaxSubKeyLen, size))));
                    lpcbMaxSubKeyLen = size;
                }
            }

            ::RegCloseKey(hKey);
        }

        return result;
    }
    auto cmdline()
    {
        auto result = list{};
        auto argc = int{ 0 };
        auto params = std::shared_ptr<void>(::CommandLineToArgvW(GetCommandLineW(), &argc), ::LocalFree);
        auto argv = (LPWSTR*)params.get();
        for (auto i = 0; i < argc; i++)
        {
            result.push_back(utf::to_utf(argv[i]));
        }

        return result;
    }
    static auto delete_registry_tree(text&& path) // static required by ::SHDeleteKey
    {
        auto hKey = HKEY{};
        auto status = ::RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                                      path.c_str(),
                                      0,
                                      KEY_ALL_ACCESS,
                                      &hKey);

        if (status == ERROR_SUCCESS && hKey != NULL)
        {
            status = ::SHDeleteKey(hKey, path.c_str());
            ::RegCloseKey(hKey);
        }

        auto result = status == ERROR_SUCCESS;

        if (!result)
        {
            //todo
            //error_handler();
        }

        return result;
    }
    void update_process_privileges(void)
    {
        auto hToken = INVALID_FD;
        auto tp = TOKEN_PRIVILEGES{};
        auto luid = LUID{};

        if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        {
            ::LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid);

            tp.PrivilegeCount = 1;
            tp.Privileges[0].Luid = luid;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

            ::AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
        }
    }
    auto kill_process(unsigned long proc_id)
    {
        auto result = faux;

        update_process_privileges();
        auto process_handle = ::OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, proc_id);
        if (process_handle && ::TerminateProcess(process_handle, 0))
        {
            result = ::WaitForSingleObject(process_handle, 10000) == WAIT_OBJECT_0;
        }
        else
        {
            //todo
            //error_handler();
        }

        if (process_handle) ::CloseHandle(process_handle);

        return result;
    }
    auto global_startup_dir()
    {
        auto result = text{};

        //todo vista & later
        // use SHGetFolderPath
        //PWSTR path;
        //if (S_OK != SHGetKnownFolderPath(FOLDERID_CommonStartup, 0, NULL, &path))
        //{
        //	//todo
        //	//error_handler();
        //}
        //else
        //{
        //	result = utils::to_utf(std::wstring(path)) + '\\';
        //	CoTaskMemFree(path);
        //}

        auto pidl = PIDLIST_ABSOLUTE{};
        if (S_OK == ::SHGetFolderLocation(NULL, CSIDL_COMMON_STARTUP, NULL, 0, &pidl))
        {
            char path[MAX_PATH];
            if (TRUE == ::SHGetPathFromIDList(pidl, path))
            {
                result = text(path) + '\\';
            }
            ::ILFree(pidl);
        }
        else
        {
            //todo
            //error_handler();
        }

        return result;
    }
    auto check_pipe(text const& path, text prefix = "\\\\.\\pipe\\")
    {
        auto hits = faux;
        auto next = WIN32_FIND_DATAA{};
        auto name = path.substr(prefix.size());
        auto what = prefix + '*';
        auto hndl = ::FindFirstFileA(what.c_str(), &next);
        if (hndl != INVALID_FD)
        {
            do hits = next.cFileName == name;
            while (!hits && ::FindNextFileA(hndl, &next));

            if (hits) log("path: ", path);
            ::FindClose(hndl);
        }
        return hits;
    }

    #endif  // Windows specific

    class fire
    {
        #if defined(_WIN32)
        
            fd_t h; // fire: Descriptor for IO interrupt.

        public:
            operator auto () { return h; }
            fire()           { ok(h = ::CreateEvent(NULL, TRUE, FALSE, NULL), "CreateEvent error"); }
           ~fire()           { os::close(h); }
            void reset()     { ok(::SetEvent(h), "SetEvent error"); }
            void flush()     { ok(::ResetEvent(h), "ResetEvent error"); }

        #else

            fd_t h[2] = { INVALID_FD, INVALID_FD }; // fire: Descriptors for IO interrupt.
            char x = 1;

        public:
            operator auto () { return h[0]; }
            fire()           { ok(::pipe(h), "pipe[2] creation failed"); }
           ~fire()           { for (auto& f : h) os::close(f); }
            void reset()     { os::send(h[1], &x, sizeof(x)); }
            void flush()     { os::recv(h[0], &x, sizeof(x)); }

        #endif
    };

    auto set_palette(si32 legacy)
    {
        auto yield = ansi::esc{};
        bool legacy_mouse = legacy & os::legacy::mouse;
        bool legacy_color = legacy & os::legacy::vga16;
        if (legacy_color)
        {
            auto set_pal = [&](auto p)
            {
                (yield.*p)(0,  rgba::color16[tint16::blackdk  ]);
                (yield.*p)(1,  rgba::color16[tint16::blacklt  ]);
                (yield.*p)(2,  rgba::color16[tint16::graydk   ]);
                (yield.*p)(3,  rgba::color16[tint16::graylt   ]);
                (yield.*p)(4,  rgba::color16[tint16::whitedk  ]);
                (yield.*p)(5,  rgba::color16[tint16::whitelt  ]);
                (yield.*p)(6,  rgba::color16[tint16::redlt    ]);
                (yield.*p)(7,  rgba::color16[tint16::bluelt   ]);
                (yield.*p)(8,  rgba::color16[tint16::greenlt  ]);
                (yield.*p)(9,  rgba::color16[tint16::yellowlt ]);
                (yield.*p)(10, rgba::color16[tint16::magentalt]);
                (yield.*p)(11, rgba::color16[tint16::reddk    ]);
                (yield.*p)(12, rgba::color16[tint16::bluedk   ]);
                (yield.*p)(13, rgba::color16[tint16::greendk  ]);
                (yield.*p)(14, rgba::color16[tint16::yellowdk ]);
                (yield.*p)(15, rgba::color16[tint16::cyanlt   ]);
            };
            yield.save_palette();
            //if (legacy_mouse) set_pal(&ansi::esc::old_palette);
            //else              set_pal(&ansi::esc::osc_palette);
            set_pal(&ansi::esc::old_palette);
            set_pal(&ansi::esc::osc_palette);

            os::send(STDOUT_FD, yield.data(), yield.size());
            yield.clear();
        }
    }
    auto rst_palette(si32 legacy)
    {
        auto yield = ansi::esc{};
        bool legacy_mouse = legacy & os::legacy::mouse;
        bool legacy_color = legacy & os::legacy::vga16;
        if (legacy_color)
        {
            //if (legacy_mouse) yield.old_palette_reset();
            //else              yield.osc_palette_reset();
            yield.old_palette_reset();
            yield.osc_palette_reset();
            yield.load_palette();
            os::send(STDOUT_FD, yield.data(), yield.size());
            yield.clear();
            log(" tty: palette restored");
        }
    }

    class pool
    {
        struct item
        {
            bool        state;
            std::thread guest;
        };

        std::recursive_mutex            mutex;
        std::condition_variable_any     synch;
        std::map<std::thread::id, item> index;
        si32                            count;
        bool                            alive;
        std::thread                     agent;

        void worker()
        {
            auto guard = std::unique_lock{ mutex };
            log("pool: session control started");

            while (alive)
            {
                synch.wait(guard);
                for (auto it = index.begin(); it != index.end();)
                {
                    auto& [sid, session] = *it;
                    auto& [state, guest] = session;
                    if (state == faux || !alive)
                    {
                        if (guest.joinable())
                        {
                            guard.unlock();
                            guest.join();
                            guard.lock();
                            log("pool: id: ", sid, " session joined");
                        }
                        it = index.erase(it);
                    }
                    else ++it;
                }
            }
        }
        void checkout()
        {
            auto guard = std::lock_guard{ mutex };
            auto session_id = std::this_thread::get_id();
            index[session_id].state = faux;
            synch.notify_one();
            log("pool: id: ", session_id, " session deleted");
        }

    public:
        template<class PROC>
        void run(PROC process)
        {
            auto guard = std::lock_guard{ mutex };
            auto next_id = count++;
            auto session = std::thread([&, process, next_id]()
            {
                process(next_id);
                checkout();
            });
            auto session_id = session.get_id();
            index[session_id] = { true, std::move(session) };
            log("pool: id: ", session_id, " session created");
        }
        auto size()
        {
            return index.size();
        }

        pool()
            : count{ 0    },
              alive{ true },
              agent{ &pool::worker, this }
        { }
        ~pool()
        {
            mutex.lock();
            alive = faux;
            synch.notify_one();
            mutex.unlock();

            if (agent.joinable())
            {
                log("pool: joining agent");
                agent.join();
            }
            log("pool: session control ended");
        }
    };

    namespace ipc
    {
        class iobase
        {
        protected:
            using flux = std::ostream;

            bool active{};
            text buffer{}; // ipc::iobase: Receive buffer.

        public:
            virtual ~iobase()
            { }
            operator bool () { return active; }
            virtual bool  send(view data)   = 0;
            virtual qiew  recv()            = 0;
            virtual bool  recv(char&)       = 0;
            virtual flux& show(flux&) const = 0;
            virtual void  stop()
            {
                active = faux;
            }
            virtual void  shut()
            {
                active = faux;
            }
            // ipc::iobase: Read until the delimeter appears.
            auto line(char delim)
            {
                char c;
                auto crop = text{};
                while (recv(c) && c != delim)
                {
                    crop.push_back(c);
                }
                return crop;
            }
            friend auto& operator << (flux& s, ipc::iobase const& sock)
            {
                return sock.show(s << "{ xipc: ") << " }";
            }
            friend auto& operator << (std::ostream& s, netxs::os::xipc const& sock)
            {
                return s << *sock;
            }
        };

        template<role ROLE>
        class memory
            : public iobase
        {
            sptr<fifo> server;
            sptr<fifo> client;

        public:
            memory(sptr<fifo> srv_queue, sptr<fifo> clt_queue)
                : server{ srv_queue },
                  client{ clt_queue }
            {
                active = true;
            }
            qiew recv() override
            {
                buffer.clear();
                if constexpr (ROLE == role::server) server->read(buffer);
                else                                client->read(buffer);
                return { buffer };
            }
            bool recv(char& c) override
            {
                if constexpr (ROLE == role::server) return server->read(c);
                else                                return client->read(c);
            }
            bool send(view data) override
            {
                if constexpr (ROLE == role::server) return client->send(data);
                else                                return server->send(data); 
            }
            flux& show(flux& s) const override
            {
                return s << "local pipe: server=" << server.get() << " client=" << client.get();
            }
            void shut() override
            {
                active = faux;
                server->stop();
                client->stop();
            }
            void stop() override
            {
                shut();
            }
        };

        class ptycon
            : public iobase
        {
            file handle; // ipc::ptycon: Stdio file descriptor.

        public:
            ptycon() = default;
            ptycon(fd_t r, fd_t w)
                : handle{ r, w }
            {
                active = true;
                buffer.resize(PIPE_BUF);
            }

            void set(fd_t r, fd_t w)
            {
                handle = { r, w };
                active = true;
                buffer.resize(PIPE_BUF);
            }
            auto& get_w()
            {
                return handle.get_w();
            }
            template<class SIZE_T>
            auto recv(char* buff, SIZE_T size)
            {
                return os::recv(handle, buff, size); // The read call can be interrupted by the write side when its read call is interrupted.
            }
            qiew recv() override // It's not thread safe!
            {
                return recv(buffer.data(), buffer.size());
            }
            bool recv(char& c) override
            {
                return recv(&c, sizeof(c));
            }
            bool send(view buff) override
            {
                auto data = buff.data();
                auto size = buff.size();
                return os::send<true>(handle.get_w(), data, size);
            }
            void shut() override
            {
                active = faux;
                handle.shutdown(); // Close the writing handle to interrupt a reading call on the server side and trigger to close the server writing handle to interrupt owr reading call.
            }
            void stop() override
            {
                shut();
            }
            flux& show(flux& s) const override
            {
                return s << handle;
            }
        };

        class direct
            : public iobase
        {
            dstd handle; // ipc::direct: Stdio file descriptor.

        public:
            direct() = default;
            direct(fd_t r, fd_t w, fd_t l)
                : handle{ r, w, l }
            {
                active = true;
                buffer.resize(PIPE_BUF);
            }

            void set(fd_t r, fd_t w, fd_t l)
            {
                handle = { r, w, l };
                active = true;
                buffer.resize(PIPE_BUF);
            }
            auto& get_w()
            {
                return handle.get_w();
            }
            template<class SIZE_T>
            auto recv(char* buff, SIZE_T size)
            {
                return os::recv(handle.get_r(), buff, size); // The read call can be interrupted by the write side when its read call is interrupted.
            }
            qiew recv() override // It's not thread safe!
            {
                return recv(buffer.data(), buffer.size());
            }
            bool recv(char& c) override
            {
                return recv(&c, sizeof(c));
            }
            bool send(view buff) override
            {
                auto data = buff.data();
                auto size = buff.size();
                return os::send<true>(handle.get_w(), data, size);
            }
            template<class SIZE_T>
            qiew rlog(char* buff, SIZE_T size)
            {
                return os::recv(handle.get_l(), buff, size); // The read call can be interrupted by the write side when its read call is interrupted.
            }
            void shut() override // Only for server side.
            {
                active = faux;
                handle.shutsend(); // Close only writing handle to interrupt a reading call on the server side and trigger to close the server writing handle to interrupt owr reading call.
            }
            void stop() override
            {
                active = faux;
                handle.shutdown(); // Close all writing handles (output + logs) to interrupt a reading call on the server side and trigger to close the server writing handle to interrupt owr reading call.
            }
            flux& show(flux& s) const override
            {
                return s << handle;
            }
        };

        class socket
            : public iobase
        {
            file handle; // ipc:socket: Socket file descriptor.
            text scpath; // ipc:socket: Socket path (in order to unlink).
            fire signal; // ipc:socket: Interruptor.

        public:
            socket(file descriptor = {})
                : handle{ std::move(descriptor) }
            {
                if (handle) init();
            }
            ~socket()
            {
                #if defined(__BSD__)

                    if (scpath.length())
                    {
                        ::unlink(scpath.c_str()); // Cleanup file system unix domain socket.
                    }

                #endif
            }

            void set_path(text path)
            {
                scpath = path;
            }
            void init(si32 buff_size = PIPE_BUF)
            {
                active = true;
                buffer.resize(buff_size);
            }
            auto& get_r()
            {
                return handle.get_r();
            }
            auto& get_w()
            {
                return handle.get_w();
            }
            template<class T>
            auto cred(T id) const // Check peer cred.
            {
                #if defined(_WIN32)

                    //Note: Named Pipes - default ACL used for a named pipe grant full control to the LocalSystem, admins, and the creator owner
                    //https://docs.microsoft.com/en-us/windows/win32/ipc/named-pipe-security-and-access-rights

                #elif defined(__linux__)

                    auto cred = ucred{};
                    #ifndef __ANDROID__
                        auto size = socklen_t{ sizeof(cred) };
                    #else
                        auto size = unsigned{ sizeof(cred) };
                    #endif

                    if (!ok(::getsockopt(handle.get_r(), SOL_SOCKET, SO_PEERCRED, &cred, &size), "getsockopt(SOL_SOCKET) failed"))
                    {
                        return faux;
                    }

                    if (cred.uid && id != cred.uid)
                    {
                        log("sock: other users are not allowed to the session, abort");
                        return faux;
                    }

                    log("sock: creds from SO_PEERCRED:",
                            "\n\t  pid: ", cred.pid,
                            "\n\t euid: ", cred.uid,
                            "\n\t egid: ", cred.gid);

                #elif defined(__BSD__)

                    auto euid = uid_t{};
                    auto egid = gid_t{};

                    if (!ok(::getpeereid(handle.get_r(), &euid, &egid), "getpeereid failed"))
                    {
                        return faux;
                    }

                    if (euid && id != euid)
                    {
                        log("sock: other users are not allowed to the session, abort");
                        return faux;
                    }

                    log("sock: creds from getpeereid:",
                            "\n\t  pid: ", id,
                            "\n\t euid: ", euid,
                            "\n\t egid: ", egid);

                #endif

                return true;
            }
            auto meet() -> std::shared_ptr<ipc::socket>
            {
                #if defined(_WIN32)

                    auto to_server = RD_PIPE_PATH + scpath;
                    auto to_client = WR_PIPE_PATH + scpath;
                    auto next_link = [&](auto h, auto const& path, auto type)
                    {
                        auto next_waiting_point = INVALID_FD;
                        auto connected = ::ConnectNamedPipe(h, NULL)
                            ? true
                            : (::GetLastError() == ERROR_PIPE_CONNECTED);

                        if (active && connected) // Recreate the waiting point for the next client.
                        {
                            next_waiting_point =
                                ::CreateNamedPipe( path.c_str(),             // pipe path
                                                   type,                     // read/write access
                                                   PIPE_TYPE_BYTE |          // message type pipe
                                                   PIPE_READMODE_BYTE |      // message-read mode
                                                   PIPE_WAIT,                // blocking mode
                                                   PIPE_UNLIMITED_INSTANCES, // max. instances
                                                   PIPE_BUF,                 // output buffer size
                                                   PIPE_BUF,                 // input buffer size
                                                   0,                        // client time-out
                                                   NULL);                    // DACL (pipe_acl)
                            // DACL: auto pipe_acl = security_descriptor(security_descriptor_string);
                            //       The ACLs in the default security descriptor for a named pipe grant full control to the
                            //       LocalSystem account, administrators, and the creator owner.They also grant read access to
                            //       members of the Everyone groupand the anonymous account.
                            //       Without write access, the desktop will be inaccessible to non-owners.
                        }
                        else os::fail("not active");

                        return next_waiting_point;
                    };

                    auto r = next_link(handle.get_r(), to_server, PIPE_ACCESS_INBOUND);
                    if (r == INVALID_FD)
                    {
                        return os::fail("CreateNamedPipe error (read)");
                    }

                    auto w = next_link(handle.get_w(), to_client, PIPE_ACCESS_OUTBOUND);
                    if (w == INVALID_FD)
                    {
                        ::CloseHandle(r);
                        return os::fail("CreateNamedPipe error (write)");
                    }

                    auto connector = std::make_shared<ipc::socket>(std::move(handle));
                    handle = { r, w };

                    return connector;

                #else

                    auto result = std::shared_ptr<ipc::socket>{};
                    auto h_proc = [&]()
                    {
                        auto h = ::accept(handle.get_r(), 0, 0);
                        auto s = file{ h, h };
                        if (s) result = std::make_shared<ipc::socket>(std::move(s));
                    };
                    auto f_proc = [&]()
                    {
                        log("xipc: signal fired");
                        signal.flush();
                    };

                    os::select(handle.get_r(), h_proc,
                               signal        , f_proc);

                    return result;

                #endif
            }
            template<class SIZE_T>
            auto recv(char* buff, SIZE_T size)
            {
                return os::recv(handle, buff, size);
            }
            qiew recv() override // It's not thread safe!
            {
                return recv(buffer.data(), buffer.size());
            }
            bool recv(char& c) override
            {
                return recv(&c, sizeof(c));
            }
            bool send(view buff) override
            {
                auto data = buff.data();
                auto size = buff.size();
                return os::send<faux>(handle.get_w(), data, size);
            }
            void shut() override
            {
                active = faux;
                #if defined(_WIN32)

                    // Interrupt ::ConnectNamedPipe(). Disconnection order does matter.
                    auto to_client = WR_PIPE_PATH + scpath;
                    auto to_server = RD_PIPE_PATH + scpath;
                    if (handle.get_w() != INVALID_FD) ok(::DeleteFileA(to_client.c_str()));
                    if (handle.get_r() != INVALID_FD) ok(::DeleteFileA(to_server.c_str()));
                    handle.close();

                #else

                    signal.reset();

                #endif
            }
            void stop() override
            {
                active = faux;
                #if defined(_WIN32)

                    // Disconnection order does matter.
                    if (handle.get_w() != INVALID_FD) ok(::DisconnectNamedPipe(handle.get_w()));
                    if (handle.get_r() != INVALID_FD) ok(::DisconnectNamedPipe(handle.get_r()));

                #else

                    //an important conceptual reason to want
                    //to use shutdown:
                    //             to signal EOF to the peer
                    //             and still be able to receive
                    //             pending data the peer sent.
                    //"shutdown() doesn't actually close the file descriptor
                    //            — it just changes its usability.
                    //To free a socket descriptor, you need to use close()."

                    log("xipc: shutdown: handle descriptor: ", handle.get_r());
                    if (!ok(::shutdown(handle.get_r(), SHUT_RDWR), "descriptor shutdown error"))  // Further sends and receives are disallowed.
                    {
                        switch (errno)
                        {
                            case EBADF:    os::fail("EBADF: The socket argument is not a valid file descriptor.");                             break;
                            case EINVAL:   os::fail("EINVAL: The how argument is invalid.");                                                   break;
                            case ENOTCONN: os::fail("ENOTCONN: The socket is not connected.");                                                 break;
                            case ENOTSOCK: os::fail("ENOTSOCK: The socket argument does not refer to a socket.");                              break;
                            case ENOBUFS:  os::fail("ENOBUFS: Insufficient resources were available in the system to perform the operation."); break;
                            default:       os::fail("unknown reason");                                                                         break;
                        }
                    }

                #endif
            }
            flux& show(flux& s) const override
            {
                return s << handle;
            }
        };

        template<role ROLE, class P = noop>
        auto open(text path, datetime::period retry_timeout = {}, P retry_proc = P())
            -> std::shared_ptr<ipc::socket>
        {
            auto sock_ptr = std::make_shared<ipc::socket>();
            auto& r_sock = sock_ptr->get_r();
            auto& w_sock = sock_ptr->get_w();

            auto try_start = [&](auto play) -> bool
            {
                auto done = play();
                if (!done)
                {
                    if constexpr (ROLE == role::client)
                    {
                        if (!retry_proc())
                        {
                            return os::fail("failed to start server");
                        }
                    }

                    auto stop = datetime::tempus::now() + retry_timeout;
                    do
                    {
                        std::this_thread::sleep_for(100ms);
                        done = play();
                    }
                    while (!done && stop > datetime::tempus::now());
                }
                return done;
            };

            #if defined(_WIN32)

                //security_descriptor pipe_acl(security_descriptor_string);
                //log("pipe: DACL=", pipe_acl.security_string);

                sock_ptr->set_path(path);
                auto to_server = RD_PIPE_PATH + path;
                auto to_client = WR_PIPE_PATH + path;

                if constexpr (ROLE == role::server)
                {
                    if (os::check_pipe(to_server))
                    {
                        return os::fail("server already running");
                    }

                    auto pipe = [](auto const& path, auto type)
                    {
                        return ::CreateNamedPipe( path.c_str(),             // pipe path
                                                  type,                     // read/write access
                                                  PIPE_TYPE_BYTE |          // message type pipe
                                                  PIPE_READMODE_BYTE |      // message-read mode
                                                  PIPE_WAIT,                // blocking mode
                                                  PIPE_UNLIMITED_INSTANCES, // max instances
                                                  PIPE_BUF,                 // output buffer size
                                                  PIPE_BUF,                 // input buffer size
                                                  0,                        // client time-out
                                                  NULL);                    // DACL
                    };

                    auto r = pipe(to_server, PIPE_ACCESS_INBOUND);
                    if (r == INVALID_FD)
                    {
                        return os::fail("CreateNamedPipe error (read)");
                    }

                    auto w = pipe(to_client, PIPE_ACCESS_OUTBOUND);
                    if (w == INVALID_FD)
                    {
                        ::CloseHandle(r);
                        return os::fail("CreateNamedPipe error (write)");
                    }

                    r_sock = r;
                    w_sock = w;
                }
                else if constexpr (ROLE == role::client)
                {
                    auto pipe = [](auto const& path, auto type)
                    {
                        return ::CreateFile( path.c_str(),  // pipe path
                                             type,
                                             0,             // no sharing
                                             NULL,          // default security attributes
                                             OPEN_EXISTING, // opens existing pipe
                                             0,             // default attributes
                                             NULL);         // no template file
                    };
                    auto play = [&]()
                    {
                        auto w = pipe(to_server, GENERIC_WRITE);
                        if (w == INVALID_FD)
                        {
                            return faux;
                        }

                        auto r = pipe(to_client, GENERIC_READ);
                        if (r == INVALID_FD)
                        {
                            ::CloseHandle(w);
                            return faux;
                        }

                        r_sock = r;
                        w_sock = w;
                        return true;
                    };
                    if (!try_start(play))
                    {
                        return os::fail("connection error");
                    }
                }

            #else

                ok(::signal(SIGPIPE, SIG_IGN), "failed to set SIG_IGN");

                auto addr = sockaddr_un{};
                auto sun_path = addr.sun_path + 1; // Abstract namespace socket (begins with zero). The abstract socket namespace is a nonportable Linux extension.

                #if defined(__BSD__)
                    auto home = os::homepath() + MONOTTY_FOLDER;
                    if (!std::filesystem::exists(home))
                    {
                        log("path: create home directory ", home);
                        std::filesystem::create_directory(home);
                    }
                    path = home + path + ".sock";
                    sun_path--; // File system unix domain socket.
                    log("open: file system socket ", path);
                #endif

                if (path.size() > sizeof(sockaddr_un::sun_path) - 2)
                {
                    return os::fail("socket path too long");
                }

                if ((w_sock = ::socket(AF_UNIX, SOCK_STREAM, 0)) == INVALID_FD)
                {
                    return os::fail("open unix domain socket error");
                }
                r_sock = w_sock;

                addr.sun_family = AF_UNIX;
                auto sock_addr_len = (socklen_t)(sizeof(addr) - (sizeof(sockaddr_un::sun_path) - path.size() - 1));
                std::copy(path.begin(), path.end(), sun_path);

                auto play = [&]()
                {
                    return -1 != ::connect(r_sock, (struct sockaddr*)&addr, sock_addr_len);
                };

                if constexpr (ROLE == role::server)
                {
                    #if defined(__BSD__)
                        if (std::filesystem::exists(path))
                        {
                            if (play())
                            {
                                os::close(r_sock);
                                return os::fail("server already running");
                            }
                            else
                            {
                                log("path: cleanup file system socket file ", path);
                                ::unlink(path.c_str()); // Cleanup file system socket.
                            }
                        }
                    #endif

                    sock_ptr->set_path(path); // For unlink on exit (file system socket).

                    if (::bind(r_sock, (struct sockaddr*)&addr, sock_addr_len) == -1)
                    {
                        os::close(r_sock);
                        return os::fail("error unix socket bind for ", path);
                    }

                    if (::listen(r_sock, 5) == -1)
                    {
                        os::close(r_sock);
                        return os::fail("error listen socket for ", path);
                    }
                }
                else if constexpr (ROLE == role::client)
                {
                    if (!try_start(play))
                    {
                        return os::fail("connection error");
                    }
                }

            #endif

            sock_ptr->init();
            return sock_ptr;
        }
        auto local(si32 vtmode) -> std::pair<sptr<ipc::iobase>, sptr<ipc::iobase>>
        {
            if (vtmode & os::legacy::direct)
            {
                auto server = std::make_shared<ipc::direct>(STDIN_FD, STDOUT_FD, STDERR_FD);
                auto client = server;
                return std::make_pair( server, client );
            }
            else
            {
                auto squeue = std::make_shared<fifo>();
                auto cqueue = std::make_shared<fifo>();
                auto server = std::make_shared<ipc::memory<os::server>>(squeue, cqueue);
                auto client = std::make_shared<ipc::memory<os::client>>(squeue, cqueue);
                return std::make_pair( server, client );
            }
        }
        template<class G>
        auto splice(G& gate, si32 vtmode)
        {
            gate.output(ansi::esc{}.save_title()
                                   .altbuf(true)
                                   .vmouse(true)
                                   .cursor(faux)
                                   .bpmode(true)
                                   .setutf(true));
            gate.splice(vtmode);
            gate.output(ansi::esc{}.scrn_reset()
                                   .vmouse(faux)
                                   .cursor(true)
                                   .altbuf(faux)
                                   .bpmode(faux)
                                   .load_title());
            std::this_thread::sleep_for(200ms); // Pause to complete consuming/receiving buffered input (e.g. mouse tracking) that has just been canceled.
        }
        auto logger(si32 vtmode)
        {
            auto direct = !!(vtmode & os::legacy::direct);
            return direct ? netxs::logger([](auto data) { os::stdlog(data); })
                          : netxs::logger([](auto data) { os::syslog(data); });
        }
    }

    class tty
    {
        fire signal;

        template<class V>
        struct _globals
        {
            static xipc        ipcio;
            static conmode     state;
            static testy<twod> winsz;
            static void resize_handler()
            {
                static constexpr auto winsz_fallback = twod{ 132, 60 };

                #if defined(_WIN32)

                    auto cinfo = CONSOLE_SCREEN_BUFFER_INFO{};
                    if (ok(::GetConsoleScreenBufferInfo(STDOUT_FD, &cinfo), "GetConsoleScreenBufferInfo failed"))
                    {
                        winsz({ cinfo.srWindow.Right  - cinfo.srWindow.Left + 1,
                                cinfo.srWindow.Bottom - cinfo.srWindow.Top  + 1 });
                    }

                #else

                    auto size = winsize{};
                    if (ok(::ioctl(STDOUT_FD, TIOCGWINSZ, &size), "ioctl(STDOUT_FD, TIOCGWINSZ) failed"))
                    {
                        winsz({ size.ws_col, size.ws_row });
                    }

                #endif

                    else
                    {
                        log("xtty: fallback tty window size ", winsz_fallback, " (consider using 'ssh -tt ...')");
                        winsz(winsz_fallback);
                    }

                if (winsz.test)
                {
                    ipcio->send(ansi::win(winsz.last));
                }
            }

            #if defined(_WIN32)

                static void default_mode()
                {
                    ok(::SetConsoleMode(STDOUT_FD, state[0]), "SetConsoleMode failed (revert_o)");
                    ok(::SetConsoleMode(STDIN_FD , state[1]), "SetConsoleMode failed (revert_i)");
                }
                static BOOL signal_handler(DWORD signal)
                {
                    switch (signal)
                    {
                        case CTRL_C_EVENT:
                            ipcio->send(view{ &ansi::C0_ETX, 1 });
                            break;
                        case CTRL_BREAK_EVENT:
                            ipcio->send(view{ &ansi::C0_ETX, 1 });
                            break;
                        case CTRL_CLOSE_EVENT:
                            /**/
                            break;
                        case CTRL_LOGOFF_EVENT:
                            /**/
                            break;
                        case CTRL_SHUTDOWN_EVENT:
                            /**/
                            break;
                        default:
                            break;
                    }
                    return TRUE;
                }

            #else

                static void default_mode()
                {
                    ::tcsetattr(STDIN_FD, TCSANOW, &state);
                }
                static void shutdown_handler(int signal)
                {
                    ipcio->stop();
                    log(" tty: sock->xipc::shut called");
                    ::signal(signal, SIG_DFL);
                    ::raise(signal);
                }
                static void signal_handler(int signal)
                {
                    switch (signal)
                    {
                        case SIGWINCH:
                            resize_handler();
                            return;
                        case SIGHUP:
                            log(" tty: SIGHUP");
                            shutdown_handler(signal);
                            break;
                        case SIGTERM:
                            log(" tty: SIGTERM");
                            shutdown_handler(signal);
                            break;
                        default:
                            break;
                    }
                    log(" tty: signal_handler, signal=", signal);
                }

            #endif
        };

        void reader(si32 mode)
        {
            log(" tty: id: ", std::this_thread::get_id(), " reading thread started");
            auto& ipcio =*_globals<void>::ipcio;

            #if defined(_WIN32)

            // The input codepage to UTF-8 is severely broken in all Windows versions.
            // ReadFile and ReadConsoleA either replace non-ASCII characters with NUL
            // or return 0 bytes read.
            auto reply = std::vector<INPUT_RECORD>(1);
            auto yield = ansi::esc{};
            auto count = DWORD{};
            fd_t waits[] = { STDIN_FD, signal };

            #ifdef VTM_USE_CLASSICAL_WIN32_INPUT

            auto xlate_bttns = [](auto bttns)
            {
                auto b = si32{};
                b |= bttns & FROM_LEFT_1ST_BUTTON_PRESSED ? (1 << input::sysmouse::left  ) : 0;
                b |= bttns & RIGHTMOST_BUTTON_PRESSED     ? (1 << input::sysmouse::right ) : 0;
                b |= bttns & FROM_LEFT_2ND_BUTTON_PRESSED ? (1 << input::sysmouse::middle) : 0;
                b |= bttns & FROM_LEFT_3RD_BUTTON_PRESSED ? (1 << input::sysmouse::wheel ) : 0;
                b |= bttns & FROM_LEFT_4TH_BUTTON_PRESSED ? (1 << input::sysmouse::win   ) : 0;
                return b;
            };

            while (WAIT_OBJECT_0 == ::WaitForMultipleObjects(2, waits, FALSE, INFINITE))
            {
                if (!::GetNumberOfConsoleInputEvents(STDIN_FD, &count))
                {
                    // ERROR_PIPE_NOT_CONNECTED
                    // 233 (0xE9)
                    // No process is on the other end of the pipe.
                    //defeat("GetNumberOfConsoleInputEvents error");
                    os::exit(-1, " tty: GetNumberOfConsoleInputEvents error ", ::GetLastError());
                    break;
                }
                else if (count)
                {
                    if (count > reply.size()) reply.resize(count);

                    if (!::ReadConsoleInputW(STDIN_FD, reply.data(), (DWORD)reply.size(), &count))
                    {
                        //ERROR_PIPE_NOT_CONNECTED = 0xE9 - it's means that the console is gone/crashed
                        //defeat("ReadConsoleInput error");
                        os::exit(-1, " tty: ReadConsoleInput error ", ::GetLastError());
                        break;
                    }
                    else
                    {
                        auto entry = reply.begin();
                        auto limit = entry + count;
                        yield.dtvt_begin();
                        while (entry != limit)
                        {
                            auto& reply = *entry++;
                            switch (reply.EventType)
                            {
                                case KEY_EVENT:
                                    yield.dtvt_keybd(0,
                                        reply.Event.KeyEvent.wVirtualKeyCode,
                                        reply.Event.KeyEvent.wVirtualScanCode,
                                        reply.Event.KeyEvent.bKeyDown,
                                        os::kbstate(reply.Event.KeyEvent.dwControlKeyState, reply.Event.KeyEvent.wVirtualScanCode),
                                        reply.Event.KeyEvent.wRepeatCount,
                                        utf::to_utf(reply.Event.KeyEvent.uChar.UnicodeChar));
                                    break;
                                case MOUSE_EVENT:
                                    yield.dtvt_mouse(0,
                                        xlate_bttns(reply.Event.MouseEvent.dwButtonState),
                                        os::kbstate(reply.Event.MouseEvent.dwControlKeyState),
                                        reply.Event.MouseEvent.dwEventFlags,
                                        static_cast<int16_t>((0xFFFF0000 & reply.Event.MouseEvent.dwButtonState) >> 16), // dwButtonState too large when mouse scrolls
                                        reply.Event.MouseEvent.dwMousePosition.X,
                                        reply.Event.MouseEvent.dwMousePosition.Y);
                                    break;
                                case WINDOW_BUFFER_SIZE_EVENT:
                                    _globals<void>::resize_handler();
                                    break;
                                case FOCUS_EVENT:
                                    yield.dtvt_focus(0,
                                        reply.Event.FocusEvent.bSetFocus);
                                    break;
                                default:
                                    break;
                            }
                        }
                        yield.dtvt_close();
                        ipcio.send(yield);
                        yield.clear();
                    }
                }
            }

            #else

            auto buff = std::vector<wchar_t>(STDIN_BUF);
            while (WAIT_OBJECT_0 == ::WaitForMultipleObjects(2, waits, FALSE, INFINITE))
            {
                if (!::GetNumberOfConsoleInputEvents(STDIN_FD, &count))
                {
                    defeat("GetNumberOfConsoleInputEvents error");
                }
                else if (count)
                {
                    if (count > reply.size()) reply.resize(count);

                    if (!::PeekConsoleInput(STDIN_FD, reply.data(), (DWORD)reply.size(), &count))
                    {
                        //ERROR_PIPE_NOT_CONNECTED = 0xE9 - it's means that the console is gone/crashed
                        defeat("PeekConsoleInput error");
                    }
                    else
                    {
                        auto entry = reply.begin();
                        auto limit = entry + count;
                        auto vtcon = faux;
                        yield.clear();
                        while (entry != limit)
                        {
                            auto& reply = *entry++;
                            switch (reply.EventType)
                            {
                                case KEY_EVENT:
                                    // ReadConsole ignores key up event
                                    vtcon |= static_cast<bool>(reply.Event.KeyEvent.bKeyDown);
                                    break;
                                case MOUSE_EVENT:
                                    break;
                                case WINDOW_BUFFER_SIZE_EVENT: // Valid only for alt buffer.
                                    yield += ansi::win({
                                        reply.Event.WindowBufferSizeEvent.dwSize.X,
                                        reply.Event.WindowBufferSizeEvent.dwSize.Y });
                                    break;
                                case FOCUS_EVENT:
                                    yield += ansi::fcs(
                                        reply.Event.FocusEvent.bSetFocus);
                                    break;
                                default:
                                    break;
                            }
                        }

                        if (vtcon)
                        {
                            auto state = CONSOLE_READCONSOLE_CONTROL{ sizeof(state) };

                            ::ReadConsoleW( input, // Auto flushed after reading.
                                            buff.data(),
                                            (DWORD)buff.size(),
                                            &count,
                                            &state);

                            //todo forward key ctrl state too
                            yield += utf::to_utf(buff.data(), count);
                        }
                        else ::FlushConsoleInputBuffer(STDIN_FD);

                        ipcio.send(yield);
                    }
                }
            }

            #endif // USE_WIN32_INPUT

            #else

                bool legacy_mouse = mode & os::legacy::mouse;
                bool legacy_color = mode & os::legacy::vga16;
                auto micefd = INVALID_FD;
                twod mcoor;
                auto buffer = text(STDIN_BUF, '\0');
                si32 ttynum = 0;
                auto yield = ansi::esc{};

                struct
                {
                    testy<twod> coord;
                    testy<si32> shift = 0;
                    testy<si32> bttns = 0;
                    si32        flags = 0;
                } state;
                auto get_kb_state = []()
                {
                    si32 state = 0;
                    #if defined(__linux__)
                        si32 shift_state = 6;
                        ok(::ioctl(STDIN_FD, TIOCLINUX, &shift_state), "ioctl(STDIN_FD, TIOCLINUX) failed");
                        state = 0
                            | (shift_state & (1 << KG_ALTGR)) >> 1 // 0x1
                            | (shift_state & (1 << KG_ALT  )) >> 2 // 0x2
                            | (shift_state & (1 << KG_CTRLR)) >> 5 // 0x4
                            | (shift_state & (1 << KG_CTRL )) << 1 // 0x8
                            | (shift_state & (1 << KG_SHIFT)) << 4 // 0x10
                            ;
                    #endif
                    return state;
                };
                ok(::ttyname_r(STDOUT_FD, buffer.data(), buffer.size()), "ttyname_r(STDOUT_FD) failed");
                auto tty_name = view(buffer.data());
                log(" tty: pseudoterminal ", tty_name);
                if (legacy_mouse)
                {
                    log(" tty: compatibility mode");
                    auto imps2_init_string = "\xf3\xc8\xf3\x64\xf3\x50";
                    auto mouse_device = "/dev/input/mice";
                    auto mouse_fallback = "/dev/input/mice_vtm";
                    auto fd = ::open(mouse_device, O_RDWR);
                    if (fd == -1)
                    {
                        fd = ::open(mouse_fallback, O_RDWR);
                    }
                    if (fd == -1)
                    {
                        log(" tty: error opening ", mouse_device, " and ", mouse_fallback, ", error ", errno, errno == 13 ? " - permission denied" : "");
                    }
                    else if (os::send(fd, imps2_init_string, sizeof(imps2_init_string)))
                    {
                        char ack;
                        os::recv(fd, &ack, sizeof(ack));
                        micefd = fd;
                        auto tty_word = tty_name.find("tty", 0);
                        if (tty_word != text::npos)
                        {
                            tty_word += 3; /*skip tty letters*/
                            auto tty_number = utf::to_view(buffer.data() + tty_word, buffer.size() - tty_word);
                            if (auto cur_tty = utf::to_int(tty_number))
                            {
                                ttynum = cur_tty.value();
                            }
                        }
                        yield.show_mouse(true);
                        ipcio.send(view(yield));
                        yield.clear();
                        if (ack == '\xfa') log(" tty: ImPS/2 mouse connected, fd: ", fd);
                        else               log(" tty: unknown PS/2 mouse connected, fd: ", fd, " ack: ", (int)ack);
                    }
                    else
                    {
                        log(" tty: mouse initialization error");
                        os::close(fd);
                    }
                }

                auto h_proc = [&]()
                {
                    if (micefd != INVALID_FD
                     && state.shift(get_kb_state()))
                    {
                        yield.meta_state(state.shift.last);
                        auto data = os::recv(STDIN_FD, buffer.data(), buffer.size());
                        yield.add(data);
                        ipcio.send(yield);
                        yield.clear();
                    }
                    else
                    {
                        auto data = os::recv(STDIN_FD, buffer.data(), buffer.size());
                        ipcio.send(data);
                    }
                };
                auto m_proc = [&]()
                {
                    auto data = os::recv(micefd, buffer.data(), buffer.size());
                    auto size = data.size();
                    if (size == 4 /* ImPS/2 */
                     || size == 3 /* PS/2 compatibility mode */)
                    {
                    #if defined(__linux__)
                        vt_stat vt_state;
                        ok(::ioctl(STDOUT_FD, VT_GETSTATE, &vt_state), "ioctl(VT_GETSTATE) failed");
                        if (vt_state.v_active == ttynum) // Proceed current active tty only.
                        {
                            auto scale = twod{ 6,12 }; //todo magic numbers
                            auto bttns = data[0] & 7;
                            mcoor.x   += data[1];
                            mcoor.y   -= data[2];
                            auto wheel =-size == 4 ? data[3] : 0;
                            auto limit = _globals<void>::winsz.last * scale;
                            if (bttns == 0) mcoor = std::clamp(mcoor, dot_00, limit);
                            state.flags = wheel ? 4 : 0;
                            if (state.coord(mcoor / scale)
                             || state.bttns(bttns)
                             || state.shift(get_kb_state())
                             || state.flags)
                            {
                                yield.dtvt_begin()
                                     .dtvt_mouse(0,
                                        state.bttns.last,
                                        state.shift.last,
                                        state.flags,
                                        wheel,
                                        state.coord.last.x,
                                        state.coord.last.y)
                                     .dtvt_close();
                                ipcio.send(view(yield));
                                yield.clear();
                            }
                        }
                    #endif
                    }
                };
                auto f_proc = [&]()
                {
                    log(" tty: signal fired");
                    signal.flush();
                };

                while (ipcio)
                {
                    if (micefd != INVALID_FD)
                    {
                        os::select(STDIN_FD, h_proc,
                                   micefd,   m_proc,
                                   signal,   f_proc);
                    }
                    else
                    {
                        os::select(STDIN_FD, h_proc,
                                   signal,   f_proc);
                    }
                }

                os::close(micefd);

            #endif

            log(" tty: id: ", std::this_thread::get_id(), " reading thread ended");
        }

        tty()
        { }

    public:
        static auto proxy(xipc pipe_link)
        {
            _globals<void>::ipcio = pipe_link;
            return tty{};
        }
        bool output(view utf8)
        {
            return os::send<true>(STDOUT_FD, utf8.data(), utf8.size());
        }
        auto ignite(si32 vtmode)
        {
            if (vtmode & os::legacy::direct)
            {
                auto& winsz = _globals<void>::winsz;
                winsz = os::legacy::get_winsz();
                return winsz;
            }

            auto& sig_hndl = _globals<void>::signal_handler;

            #if defined(_WIN32)

                auto& omode = _globals<void>::state[0];
                auto& imode = _globals<void>::state[1];

                ok(::GetConsoleMode(STDOUT_FD, &omode), "GetConsoleMode(STDOUT_FD) failed");
                ok(::GetConsoleMode(STDIN_FD , &imode), "GetConsoleMode(STDIN_FD) failed");

                DWORD inpmode = 0
                              | ENABLE_EXTENDED_FLAGS
                              | ENABLE_PROCESSED_INPUT
                              | ENABLE_WINDOW_INPUT
                              | ENABLE_MOUSE_INPUT
                            #ifndef VTM_USE_CLASSICAL_WIN32_INPUT
                              | ENABLE_VIRTUAL_TERMINAL_INPUT
                            #endif
                              ;
                ok(::SetConsoleMode(STDIN_FD, inpmode), "SetConsoleMode(STDIN_FD) failed");

                DWORD outmode = 0
                              | ENABLE_PROCESSED_OUTPUT
                              | ENABLE_VIRTUAL_TERMINAL_PROCESSING
                              | DISABLE_NEWLINE_AUTO_RETURN
                              ;
                ok(::SetConsoleMode(STDOUT_FD, outmode), "SetConsoleMode(STDOUT_FD) failed");
                ok(::SetConsoleCtrlHandler(sig_hndl, TRUE), "SetConsoleCtrlHandler failed");

            #else

                auto& state = _globals<void>::state;
                if (ok(::tcgetattr(STDIN_FD, &state), "tcgetattr(STDIN_FD) failed")) // Set stdin raw mode.
                {
                    auto raw_mode = state;
                    ::cfmakeraw(&raw_mode);
                    ok(::tcsetattr(STDIN_FD, TCSANOW, &raw_mode), "tcsetattr(STDIN_FD, TCSANOW) failed");
                }
                else
                {
                    if (_globals<void>::ipcio)
                    {
                        _globals<void>::ipcio->stop();
                    }
                    os::fail("warning: check you are using the proper tty device, try `ssh -tt ...` option");
                }

                ok(::signal(SIGPIPE , SIG_IGN ), "set signal(SIGPIPE ) failed");
                ok(::signal(SIGWINCH, sig_hndl), "set signal(SIGWINCH) failed");
                ok(::signal(SIGTERM , sig_hndl), "set signal(SIGTERM ) failed");
                ok(::signal(SIGHUP  , sig_hndl), "set signal(SIGHUP  ) failed");

            #endif

            ::atexit(_globals<void>::default_mode);
            _globals<void>::resize_handler();

            return _globals<void>::winsz;
        }
        void splice(si32 mode)
        {
            auto& ipcio = *_globals<void>::ipcio;

            os::set_palette(mode);
            os::vgafont_update(mode);

            auto input = std::thread{ [&]() { reader(mode); } };

            while (output(ipcio.recv()))
            { }
            os::rst_palette(mode);

            ipcio.stop();
            signal.reset();
            input.join();
        }
    };

    template<class V> xipc        tty::_globals<V>::ipcio;
    template<class V> conmode     tty::_globals<V>::state;
    template<class V> testy<twod> tty::_globals<V>::winsz;

    class pty // Note: STA.
    {
        #if defined(_WIN32)

            HPCON  hPC      { INVALID_FD };
            HANDLE hProcess { INVALID_FD };
            HANDLE hThread  { INVALID_FD };
            HANDLE gameover { INVALID_FD }; // ConPTY do not close pipe handles when client process exits,
            std::thread client_exit_waiter; // so we need to catch the process ending.
            // Note: Not closing STDERR_FD (STDERR_FD and STDIN_FD the same)
            //       causes the reading process to not stop reading when only STDIN_FD is closed.
            //       An open STDERR_FD on the client side blocks the read interrupt on the ConPTY side.

        #else

            pid_t pid = 0;

        #endif

        ipc::ptycon               termlink{};
        testy<twod>               termsize{};
        std::thread               stdinput{};
        std::thread               stdwrite{};
        std::function<void(view)> receiver{};
        std::function<void(si32)> shutdown{};
        text                      writebuf{};
        std::mutex                writemtx{};
        std::condition_variable   writesyn{};

    public:
        ~pty()
        {
            log("xpty: dtor started");
            if (termlink)
            {
                wait_child();
            }
            if (stdwrite.joinable())
            {
                writesyn.notify_one();
                log("xpty: id: ", stdwrite.get_id(), " writing thread joining");
                stdwrite.join();
            }
            if (stdinput.joinable())
            {
                log("xpty: id: ", stdinput.get_id(), " reading thread joining");
                stdinput.join();
            }
            #if defined(_WIN32)
                auto id = client_exit_waiter.get_id();
                if (client_exit_waiter.joinable())
                {
                    log("xpty: id: ", id, " child process waiter thread joining");
                    client_exit_waiter.join();
                }
                log("xpty: id: ", id, " child process waiter thread joined");
            #endif
            log("xpty: dtor complete");
        }
        
        operator bool () { return termlink; }

        void start(text cmdline, twod winsz, std::function<void(view)> input_hndl
                                           , std::function<void(si32)> shutdown_hndl)
        {
            receiver = input_hndl;
            shutdown = shutdown_hndl;
            log("xpty: new child process: ", cmdline);

            #if defined(_WIN32)

                termsize(winsz);
                auto s_pipe_r = INVALID_FD;
                auto s_pipe_w = INVALID_FD;
                auto m_pipe_r = INVALID_FD;
                auto m_pipe_w = INVALID_FD;
                auto startinf = STARTUPINFOEX{ sizeof(STARTUPINFOEX) };
                auto procsinf = PROCESS_INFORMATION{};
                auto attrbuff = std::vector<uint8_t>{};

                auto pseudo = [&]()
                {
                    auto errcode = HRESULT{ E_UNEXPECTED };
                    auto dwFlags = DWORD{ 0 };

                    if (::CreatePipe(&m_pipe_r, &s_pipe_w, nullptr, 0)
                     && ::CreatePipe(&s_pipe_r, &m_pipe_w, nullptr, 0))
                    {
                        auto consz = COORD{};
                        consz.X = winsz.x;
                        consz.Y = winsz.y;
                        errcode = ::CreatePseudoConsole(consz, s_pipe_r, s_pipe_w, dwFlags, &hPC);
                        os::close(s_pipe_w);
                        os::close(s_pipe_r);
                    }
                    return errcode ? faux : true;
                };
                auto fillup = [&]()
                {
                    auto attr_size = SIZE_T{ 0 };
                    ::InitializeProcThreadAttributeList(nullptr, 1, 0, &attr_size);
                    attrbuff.resize(attr_size);
                    startinf.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrbuff.data());

                    if (::InitializeProcThreadAttributeList(startinf.lpAttributeList, 1, 0, &attr_size)
                     && ::UpdateProcThreadAttribute( startinf.lpAttributeList,
                                                     0,
                                                     PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                                     hPC,
                                                     sizeof(hPC),
                                                     nullptr,
                                                     nullptr))
                    {
                        return true;
                    }
                    else return faux;
                };
                auto create = [&]()
                {
                    startinf.StartupInfo.dwFlags |= STARTF_USESTDHANDLES; // Force the new process to not inherit the redirected default handles.
                                                                          // https://github.com/microsoft/terminal/issues/11276#issuecomment-923207186
                    auto result = ::CreateProcessA( nullptr,                      // lpApplicationName
                                                    cmdline.data(),               // lpCommandLine
                                                    nullptr,                      // lpProcessAttributes
                                                    nullptr,                      // lpThreadAttributes
                                                    FALSE,                        // bInheritHandles
                                                    EXTENDED_STARTUPINFO_PRESENT, // dwCreationFlags (override startupInfo type)
                                                    nullptr,                      // lpCurrentDirectory
                                                    nullptr,                      // lpEnvironment
                                                    &startinf.StartupInfo,        // lpStartupInfo (ptr to STARTUPINFOEX)
                                                    &procsinf);                   // lpProcessInformation
                    return result;
                };
                if (pseudo()
                 && fillup()
                 && create())
                {
                    hProcess = procsinf.hProcess;
                    hThread  = procsinf.hThread;
                    gameover = ::CreateEvent( NULL,   // security attributes
                                              FALSE,  // auto-reset
                                              FALSE,  // initial state
                                              NULL);
                    client_exit_waiter = std::thread([&] // ConPTY do not catch the process ending. Or write-handle leaks on ConPTY side.
                    {
                        os::select(hProcess, [](){ log("xpty: child process terminated itself"); },
                                   gameover, [](){ log("xpty: child process will be terminated forcibly"); });
                        os::close(gameover);
                        if (termlink)
                        {
                            auto exit_code = wait_child();
                            shutdown(exit_code);
                        }
                        log("xpty: child process waiter ended");
                    });
                    termlink.set(m_pipe_r, m_pipe_w);
                    log("xpty: conpty created: ", winsz);
                }
                else log("xpty: child process creation error ", ::GetLastError());

                //todo workaround for GH#10400 (resolved) https://github.com/microsoft/terminal/issues/10400
                std::this_thread::sleep_for(250ms);

            #else

                auto fdm = ::posix_openpt(O_RDWR | O_NOCTTY); // Get master PTY.
                auto rc1 = ::grantpt     (fdm);               // Grant master PTY file access.
                auto rc2 = ::unlockpt    (fdm);               // Unlock master PTY.
                auto fds = ::open(ptsname(fdm), O_RDWR);      // Open slave PTY via string ptsname(fdm).

                termlink.set(fdm, fdm);
                resize(winsz);

                pid = ::fork();
                if (pid == 0) // Child branch.
                {
                    os::close(fdm);
                    ok(::setsid(), "setsid error"); // Make the current process a new session leader, return process group id.

                    // In order to receive WINCH signal make fds the controlling
                    // terminal of the current process.
                    // Current process must be a session leader (::setsid()) and not have
                    // a controlling terminal already.
                    // arg = 0: 1 - to stole fds from another process, it doesn't matter here.
                    ok(::ioctl(fds, TIOCSCTTY, 0), "xpty: assign controlling terminal error");

                    ::signal(SIGINT,  SIG_DFL); // Reset control signals to default values.
                    ::signal(SIGQUIT, SIG_DFL); //
                    ::signal(SIGTSTP, SIG_DFL); //
                    ::signal(SIGTTIN, SIG_DFL); //
                    ::signal(SIGTTOU, SIG_DFL); //
                    ::signal(SIGCHLD, SIG_DFL); //

                    ::dup2 (fds, STDIN_FD);  // Assign stdio lines atomically
                    ::dup2 (fds, STDOUT_FD); // = close(new);
                    ::dup2 (fds, STDERR_FD); // fcntl(old, F_DUPFD, new)
                    os::close(fds);

                    auto args = os::split_cmdline(cmdline);
                    auto argv = std::vector<char*>{};
                    for (auto& c : args)
                    {
                        argv.push_back(c.data());
                    }
                    argv.push_back(nullptr);

                    ::setenv("TERM", "xterm-256color", 1); //todo too hacky
                    ok(::execvp(argv.front(), argv.data()), "execvp failed");
                    os::exit(1, "xpty: exec error ", errno);
                }

                // Parent branch.
                os::close(fds);

            #endif

            stdinput = std::thread([&] { read_socket_thread(); });
            stdwrite = std::thread([&] { send_socket_thread(); });

            writesyn.notify_one(); // Flush temp buffer.
        }

        si32 wait_child()
        {
            auto exit_code = si32{};
            log("xpty: wait child process, tty=", termlink);
            termlink.stop();

            #if defined(_WIN32)

                ::ClosePseudoConsole(hPC);
                auto code = DWORD{ 0 };
                if (!::GetExitCodeProcess(hProcess, &code))
                {
                    log("xpty: GetExitCodeProcess() return code: ", ::GetLastError());
                }
                else if (code == STILL_ACTIVE)
                {
                    log("xpty: child process still running");
                    auto result = WAIT_OBJECT_0 == ::WaitForSingleObject(hProcess, 10000 /*10 seconds*/);
                    if (!result || !::GetExitCodeProcess(hProcess, &code))
                    {
                        ::TerminateProcess(hProcess, 0);
                        code = 0;
                    }
                }
                else log("xpty: child process exit code ", code);
                exit_code = code;
                if (gameover != INVALID_FD) ::SetEvent(gameover);
                os::close(hProcess);
                os::close(hThread);

            #else

            if (pid != 0)
            {
                auto status = int{};
                ok(::kill(pid, SIGKILL), "kill(pid, SIGKILL) failed");
                ok(::waitpid(pid, &status, 0), "waitpid(pid) failed"); // Wait for the child to avoid zombies.
                if (WIFEXITED(status))
                {
                    exit_code = WEXITSTATUS(status);
                    log("xpty: child process exit code ", exit_code);
                }
                else
                {
                    exit_code = 0;
                    log("xpty: warning: child process exit code not detected");
                }
            }

            #endif
            log("xpty: child waiting complete");
            return exit_code;
        }
        void read_socket_thread()
        {
            log("xpty: id: ", stdinput.get_id(), " reading thread started");
            auto flow = text{};
            while (termlink)
            {
                auto shot = termlink.recv();
                if (shot && termlink)
                {
                    flow += shot;
                    auto crop = ansi::purify(flow);
                    receiver(crop);
                    flow.erase(0, crop.size()); // Delete processed data.
                }
                else break;
            }
            if (termlink)
            {
                auto exit_code = wait_child();
                shutdown(exit_code);
            }
            log("xpty: id: ", stdinput.get_id(), " reading thread ended");
        }
        void send_socket_thread()
        {
            log("xpty: id: ", stdwrite.get_id(), " writing thread started");
            auto guard = std::unique_lock{ writemtx };
            auto cache = text{};
            while ((void)writesyn.wait(guard, [&]{ return writebuf.size() || !termlink; }), termlink)
            {
                std::swap(cache, writebuf);
                guard.unlock();

                if (termlink.send(cache)) cache.clear();
                else                      break;

                guard.lock();
            }
            log("xpty: id: ", stdwrite.get_id(), " writing thread ended");
        }
        void resize(twod const& newsize)
        {
            if (termlink && termsize(newsize))
            {
                #if defined(_WIN32)

                    auto winsz = COORD{};
                    winsz.X = newsize.x;
                    winsz.Y = newsize.y;
                    auto hr = ::ResizePseudoConsole(hPC, winsz);
                    if (hr != S_OK) log("xpty: ResizePseudoConsole error, ", hr);

                #else

                    winsize winsz;
                    winsz.ws_col = newsize.x;
                    winsz.ws_row = newsize.y;
                    ok(::ioctl(termlink.get_w(), TIOCSWINSZ, &winsz), "ioctl(termlink.get(), TIOCSWINSZ) failed");

                #endif
            }
        }
        void write(view data)
        {
            auto guard = std::lock_guard{ writemtx };
            writebuf += data;
            if (termlink) writesyn.notify_one();
        }
    };

    namespace direct
    {
        class pty
        {
            #if defined(_WIN32)

                HANDLE hProcess { INVALID_FD };
                HANDLE hThread  { INVALID_FD };
                DWORD  Proc_id  { 0          };

            #else

                pid_t Proc_id = 0;

            #endif

            ipc::direct               termlink{};
            testy<twod>               termsize{};
            std::thread               stdinput{};
            std::thread               stdwrite{};
            std::thread               stderror{};
            std::function<void(view)> receiver{};
            std::function<void(view)> loggerfx{};
            std::function<void(si32)> shutdown{};
            std::function<void(si32)> preclose{};
            text                      writebuf{};
            std::mutex                writemtx{};
            std::condition_variable   writesyn{};

        public:
            ~pty()
            {
                log("dtvt: dtor started");
                if (termlink)
                {
                    stop();
                }
                if (stdwrite.joinable())
                {
                    writesyn.notify_one();
                    log("dtvt: id: ", stdwrite.get_id(), " writing thread joining");
                    stdwrite.join();
                }
                if (stdinput.joinable())
                {
                    log("dtvt: id: ", stdinput.get_id(), " reading thread joining");
                    stdinput.join();
                }
                if (stderror.joinable())
                {
                    log("dtvt: id: ", stderror.get_id(), " logging thread joining");
                    stderror.join();
                }
                log("dtvt: dtor complete");
            }
            
            operator bool () { return termlink; }

            auto start(text cmdline, twod winsz, std::function<void(view)> input_hndl
                                               , std::function<void(view)> logs_hndl
                                               , std::function<void(si32)> preclose_hndl
                                               , std::function<void(si32)> shutdown_hndl)
            {
                receiver = input_hndl;
                loggerfx = logs_hndl;
                preclose = preclose_hndl;
                shutdown = shutdown_hndl;
                termsize(winsz);
                log("dtvt: new child process: ", cmdline);

                #if defined(_WIN32)

                    auto s_pipe_r = INVALID_FD;
                    auto s_pipe_w = INVALID_FD;
                    auto s_pipe_l = INVALID_FD;
                    auto m_pipe_r = INVALID_FD;
                    auto m_pipe_w = INVALID_FD;
                    auto m_pipe_l = INVALID_FD;
                    auto startinf = STARTUPINFOEX{ sizeof(STARTUPINFOEX) };
                    auto procsinf = PROCESS_INFORMATION{};
                    auto attrbuff = std::vector<uint8_t>{};
                    auto attrsize = SIZE_T{ 0 };
                    auto stdhndls = std::array<HANDLE, 3>{};

                    auto tunnel = [&]()
                    {
                        auto sa = SECURITY_ATTRIBUTES{};
                        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
                        sa.lpSecurityDescriptor = NULL;
                        sa.bInheritHandle = TRUE;
                        if (::CreatePipe(&m_pipe_r, &s_pipe_w, &sa, 0)
                         && ::CreatePipe(&s_pipe_r, &m_pipe_w, &sa, 0)
                         && ::CreatePipe(&m_pipe_l, &s_pipe_l, &sa, 0))
                        {
                            os::legacy::send_dmd(m_pipe_w, winsz);

                            startinf.StartupInfo.dwFlags    = STARTF_USESTDHANDLES;
                            startinf.StartupInfo.hStdInput  = s_pipe_r;
                            startinf.StartupInfo.hStdOutput = s_pipe_w;
                            startinf.StartupInfo.hStdError  = s_pipe_l;
                            return true;
                        }
                        else
                        {
                            os::close(m_pipe_w);
                            os::close(m_pipe_r);
                            os::close(m_pipe_l);
                            return faux;
                        }
                    };
                    auto fillup = [&]()
                    {
                        stdhndls[0] = s_pipe_r;
                        stdhndls[1] = s_pipe_w;
                        stdhndls[2] = s_pipe_l;
                        ::InitializeProcThreadAttributeList(nullptr, 1, 0, &attrsize);
                        attrbuff.resize(attrsize);
                        startinf.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrbuff.data());

                        if (::InitializeProcThreadAttributeList(startinf.lpAttributeList, 1, 0, &attrsize)
                         && ::UpdateProcThreadAttribute( startinf.lpAttributeList,
                                                         0,
                                                         PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                                         &stdhndls,
                                                         sizeof(stdhndls),
                                                         nullptr,
                                                         nullptr))
                        {
                            return true;
                        }
                        else return faux;
                    };
                    auto create = [&]()
                    {
                        return ::CreateProcessA( nullptr,                      // lpApplicationName
                                                 cmdline.data(),               // lpCommandLine
                                                 nullptr,                      // lpProcessAttributes
                                                 nullptr,                      // lpThreadAttributes
                                                 TRUE,                         // bInheritHandles
                                                 DETACHED_PROCESS |            // create without attached console, dwCreationFlags
                                                 EXTENDED_STARTUPINFO_PRESENT, // override startupInfo type
                                                 nullptr,                      // lpCurrentDirectory
                                                 nullptr,                      // lpEnvironment
                                                 &startinf.StartupInfo,        // lpStartupInfo (ptr to STARTUPINFO)
                                                 &procsinf);                   // lpProcessInformation
                    };

                    if (tunnel()
                     && fillup()
                     && create())
                    {
                        hProcess = procsinf.hProcess;
                        Proc_id  = procsinf.dwProcessId;
                        hThread  = procsinf.hThread;
                        termlink.set(m_pipe_r, m_pipe_w, m_pipe_l);
                        log("dtvt: conpty created: ", winsz);
                    }
                    else log("dtvt: child process creation error ", ::GetLastError());

                    os::close(s_pipe_w); // Close inheritable handles to avoid deadlocking at process exit.
                    os::close(s_pipe_r); // Only when all write handles to the pipe are closed, the ReadFile function returns zero.
                    os::close(s_pipe_l); //

                #else

                    fd_t to_server[2] = { INVALID_FD, INVALID_FD };
                    fd_t to_client[2] = { INVALID_FD, INVALID_FD };
                    fd_t to_srvlog[2] = { INVALID_FD, INVALID_FD };
                    ok(::pipe(to_server), "dtvt: server ipc error");
                    ok(::pipe(to_client), "dtvt: client ipc error");
                    ok(::pipe(to_srvlog), "dtvt: srvlog ipc error");

                    termlink.set(to_server[0], to_client[1], to_srvlog[0]);
                    os::legacy::send_dmd(to_client[1], winsz);

                    Proc_id = ::fork();
                    if (Proc_id == 0) // Child branch.
                    {
                        os::close(to_server[0]);
                        os::close(to_client[1]);
                        os::close(to_srvlog[0]);

                        ::signal(SIGINT,  SIG_DFL); // Reset control signals to default values.
                        ::signal(SIGQUIT, SIG_DFL); //
                        ::signal(SIGTSTP, SIG_DFL); //
                        ::signal(SIGTTIN, SIG_DFL); //
                        ::signal(SIGTTOU, SIG_DFL); //
                        ::signal(SIGCHLD, SIG_DFL); //

                        ::dup2(to_client[0], STDIN_FD ); // Assign stdio lines atomically
                        ::dup2(to_server[1], STDOUT_FD); // = close(new); fcntl(old, F_DUPFD, new).
                        ::dup2(to_srvlog[1], STDERR_FD); // Used for the logs output
                        os::close(to_client[0]);
                        os::close(to_server[1]);
                        os::close(to_srvlog[1]);

                        auto args = os::split_cmdline(cmdline);
                        auto argv = std::vector<char*>{};
                        for (auto& c : args)
                        {
                            argv.push_back(c.data());
                        }
                        argv.push_back(nullptr);

                        ok(::execvp(argv.front(), argv.data()), "execvp failed");
                        os::exit(1, "dtvt: exec error ", errno);
                    }

                    // Parent branch.
                    os::close(to_client[0]);
                    os::close(to_server[1]);
                    os::close(to_srvlog[1]);

                #endif

                stdinput = std::thread([&] { read_socket_thread(); });
                stdwrite = std::thread([&] { send_socket_thread(); });
                stderror = std::thread([&] { logs_socket_thread(); });

                writesyn.notify_one(); // Flush temp buffer.

                return Proc_id;
            }

            void stop()
            {
                termlink.shut();
                writesyn.notify_one();
            }
            si32 wait_child()
            {
                auto exit_code = si32{};
                log("dtvt: wait child process, tty=", termlink);
                if (termlink)
                {
                    termlink.shut();
                }

                #if defined(_WIN32)

                    auto code = DWORD{ 0 };
                    if (!::GetExitCodeProcess(hProcess, &code))
                    {
                        log("dtvt: GetExitCodeProcess() return code: ", ::GetLastError());
                    }
                    else if (code == STILL_ACTIVE)
                    {
                        log("dtvt: child process still running");
                        //std::this_thread::sleep_for(15s);
                        auto result = WAIT_OBJECT_0 == ::WaitForSingleObject(hProcess, 10000 /*10 seconds*/);
                        if (!result || !::GetExitCodeProcess(hProcess, &code))
                        {
                            ::TerminateProcess(hProcess, 0);
                            code = 0;
                        }
                    }
                    else log("dtvt: child process exit code ", code);
                    exit_code = code;
                    os::close(hProcess);
                    os::close(hThread);

                #else

                if (Proc_id != 0)
                {
                    int status;
                    ok(::kill(Proc_id, SIGKILL), "kill(pid, SIGKILL) failed");
                    ok(::waitpid(Proc_id, &status, 0), "waitpid(pid) failed"); // Wait for the child to avoid zombies.
                    if (WIFEXITED(status))
                    {
                        exit_code = WEXITSTATUS(status);
                        log("dtvt: child process exit code ", exit_code);
                    }
                    else
                    {
                        exit_code = 0;
                        log("dtvt: warning: child process exit code not detected");
                    }
                }

                #endif
                log("dtvt: child waiting complete");
                return exit_code;
            }
            void logs_socket_thread()
            {
                auto thread_id = stderror.get_id();
                log("dtvt: id: ", thread_id, " logging thread for process:", Proc_id, " started");
                auto buff = std::vector<char>(PIPE_BUF);
                while (termlink)
                {
                    auto shot = termlink.rlog(buff.data(), buff.size());
                    if (shot && termlink)
                    {
                        loggerfx(shot);
                    }
                    else break;
                }
                log("dtvt: id: ", thread_id, " logging thread ended");
            }
            void read_socket_thread()
            {
                using frame_t = ansi::dtvt::header_t<ansi::dtvt::frame_type::any>;

                log("dtvt: id: ", stdinput.get_id(), " reading thread started");

                auto flow = text{};
                while (termlink)
                {
                    auto shot = termlink.recv();
                    if (shot && termlink)
                    {
                        flow += shot;
                        if (auto crop = frame_t::intergity(flow))
                        {
                            receiver(crop);
                            flow.erase(0, crop.size()); // Delete processed data.
                        }
                    }
                    else break;
                }
                //todo test
                //if (termlink)
                {
                    preclose(0); //todo send msg from the client app
                    auto exit_code = wait_child();
                    shutdown(exit_code);
                }
                log("dtvt: id: ", stdinput.get_id(), " reading thread ended");
            }
            void send_socket_thread()
            {
                log("dtvt: id: ", stdwrite.get_id(), " writing thread started");
                auto guard = std::unique_lock{ writemtx };
                auto cache = text{};
                while ((void)writesyn.wait(guard, [&]{ return writebuf.size() || !termlink; }), termlink)
                {
                    std::swap(cache, writebuf);
                    guard.unlock();

                    if (termlink.send(cache)) cache.clear();
                    else                      break;

                    guard.lock();
                }
                guard.unlock(); // To avoid debug output deadlocking. See ui::dtvt::request_debug() - e2::debug::logs
                log("dtvt: id: ", stdwrite.get_id(), " writing thread ended");
            }
            void resize(twod const& newsize)
            {
                if (termlink && termsize(newsize))
                {
                    write(ansi::win(newsize));
                }
            }
            void write(view data)
            {
                auto guard = std::lock_guard{ writemtx };
                writebuf += data;
                if (termlink) writesyn.notify_one();
            }
        };
    }
}

#endif // NETXS_SYSTEM_HPP