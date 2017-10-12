// boost::program_options must be built:
// C:\dev\boost_1_65_1>bootstrap.bat
// C:\dev\boost_1_65_1>bjam.exe --build-type=minimal msvc stage --with-program_options address-model=64

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#define BOOST_DATE_TIME_NO_LIB
#define BOOST_REGEX_NO_LIB
#include <asio.hpp>
#include <asio/steady_timer.hpp>

#include <boost/program_options.hpp>
#include <chrono>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <filesystem>

namespace po = boost::program_options;
namespace fs = std::experimental::filesystem;
using namespace std;
using namespace std::chrono;

vector<char> gen_random(size_t len) 
{
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    vector<char> v(len);
    for (int i = 0; i < len; ++i) {
        v[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    return v;
}

string to_string(const vector<char>& v)
{
    return string(v.begin(), v.end());
}

struct Config
{
    Config(bool is_help=false)
        :is_help(is_help)
    {}

    bool is_help;
    size_t chunk, depth, dir_count, file_count, name_length, file_length,
        thread_pool;
    string target_dir, file_ext;
    bool chunk_reused;

    bool Help() const
    {
        return is_help;
    }
};

Config Get_config(int ac, char* av[])
{
    size_t HW_cores = thread::hardware_concurrency();
    Config cfg;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Produce this help message.")
        ("target", po::value<string>(&cfg.target_dir)->default_value("."),
            "Generate in that directory, default is current, if not exist create.\n"
            "chicho does not add or modify an existing directory."
            )
        ("chunk", po::value<size_t>(&cfg.chunk)->default_value(1048),
            "Chunk size.\n"
            "Files are filled with a buffer of random character, the size of this buffer if the chunk size.\n"
            "Chunk allows to set RAM memory impact versus number of calls to the OS write file API"
            )
        ("chunk_reused", po::value<bool>(&cfg.chunk_reused)->default_value(false),
            "Use the same chunk for all files.")
        ("depth", po::value<size_t>(&cfg.depth)->default_value(3),
            "Directories depth.")
        ("dir_count", po::value<size_t>(&cfg.dir_count)->default_value(3),
            "Directories count per depth level.")
        ("file_count", po::value<size_t>(&cfg.file_count)->default_value(5),
            "Files count count per directory.")
        ("name_length", po::value<size_t>(&cfg.name_length)->default_value(5),
            "Directory/file name length.")
        ("file_length", po::value<size_t>(&cfg.file_length)->default_value(1049),
            "File length.")
        ("file_ext", po::value<string>(&cfg.file_ext)->default_value("rdm"),
            "File extension.")
        ("thread_pool", po::value<size_t>(&cfg.thread_pool)->default_value(HW_cores),
            "thread pool size, default to count of HW concurrent threads.")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(ac, av, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        cout << desc << "\n";
        return Config(true);
    }

    return cfg;
}

class Chicho : private boost::noncopyable
{
    mutable shared_mutex _dir_count_mutex;
    size_t _dir_count = 0;

    mutable shared_mutex _files_count_mutex;
    size_t _files_count = 0;

    size_t _files_total = 0;

    const Config& _cfg;
    fs::path _target_dir;
    const chrono::time_point<steady_clock> _start;
    vector<char> _buffer;
    chrono::seconds _verbose_period;
    asio::steady_timer _verbose_timer;

    void inc_dir_count()
    {
        unique_lock<shared_mutex> lock(_dir_count_mutex);
        _dir_count++;
    }

    size_t get_dir_count() const
    {
        shared_lock<shared_mutex> lock(_dir_count_mutex);
        return _dir_count;
    }

    void inc_files_count()
    {
        unique_lock<shared_mutex> lock(_files_count_mutex);
        _files_count++;
        if (_files_total == _files_count)
        {
            asio::error_code ec;
            _verbose_timer.cancel(ec);
            get_io_service().stop();
        }
    }

    size_t get_files_count() const
    {
        shared_lock<shared_mutex> lock(_files_count_mutex);
        return _files_count;
    }

    asio::io_service& get_io_service()
    {
        return _verbose_timer.get_io_service();
    }

    void on_verbose_timer(const error_code& error)
    {
        // timer was cancelled
        if (error == asio::error::operation_aborted)
            return;

        status();
        if (!_verbose_timer.get_io_service().stopped() )
        {
            _verbose_timer.expires_from_now(_verbose_period);
            _verbose_timer.async_wait([this](const error_code& error)
                {this->on_verbose_timer(error); });
        }
    }

    void status() const
    {
        auto elapsed = (steady_clock::now() - _start);
        minutes mm = duration_cast<minutes>(elapsed);
        seconds ss = duration_cast<seconds>(elapsed % minutes(1));
        milliseconds ms = duration_cast<milliseconds>(elapsed % seconds(1));
        std::cout << get_files_count() << " files in " << get_dir_count() << " directories in " <<
            setfill('0') << setw(2) << mm.count() << " mns " << setw(2) << ss.count() << "." << setw(3) << ms.count() << " secs" <<
            endl;
    }

public:
    Chicho(const Config& cfg, asio::io_service& io_service)
        :_dir_count(0)
        ,_files_count(0)
        ,_files_total(0)
        ,_cfg(cfg)
        ,_start(steady_clock::now())
        ,_verbose_period(1)
        ,_verbose_timer(io_service, chrono::steady_clock::now() + _verbose_period)
    {
        size_t dir_total = 0;
        for (size_t i = 1; i <= cfg.depth; i++)
            dir_total += static_cast<size_t>(pow(cfg.dir_count, i));
        _files_total = dir_total * cfg.file_count;

        if (_cfg.chunk_reused)
            _buffer = gen_random(_cfg.chunk);

        _verbose_timer.async_wait([this](const error_code& error) 
            {this->on_verbose_timer(error); });

        _target_dir = cfg.target_dir;
        std::error_code ec;
        if (!exists(_target_dir, ec))
        {
            fs::create_directory(_target_dir);
            inc_dir_count();
        }
    }

    ~Chicho()
    {
        status();
    }

    void run()
    {
        get_io_service().post([this]() {
            vector<fs::path> end_paths = generate(_cfg.target_dir);
            browse(end_paths);
        });
    }

    void browse(vector<fs::path> end_paths)
    {
        Chicho& chicho = *this;
        for (size_t v = 1; v < _cfg.depth && !get_io_service().stopped(); v++)
        {
            vector<fs::path> new_end_paths;
            for_each(end_paths.begin(), end_paths.end(),
                [this, &new_end_paths](const fs::path& ipath)
            {
                if (get_io_service().stopped())
                    return;
                vector<fs::path> generated = generate(ipath);
                new_end_paths.insert(new_end_paths.end(), generated.begin(), generated.end());
            });
            end_paths = new_end_paths;
        }
    }

    fs::path new_file_name(const fs::path& parent) const
    {
        fs::path new_file = parent / to_string(gen_random(_cfg.name_length));
        new_file += "." + _cfg.file_ext;
        return new_file;
    }

    vector<char> get_chunck(size_t len=0)
    {
        BOOST_ASSERT((_cfg.chunk_reused && !_buffer.empty()) || !_cfg.chunk_reused);

        if(!_cfg.chunk_reused)
            return gen_random(len ? len : _cfg.chunk);
        if(!len || len == _cfg.chunk)
            return _buffer;
        return gen_random(len);
    }

    fs::path create_directory_and_files(
        fs::path parent)
    {
        fs::path new_dir = parent / to_string(gen_random(_cfg.name_length));
        error_code ec;
        size_t count = 0;
        while (exists(new_dir, ec))
        {
            if (++count > 10)
            {
                stringstream ss;
                ss << "Unable to generate a new directory name (" << new_dir << ")";
                throw runtime_error(ss.str());
            }
            new_dir = parent / to_string(gen_random(_cfg.name_length));
        }

        fs::create_directory(new_dir);
        inc_dir_count();

        get_io_service().post([this, new_dir]() {
                this->file_generation(new_dir); });

        return new_dir;
    }

    void file_generation(fs::path new_dir)
    {
        for (size_t f = 0; f < _cfg.file_count && !get_io_service().stopped(); f++)
        {
            fs::path new_file = new_file_name(new_dir);
            error_code ec;
            size_t count = 0;
            while (exists(new_file, ec) && !get_io_service().stopped())
            {
                if (++count > 10)
                {
                    get_io_service().stop();
                    stringstream ss;
                    ss << "Unable to generate a new file name (" << new_file << ")";
                    throw runtime_error(ss.str());
                }
                fs::path new_file = new_file_name(new_dir);
            }
            shared_ptr<pair<ofstream,asio::strand>> file_strand = 
                make_shared<pair<ofstream, asio::strand>>(new_file, get_io_service());
            inc_files_count();

            size_t i = 0;
            auto& file_length = _cfg.file_length;
            auto& chunk = _cfg.chunk;
            for (; i < (file_length % chunk) && !get_io_service().stopped(); i++)
            {
                file_strand->second.post([this, file_strand]() {
                    vector<char> v = this->get_chunck();
                    file_strand->first.write(&v[0], v.size());});

            }
            if (get_io_service().stopped())
                return;
            const size_t fillup = file_length - i * chunk;
            if (fillup)
            {
                size_t size = file_length - i * chunk;
                file_strand->second.post([this, file_strand, size]() {
                    vector<char> v = this->get_chunck(size);
                    file_strand->first.write(&v[0], v.size()); });
            }
        }
    }

    vector<fs::path> generate(const fs::path& parent)
    {
        vector<fs::path> end_paths;
        for (size_t h = 0; h < _cfg.dir_count && !get_io_service().stopped(); h++)
        {
            fs::path generated = create_directory_and_files(parent);
            if (get_io_service().stopped())
                break;
            end_paths.push_back(generated);
        }
        return end_paths;
    }
};

asio::io_service* g_io_service = nullptr;
void sig_handler(int param)
{
    if (!g_io_service)
        return;
    g_io_service->stop();
}

int main(int ac, char* av[])
{
    try {
        const Config cfg = Get_config(ac, av);
        if (cfg.Help())
            return 0;

        asio::io_service io_service;
        g_io_service = &io_service;

        // Register to handle the signals that indicate when the server should exit.
        g_io_service = &io_service;
        signal(SIGINT, sig_handler);
        signal(SIGTERM, sig_handler);
#if defined(SIGQUIT)
        signal(SIGQUIT, sig_handler);
#endif // defined(SIGQUIT)

        Chicho chicho(cfg, io_service);
        chicho.run();

        // Create a pool of threads to run all of the io_services.
        /// The io_service used to perform asynchronous operations.
        std::vector<shared_ptr<thread> > threads;
        for (std::size_t i = 0; i < cfg.thread_pool; ++i)
            threads.push_back( make_shared<thread>(
                [&io_service]() {io_service.run(); }));

        // Wait for all threads in the pool to exit.
        for (std::size_t i = 0; i < threads.size(); ++i)
            threads[i]->join();
    }
    catch (exception& e) {
        cerr << "error: " << e.what() << "\n";
        return 1;
    }
    catch (...) {
        cerr << "Exception of unknown type!\n";
        return 1;
    }

    return 0;
}
