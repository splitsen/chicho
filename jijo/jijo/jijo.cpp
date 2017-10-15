// boost::program_options must be built:
// C:\dev\boost_1_65_1>bootstrap.bat
// C:\dev\boost_1_65_1>bjam.exe --build-type=minimal msvc stage --with-program_options address-model=64

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#define BOOST_DATE_TIME_NO_LIB
#define BOOST_REGEX_NO_LIB
#include <asio.hpp>

#include <boost/program_options.hpp>
#include <chrono>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <thread>
#include <atomic>
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
    for (int i = 0; i < len; ++i)
        v[i] = alphanum[rand() % (sizeof(alphanum) - 1)];

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

    pair<size_t, size_t> eval() const
    {
        size_t dir_total = 0, files_total = 0;
        for (size_t i = 1; i <= depth; i++)
            dir_total += static_cast<size_t>(pow(dir_count, i));
        files_total = dir_total * file_count;

        return make_pair(dir_total, files_total);
    }

    bool Help() const
    {
        return is_help;
    }
};

Config Get_config(int ac, char* av[])
{
    size_t HW_cores = max<size_t>(1, thread::hardware_concurrency());
    Config cfg;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Produce this help message.")
        ("target,t", po::value<string>(&cfg.target_dir)->default_value("."),
            "Generate in that directory, default is current, if not exist create.\n"
            "chicho does not add or modify an existing directory."
            )
        ("depth,d", po::value<size_t>(&cfg.depth)->default_value(3),
            "Directories depth.")
        ("dir_count,r", po::value<size_t>(&cfg.dir_count)->default_value(3),
            "Directories count per depth level.")
        ("file_count,f", po::value<size_t>(&cfg.file_count)->default_value(5),
            "Files count count per directory.")
        ("file_length,l", po::value<size_t>(&cfg.file_length)->default_value(1049),
            "File length.")
        ("eval,e", "Display count of files, total bytes with current args.")
        ("file_ext", po::value<string>(&cfg.file_ext)->default_value("rdm"),
            "File extension.")
        ("name_length", po::value<size_t>(&cfg.name_length)->default_value(5),
            "Directory/file name length.")
        ("chunk", po::value<size_t>(&cfg.chunk)->default_value(1048),
            "Chunk size.\n"
            "Files are filled with a buffer of random character, the size of this buffer if the chunk size.")
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
    if (vm.count("eval")) {
        fs::space_info devi = fs::space(cfg.target_dir);
        cout << "Target dir: " << fs::absolute(fs::path(cfg.target_dir)) << endl;
        cout << "         Capacity       Free      Available\n"
            << "       " << devi.capacity << "   "
            << devi.free << "   " << devi.available << endl;
        const auto total = cfg.eval();
        const auto total_bytes = total.second * cfg.file_length;
        cout << "Chicho could create:" << endl << "\t" <<
            total.second << " files in " << total.first << " directories." << endl << "\t" <<
            "for a total of " << total_bytes << " bytes" <<
            " (" << static_cast<int>((100. * total_bytes) / double(devi.available)) << "% of available)" <<
            endl;
        return Config(true);
    }

    return cfg;
}

class Chicho : private boost::noncopyable
{
    atomic<size_t> _dir_count = 0;
    atomic<size_t> _files_count = 0;
    size_t _files_total = 0;

    const Config& _cfg;
    fs::path _target_dir;
    chrono::time_point<steady_clock> _start;
    asio::signal_set _signals;

    void inc_dir_count()
    {
        _dir_count++;
    }

    size_t get_dir_count() const
    {
        return _dir_count;
    }

    void inc_files_count()
    {
        _files_count++;
        if (_files_total == _files_count)
            stop();
    }

    size_t get_files_count() const
    {
        return _files_count;
    }

    asio::io_service& get_io_service()
    {
        return _signals.get_io_service();
    }

    bool is_stopped()
    {
        return get_io_service().stopped();
    }

    void status(size_t files_count, size_t dir_count)
    {
        auto elapsed = (steady_clock::now() - _start);
        minutes mm = duration_cast<minutes>(elapsed);
        seconds ss = duration_cast<seconds>(elapsed % minutes(1));
        milliseconds ms = duration_cast<milliseconds>(elapsed % seconds(1));
        std::cout << files_count << " files in " << dir_count << " directories in " <<
            setfill('0') << setw(2) << mm.count() << " mns " << setw(2) << ss.count() << "." << setw(3) << ms.count() << " secs" <<
            endl;
    }

    void handle_stop(const asio::error_code& error, int signal_number)
    {
        stop();
    }

    void stop()
    {
        get_io_service().stop();
    }

    fs::path new_file_name(const fs::path& parent) const
    {
        fs::path new_file = parent / to_string(gen_random(_cfg.name_length));
        new_file += "." + _cfg.file_ext;
        return new_file;
    }

    vector<char> get_chunck(size_t len=0)
    {
        return gen_random(len ? len : _cfg.chunk);
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
                stop();
                cerr << "Unable to generate a new directory name (" << new_dir << ")" << endl;
                return fs::path();
            }
            new_dir = parent / to_string(gen_random(_cfg.name_length));
        }

        if (!fs::create_directory(new_dir, ec))
        {
            stop();
            cerr << "Unable to create new directory (" << new_dir << ")" << endl;
            return fs::path();
        }
        inc_dir_count();

        get_io_service().post([this, new_dir]() {
            file_generation(new_dir); });

        return new_dir;
    }

    void file_write(shared_ptr<pair<ofstream, asio::strand>> file_strand, size_t size=0 )
    {
        auto& file = file_strand->first;

        try {
            vector<char> v = get_chunck(size);
            file.write(&v[0], v.size());
        }
        catch (const std::ios_base::failure& e)
        {
            stop();
            cerr << e.what() << endl;
        }
    }

    void file_create(fs::path new_file)
    {
        try {
            ofstream file(new_file);
            file.close();
        }
        catch (const std::ios_base::failure& e)
        {
            stop();
            cerr << e.what() << endl;
        }
    }

    void file_generation(fs::path new_dir)
    {
        for (size_t f = 0; f < _cfg.file_count && !is_stopped(); f++)
        {
            fs::path new_file = new_file_name(new_dir);
            error_code ec;
            size_t count = 0;
            while (exists(new_file, ec))
            {
                if (++count > 10)
                {
                    stop();
                    cerr << "Unable to generate a new file name (" << new_file << ")" << endl;
                    return;
                }
                fs::path new_file = new_file_name(new_dir);
            }

            auto& file_length = _cfg.file_length;
            shared_ptr<pair<ofstream, asio::strand>> file_strand;
            try {
                inc_files_count();
                if (!file_length)
                {
                    get_io_service().post([this, new_file]() {
                        file_create(new_file);});
                    continue;
                }
                file_strand = make_shared<pair<ofstream, asio::strand>>(new_file, get_io_service());
            }
            catch (const std::ios_base::failure& e)
            {
                stop();
                cerr << e.what() << endl;
                return;
            }

            auto& file = file_strand->first;
            // set to throw
            file.exceptions(file.failbit);

            size_t i = 0;
            auto& chunk = _cfg.chunk;
            for (; i < (file_length % chunk) && !is_stopped(); i++)
                file_strand->second.post([this, file_strand]() {
                    file_write(file_strand);});
            if (is_stopped())
                return;

            const size_t fillup = file_length - i * chunk;
            if (fillup)
                file_strand->second.post([this, file_strand, fillup]() {
                    file_write(file_strand, fillup); });
        }
    }

    vector<fs::path> generate(const fs::path& parent)
    {
        vector<fs::path> end_paths;
        for (size_t h = 0; h < _cfg.dir_count && !is_stopped(); h++)
        {
            fs::path generated = create_directory_and_files(parent);
            if (is_stopped())
                return vector<fs::path>();
            end_paths.push_back(generated);
        }
        return end_paths;
    }

    void iterate()
    {
        vector<fs::path> end_paths = generate(_target_dir);

        for (size_t v = 1; v < _cfg.depth && !is_stopped(); v++)
        {
            vector<fs::path> new_end_paths;
            for_each(end_paths.begin(), end_paths.end(),
                [this, &new_end_paths](const fs::path& ipath)
            {
                if (is_stopped())
                    return;
                vector<fs::path> generated = generate(ipath);
                if (is_stopped())
                    return;
                new_end_paths.insert(new_end_paths.end(), generated.begin(), generated.end());
            });
            end_paths = new_end_paths;
        }
    }

public:
    Chicho(const Config& cfg, asio::io_service& io_service)
        :_dir_count(0)
        , _files_count(0)
        , _files_total(cfg.eval().second)
        , _cfg(cfg)
        , _start(steady_clock::now())
        , _signals(io_service)
    {
        // Register to handle the signals that indicate when the server should exit.
        _signals.add(SIGINT);
        _signals.add(SIGTERM);
#if defined(SIGQUIT)
        _signals_.add(SIGQUIT);
#endif // defined(SIGQUIT)
        _signals.async_wait([this](const asio::error_code& error, int signal_number) {
            handle_stop(error, signal_number); });

        _target_dir = cfg.target_dir;
        if (!exists(_target_dir))
        {
            fs::create_directory(_target_dir);
            inc_dir_count();
        }
    }

    ~Chicho()
    {
        status(_files_count, _dir_count);
    }

    void run()
    {
        get_io_service().post([this]() {
            iterate();});
    }
};

int main(int ac, char* av[])
{
    try {
        const Config cfg = Get_config(ac, av);
        if (cfg.Help())
            return 0;

        asio::io_service io_service;

        Chicho chicho(cfg, io_service);
        chicho.run();

        // Create a pool of threads to run all of the io_services.
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
